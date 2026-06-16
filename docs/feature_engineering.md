# Feature Engineering

OceanWatchAI extracts deterministic summary features from each `VesselTrack`. These features are intended as transparent inputs for later suspicious-behaviour scoring and are not a trained model.

## Track Assumptions

- Tracks are expected to be sorted by timestamp in ascending order.
- Timestamps use UTC ISO-8601 strings in the form `YYYY-MM-DDTHH:MM:SSZ`.
- Distances use the lightweight Haversine utilities described in `docs/geospatial_methods.md`.
- Reported speed values are taken from AIS points and expressed in knots.

## Feature Definitions

`total_distance_km`

Sum of Haversine distances between consecutive AIS points.

`duration_hours`

Elapsed time from the first point to the last point. Empty and single-point tracks have duration `0`.

`mean_speed_knots`

Arithmetic mean of reported AIS speed over all points.

`max_speed_knots`

Maximum reported AIS speed over all points.

`fraction_low_speed`

Fraction of AIS points with speed in the inclusive range `[1, 5]` knots.

`fraction_high_turning`

Fraction of consecutive point pairs where the minimal heading change is greater than `45` degrees.

`mean_turning_angle`

Mean minimal heading change across consecutive point pairs.

`max_time_gap_hours`

Largest time gap between consecutive AIS points.

`number_of_ais_gaps`

Count of consecutive point pairs with a time gap greater than `2` hours.

`loitering_score`

Prototype score combining low-speed movement and repeated turning:

```text
fraction_low_speed * fraction_high_turning
```

This keeps the score high only when both behaviours occur in the same track.

`suspicious_manoeuvre_score`

Prototype score combining aggressive turning and AIS gaps:

```text
0.7 * fraction_high_turning + 0.3 * ais_gap_fraction
```

where `ais_gap_fraction` is `number_of_ais_gaps / number_of_consecutive_point_pairs`.

## Edge Cases

Empty tracks return zero-valued features and preserve the vessel id.

Single-point tracks compute point-level speed features but return zero for distance, duration, turning, gap, and score fields.

Unsorted tracks are rejected because negative segment time would make duration, gaps, and movement-derived features ambiguous.
