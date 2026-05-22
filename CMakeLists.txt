cmake_minimum_required(VERSION 3.21)

project(SmartPianoQt VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Quick QuickControls2)

qt_standard_project_setup(REQUIRES 6.5)
qt_policy(SET QTP0004 NEW)

qt_add_executable(SmartPianoQt
    src/main.cpp
    src/core/NoteUtils.cpp
    src/core/NoteUtils.h
    src/core/PianoController.cpp
    src/core/PianoController.h
    src/ui/PianoRollItem.cpp
    src/ui/PianoRollItem.h
    src/parser/MidiFileParser.cpp
    src/parser/MidiFileParser.h
)

qt_add_qml_module(SmartPianoQt
    URI SmartPianoQt
    VERSION 1.0
    QML_FILES
        qml/Main.qml
)

target_include_directories(SmartPianoQt PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(SmartPianoQt PRIVATE
    Qt6::Core
    Qt6::Quick
    Qt6::QuickControls2
)

include(GNUInstallDirs)

install(TARGETS SmartPianoQt
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
