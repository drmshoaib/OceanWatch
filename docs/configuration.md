# Configuration

OceanWatchAI can load a JSON analysis configuration with `--config <path>`.

The checked-in default file is:

```text
configs/default_config.json
```

Running without `--config` uses the same values as `configs/default_config.json`, so existing CLI behaviour and report values remain unchanged.

## CLI Example

```powershell
.\out\build\ninja-debug\OceanWatchAI.exe `
  --ais data\sample\sample_ais.csv `
  --protected-areas data\sample\protected_areas.csv `
  --config configs\default_config.json `
  --output reports\risk_report.csv
```

## Schema Overview

The config is a JSON object with optional sections. Missing values fall back to the built-in defaults.

The current CLI applies `feature_extraction`, `risk_scoring`, and `risk_bands`. The `protected_area` section is loaded and validated as part of the shared default model, but protected-area report scoring remains unchanged in this step.

```json
{
  "feature_extraction": {
    "low_speed_min_knots": 1.0,
    "low_speed_max_knots": 5.0,
    "high_turning_threshold_deg": 45.0,
    "ais_gap_threshold_hours": 2.0,
    "suspicious_manoeuvre_turning_weight": 0.7,
    "suspicious_manoeuvre_gap_weight": 0.3
  },
  "risk_scoring": {
    "loitering_weight": 0.30,
    "ais_gap_weight": 0.25,
    "low_speed_weight": 0.15,
    "turning_weight": 0.20,
    "route_anomaly_weight": 0.10,
    "ais_gap_threshold_hours": 2.0,
    "severe_ais_gap_hours": 6.0,
    "max_gap_scale_hours": 10.0,
    "gap_count_scale": 3.0,
    "elevated_component_threshold": 50.0
  },
  "risk_bands": {
    "low_upper_bound": 25.0,
    "medium_upper_bound": 50.0,
    "high_upper_bound": 75.0
  },
  "protected_area": {
    "near_buffer_km": 5.0,
    "loitering_time_threshold_hours": 1.0,
    "inside_score_per_hour": 50.0,
    "near_score_per_hour": 25.0
  }
}
```

## Validation Rules

- Numeric values must be finite.
- Low-speed thresholds must satisfy `0 <= low_speed_min_knots <= low_speed_max_knots`.
- Turning thresholds must be in `[0, 180]` degrees.
- Risk-scoring weights must be non-negative and sum to `1.0`.
- Risk-band upper bounds must be strictly increasing.
- Scale factors such as `max_gap_scale_hours` and `gap_count_scale` must be positive.

Invalid config files cause the CLI to exit with an error before AIS analysis starts.
