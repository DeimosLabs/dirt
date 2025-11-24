cmake_minimum_required(VERSION 3.10)

project(dirt
    VERSION 0.1
    DESCRIPTION "DIRT (Delt's Impulse Response Tool) - a simple but effective deconvolver and IR extractor"
    LANGUAGES CXX)

add_compile_definitions(DIRT_VERSION="${PROJECT_VERSION}")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)

pkg_check_modules(SNDFILE REQUIRED sndfile)
pkg_check_modules(FFTW3 REQUIRED fftw3)
pkg_check_modules(JACK REQUIRED jack)

add_executable(dirt
    dirt.cpp
)

target_include_directories(dirt PRIVATE
    ${SNDFILE_INCLUDE_DIRS}
    ${FFTW3_INCLUDE_DIRS}
    ${JACK_INCLUDE_DIRS}
    ${PROJECT_SOURCE_DIR}
)

target_link_libraries(dirt PRIVATE
    ${SNDFILE_LIBRARIES}
    ${FFTW3_LIBRARIES}
    ${JACK_LIBRARIES}
)

# Nice default: `cmake -DCMAKE_BUILD_TYPE=Release ..`
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

install(TARGETS dirt RUNTIME DESTINATION bin)
