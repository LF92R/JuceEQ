# JuceEQ

Parametric EQ built with JUCE. Works as a standalone app for Windows for now. 
Has I/O gain sliders, HPF and LPF, and up to 8 peaking bands.

## Requirements
- Windows with Visual Studio 2022
- CMake 3.21+
- C++20
- JUCE 8 (fetched automatically via CPM in CMake)

## Build (Windows, CLI + Visual Studio)
1) Clone and enter the project
   git clone https://github.com/LF92R/JuceEQ.git
   cd JuceEQ

2) Generate a VS 2022 solution in ./build
   cmake -S . -B build -G "Visual Studio 17 2022" -A x64

3) Open the solution and run
   start build\JuceEQ.sln
   // In Visual Studio: choose Debug or Release, then press F5 to run the Standalone target

Note: First configure needs internet (JUCE is fetched via CPM).

## License
All rights reserved. Source available to reviewers on request.