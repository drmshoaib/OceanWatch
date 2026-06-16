# Protected Area Proximity

OceanWatchAI uses a lightweight protected-area proximity module for prototype maritime analysis. It checks AIS points and vessel tracks against simple circular protected areas.

## Protected Area Model

Each area is represented as:

```text
name
centre_latitude
centre_longitude
radius_km
```

This is a deliberate approximation. Real marine protected areas are usually polygons with complex boundaries, seasonal closures, and legal definitions. Circular areas are useful here for transparent prototype behaviour and unit-testable trajectory features.

## CSV Format

Protected areas are loaded from CSV with this header:

```text
name,centre_latitude,centre_longitude,radius_km
```

Malformed protected-area reference data is treated as a file-level error. This differs from AIS trajectory rows, where malformed operational records can be skipped with warnings.

## Entry And Near-Zone Checks

An AIS point is inside a protected area when its Haversine distance from the area centre is less than or equal to `radius_km`.

An AIS point is near a protected area when it is outside the area but within:

```text
radius_km + near_buffer_km
```

The default near buffer is `5 km`.

## Time Inside And Near Areas

Track-level time is estimated from consecutive AIS point pairs:

- both endpoints inside: count the full segment duration as inside
- one endpoint inside: count half the segment duration as inside
- neither endpoint inside: count zero inside time

The same endpoint approximation is used for the near zone, excluding points that are already inside the protected area.

This is not a geometric line-intersection calculation. It is a simple, explainable approximation suitable for early-stage AIS intelligence features.

## Loitering Explanations

The analyzer emits explanations when:

- any AIS point enters a protected area
- estimated time inside an area is at least the loitering threshold
- estimated time near an area is at least the loitering threshold

The default loitering threshold is `1 hour`.
