# Geospatial Methods

OceanWatchAI uses lightweight geospatial methods for AIS trajectory analysis. The goal is to produce transparent movement features without introducing heavyweight GIS dependencies at this stage.

## Coordinate Model

Coordinates are decimal degrees:

- latitude in `[-90, 90]`
- longitude in `[-180, 180]`

Utility functions validate coordinate ranges and reject non-finite values.

## Distance

Distance is calculated with the Haversine formula using a mean Earth radius of `6371.0088 km`.

This assumes Earth is spherical. That approximation is acceptable for prototype-level AIS segment analysis, but it is not a replacement for ellipsoidal geodesics when legal or survey-grade boundary precision is required.

## Headings

Course and bearing angles are measured in degrees. Angles are normalized into `[0, 360)`, and angular differences are returned as the smallest absolute turn in `[0, 180]`.

Examples:

- `350` to `10` degrees is a `20` degree change.
- `359` to `1` degrees is a `2` degree change.
- `90` to `270` degrees is a `180` degree change.

This is used for turning behaviour and repeated manoeuvre features.

## Time

Timestamps are interpreted as UTC ISO-8601 strings in this exact shape:

```text
YYYY-MM-DDTHH:MM:SSZ
```

The implementation avoids local-time conversion APIs and converts valid UTC civil dates into seconds since the Unix epoch.

Current limitations:

- no timezone offsets such as `+01:00`
- no fractional seconds
- no leap-second support

## Derived Movement Checks

`segment_speed_knots`

Calculates distance divided by elapsed time and converts kilometres to nautical miles.

`is_speed_consistent`

Compares implied segment speed with the average reported AIS speed. This is useful as a screening heuristic for obviously inconsistent track segments.

`approximate_acceleration_knots_per_hour`

Calculates reported speed delta over elapsed hours.

`turning_angle_deg`

Calculates the minimal heading change between consecutive course headings.

## Protected-Area Approximation

Protected areas are currently represented as circles:

```text
centre_latitude, centre_longitude, radius_km
```

This is intentionally simple. Real protected areas are usually polygons with legal definitions, seasonal changes, and jurisdictional nuance. The circular approximation is documented and testable, but it should be replaced with polygon geometry before operational use.
