cmake_minimum_required(VERSION 3.14)

project(
    bpmanalyzer
    VERSION 0.1.0
    DESCRIPTION "BPM Analyzer plugin for fooyin"
    HOMEPAGE_URL "https://github.com/l-raider/fooyin-plugins"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

find_package(Fooyin REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Concurrent)

# ---------------------------------------------------------------------------
# SoundTouch — built from the git submodule at 3rdparty/soundtouch.
# We build it as a static library so the plugin bundle is self-contained and
# does not require SoundTouch to be installed on the host system.
# ---------------------------------------------------------------------------
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/soundtouch/CMakeLists.txt")
    message(FATAL_ERROR
        "SoundTouch submodule is not initialised.\n"
        "Run:  git submodule update --init --recursive\n"
        "inside the bpmanalyzer directory (or the repo root).")
endif()

# Disable options we do not need
set(SOUNDSTRETCH   OFF CACHE BOOL "Build soundstretch CLI utility" FORCE)
set(SOUNDTOUCH_DLL OFF CACHE BOOL "Build SoundTouchDLL C wrapper"  FORCE)
set(OPENMP         OFF CACHE BOOL "SoundTouch OpenMP support"       FORCE)

# Force static build and PIC for the submodule only, then restore the caller's
# BUILD_SHARED_LIBS so create_fooyin_plugin still produces a shared module.
set(_bpm_saved_shared_libs ${BUILD_SHARED_LIBS})
set(BUILD_SHARED_LIBS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_subdirectory(3rdparty/soundtouch EXCLUDE_FROM_ALL)

set(BUILD_SHARED_LIBS ${_bpm_saved_shared_libs})
unset(_bpm_saved_shared_libs)

# Provide the namespaced alias expected by our source files
if(NOT TARGET SoundTouch::SoundTouch)
    add_library(SoundTouch::SoundTouch ALIAS SoundTouch)
endif()

create_fooyin_plugin(
    bpmanalyzer
    JSON_IN src/bpmanalyzer.json.in
    DEPENDS Fooyin::Core
            Fooyin::Gui
            Qt6::Concurrent
            SoundTouch::SoundTouch
    SOURCES src/bpmanalyzerdefs.h
            src/bpmanalyzerresult.h
            src/bpmanalyzerworker.h
            src/bpmanalyzerworker.cpp
            src/bpmanalyzerscanner.h
            src/bpmanalyzerscanner.cpp
            src/bpmanalyzerresultsmodel.h
            src/bpmanalyzerresultsmodel.cpp
            src/bpmanalyzerresults.h
            src/bpmanalyzerresults.cpp
            src/bpmanalyzerplugin_settings.h
            src/bpmanalyzerplugin_settings.cpp
            src/bpmanalyzerplugin.h
            src/bpmanalyzerplugin.cpp
)
