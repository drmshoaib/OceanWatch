# Risk Model

OceanWatchAI uses a deterministic rule-based risk model. The model is designed for explainability and triage, not for making enforcement conclusions.

## Inputs

The scorer consumes `TrackFeatures` generated from AIS vessel tracks:

- distance and duration
- mean and maximum speed
- fraction of low-speed points
- fraction of high-turning segments
- mean turning angle
- maximum AIS time gap
- number of AIS gaps
- loitering score
- suspicious manoeuvre score

The route-anomaly component can use either the built-in manoeuvre proxy or an optional fleet-baseline anomaly score.

## Component Scores

All component scores are normalized to `0-100`.

`loitering`

```text
100 * loitering_score
```

`AIS_gap`

The AIS gap component is the maximum of:

```text
100 * clamp((max_time_gap_hours - 2) / 10)
100 * clamp(number_of_ais_gaps / 3)
```

Gaps start contributing after `2` hours. The duration component reaches `100` at a `12` hour gap.

`low_speed`

```text
100 * fraction_low_speed
```

Low speed is defined as reported AIS speed in the inclusive range `[1, 5]` knots.

`turning`

```text
100 * fraction_high_turning
```

High turning is defined as a heading change greater than `45` degrees.

`route_anomaly`

Default proxy:

```text
100 * suspicious_manoeuvre_score
```

Optional anomaly integration:

```text
anomaly_score
```

The optional path uses `StatisticalAnomalyDetector` output. It is useful when a fleet baseline is available.

## Weighted Total

```text
0.30 * loitering
+ 0.25 * AIS_gap
+ 0.15 * low_speed
+ 0.20 * turning
+ 0.10 * route_anomaly
```

The result is clamped to `0-100`.

## Risk Bands

```text
Low       score < 25
Medium    25 <= score < 50
High      50 <= score < 75
Critical  score >= 75
```

## Explanations

The scorer emits human-readable explanations when rules are triggered. Examples:

- AIS gap exceeded `6` hours
- vessel displayed slow repeated turning behaviour
- many points were in the `1-5` knot low-speed band
- frequent heading changes above `45` degrees
- route anomaly proxy or fleet-baseline anomaly score was elevated

## Interpretation

Risk scores should be treated as triage indicators. A high score means the track deserves review; it does not prove illegal fishing. AIS-only analysis is especially vulnerable to missing context, spoofing, benign operational patterns, weather effects, and data gaps.
