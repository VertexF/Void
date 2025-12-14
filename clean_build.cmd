rmdir VS /S /Q
mkdir VS
:: Adding "src" in the commandline makes it so CMake is looking for the main CMakeLists.txt in the src directory, not the root.
cmake -G "Visual Studio 18 2026" -S . -B build
