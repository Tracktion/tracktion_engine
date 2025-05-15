Demo set-up steps for macOS

CLONING:
```
git clone --recurse-submodules https://github.com/lavinialei2/musiclingo.git
```
There will be an error about being unable to clone the subrepo juce, so clone that separately with
```
git clone https://github.com/juce-framework/JUCE.git
```
Then once this repo is cloned, drag and drop it into musiclingo/modules (and you'll be prompted with are you sure you want to replace this other folder with the same name, and say yes because the current juce is empty).


BUILDING:
If you don't have cmake already, install using brew (and if you don't have brew, google how to install homebrew):
```
brew install cmake
```

Now time to build the examples! You may run into an error with being unable to find CC or CXX compiler when running the cmake command, but you can set the compiler to clang with the following:
```
export CC=/usr/bin/clang
export CXX=/usr/bin/clang++
```
First cd into the musiclingo directory, wherever you have stored it, and then cd into the tests directory to generate the examples:
```
cd [path to your musiclingo directory]
cd tests
./generate_examples
```
And to actually build (the below snippet cds out of tests and into examples/DemoRunner, removes any existing build in case you've messed up and need to restart, and then builds):
```
cd ../examples/DemoRunner
rm -rf build
mkdir build && cd build
cmake .. -G Xcode
```
This builds the demos into an Xcode project, which you can open in Xcode with:
```
open DemoRunner.xcodeproj
```


IN XCODE:
Make sure you have version 15.1 or later. Make sure the scheme is DemoRunner (the default may say DemoRunner Binary Data or All Build) and target is My Mac, which should be the default. You should be able to just run and have it say build successful and have the basic GUI pop up. 

Potential CodeSignature issue:
Make sure the repo is not in an iCloud directory, and if you still get a Codesign build error, then do this:
Click the Xcode project in the left sidebar which will pull up some settings
Then select the project under Targets (not project)
Click on Signing & Capabilities 
Check the box for “automatically manage signing”
Select a team and make sure the signing certificate is set to Development
Command + Shift + K
Command + B


HOW TO ADD A DEMO TO DEMORUNNER:
In examples/DemoRunner/demos, create a new header file. I called it TestDemo.h, and it's functionally just a copy of MidiRecordingDemo.h, but I found and replaced all instances of "MidiRecordingDemo" with "TestDemo" in the file. To make sure it is included in the build, go to examples/DemoRunner/DemoRunner.h and ctrl+f "MidiRecordingDemo" and you should find an include block of all the demo files. Add your newly made file (#include "demos/TestDemo.h" in my case). Then go to examples/DemoRunner/CMakeLists.txt and ctrl+f "MidiRecordingDemo" and you should see a set(SourceFiles...), where you should add your new demo as well. Then, follow the instructions above again to regenerate the examples and build.

To make regenerating examples easier out of build
```
cd ..
cd ..
cd ../tests
./generate_examples
```


![](tutorials/images/tracktion_engine_powered.png)

master: [![Build](https://github.com/Tracktion/tracktion_engine/actions/workflows/build.yaml/badge.svg?branch=master)](https://github.com/Tracktion/tracktion_engine/actions/workflows/build.yaml)

develop: [![Build](https://github.com/Tracktion/tracktion_engine/actions/workflows/build.yaml/badge.svg?branch=develop)](https://github.com/Tracktion/tracktion_engine/actions/workflows/build.yaml)
[![codecov](https://codecov.io/gh/Tracktion/tracktion_engine/branch/develop/graph/badge.svg?token=jirhU03pQO)](https://codecov.io/gh/Tracktion/tracktion_engine)
[![juce_compatability](https://github.com/Tracktion/tracktion_engine/actions/workflows/juce_compat.yaml/badge.svg)](https://github.com/Tracktion/tracktion_engine/actions/workflows/juce_compat.yaml)

# N.B. Enterprise licensees, please check the terms of your license as it may not include v3. Please contact us for upgrade options

# tracktion_engine
##### Welcome to the Tracktion Engine repository!
The aim of Tracktion Engine is to provide a high level data model and set of classes for building sequence based audio applications. You can build anything from a simple file-player or sequencer to a full blown DAW.

Take a look at the [features document](FEATURES.md) for the full range of features.

##### Supported Platforms
- macOS
- Windows
- Linux
- Raspberry PI
- iOS
- Android

*N.B. Tracktion Engine requires C++20*

## Contents
- [Getting Started](#getting-started)
- [Examples](#examples)
- [Tutorials](#tutorials)
- [Documentation](#documentation)
- [Benchmarks](#benchmarks)
- [Contributing](#contributing)
- [License](#license)

## Getting Started
Tracktion Engine is supplied as a `JUCE module` so it can easily fit in to an existing JUCE application. You'll find the module code under `modules/tracktion_engine`. Additionally, JUCE is added as a Git Submodule here in order to build the examples.

To start with, clone the repo and recurse the submodules:
```
$ git clone --recurse-submodules https://github.com/Tracktion/tracktion_engine.git
```

## Examples
Example projects are located in `/examples`.
There are two main example projects, `DemoRunner` and `EngineInPluginDemo`. In each of these folder is a CMakeLists.txt file you can use to build them (or run the `build` script mentioned below).

`DemoRunner` contains a number of app examples showcasing various Engine functionality.
`EngineInPluginDemo` builds a plugin which contains Tracktion Engine and syncs the host timeline to the Edit's timeline.

Additionally there are `Benchmark` an `TestRunner` apps used by CI to test Engine functionality and performance.

#### Scripts
To generate all the examples for the running platform use the script in `/tests`.
These are bash scripts so if you're on Windows you can use the `Git Bash` shell that comes with Git to run the following.
```
$ cd tests
$ ./generate_examples
```
`generate_examples` will generate the IDE project files for you. Alternatively you can run the `build` script to build the examples as well, ready to run.

Once the example projects have been generated or built you can find them in `examples/example_name/build`.

#### CMake
Alternatively, you can run cmake in the root directory which will create a project with the `DemoRunner`, `EngineInPluginDemo`, `TestRunner` and `Benchmark` targets. E.g.
```shell
cmake -G <generator_name> -B build
```

## Tutorials
Once you're ready to dive in to the code, open the IDE files and have a read through the tutorials in `/tutorials`. You can view these on GitHub [here](/tutorials) to see the rendered Markdown.

## Documentation
We are still in the process of fleshing out Doxygen formatted comments but the Doxygen generated documentation can be found here: https://tracktion.github.io/tracktion_engine/modules.html

## Benchmarks
Benchmarks are really for our own internal use but might be of interest to some people:
https://tracktion.github.io/tracktion_engine/benchmarks.html

## Contributing
Tracktion Engine is provided in JUCE module format, for bug reports and features requests, please visit the [JUCE Forum and post using the Tracktion Engine category](https://forum.juce.com/c/tracktion-engine) -
the Tracktion Engine developers are active there and will read every post and respond accordingly.
We don't accept third party GitHub pull requests directly due to copyright restrictions
but if you would like to contribute any changes please contact us.

## License
Tracktion Engine is covered by a [GPL](https://www.gnu.org/licenses/gpl-3.0.en.html)/[Commercial license](https://www.tracktion.com/develop/tracktion-engine).

There are multiple commercial licensing tiers for Tracktion Engine, with different terms for each.
For prices, see the [Tracktion Developers Page](https://www.tracktion.com/develop/tracktion-engine).

**N.B.** *Although Tracktion Engine utilises JUCE, it is not part of JUCE nor owned by the same company. As such it is licensed separately and you must make sure you have an appropriate JUCE licence from [juce.com](juce.com) when distributing Tracktion Engine based products. Similarly, Tracktion Engine is not included in a JUCE licence and you must get the above mentioned Tracktion Engine licence to distribute products.*

___
Tracktion Engine utilises and contains copies of the following libraries. Please make sure you adhere to the license terms where necessary:
- [rpmalloc](https://www.github.com/mjansson/rpmalloc) - Public domain
- [moodycamel::ConcurrentQueue](https://www.github.com/cameron314/concurrentqueue) - Simplified BSD/BSL
- [choc](https://www.github.com/Tracktion/choc) - ISC
- [crill](https://www.github.com/crill-dev/crill) - BSL-1.0
- [expected](https://www.github.com/TartanLlama/expected) - CC0 1.0
- [libsamplerate](https://www.github.com/libsndfile/libsamplerate) - BSD-2-Clause
- [nanorange](https://www.github.com/tcbrindle/NanoRange) - BSL-1.0
- [rigtorp/MPMCQueue](https://www.github.com/rigtorp/MPMCQueue) - MIT
- [magic_enum](https://www.github.com/Neargye/magic_enum) - MIT
- [farbot](https://www.github.com/hogliux/farbot) - MIT
- [doctest](https://www.github.com/doctest/doctest) - MIT
