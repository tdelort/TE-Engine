cmake_minimum_required(VERSION 3.20)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
project(TEEngine VERSION 0.1.0)

add_library(TEEngine STATIC )

target_sources(TEEngine
    PRIVATE
)

target_compile_features(TEEngine PUBLIC cxx_std_20)
set_target_properties(
    TEEngine PROPERTIES

    CMAKE_CXX_STANDARD 20
    CMAKE_CXX_STANDARD_REQUIRED TRUE
    CMAKE_CXX_SCAN_FOR_MODULES ON
    CMAKE_CXX_EXTENSIONS OFF

    DEBUG_POSTFIX "D"
    RELWITHDEBINFO_POSTFIX "RD"
)

target_compile_definitions(TEEngine PRIVATE
  $<$<CONFIG:Debug>:_DEBUG>
)

target_link_options(TEEngine PRIVATE
  $<$<CONFIG:Debug>:-DEBUG>
)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g -Xclang -gcodeview -D_DEBUG -DEBUG")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -O3 -D_DEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

set(VEER_PROJECT_PATH libraries/Veer)
add_subdirectory(${VEER_PROJECT_PATH})

target_link_libraries(TEEngine
    PUBLIC 
        veer
)
