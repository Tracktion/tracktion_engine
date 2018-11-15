/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

/**
    Create a subclass of PropertyStorage to customize how settings are saved
    and recalled
*/
class PropertyStorage
{
public:
    //==============================================================================
    PropertyStorage (juce::String appName_) : appName (appName_) {}
    virtual ~PropertyStorage() {}

    static juce::String settingToString (SettingID);

    //==============================================================================
    virtual juce::File getAppCacheFolder();
    virtual juce::File getAppPrefsFolder();

    virtual void flushSettingsToDisk() {}

    //==============================================================================
    virtual void removeProperty (SettingID);

    virtual juce::var getProperty (SettingID setting, const juce::var& defaultValue = {});
    virtual void setProperty (SettingID setting, const juce::var& value);

    virtual std::unique_ptr<juce::XmlElement> getXmlProperty (SettingID setting);
    virtual void setXmlProperty (SettingID setting, const juce::XmlElement&);

    //==============================================================================
    virtual void removePropertyItem (SettingID setting, juce::StringRef item);

    virtual juce::var getPropertyItem (SettingID setting, juce::StringRef item, const juce::var& defaultValue = {});
    virtual void setPropertyItem (SettingID setting, juce::StringRef item, const juce::var& value);

    virtual std::unique_ptr<juce::XmlElement> getXmlPropertyItem (SettingID setting, juce::StringRef item);
    virtual void setXmlPropertyItem (SettingID setting, juce::StringRef item, const juce::XmlElement&);

    //==============================================================================
    virtual juce::File getDefaultLoadSaveDirectory (juce::StringRef label);
    virtual void setDefaultLoadSaveDirectory (juce::StringRef label, const juce::File& newPath);

    virtual juce::File getDefaultLoadSaveDirectory (ProjectItem::Category);

    //==============================================================================
    virtual juce::String getUserName();
    virtual juce::String getApplicationName()       { return appName; }
    virtual juce::String getApplicationVersion();

private:
    juce::String appName;
};

} // namespace tracktion_engine
