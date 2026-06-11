cmake_minimum_required(VERSION 3.21)

project(SmartPianoQt VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Multimedia Quick QuickControls2 Sql)

qt_standard_project_setup(REQUIRES 6.5)
if(Qt6_VERSION VERSION_GREATER_EQUAL 6.8.0)
    qt_policy(SET QTP0004 NEW)
endif()

qt_add_executable(SmartPianoQt
    src/main.cpp
    src/audio/MidiSynth.cpp
    src/audio/MidiSynth.h
    src/core/NoteUtils.cpp
    src/core/NoteUtils.h
    src/core/PianoController.cpp
    src/core/PianoController.h
    src/core/Song.h
    src/library/LocalSheetModel.cpp
    src/library/LocalSheetModel.h
    src/library/MidiLibraryService.cpp
    src/library/MidiLibraryService.h
    src/midi/MidiInputService.cpp
    src/midi/MidiInputService.h
    src/parser/JsonSheetParser.cpp
    src/parser/JsonSheetParser.h
    src/playback/PlaybackClock.cpp
    src/playback/PlaybackClock.h
    src/playback/PlaybackEngine.cpp
    src/playback/PlaybackEngine.h
    src/practice/PracticeEngine.cpp
    src/practice/PracticeEngine.h
    src/practice/PracticeSessionController.cpp
    src/practice/PracticeSessionController.h
    src/storage/PracticeRecordStore.cpp
    src/storage/PracticeRecordStore.h
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
        qml/components/ControlPanel.qml
        qml/components/MidiInputPanel.qml
        qml/components/PlaybackControls.qml
        qml/components/PracticeReportPanel.qml
        qml/components/PracticeStatusPanel.qml
        qml/components/LocalMidiPanel.qml
        qml/components/ShortcutSidebar.qml
        qml/components/VirtualKeyboard.qml
)

target_include_directories(SmartPianoQt PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(SmartPianoQt PRIVATE
    Qt6::Core
    Qt6::Multimedia
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Sql
)

if(WIN32)
    target_link_libraries(SmartPianoQt PRIVATE winmm)
endif()

include(CTest)

if(BUILD_TESTING)
    add_executable(SmartPianoQtTests
        tests/test_core.cpp
        src/core/NoteUtils.cpp
        src/core/NoteUtils.h
        src/core/Song.h
        src/parser/JsonSheetParser.cpp
        src/parser/JsonSheetParser.h
        src/midi/MidiInputService.cpp
        src/midi/MidiInputService.h
        src/parser/MidiFileParser.cpp
        src/parser/MidiFileParser.h
        src/playback/PlaybackClock.cpp
        src/playback/PlaybackClock.h
        src/playback/PlaybackEngine.cpp
        src/playback/PlaybackEngine.h
        src/practice/PracticeEngine.cpp
        src/practice/PracticeEngine.h
        src/storage/PracticeRecordStore.cpp
        src/storage/PracticeRecordStore.h
    )

    target_include_directories(SmartPianoQtTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(SmartPianoQtTests PRIVATE
        Qt6::Core
        Qt6::Sql
    )

    if(WIN32)
        target_link_libraries(SmartPianoQtTests PRIVATE winmm)
    endif()

    if(WIN32)
        add_test(
            NAME SmartPianoQtCoreTests
            COMMAND ${CMAKE_COMMAND} -E env
                "PATH=$<TARGET_FILE_DIR:Qt6::Core>$<SEMICOLON>$ENV{PATH}"
                $<TARGET_FILE:SmartPianoQtTests>
        )
    else()
        add_test(NAME SmartPianoQtCoreTests COMMAND SmartPianoQtTests)
    endif()
endif()

include(GNUInstallDirs)

install(TARGETS SmartPianoQt
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
