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

/** Components that represent a Plugin should inherit from this to ensure
    they are correctly notified when their colour changes e.g. from a controller
    bank change.
*/
struct PluginComponent
{
    PluginComponent (Plugin& p)  : plugin (p)
    {
        getActiveComps().add (this);
    }

    virtual ~PluginComponent()
    {
        getActiveComps().removeFirstMatchingValue (this);
    }

    virtual void updateColour() = 0;

    Plugin& plugin;

    static PluginComponent* getComponentFor (const Plugin& p) noexcept
    {
        for (auto fc : getActiveComps())
            if (&fc->plugin == &p)
                return fc;

        return {};
    }

    static juce::Array<PluginComponent*> getComponentsFor (const Plugin& p) noexcept
    {
        juce::Array<PluginComponent*> comps;

        for (auto fc : getActiveComps())
            if (&fc->plugin == &p)
                comps.add (fc);

        return comps;
    }

private:
    static juce::Array<PluginComponent*>& getActiveComps()
    {
        static juce::Array<PluginComponent*> comps;
        return comps;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginComponent)
};

} // namespace tracktion_engine
