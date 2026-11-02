cmake_minimum_required(VERSION 3.22)

project(ProjectSeven VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Static MSVC runtime so the distributed VST3 does not require the Visual C++
# redistributable on a clean Windows machine.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set(JUCE_ROOT "C:/SDK/JUCE" CACHE PATH "Path to a JUCE source or install tree")
if(EXISTS "${JUCE_ROOT}/install/lib/cmake")
    list(APPEND CMAKE_PREFIX_PATH "${JUCE_ROOT}/install")
endif()

find_package(JUCE CONFIG REQUIRED)

# JUCE 9 ships its SVG/plutovg implementation as a separate C translation unit
# which the installed CMake package does not add to the build. Add it once here
# so every target that compiles juce_graphics resolves those symbols.
set(PROJECT_SEVEN_PLUTOVG_SOURCE
    "${JUCE_ROOT}/install/include/JUCE-9.0.4/modules/juce_graphics/juce_graphics_lunasvg.c")

# We only write WAV files in the tests; disabling these avoids third-party codec
# link requirements in a project that has no use for them.
add_compile_definitions(
    JUCE_USE_FLAC=0
    JUCE_USE_OGGVORBIS=0
    JUCE_USE_OPUS=0
)

option(PROJECT_SEVEN_BUILD_TESTS "Build the Project Seven test suite" ON)
option(PROJECT_SEVEN_BUILD_VST3_SMOKE "Build the VST3 host smoke test" ON)
option(PROJECT_SEVEN_BUILD_EDITOR_SNAPSHOT "Build the offline editor screenshot tool" OFF)

# ---------------------------------------------------------------------------
# Shared engine / processor core. Kept separate from the plugin wrapper so the
# test executable can drive the exact same code headlessly.
# ---------------------------------------------------------------------------
add_library(projectseven_core STATIC
    src/params/Parameters.cpp
    src/params/TuningManager.cpp
    src/params/PresetManager.cpp
    src/dsp/StringModel.cpp
    src/dsp/PickupModel.cpp
    src/dsp/SevenAudioEngine.cpp
    src/core/PluginProcessor.cpp
    src/core/PluginEditor.cpp
    ${PROJECT_SEVEN_PLUTOVG_SOURCE}
)

target_include_directories(projectseven_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")

target_link_libraries(projectseven_core PUBLIC
    juce::juce_audio_basics
    juce::juce_audio_processors
    juce::juce_gui_basics
    juce::juce_data_structures
    juce::juce_dsp
    juce::juce_events
)

target_compile_definitions(projectseven_core PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

# ---------------------------------------------------------------------------
# VST3 instrument only. No VST2, AU, AAX or standalone target is generated.
# ---------------------------------------------------------------------------
juce_add_plugin(ProjectSeven
    PRODUCT_NAME "Project Seven"
    COMPANY_NAME "Project Seven Audio"
    BUNDLE_ID "com.projectseven.instrument"
    PLUGIN_MANUFACTURER_CODE Pjsn
    PLUGIN_CODE Ps7v
    FORMATS VST3
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES "Instrument" "Synth"
)

target_sources(ProjectSeven PRIVATE
    src/core/PluginEntry.cpp
)

target_link_libraries(ProjectSeven PRIVATE
    projectseven_core
    juce::juce_audio_utils
    juce::juce_gui_extra
    juce::juce_graphics
    juce::juce_recommended_config_flags
)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(PROJECT_SEVEN_BUILD_TESTS)
    enable_testing()

    juce_add_console_app(ProjectSevenTests PRODUCT_NAME "ProjectSevenTests")

    target_sources(ProjectSevenTests PRIVATE
        tests/TestMain.cpp
    )

    target_link_libraries(ProjectSevenTests PRIVATE
        projectseven_core
        juce::juce_audio_formats
        juce::juce_audio_utils
        juce::juce_recommended_config_flags
    )

    target_compile_definitions(ProjectSevenTests PRIVATE
        PROJECT_SEVEN_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
        JUCE_MODAL_LOOPS_PERMITTED=1
    )

    add_test(NAME ProjectSevenTests COMMAND ProjectSevenTests)
    set_tests_properties(ProjectSevenTests PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        TIMEOUT 300
    )
endif()

if(PROJECT_SEVEN_BUILD_VST3_SMOKE)
    juce_add_console_app(ProjectSevenVst3Smoke PRODUCT_NAME "ProjectSevenVst3Smoke")

    target_sources(ProjectSevenVst3Smoke PRIVATE
        tests/Vst3Smoke.cpp
    )

    target_link_libraries(ProjectSevenVst3Smoke PRIVATE
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_recommended_config_flags
    )

    target_compile_definitions(ProjectSevenVst3Smoke PRIVATE JUCE_PLUGINHOST_VST3=1)

    add_dependencies(ProjectSevenVst3Smoke ProjectSeven)

    add_test(NAME ProjectSevenVst3Smoke
        COMMAND ProjectSevenVst3Smoke "$<TARGET_FILE_DIR:ProjectSeven>/VST3/Project Seven.vst3")
    set_tests_properties(ProjectSevenVst3Smoke PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        TIMEOUT 120
    )
endif()

if(PROJECT_SEVEN_BUILD_EDITOR_SNAPSHOT)
    juce_add_console_app(ProjectSevenEditorSnapshot PRODUCT_NAME "ProjectSevenEditorSnapshot")

    target_sources(ProjectSevenEditorSnapshot PRIVATE
        tests/EditorSnapshot.cpp
    )

    target_link_libraries(ProjectSevenEditorSnapshot PRIVATE
        projectseven_core
        juce::juce_graphics
        juce::juce_recommended_config_flags
    )

    target_compile_definitions(ProjectSevenEditorSnapshot PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1)
endif()

# Put all runtime outputs in a predictable folder for packaging scripts.
set_target_properties(projectseven_core PROPERTIES FOLDER "Project Seven")
set_target_properties(ProjectSeven PROPERTIES FOLDER "Project Seven")
