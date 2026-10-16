# SPDX-License-Identifier: GPL-3.0-only
# MONO2K's reference renderer, built against a gearmulator-md-mm checkout (with its submodules) and only the parts of
# it the Monomachine needs: the DSP56300 and 68k emulators, the hardware and synth libraries, mdLib.
#   cmake -S tools/ref -B build/ref -G Ninja -DGEARMULATOR=/path/to/gearmulator-md-mm && cmake --build build/ref
cmake_minimum_required(VERSION 3.15)
project(m2k_ref C CXX)

set(GEARMULATOR "" CACHE PATH "a gearmulator-md-mm checkout, submodules included")
if(NOT EXISTS "${GEARMULATOR}/source/elektron/md/mdLib/CMakeLists.txt")
	message(FATAL_ERROR "GEARMULATOR must point at a gearmulator-md-mm checkout")
endif()

set(CMAKE_PROJECT_NAME gearmulator)
include(${GEARMULATOR}/base.cmake)

set(ASMJIT_STATIC TRUE)
set(ASMJIT_NO_INSTALL TRUE)
set(BUILD_SHARED_LIBS OFF)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)

set(S ${GEARMULATOR}/source)
add_subdirectory(${S}/dsp56300/source dsp56300 EXCLUDE_FROM_ALL)
add_subdirectory(${S}/baseLib baseLib EXCLUDE_FROM_ALL)
add_subdirectory(${S}/libresample libresample EXCLUDE_FROM_ALL)
add_subdirectory(${S}/synthLib synthLib EXCLUDE_FROM_ALL)
add_subdirectory(${S}/mc68k mc68k EXCLUDE_FROM_ALL)
add_subdirectory(${S}/hardwareLib hardwareLib EXCLUDE_FROM_ALL)
add_subdirectory(${S}/elektron/md/mdLib mdLib EXCLUDE_FROM_ALL)

add_executable(mm_ref mm_ref.cpp)
target_link_libraries(mm_ref PRIVATE mdLib)
