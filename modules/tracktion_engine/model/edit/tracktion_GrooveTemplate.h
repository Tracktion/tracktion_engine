/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

//==============================================================================
/**
*/
class GrooveTemplate
{
public:
    GrooveTemplate();
    GrooveTemplate (const juce::XmlElement*);
    GrooveTemplate (const GrooveTemplate&);
    ~GrooveTemplate();

    const GrooveTemplate& operator= (const GrooveTemplate& other);

    //==============================================================================
    bool isEmpty() const;

    //==============================================================================
    /** Apply this groove to a time, in beats */
    double beatsTimeToGroovyTime (double beatsTime) const;

    /** Apply this groove to a time, in seconds */
    double editTimeToGroovyTime (double editTime, Edit& edit) const;

    //==============================================================================
    const juce::String& getName() const                     { return name; }
    void setName (const juce::String&);

    // all indexes are in quarter-notes
    int getNumberOfNotes() const                            { return numNotes; }
    void setNumberOfNotes (int notes);

    int getNotesPerBeat() const                             { return notesPerBeat; }
    void setNotesPerBeat (int notes);

    // value is 0 to 1.0 of the length of a note
    float getLatenessProportion (int noteNumber) const;
    void setLatenessProportion (int noteNumber, float p);
    void clearLatenesses();

    //==============================================================================
    static const char* grooveXmlTag;
    juce::XmlElement* createXml() const;

private:
    //==============================================================================
    juce::Array<float> latenesses;
    int numNotes, notesPerBeat;
    juce::String name;

    //==============================================================================
    JUCE_LEAK_DETECTOR (GrooveTemplate)
};

//==============================================================================
/**
    Looks after the list of groove templates.
*/
class GrooveTemplateManager
{
public:
    //==============================================================================
    GrooveTemplateManager (Engine&);

    //==============================================================================
    int getNumTemplates() const;
    juce::String getTemplateName (int index) const;

    juce::StringArray getTemplateNames() const;

    const GrooveTemplate* getTemplate (int index);
    const GrooveTemplate* getTemplateByName (const juce::String& name);

    void updateTemplate (int index, const GrooveTemplate&);
    void deleteTemplate (int index);

    /** called when usersettings change, because that's where the grooves are kept. */
    void reload();

private:
    //==============================================================================
    Engine& engine;
    juce::OwnedArray<GrooveTemplate> knownGrooves;

    //==============================================================================
    void save();
    void reload (const juce::XmlElement*);

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GrooveTemplateManager)
};

} // namespace tracktion_engine
