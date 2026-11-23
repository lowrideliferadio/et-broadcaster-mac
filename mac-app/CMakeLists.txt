cmake_minimum_required(VERSION 3.22)
project(ETBroadcaster VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(APPLE)
  # Defaults only — do not FORCE, so UNIVERSAL=1 / -DCMAKE_OSX_ARCHITECTURES=arm64;x86_64 is honoured.
  if(NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "macOS architectures")
  endif()
  if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "macOS deployment target")
  endif()
endif()

# Portable engine (no JUCE): scratch resampler, platter physics, crossfader, MIDI jog
add_library(etb_engine STATIC engine/src/SincResampler.cpp engine/src/Deck.cpp)
target_include_directories(etb_engine PUBLIC engine/include)
target_compile_options(etb_engine PRIVATE -O3 -Wall -Wextra)

option(ETB_TESTS "Build offline engine tests" ON)
if(ETB_TESTS)
  add_executable(scratch_tests tests/scratch_tests.cpp)
  target_link_libraries(scratch_tests PRIVATE etb_engine)
  target_compile_options(scratch_tests PRIVATE -O2)
endif()

# Studio app (JUCE). Point JUCE_DIR at a JUCE checkout:  cmake -DJUCE_DIR=~/JUCE ..
option(ETB_APP "Build the studio app (needs JUCE)" OFF)
if(ETB_APP)
  add_subdirectory(${JUCE_DIR} juce)
  add_subdirectory(app)
endif()
