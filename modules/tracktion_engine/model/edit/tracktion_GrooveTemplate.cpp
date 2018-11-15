/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


const char* GrooveTemplate::grooveXmlTag = "GROOVETEMPLATE";

GrooveTemplate::GrooveTemplate()
    : numNotes (16),
      notesPerBeat (2),
      name (TRANS("Unnamed"))
{
}

GrooveTemplate::GrooveTemplate (const XmlElement* node)
    : numNotes (16),
      notesPerBeat (2)
{
    if (node != nullptr && node->hasTagName (grooveXmlTag))
    {
        numNotes = node->getIntAttribute ("numberOfNotes", 16);
        notesPerBeat = node->getIntAttribute ("notesPerBeat", 2);
        name = node->getStringAttribute ("name", TRANS("Unnamed"));

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

const GrooveTemplate& GrooveTemplate::operator= (const GrooveTemplate& other)
{
    latenesses = other.latenesses;
    numNotes = other.numNotes;
    notesPerBeat = other.notesPerBeat;
    name = other.name;

    return *this;
}

void GrooveTemplate::setName (const String& n)
{
    name = n;
}

XmlElement* GrooveTemplate::createXml() const
{
    auto node = new XmlElement (grooveXmlTag);
    node->setAttribute ("name", name);
    node->setAttribute ("numberOfNotes", numNotes);
    node->setAttribute ("notesPerBeat", notesPerBeat);

    int lastNonZeroNote = numNotes;
    while (--lastNonZeroNote >= 0)
        if (latenesses[lastNonZeroNote] != 0.0f)
            break;

    for (int i = 0; i <= lastNonZeroNote; ++i)
    {
        auto n = new XmlElement ("SHIFT");
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

float GrooveTemplate::getLatenessProportion (int noteNumber) const
{
    return latenesses[noteNumber];
}

void GrooveTemplate::setLatenessProportion (int noteNumber, float p)
{
    while (latenesses.size() < noteNumber)
        latenesses.add (0.0f);

    latenesses.set (noteNumber, jlimit (-1.0f, 1.0f, p));
}

void GrooveTemplate::clearLatenesses()
{
    latenesses.clearQuick();
}

double GrooveTemplate::beatsTimeToGroovyTime (double beatsTime) const
{
    const double beatNum    = std::floor (beatsTime * notesPerBeat);
    const double offset     = notesPerBeat * (beatsTime - (beatNum / notesPerBeat));
    const int latenessIndex = roundToInt (beatNum) % numNotes;

    const double lateness   = (double) latenesses[latenessIndex];
    const double t1         = (beatNum + 0.5f * lateness);
    const double t2minust1  = 1.0 + 0.5f * (latenesses[(latenessIndex + 1) % numNotes] - lateness);

    return (t1 + offset * t2minust1) / notesPerBeat;
}

double GrooveTemplate::editTimeToGroovyTime (double editTime, Edit& edit) const
{
    double beats = edit.tempoSequence.timeToBeats (editTime);
    beats = beatsTimeToGroovyTime (beats);
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
}

//==============================================================================
void GrooveTemplateManager::reload()
{
    if (auto xml = engine.getPropertyStorage().getXmlProperty (SettingID::grooveTemplates))
        reload (xml.get());
}

void GrooveTemplateManager::reload (const XmlElement* grooves)
{
    knownGrooves.clearQuick (true);

    if (grooves != nullptr && grooves->hasTagName ("GROOVETEMPLATES"))
        forEachXmlChildElementWithTagName (*grooves, n, GrooveTemplate::grooveXmlTag)
            knownGrooves.add (new GrooveTemplate (n));
}

void GrooveTemplateManager::save()
{
    XmlElement n ("GROOVETEMPLATES");

    for (auto gt : knownGrooves)
        if (gt != nullptr)
            n.addChildElement (gt->createXml());

    engine.getPropertyStorage().setXmlProperty (SettingID::grooveTemplates, n);
}

//==============================================================================
int GrooveTemplateManager::getNumTemplates() const
{
    return knownGrooves.size();
}

String GrooveTemplateManager::getTemplateName (int index) const
{
    if (auto gt = knownGrooves[index])
        return gt->getName();

    return {};
}

const GrooveTemplate* GrooveTemplateManager::getTemplate (int index)
{
    return knownGrooves[index];
}

const GrooveTemplate* GrooveTemplateManager::getTemplateByName (const String& name)
{
    for (auto gt : knownGrooves)
        if (gt->getName() == name)
            return gt;

    return {};
}

StringArray GrooveTemplateManager::getTemplateNames() const
{
    StringArray names;

    for (auto gt : knownGrooves)
        names.add (gt->getName());

    return names;
}

void GrooveTemplateManager::updateTemplate (int index, const GrooveTemplate& gt)
{
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

    int suff = 2;
    while (getTemplateByName (n) != nullptr)
        n = originalName + " (" + String (suff++) + ")";

    GrooveTemplate* newGroove = knownGrooves.insert (index, new GrooveTemplate (gt));
    newGroove->setName (n);

    save();
    TransportControl::restartAllTransports (engine, false);
}

void GrooveTemplateManager::deleteTemplate (int index)
{
    knownGrooves.remove (index);

    save();
    TransportControl::restartAllTransports (engine, false);
}
