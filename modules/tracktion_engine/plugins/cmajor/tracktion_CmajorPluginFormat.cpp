/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion::inline engine {

//==============================================================================
juce::String getCmajorPatchPluginFormatName()
{
    return "Cmajor";
}

bool isCmajorPatchPluginFormat (const juce::String& formatName)
{
    return formatName == getCmajorPatchPluginFormatName();
}

bool isCmajorPatchPluginFormat (const juce::PluginDescription& pd)
{
    return isCmajorPatchPluginFormat (pd.pluginFormatName);
}

//==============================================================================
#if TRACKTION_ENABLE_CMAJOR

} // namespace tracktion::inline engine

// N.B. cmajor's cmaj_JUCEPlugin.h guards on JUCE_MAJOR_VERSION == 8, so on JUCE 9 it
// falls back to the deprecated Font constructors. The vendored repo can't be patched,
// so the deprecation warning is suppressed for this header only.
JUCE_BEGIN_IGNORE_DEPRECATION_WARNINGS
#include "cmajor/helpers/cmaj_JUCEPluginFormat.h"
JUCE_END_IGNORE_DEPRECATION_WARNINGS
#include "cmajor/helpers/cmaj_FileBasedCacheDatabase.h"

namespace tracktion::inline engine {

std::string getCmajorVersion() { return cmaj::Library::getVersion(); }

cmaj::CacheDatabaseInterface::Ptr createCompilerCache (tracktion::Engine& engine)
{
    auto cacheFolder = engine.getTemporaryFileManager().getTempDirectory().getChildFile ("cmajor_patch_cache");

    if (cacheFolder.createDirectory())
        return choc::com::create<cmaj::FileBasedCacheDatabase> (cacheFolder.getFullPathName().toStdString(), 100u);

    return {};
}

/** Returns true if a rebuilt patch has parameter endpoints that differ from the
    hosted parameters the plugin was created with.
*/
static bool hasParameterSetChanged (cmaj::plugin::SinglePatchJITPlugin& plugin)
{
    std::set<juce::String> endpointIDs, hostedIDs;

    for (auto& p : plugin.patch->getParameterList())
        endpointIDs.insert (juce::String (p->properties.endpointID));

    // While a new build is being swapped in, the patch reports an empty list
    if (endpointIDs.empty())
        return false;

    for (auto p : plugin.getParameters())
        if (auto hosted = dynamic_cast<juce::HostedAudioProcessorParameter*> (p))
            hostedIDs.insert (hosted->getParameterID());

    return endpointIDs != hostedIDs;
}

std::unique_ptr<juce::AudioPluginFormat> createCmajorPatchPluginFormat (tracktion::Engine& engine)
{
    TRACKTION_LOG ("Cmajor plugin format enabled");

    jassert (getCmajorPatchPluginFormatName() == cmaj::plugin::JUCEPluginFormat::pluginFormatName);

    auto onPatchChange = [&engine] (cmaj::plugin::SinglePatchJITPlugin& plugin)
    {
        for (auto& edit : engine.getActiveEdits().getEdits())
        {
            if (auto p = edit->getPluginCache().getPluginFor (plugin))
            {
                if (! p->isInitialising())
                {
                    if (auto ex = dynamic_cast<ExternalPlugin*> (p.get()))
                    {
                        plugin.suspendProcessing (true);
                        plugin.fillInPluginDescription (ex->desc);

                        // A fixed patch's hosted parameters can't be added to or removed
                        // after it's created, so if the rebuild changed the set, the
                        // instance has to be re-created for the host to see the new set.
                        // That deletes this plugin, so it can't happen in its own callback.
                        if (hasParameterSetChanged (plugin))
                            juce::MessageManager::callAsync ([ref = makeSafeRef (*ex)]
                                                             {
                                                                 if (ref != nullptr)
                                                                     ref->recreatePluginInstance();
                                                             });
                        else
                            ex->forceFullReinitialise();

                        plugin.suspendProcessing (false);
                    }
                    else
                    {
                        jassertfalse;
                    }
                }

                return;
            }
        }
    };

    return std::make_unique<cmaj::plugin::JUCEPluginFormat> (createCompilerCache (engine),
                                                             std::move (onPatchChange),
                                                             engine.getPropertyStorage().getApplicationVersion().toStdString());
}

juce::String getCmajorPatchCompileError (Plugin& p)
{
    if (auto ex = dynamic_cast<ExternalPlugin*> (&p))
        if (auto plugin = dynamic_cast<cmaj::plugin::SinglePatchJITPlugin*> (ex->getAudioPluginInstance()))
            if (plugin->isStatusMessageError)
                return plugin->statusMessage;

    return {};
}

#else

//==============================================================================
std::string getCmajorVersion() { return {}; }
juce::String getCmajorPatchCompileError (Plugin&) { return {}; }
std::unique_ptr<juce::AudioPluginFormat> createCmajorPatchPluginFormat (tracktion::Engine&)  { return {}; }

#endif

} // namespace tracktion::inline engine
