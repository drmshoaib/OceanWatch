include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.6.0
)

FetchContent_MakeAvailable(Catch2)

add_executable(oceanwatchai_tests
    AnalysisConfigTests.cpp
    CsvLoaderTests.cpp
    FeatureExtractorTests.cpp
    GeospatialUtilsTests.cpp
    ProtectedAreaProximityTests.cpp
    ReportWriterTests.cpp
    RiskScoringEngineTests.cpp
    StatisticalAnomalyDetectorTests.cpp
)

target_link_libraries(oceanwatchai_tests
    PRIVATE
        OceanWatchAI::oceanwatchai
        Catch2::Catch2WithMain
)

target_compile_features(oceanwatchai_tests PRIVATE cxx_std_20)

target_compile_definitions(oceanwatchai_tests
    PRIVATE OCEANWATCHAI_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)

if(MSVC)
    target_compile_options(oceanwatchai_tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(oceanwatchai_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(oceanwatchai_tests)

add_test(
    NAME cli_config_loading
    COMMAND
        "${CMAKE_COMMAND}"
        "-DCLI_EXECUTABLE=$<TARGET_FILE:OceanWatchAI>"
        "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}"
        "-DTEST_WORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli_config_loading"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CliConfigIntegrationTest.cmake
)
