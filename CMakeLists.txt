cmake_minimum_required(VERSION 3.21)

project(
    OceanWatchAI
    VERSION 0.1.0
    DESCRIPTION "Maritime intelligence prototype for AIS trajectory analysis"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(OCEANWATCHAI_BUILD_TESTS "Build OceanWatchAI unit tests" ${PROJECT_IS_TOP_LEVEL})

add_library(oceanwatchai
    src/AisCsvLoader.cpp
    src/FeatureExtractor.cpp
    src/GeospatialUtils.cpp
    src/ProtectedAreaProximity.cpp
    src/ReportWriter.cpp
    src/RiskScoringEngine.cpp
    src/StatisticalAnomalyDetector.cpp
    src/VesselTrack.cpp
)

add_library(OceanWatchAI::oceanwatchai ALIAS oceanwatchai)

target_include_directories(oceanwatchai
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(oceanwatchai PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(oceanwatchai PRIVATE /W4 /permissive-)
else()
    target_compile_options(oceanwatchai PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(OceanWatchAI
    src/main.cpp
)

target_link_libraries(OceanWatchAI
    PRIVATE OceanWatchAI::oceanwatchai
)

set_target_properties(OceanWatchAI PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
)

if(OCEANWATCHAI_BUILD_TESTS)
    include(CTest)
    enable_testing()
    add_subdirectory(tests)
endif()
