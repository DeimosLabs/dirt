cmake_minimum_required(VERSION 3.10)

project(dirt
    VERSION 0.1.1
    DESCRIPTION "DIRT (Delt's Impulse Response Tool) - a simple but effective deconvolver and IR extractor"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Option to explicitly disable JACK
option(DONT_USE_JACK "Disable JACK support even if the library is available" OFF)
option(DEBUG "Debug build" OFF)

find_package(PkgConfig REQUIRED)

pkg_check_modules(SNDFILE REQUIRED sndfile)
pkg_check_modules(FFTW3   REQUIRED fftw3)
pkg_check_modules(JACK             jack)

# Define the executable target first
add_executable(dirt
    dirt.cpp
)

# Version macro (per-target is cleaner than global add_compile_definitions)
target_compile_definitions(dirt PRIVATE DIRT_VERSION="${PROJECT_VERSION}")

# Always-needed includes/libs
target_include_directories(dirt PRIVATE
    ${SNDFILE_INCLUDE_DIRS}
    ${FFTW3_INCLUDE_DIRS}
    ${PROJECT_SOURCE_DIR}
)

target_link_libraries(dirt PRIVATE
    ${SNDFILE_LIBRARIES}
    ${FFTW3_LIBRARIES}
)

# Optional JACK stuff
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

if (DEBUG)
    MESSAGE(STATUS "Debug enabled")
    target_compile_definitions(dirt PRIVATE DEBUG)
endif()    

# Nice default: `cmake -DCMAKE_BUILD_TYPE=Release ..`
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

install(TARGETS dirt RUNTIME DESTINATION bin)
