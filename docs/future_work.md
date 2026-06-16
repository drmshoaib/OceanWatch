# Future Work

OceanWatchAI is currently a transparent AIS trajectory analysis prototype. The next improvements should focus on stronger geospatial realism, better baselines, larger data handling, and clearer analyst workflows.

## Geospatial Accuracy

- Replace circular protected areas with polygon boundaries.
- Add support for EEZs, marine protected areas, port zones, and seasonal closures.
- Use robust line-polygon intersection for time-inside estimates.
- Support geodesic calculations where boundary precision matters.

## AIS Data Handling

- Support larger AIS datasets with streaming or chunked processing.
- Add schema validation profiles for different AIS providers.
- Support additional fields such as MMSI, vessel type, destination, navigational status, and draught.
- Add duplicate-point handling and configurable interpolation.

## Behavioural Analysis

- Build vessel-type-aware baselines.
- Compare vessels against peer groups rather than a single global fleet.
- Add port-approach and fishing-ground context.
- Detect repeated visits, rendezvous patterns, and suspicious stop-start behaviour.
- Improve route anomaly modelling beyond simple statistical feature deviations.

## Reporting And Analyst Workflow

- Add map visualisations for tracks and protected areas.
- Add interactive filtering by risk band, component score, and vessel id.
- Export richer JSON for downstream systems.
- Add report provenance metadata such as model version, data source, and run timestamp.

## Performance And Production Readiness

- Benchmark on larger track volumes.
- Add memory and runtime profiling.
- Add structured logging.
- Add configuration files for thresholds and scoring weights.
- Add CI builds for Windows and Linux.

## Data Integration

The project should remain honest about data sources. Current analysis is AIS-only. Future integrations could include satellite imagery, radar, licensing records, or enforcement datasets, but those are not implemented today and would require separate data pipelines, validation, and documentation.
