cmake_minimum_required(VERSION 3.15)
project(Gritbaal VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/clap/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

# Source files
set(CORE_SOURCES
    src/core/SynthEngine.cpp
    src/core/Oscillator.cpp
    src/core/Filter.cpp
    src/core/Envelope.cpp
)

set(CLAP_SOURCES
    src/clap/GritbaalClap.cpp
)

set(GUI_SOURCES
    src/gui/Font.cpp
    src/gui/Graphics.cpp
    src/gui/TB303ControlRenderer.cpp
    src/gui/IndustrialGritbaalRenderer.cpp
    src/gui/GuiWindow.cpp
)

# Linux X11 library dependency for GUI
if(UNIX AND NOT APPLE)
    find_package(X11 REQUIRED)
    set(GUI_LIBS ${X11_LIBRARIES})
elseif(APPLE)
    find_library(COCOA_LIBRARY Cocoa)
    set(GUI_LIBS ${COCOA_LIBRARY})
elseif(WIN32)
    set(GUI_LIBS gdi32 user32)
endif()

# Gritbaal CLAP plugin library module
add_library(gritbaal MODULE
    ${CORE_SOURCES}
    ${CLAP_SOURCES}
    ${GUI_SOURCES}
)

set_target_properties(gritbaal PROPERTIES
    PREFIX ""
    SUFFIX ".clap"
    OUTPUT_NAME "gritbaal"
)

target_link_libraries(gritbaal PRIVATE ${GUI_LIBS})

# Standalone test executable for DSP verification
add_executable(gritbaal_dsp_test
    src/core/SynthEngine.cpp
    src/core/Oscillator.cpp
    src/core/Filter.cpp
    src/core/Envelope.cpp
    src/dsp_test_main.cpp
)

# Standalone test executable for GUI frame verification
add_executable(gritbaal_gui_test
    ${CORE_SOURCES}
    ${CLAP_SOURCES}
    ${GUI_SOURCES}
    src/gui_test_main.cpp
)
target_link_libraries(gritbaal_gui_test PRIVATE ${GUI_LIBS})
