cmake_minimum_required(VERSION 3.10)

project(dirt
    VERSION 0.1.1
    DESCRIPTION "DIRT (Delt's Impulse Response Tool) - a simple but effective deconvolver and IR extractor"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Options
option(DONT_USE_JACK "Disable JACK support even if the library is available" OFF)
option(GUI           "Enable wxWidgets GUI" OFF)
option(DEBUG         "Debug build" OFF)

# Kill any -flto that wx / toolchain might sneak in
#set(CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF)
#add_compile_options(-fno-lto)
#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fno-lto")

find_package(PkgConfig REQUIRED)

# --- wxWidgets (only if GUI) ---
if (GUI)
    message(STATUS "GUI enabled: linking against wxWidgets")
    find_package(wxWidgets REQUIRED COMPONENTS adv core base)
    include(${wxWidgets_USE_FILE})
else()
    message(STATUS "GUI disabled: building CLI only")
endif()

# --- Other libraries ---
pkg_check_modules(SNDFILE REQUIRED sndfile)
pkg_check_modules(FFTW3   REQUIRED fftw3)
pkg_check_modules(JACK            jack)

# --- Executable target ---
add_executable(dirt)

# Core sources (always)
target_sources(dirt PRIVATE
    src/dirt.cpp
    src/deconvolv.cpp
    src/jack.cpp
    src/miscjunk.cpp
)

# GUI sources (only when enabled)
if (GUI)
    target_sources(dirt PRIVATE
        src/ui.cpp
        src/wxwidgets/mainwindow.cpp
    )
    target_compile_definitions(dirt PRIVATE USE_WX)
endif()

# Timestamp header
set(TIMESTAMP_FILE ${CMAKE_CURRENT_BINARY_DIR}/timestamp.h)
set_source_files_properties(${TIMESTAMP_FILE} PROPERTIES GENERATED TRUE)
add_custom_target(timestamp ALL
    COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/timestamp.sh" "${TIMESTAMP_FILE}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    COMMENT "Updating build timestamp"
)
add_dependencies(dirt timestamp)
target_include_directories(dirt PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_compile_definitions(dirt PRIVATE DIRT_VERSION="${PROJECT_VERSION}")

# Includes
target_include_directories(dirt PRIVATE
    ${SNDFILE_INCLUDE_DIRS}
    ${FFTW3_INCLUDE_DIRS}
    ${PROJECT_SOURCE_DIR}
)

# Link libraries
target_link_libraries(dirt PRIVATE
    ${SNDFILE_LIBRARIES}
    ${FFTW3_LIBRARIES}
)

if (GUI)
    # NOTE: correct variable name is wxWidgets_LIBRARIES
    target_link_libraries(dirt PRIVATE ${wxWidgets_LIBRARIES})
    target_compile_definitions(dirt PRIVATE USE_WXWIDGETS)
endif()

# Optional JACK
if (JACK_FOUND AND NOT DONT_USE_JACK)
    message(STATUS "JACK: found")
    target_include_directories(dirt PRIVATE ${JACK_INCLUDE_DIRS})
    target_link_libraries(dirt PRIVATE ${JACK_LIBRARIES})
    target_compile_definitions(dirt PRIVATE USE_JACK)
elseif (DONT_USE_JACK)
    message(STATUS "JACK: disabled")
else()
    message(STATUS "JACK: not found")
endif()

# Debug define
if (DEBUG)
    message(STATUS "Debug enabled")
    target_compile_definitions(dirt PRIVATE DEBUG)
endif()

# Default build type
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

install(TARGETS dirt RUNTIME DESTINATION bin)
