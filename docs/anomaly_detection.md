# Statistical Anomaly Detection

OceanWatchAI includes lightweight fleet-baseline anomaly detection for numeric `TrackFeatures`. The module is deterministic, explainable, and intentionally avoids heavyweight GIS or machine-learning dependencies.

## Inputs And Outputs

Input:

```text
std::vector<TrackFeatures>
```

The detector can either compare one vessel against a supplied fleet baseline or score a full fleet with leave-one-out baselines.

Output:

- `anomaly_score` in `0-100`
- one `FeatureAnomalyFlag` per numeric feature
- contribution scores per feature
- explanation strings for features that exceed the selected threshold

## Numeric Features

The detector evaluates:

- `total_distance_km`
- `duration_hours`
- `mean_speed_knots`
- `max_speed_knots`
- `fraction_low_speed`
- `fraction_high_turning`
- `mean_turning_angle`
- `max_time_gap_hours`
- `number_of_ais_gaps`
- `loitering_score`
- `suspicious_manoeuvre_score`

## Z-Score Detection

The z-score detector compares a vessel feature value against the fleet mean and population standard deviation.

```text
z = abs(value - mean) / standard_deviation
```

The default anomaly threshold is `z >= 2.0`.

## Robust MAD Detection

The robust detector compares a vessel feature value against the fleet median and median absolute deviation.

```text
modified_z = 0.6745 * abs(value - median) / MAD
```

The default anomaly threshold is `modified_z >= 3.5`.

MAD is less sensitive to extreme values already present in the baseline fleet.

## Zero-Variance Baselines

If the fleet baseline has zero spread for a feature:

- a matching vessel value receives zero deviation
- a different vessel value is treated as anomalous

This keeps constant operational baselines useful without dividing by zero.

## Anomaly Score

Each feature contributes:

```text
100 * clamp(normalized_deviation / threshold)
```

The final anomaly score is the average of the three largest feature contributions. This highlights vessels with a small number of strong anomalies without allowing many normal features to dilute the signal.

## Risk Integration

`RiskScoringEngine` can optionally accept an `AnomalyDetectionResult`. When supplied, the anomaly score replaces the earlier route-anomaly proxy component for that scoring call. The original proxy based on `suspicious_manoeuvre_score` remains available through the existing `score(features)` overload.
