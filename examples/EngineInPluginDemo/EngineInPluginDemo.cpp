#include <JuceHeader.h>
#include "EngineInPluginDemo.h"

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EngineInPluginDemo();
}
