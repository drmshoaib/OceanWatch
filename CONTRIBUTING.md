# Contributing

Thank you for your interest in OceanWatchAI. This project is a C++20 maritime trajectory-analysis prototype focused on readable, testable, and explainable AIS data processing.

## Development Setup

Required tools:

- CMake `3.21` or newer
- A C++20 compiler such as MSVC, GCC, or Clang
- Git

Recommended local workflow:

```powershell
cmake --preset ninja-debug
cmake --build --preset ninja-debug
ctest --test-dir out/build/ninja-debug --output-on-failure
```

Visual Studio users can open the repository folder directly and use the provided CMake presets.

## Contribution Guidelines

- Keep analytical methods deterministic and explainable unless the documentation clearly describes a different approach.
- Add or update Catch2 tests for parser logic, feature calculations, risk scoring, report output, and edge cases.
- Keep public headers in `include/oceanwatchai/` and implementations in `src/`.
- Document assumptions in `docs/` when adding new maritime, statistical, or geospatial methods.
- Avoid committing generated build output, local reports, editor metadata, or downloaded dependency sources.
- Use small sample data only when it is legally shareable and appropriate for public repositories.

## Pull Request Checklist

Before opening a pull request:

- Build the project with CMake.
- Run the full test suite.
- Update README or documentation if behaviour, usage, or assumptions changed.
- Keep risk-model claims framed as triage support rather than proof of illegal activity.

## Issue Reports

Helpful issue reports include:

- operating system and compiler version
- CMake version
- exact command used
- relevant test or runtime output
- minimal sample input when reporting parser or scoring behaviour
