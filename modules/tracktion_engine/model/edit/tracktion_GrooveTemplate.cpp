/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion_engine
{

const char* GrooveTemplate::grooveXmlTag = "GROOVETEMPLATE";

GrooveTemplate::GrooveTemplate()
    : numNotes (16),
      notesPerBeat (2),
      name (TRANS("Unnamed")),
      parameterized (false)
{
}

GrooveTemplate::GrooveTemplate (const juce::XmlElement* node)
    : GrooveTemplate()
{
    if (node != nullptr && node->hasTagName (grooveXmlTag))
    {
        numNotes = node->getIntAttribute ("numberOfNotes", 16);
        notesPerBeat = node->getIntAttribute ("notesPerBeat", 2);
        name = node->getStringAttribute ("name", TRANS("Unnamed"));
        parameterized = node->getBoolAttribute ("parameterized", false);

        forEachXmlChildElementWithTagName (*node, n, "SHIFT")
            latenesses.add ((float) n->getDoubleAttribute ("delta", 0.0));
    }
}

GrooveTemplate::GrooveTemplate (const GrooveTemplate& other)
{
    latenesses = other.latenesses;
    numNotes = other.numNotes;
    notesPerBeat = other.notesPerBeat;
    name = other.name;
}

GrooveTemplate::~GrooveTemplate()
{
}

bool GrooveTemplate::isParameterized() const
{
    return parameterized;
}

const GrooveTemplate& GrooveTemplate::operator= (const GrooveTemplate& other)
{
    latenesses = other.latenesses;
    numNotes = other.numNotes;
    notesPerBeat = other.notesPerBeat;
    name = other.name;
    parameterized = other.parameterized;

    return *this;
}

void GrooveTemplate::setName (const String& n)
{
    name = n;
}

XmlElement* GrooveTemplate::createXml() const
{
    auto node = new juce::XmlElement (grooveXmlTag);
    node->setAttribute ("name", name);
    node->setAttribute ("numberOfNotes", numNotes);
    node->setAttribute ("notesPerBeat", notesPerBeat);
    node->setAttribute ("parameterized", parameterized);

	int lastNonZeroNote = numNotes;
	while (--lastNonZeroNote >= 0)
		if (latenesses[lastNonZeroNote] != 0.0f)
			break;

	for (int i = 0; i <= lastNonZeroNote; ++i)
	{
		auto n = new juce::XmlElement ("SHIFT");
		n->setAttribute ("delta", 0.001 * roundToInt (1000.0 * latenesses[i]));
		node->addChildElement (n);
	}

    return node;
}

void GrooveTemplate::setNumberOfNotes (int notes)
{
    numNotes = jlimit (2, 1024, notes);
}

void GrooveTemplate::setNotesPerBeat (int notes)
{
    notesPerBeat = jlimit (1, 8, notes);
}

float GrooveTemplate::getLatenessProportion (int noteNumber, float strength) const
{
    if (parameterized)
		return latenesses[noteNumber] * strength;

    return latenesses[noteNumber];
}

void GrooveTemplate::setLatenessProportion (int noteNumber, float p, float strength)
{
    while (latenesses.size() < noteNumber)
        latenesses.add (0.0f);

    if (parameterized)
        latenesses.set (noteNumber, jlimit (-1.0f, 1.0f, p / strength));
    else
        latenesses.set (noteNumber, jlimit (-1.0f, 1.0f, p));
}

void GrooveTemplate::clearLatenesses()
{
    latenesses.clearQuick();
}

double GrooveTemplate::beatsTimeToGroovyTime (double beatsTime, float strength) const
{
	auto activeStrength = parameterized ? strength : 1.0f;

    const double beatNum    = std::floor (beatsTime * notesPerBeat);
    const double offset     = notesPerBeat * (beatsTime - (beatNum / notesPerBeat));
    const int latenessIndex = roundToInt (beatNum) % numNotes;

    const double lateness   = latenesses[latenessIndex] * activeStrength;
    const double t1         = (beatNum + 0.5f * lateness);
    const double t2minust1  = 1.0 + 0.5f * ((latenesses[(latenessIndex + 1) % numNotes] * activeStrength) - lateness);

    return (t1 + offset * t2minust1) / notesPerBeat;
}

double GrooveTemplate::editTimeToGroovyTime (double editTime, float strength, Edit& edit) const
{
    double beats = edit.tempoSequence.timeToBeats (editTime);
    beats = beatsTimeToGroovyTime (beats, strength);
    return edit.tempoSequence.beatsToTime (beats);
}

bool GrooveTemplate::isEmpty() const
{
	for (int i = latenesses.size(); --i >= 0;)
		if (latenesses.getUnchecked (i) != 0.0f)
			return false;

	return true;
}

//==============================================================================
GrooveTemplateManager::GrooveTemplateManager (Engine& e)
    : engine (e)
{
    reload();

    if (knownGrooves.isEmpty())
    {
        // load the defaults..
        std::unique_ptr<XmlElement> defSettings (XmlDocument::parse (TracktionBinaryData::groove_templates_xml));

        if (defSettings && defSettings->getTagName() == "GROOVETEMPLATES")
            reload (defSettings.get());
    }

    bool anyParameterized = false;
    for (auto g : knownGrooves)
        if (g->isParameterized())
            anyParameterized = true;

    // load the new defaults..
    if (! anyParameterized)
    {
        std::unique_ptr<XmlElement> defSettings (XmlDocument::parse (TracktionBinaryData::groove_templates_2_xml));

        if (defSettings != nullptr && defSettings->hasTagName ("GROOVETEMPLATES"))
            forEachXmlChildElementWithTagName (*defSettings, n, GrooveTemplate::grooveXmlTag)
                knownGrooves.add (new GrooveTemplate (n));
    }
}

//==============================================================================
void GrooveTemplateManager::useParameterizedGrooves (bool use)
{
	activeGrooves.clear();

	useParameterized = use;
	if (useParameterized)
	{
		for (auto g : knownGrooves)
			activeGrooves.add (g);
	}
	else
	{
		for (auto g : knownGrooves)
			if (! g->isParameterized())
				activeGrooves.add (g);
	}
}

void GrooveTemplateManager::reload()
{
    if (auto xml = engine.getPropertyStorage().getXmlProperty (SettingID::grooveTemplates))
        reload (xml.get());
}

void GrooveTemplateManager::reload (const juce::XmlElement* grooves)
{
    knownGrooves.clearQuick (true);

    if (grooves != nullptr && grooves->hasTagName ("GROOVETEMPLATES"))
        forEachXmlChildElementWithTagName (*grooves, n, GrooveTemplate::grooveXmlTag)
            knownGrooves.add (new GrooveTemplate (n));

	useParameterizedGrooves (useParameterized);
}

void GrooveTemplateManager::save()
{
    juce::XmlElement n ("GROOVETEMPLATES");

    for (auto gt : knownGrooves)
        if (gt != nullptr)
            n.addChildElement (gt->createXml());

    engine.getPropertyStorage().setXmlProperty (SettingID::grooveTemplates, n);
}

//==============================================================================
int GrooveTemplateManager::getNumTemplates() const
{
    return activeGrooves.size();
}

String GrooveTemplateManager::getTemplateName (int index) const
{
    if (auto gt = activeGrooves[index])
        return gt->getName();

    return {};
}

const GrooveTemplate* GrooveTemplateManager::getTemplate (int index)
{
    return activeGrooves[index];
}

const GrooveTemplate* GrooveTemplateManager::getTemplateByName (const String& name)
{
    for (auto gt : activeGrooves)
        if (gt->getName() == name)
            return gt;

    return {};
}

StringArray GrooveTemplateManager::getTemplateNames() const
{
    StringArray names;

    for (auto gt : activeGrooves)
        names.add (gt->getName());

    return names;
}

void GrooveTemplateManager::updateTemplate (int index, const GrooveTemplate& gt)
{
	index = knownGrooves.indexOf (activeGrooves[index]);

    // check the name doesn't clash..
    String n (gt.getName().trim());

    if (n.isEmpty())
        n = TRANS("Unnamed");
    else
        n = n.substring (0, 32);

    String originalName (n);

    if (originalName.trim().endsWithChar (')'))
    {
        const int openBracks = originalName.lastIndexOfChar ('(');
        const int closeBracks = originalName.lastIndexOfChar (')');

        if (openBracks > 0 && closeBracks > openBracks
             && originalName.substring (openBracks + 1, closeBracks).containsOnly ("0123456789"))
        {
            originalName = originalName.substring (0, openBracks).trim();
        }
    }

    if (knownGrooves[index] != nullptr)
        knownGrooves.remove (index);

	useParameterizedGrooves (useParameterized);

    int suff = 2;
    while (getTemplateByName (n) != nullptr)
        n = originalName + " (" + String (suff++) + ")";

    auto newGroove = knownGrooves.insert (index, new GrooveTemplate (gt));
    newGroove->setName (n);
	newGroove->setParameterized (useParameterized);

    save();
	useParameterizedGrooves (useParameterized);
    TransportControl::restartAllTransports (engine, false);
}

void GrooveTemplateManager::deleteTemplate (int index)
{
    knownGrooves.removeObject (activeGrooves[index]);

	useParameterizedGrooves (useParameterized);

    save();
    TransportControl::restartAllTransports (engine, false);
}

}
