# BeatConnect Specific Build Information
Because of the modifications to Tracktion Engine applied in BeatConnect's codebase, the demos, tests, and examples do not run "out-of-the-box".

## Building
To build the artifacts run the `build` script included. You may need to enable the TestRunner
in order to have the script complete successfully. You may need to start the build process of generate_examples with the script, and then finish building within Visual Studio or XCode. You may need to perform any of the above tasks with elevated privileges.

# Adding Demos
## Create the Demo
1. Duplicate DemoTemplate.h. 
2. Update the class type of the demo. 
3. Replace BitCrusherPlugin with your code of choice, or declare a new plugin as seen in DistortionEffectDemo.h.
4. Register the demo with the DemoRunner in DemoRunner.h:~100

# Running
The executable produced for the DemoRunner are found at `bc_unity_daw\BeatConnectDLL\tracktion_engine\examples\DemoRunner\build\DemoRunner_artefacts`. From this directory navigate to the build configuration of choice.

# Use
1. Select `[Load Demo]` from the top left of the DemoRunner's window.
2. Select the demo you'd like to run from the list.

# Troubleshooting
A couple temporary and settings files are used by the Demo Runner and various tests which can break. Crashes and the failure of the audio system are among the unexpected behaviours produced by ill-formed or misaligned temporary files. To determine where your temporary files are stored debug this line in tracktion_TemporaryFileManager.cpp:

```C++
// tracktion_TemporaryFileManager.cpp:~30 
const juce::File& TemporaryFileManager::getTempDirectory() const
{
    return tempDir;
}
```

