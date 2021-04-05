add_subdirectory(juce)

juce_add_modules(
        INSTALL_PATH "include/JUCE-${JUCE_VERSION}/modules"
        ALIAS_NAMESPACE juce
        tracktion_engine
        tracktion_graph)
