include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.6.0
)

FetchContent_MakeAvailable(Catch2)

add_executable(oceanwatchai_tests
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

if(MSVC)
    target_compile_options(oceanwatchai_tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(oceanwatchai_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(oceanwatchai_tests)
