# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.15

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


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
CMAKE_COMMAND = /home/lizhaolong/.local/share/JetBrains/Toolbox/apps/CLion/ch-0/193.5662.56/bin/cmake/linux/bin/cmake

# The command to remove a file.
RM = /home/lizhaolong/.local/share/JetBrains/Toolbox/apps/CLion/ch-0/193.5662.56/bin/cmake/linux/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/lizhaolong/suanfa/RocketCo

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/lizhaolong/suanfa/RocketCo/cmake-build-debug

# Include any dependencies generated for this target.
include CMakeFiles/RocketCo.dir/depend.make

# Include the progress variables for this target.
include CMakeFiles/RocketCo.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/RocketCo.dir/flags.make

CMakeFiles/RocketCo.dir/main.cpp.o: CMakeFiles/RocketCo.dir/flags.make
CMakeFiles/RocketCo.dir/main.cpp.o: ../main.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/lizhaolong/suanfa/RocketCo/cmake-build-debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/RocketCo.dir/main.cpp.o"
	/usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/RocketCo.dir/main.cpp.o -c /home/lizhaolong/suanfa/RocketCo/main.cpp

CMakeFiles/RocketCo.dir/main.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/RocketCo.dir/main.cpp.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/lizhaolong/suanfa/RocketCo/main.cpp > CMakeFiles/RocketCo.dir/main.cpp.i

CMakeFiles/RocketCo.dir/main.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/RocketCo.dir/main.cpp.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/lizhaolong/suanfa/RocketCo/main.cpp -o CMakeFiles/RocketCo.dir/main.cpp.s

CMakeFiles/RocketCo.dir/coswap.cpp.o: CMakeFiles/RocketCo.dir/flags.make
CMakeFiles/RocketCo.dir/coswap.cpp.o: ../coswap.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/lizhaolong/suanfa/RocketCo/cmake-build-debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building CXX object CMakeFiles/RocketCo.dir/coswap.cpp.o"
	/usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/RocketCo.dir/coswap.cpp.o -c /home/lizhaolong/suanfa/RocketCo/coswap.cpp

CMakeFiles/RocketCo.dir/coswap.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/RocketCo.dir/coswap.cpp.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/lizhaolong/suanfa/RocketCo/coswap.cpp > CMakeFiles/RocketCo.dir/coswap.cpp.i

CMakeFiles/RocketCo.dir/coswap.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/RocketCo.dir/coswap.cpp.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/lizhaolong/suanfa/RocketCo/coswap.cpp -o CMakeFiles/RocketCo.dir/coswap.cpp.s

CMakeFiles/RocketCo.dir/EpollWrapper/address.cpp.o: CMakeFiles/RocketCo.dir/flags.make
CMakeFiles/RocketCo.dir/EpollWrapper/address.cpp.o: ../EpollWrapper/address.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/lizhaolong/suanfa/RocketCo/cmake-build-debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Building CXX object CMakeFiles/RocketCo.dir/EpollWrapper/address.cpp.o"
	/usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/RocketCo.dir/EpollWrapper/address.cpp.o -c /home/lizhaolong/suanfa/RocketCo/EpollWrapper/address.cpp

CMakeFiles/RocketCo.dir/EpollWrapper/address.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/RocketCo.dir/EpollWrapper/address.cpp.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/lizhaolong/suanfa/RocketCo/EpollWrapper/address.cpp > CMakeFiles/RocketCo.dir/EpollWrapper/address.cpp.i

CMakeFiles/RocketCo.dir/EpollWrapper/address.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/RocketCo.dir/EpollWrapper/address.cpp.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/lizhaolong/suanfa/RocketCo/EpollWrapper/address.cpp -o CMakeFiles/RocketCo.dir/EpollWrapper/address.cpp.s

# Object files for target RocketCo
RocketCo_OBJECTS = \
"CMakeFiles/RocketCo.dir/main.cpp.o" \
"CMakeFiles/RocketCo.dir/coswap.cpp.o" \
"CMakeFiles/RocketCo.dir/EpollWrapper/address.cpp.o"

# External object files for target RocketCo
RocketCo_EXTERNAL_OBJECTS =

RocketCo: CMakeFiles/RocketCo.dir/main.cpp.o
RocketCo: CMakeFiles/RocketCo.dir/coswap.cpp.o
RocketCo: CMakeFiles/RocketCo.dir/EpollWrapper/address.cpp.o
RocketCo: CMakeFiles/RocketCo.dir/build.make
RocketCo: CMakeFiles/RocketCo.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/lizhaolong/suanfa/RocketCo/cmake-build-debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_4) "Linking CXX executable RocketCo"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/RocketCo.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/RocketCo.dir/build: RocketCo

.PHONY : CMakeFiles/RocketCo.dir/build

CMakeFiles/RocketCo.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/RocketCo.dir/cmake_clean.cmake
.PHONY : CMakeFiles/RocketCo.dir/clean

CMakeFiles/RocketCo.dir/depend:
	cd /home/lizhaolong/suanfa/RocketCo/cmake-build-debug && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/lizhaolong/suanfa/RocketCo /home/lizhaolong/suanfa/RocketCo /home/lizhaolong/suanfa/RocketCo/cmake-build-debug /home/lizhaolong/suanfa/RocketCo/cmake-build-debug /home/lizhaolong/suanfa/RocketCo/cmake-build-debug/CMakeFiles/RocketCo.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/RocketCo.dir/depend

