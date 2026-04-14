cmake_minimum_required(VERSION 3.14)

project(
    shortcutextender
    VERSION 0.1.0
    DESCRIPTION "Shortcut extender plugin for fooyin"
    HOMEPAGE_URL "https://github.com/fooyin/fooyin-plugins"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

find_package(Fooyin REQUIRED)

create_fooyin_plugin(
    shortcutextender
    JSON_IN src/shortcutextender.json.in
    DEPENDS Fooyin::Core
            Fooyin::Gui
            Fooyin::Utils
    SOURCES src/shortcutextenderdefs.h
            src/fileopsbridge.h
            src/fileopsscriptenvironment.h
            src/fileopsexecutor.h
            src/fileopsexecutor.cpp
            src/fileopsconfirmdialog.h
            src/fileopsconfirmdialog.cpp
            src/fileopspresetshortcuts.h
            src/fileopspresetshortcuts.cpp
            src/shortcutextendersettings.h
            src/shortcutextendersettings.cpp
            src/deleteworker.h
            src/deleteworker.cpp
            src/deletedialog.h
            src/deletedialog.cpp
            src/deletecurrentlyplaying.h
            src/deletecurrentlyplaying.cpp
            src/shortcutextenderplugin.h
            src/shortcutextenderplugin.cpp
)
