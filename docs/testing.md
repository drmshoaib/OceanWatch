# Testing

OceanWatchAI uses Catch2 unit tests and CTest integration through CMake. The test suite is intended to make the analytical assumptions visible and to guard against regressions in parsing, math, feature extraction, scoring, and report generation.

## Test Areas

`CsvLoaderTests`

- valid AIS loading
- malformed row warnings
- multiple vessels
- timestamp ordering
- missing required columns

`GeospatialUtilsTests`

- Haversine distance
- coordinate validation
- angle wrap-around near `0/360`
- UTC timestamp differences
- speed consistency
- acceleration
- turning angle

`FeatureExtractorTests`

- empty and single-point tracks
- distance and duration
- low-speed fraction
- turning fraction
- AIS gap counting
- score formulas
- unsorted-track rejection

`RiskScoringEngineTests`

- component score calculations
- risk band thresholds
- explanation generation
- invalid feature validation

`StatisticalAnomalyDetectorTests`

- z-score detection
- robust median absolute deviation
- leave-one-out fleet scoring
- optional risk-engine route-anomaly integration

`ProtectedAreaProximityTests`

- protected-area CSV loading
- point entry
- near-zone detection
- time inside and near areas
- invalid configuration

`ReportWriterTests`

- compact CSV schema
- Markdown report sections
- CSV escaping
- output directory creation

## Running Tests

```powershell
cmake --preset ninja-debug
cmake --build --preset ninja-debug
ctest --test-dir out/build/ninja-debug --output-on-failure
```

Visual Studio users can also run tests through Test Explorer after opening the CMake folder.

## Current Coverage Philosophy

The tests use small synthetic datasets where expected values are easy to verify. This is intentional: the project prioritizes auditability of formulas and rule thresholds.

Future testing should add larger AIS fixtures, fuzzing for CSV input, performance tests, and integration tests over representative real-world data after licensing and privacy constraints are understood.
