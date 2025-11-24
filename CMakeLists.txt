cmake_minimum_required(VERSION 3.10)

project(dirt
    VERSION 0.1
    DESCRIPTION "DIRT - Delt's Impulse Response Tool (deconvolver)"
    LANGUAGES CXX)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default to Release if nothing is specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Sources
add_executable(dirt
    dirt.cpp
)

# Dependencies via pkg-config
find_package(PkgConfig REQUIRED)

pkg_check_modules(SNDFILE REQUIRED sndfile)
pkg_check_modules(FFTW3  REQUIRED fftw3)
pkg_check_modules(JACK   REQUIRED jack)

target_include_directories(dirt
    PRIVATE
        ${SNDFILE_INCLUDE_DIRS}
        ${FFTW3_INCLUDE_DIRS}
        ${JACK_INCLUDE_DIRS}
)

target_link_libraries(dirt
    PRIVATE
        ${SNDFILE_LIBRARIES}
        ${FFTW3_LIBRARIES}
        ${JACK_LIBRARIES}
)

# Pass any extra flags from pkg-config (if present)
target_compile_options(dirt PRIVATE
    ${SNDFILE_CFLAGS_OTHER}
    ${FFTW3_CFLAGS_OTHER}
    ${JACK_CFLAGS_OTHER}
)

# Install rule (optional, but handy)
install(TARGETS dirt
    RUNTIME DESTINATION bin
)
