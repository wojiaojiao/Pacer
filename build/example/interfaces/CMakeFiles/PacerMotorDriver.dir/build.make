# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.2

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list

# Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/local/Cellar/cmake/3.2.1/bin/cmake

# The command to remove a file.
RM = /usr/local/Cellar/cmake/3.2.1/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = "/Volumes/Macintosh HD/Projects/Pacer"

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = "/Volumes/Macintosh HD/Projects/Pacer/build"

# Include any dependencies generated for this target.
include example/interfaces/CMakeFiles/PacerMotorDriver.dir/depend.make

# Include the progress variables for this target.
include example/interfaces/CMakeFiles/PacerMotorDriver.dir/progress.make

# Include the compile flags for this target's objects.
include example/interfaces/CMakeFiles/PacerMotorDriver.dir/flags.make

example/interfaces/CMakeFiles/PacerMotorDriver.dir/motor.cpp.o: example/interfaces/CMakeFiles/PacerMotorDriver.dir/flags.make
example/interfaces/CMakeFiles/PacerMotorDriver.dir/motor.cpp.o: ../example/interfaces/motor.cpp
	$(CMAKE_COMMAND) -E cmake_progress_report "/Volumes/Macintosh HD/Projects/Pacer/build/CMakeFiles" $(CMAKE_PROGRESS_1)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object example/interfaces/CMakeFiles/PacerMotorDriver.dir/motor.cpp.o"
	cd "/Volumes/Macintosh HD/Projects/Pacer/build/example/interfaces" && /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/PacerMotorDriver.dir/motor.cpp.o -c "/Volumes/Macintosh HD/Projects/Pacer/example/interfaces/motor.cpp"

example/interfaces/CMakeFiles/PacerMotorDriver.dir/motor.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/PacerMotorDriver.dir/motor.cpp.i"
	cd "/Volumes/Macintosh HD/Projects/Pacer/build/example/interfaces" && /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E "/Volumes/Macintosh HD/Projects/Pacer/example/interfaces/motor.cpp" > CMakeFiles/PacerMotorDriver.dir/motor.cpp.i

example/interfaces/CMakeFiles/PacerMotorDriver.dir/motor.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/PacerMotorDriver.dir/motor.cpp.s"
	cd "/Volumes/Macintosh HD/Projects/Pacer/build/example/interfaces" && /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S "/Volumes/Macintosh HD/Projects/Pacer/example/interfaces/motor.cpp" -o CMakeFiles/PacerMotorDriver.dir/motor.cpp.s

example/interfaces/CMakeFiles/PacerMotorDriver.dir/motor.cpp.o.requires:
.PHONY : example/interfaces/CMakeFiles/PacerMotorDriver.dir/motor.cpp.o.requires

example/interfaces/CMakeFiles/PacerMotorDriver.dir/motor.cpp.o.provides: example/interfaces/CMakeFiles/PacerMotorDriver.dir/motor.cpp.o.requires
	$(MAKE) -f example/interfaces/CMakeFiles/PacerMotorDriver.dir/build.make example/interfaces/CMakeFiles/PacerMotorDriver.dir/motor.cpp.o.provides.build
.PHONY : example/interfaces/CMakeFiles/PacerMotorDriver.dir/motor.cpp.o.provides

example/interfaces/CMakeFiles/PacerMotorDriver.dir/motor.cpp.o.provides.build: example/interfaces/CMakeFiles/PacerMotorDriver.dir/motor.cpp.o

# Object files for target PacerMotorDriver
PacerMotorDriver_OBJECTS = \
"CMakeFiles/PacerMotorDriver.dir/motor.cpp.o"

# External object files for target PacerMotorDriver
PacerMotorDriver_EXTERNAL_OBJECTS =

example/interfaces/PacerMotorDriver: example/interfaces/CMakeFiles/PacerMotorDriver.dir/motor.cpp.o
example/interfaces/PacerMotorDriver: example/interfaces/CMakeFiles/PacerMotorDriver.dir/build.make
example/interfaces/PacerMotorDriver: /usr/local/lib/libMoby.dylib
example/interfaces/PacerMotorDriver: /usr/local/lib/libRavelin.dylib
example/interfaces/PacerMotorDriver: libPacer.dylib
example/interfaces/PacerMotorDriver: /usr/lib/libxml2.dylib
example/interfaces/PacerMotorDriver: /usr/local/lib/libMoby.dylib
example/interfaces/PacerMotorDriver: /usr/local/lib/libRavelin.dylib
example/interfaces/PacerMotorDriver: example/interfaces/CMakeFiles/PacerMotorDriver.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --red --bold "Linking CXX executable PacerMotorDriver"
	cd "/Volumes/Macintosh HD/Projects/Pacer/build/example/interfaces" && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/PacerMotorDriver.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
example/interfaces/CMakeFiles/PacerMotorDriver.dir/build: example/interfaces/PacerMotorDriver
.PHONY : example/interfaces/CMakeFiles/PacerMotorDriver.dir/build

example/interfaces/CMakeFiles/PacerMotorDriver.dir/requires: example/interfaces/CMakeFiles/PacerMotorDriver.dir/motor.cpp.o.requires
.PHONY : example/interfaces/CMakeFiles/PacerMotorDriver.dir/requires

example/interfaces/CMakeFiles/PacerMotorDriver.dir/clean:
	cd "/Volumes/Macintosh HD/Projects/Pacer/build/example/interfaces" && $(CMAKE_COMMAND) -P CMakeFiles/PacerMotorDriver.dir/cmake_clean.cmake
.PHONY : example/interfaces/CMakeFiles/PacerMotorDriver.dir/clean

example/interfaces/CMakeFiles/PacerMotorDriver.dir/depend:
	cd "/Volumes/Macintosh HD/Projects/Pacer/build" && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" "/Volumes/Macintosh HD/Projects/Pacer" "/Volumes/Macintosh HD/Projects/Pacer/example/interfaces" "/Volumes/Macintosh HD/Projects/Pacer/build" "/Volumes/Macintosh HD/Projects/Pacer/build/example/interfaces" "/Volumes/Macintosh HD/Projects/Pacer/build/example/interfaces/CMakeFiles/PacerMotorDriver.dir/DependInfo.cmake" --color=$(COLOR)
.PHONY : example/interfaces/CMakeFiles/PacerMotorDriver.dir/depend

