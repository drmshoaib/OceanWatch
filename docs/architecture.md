# Architecture Notes

## Current Scope

The current implementation is a small C++20 library plus a command-line executable. The library owns AIS CSV loading, validation, row-level warnings, vessel grouping, and timestamp ordering; the executable demonstrates loading a sample file and reporting point and vessel counts.

## Planned Analysis Flow

Future risk scoring can be layered behind the CSV loader without changing the input contract:

1. Load AIS points from CSV.
2. Group points into vessel tracks.
3. Sort each vessel trajectory by timestamp.
4. Extract movement features such as distance, duration, low-speed loitering, abrupt course changes, and signal gaps.
5. Combine feature outputs into suspicious-behaviour risk scores.

## Design Boundaries

The current version does not implement risk scores, geofencing, route clustering, persistence, or model inference. Those components should be added as separate modules once the ingestion path is stable and covered by tests.
