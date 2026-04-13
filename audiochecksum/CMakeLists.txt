cmake_minimum_required(VERSION 3.14)

project(
    audiochecksum
    VERSION 0.1.0
    DESCRIPTION "Audio checksum plugin for fooyin"
    HOMEPAGE_URL "https://github.com/fooyin/fooyin-plugins"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

find_package(Fooyin REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Concurrent)

create_fooyin_plugin(
    audiochecksum
    JSON_IN src/audiochecksum.json.in
    DEPENDS Fooyin::Core
            Fooyin::Gui
            Qt6::Concurrent
    SOURCES src/audiochecksumdefs.h
            src/audiochecksumresult.h
            src/flacstreaminfo.h
            src/flacstreaminfo.cpp
            src/audiochecksumworker.h
            src/audiochecksumworker.cpp
            src/audiochecksumscanner.h
            src/audiochecksumscanner.cpp
            src/audiochecksumresultsmodel.h
            src/audiochecksumresultsmodel.cpp
            src/audiochecksumresults.h
            src/audiochecksumresults.cpp
            src/audiochecksumplugin.h
            src/audiochecksumplugin.cpp
            src/audiochecksumplugin_settings.h
            src/audiochecksumplugin_settings.cpp
)
