![](tutorials/images/tracktion_engine_powered.png)

master: [![Build](https://github.com/Tracktion/tracktion_engine/actions/workflows/build.yaml/badge.svg?branch=master)](https://github.com/Tracktion/tracktion_engine/actions/workflows/build.yaml)

develop: [![Build](https://github.com/Tracktion/tracktion_engine/actions/workflows/build.yaml/badge.svg?branch=develop)](https://github.com/Tracktion/tracktion_engine/actions/workflows/build.yaml)
[![codecov](https://codecov.io/gh/Tracktion/tracktion_engine/branch/develop/graph/badge.svg?token=jirhU03pQO)](https://codecov.io/gh/Tracktion/tracktion_engine)

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

## Getting Started
Tracktion Engine is supplied as a `JUCE module` so it can easily fit in to an existing JUCE application. You'll find the module code under `modules/tracktion_engine`. Additionally, JUCE is added as a Git Submodule here in order to build the examples.

To start with, clone the repo and recurse the submodules:
```
$ git clone --recurse-submodules https://github.com/Tracktion/tracktion_engine.git
```

## Examples
Example projects are located in `/examples`. These are provided as JUCE PIPs, and CMakeLists.txt files to build them using CMake. You will need CMake installed to generate the examples.
To generate all the examples for the running platform use the script in `/tests`.
These are bash scripts so if you're on Windows you can use the `Git Bash` shell that comes with Git to run the following.
```
$ cd tests
$ ./generate_examples
```
`generate_examples` will generate the IDE project files for you. Alternatively you can run the `build` script to build the examples as well, ready to run.

Once the example projects have been generated or built you can find them in `examples/example_name/build`.
Start with the `PitchAndTimeDemo` or `StepSequencerDemo` to see some basic apps in action.

*If you want to build the Projucer app from JUCE you can load each of the PIP examples directly to generate the IDE files. You'll have to manually specify C++17 as the C++ standard however as this isn't currently possible via the PIP format.*

## Tutorials
Once you're ready to dive in to the code, open the IDE files and have a read through the tutorials in `/tutorials`. You can view these on GitHub [here](/tutorials) to see the rendered Markdown.

## Documentation
We are still in the process of fleshing out Doxygen formatted comments but the Doxygen generated documentation can be found here: https://tracktion.github.io/tracktion_engine/modules.html

## Benchmarks
We're in the process of creating a portal to view and examine our benchmarks. This is really for our own internal use but might be of interest to some people:
https://tracktion.github.io/tracktion_benchmarks

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
The Tracktion Graph module (also used by Tracktion Engine) includes the MIT licensed [farbot library](/modules/tracktion_graph/3rd_party/farbot) which requires the following notice to be included as part of the software:

```
MIT License

Copyright (c) 2019 Fabian Renn

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```
