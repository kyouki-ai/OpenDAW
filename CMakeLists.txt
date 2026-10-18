cmake_minimum_required(VERSION 3.22)
project(OpenDAW VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_BUILD_PARALLEL_LEVEL 1)
include(CTest)
option(OPENDAW_BUILD_APP "Build the JUCE desktop application" ON)
set(OPENDAW_JUCE_PATH "" CACHE PATH "Existing JUCE 8.0.12 source tree")

add_library(opendaw_core STATIC core/Project.cpp audio/Transport.cpp)
target_include_directories(opendaw_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
if(MSVC)
    target_compile_options(opendaw_core PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(opendaw_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BUILD_TESTING)
    add_executable(opendaw_core_tests tests/CoreTests.cpp)
    target_link_libraries(opendaw_core_tests PRIVATE opendaw_core)
    add_test(NAME core COMMAND opendaw_core_tests)
endif()

if(OPENDAW_BUILD_APP)
    if(WIN32)
        # A native CMake target needs only JUCE modules, not the juceaide executable.
        # Avoid helper-process Unicode path failures and needless bootstrap compilation.
        set(JUCE_MODULES_ONLY ON)
    endif()
    if(OPENDAW_JUCE_PATH)
        file(STRINGS "${OPENDAW_JUCE_PATH}/CMakeLists.txt" juce_version_line
             REGEX "^project\\(JUCE VERSION 8\\.0\\.12 ")
        if(NOT juce_version_line)
            message(FATAL_ERROR "OPENDAW_JUCE_PATH must contain JUCE 8.0.12")
        endif()
        add_subdirectory("${OPENDAW_JUCE_PATH}" "${CMAKE_BINARY_DIR}/JUCE")
    else()
        include(FetchContent)
        FetchContent_Declare(JUCE
            URL https://codeload.github.com/juce-framework/JUCE/zip/refs/tags/8.0.12
            URL_HASH SHA256=91ceb4b54f9cc763884f6285fcfee4372fbe72d33061a339255cee2957e34ad8)
        FetchContent_MakeAvailable(JUCE)
    endif()
    if(WIN32)
        # juceaide's narrow argv cannot read Cyrillic paths on the default Windows code page.
        # Let CMake/MSVC compile our own resources instead of invoking juceaide rcfile.
        enable_language(RC)
        configure_file(app/windows/OpenDAW.rc.in "${CMAKE_BINARY_DIR}/OpenDAW.rc" @ONLY)
        add_executable(OpenDAW WIN32 "${CMAKE_BINARY_DIR}/OpenDAW.rc" app/windows/OpenDAW.manifest)
        target_compile_definitions(OpenDAW PRIVATE JUCE_STANDALONE_APPLICATION=1)
        set_target_properties(OpenDAW PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/OpenDAW_artefacts/$<CONFIG>")
    else()
        juce_add_gui_app(OpenDAW PRODUCT_NAME "OpenDAW" COMPANY_NAME "OpenDAW Project"
                         BUNDLE_ID "local.opendaw.desktop" VERSION "${PROJECT_VERSION}")
    endif()
    target_sources(OpenDAW PRIVATE app/Main.cpp ui/MainComponent.cpp ui/Theme.cpp)
    target_include_directories(OpenDAW PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_compile_definitions(OpenDAW PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
                               JUCE_MODAL_LOOPS_PERMITTED=0)
    if(MSVC)
        target_compile_options(OpenDAW PRIVATE /utf-8)
    endif()
    target_link_libraries(OpenDAW PRIVATE opendaw_core juce::juce_gui_basics
        juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    if(BUILD_TESTING)
        add_test(NAME gui_smoke COMMAND OpenDAW --smoke-test
            --screenshot "${CMAKE_BINARY_DIR}/screenshots/opendaw.png")
        set_tests_properties(gui_smoke PROPERTIES TIMEOUT 30 RUN_SERIAL TRUE)
    endif()
endif()
