cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED CLI_EXECUTABLE)
    message(FATAL_ERROR "CLI_EXECUTABLE is required")
endif()

if(NOT DEFINED SOURCE_DIR)
    message(FATAL_ERROR "SOURCE_DIR is required")
endif()

if(NOT DEFINED TEST_WORK_DIR)
    message(FATAL_ERROR "TEST_WORK_DIR is required")
endif()

function(run_cli_success case_name)
    execute_process(
        COMMAND "${CLI_EXECUTABLE}" ${ARGN}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE stdout
        ERROR_VARIABLE stderr
    )

    if(NOT result EQUAL 0)
        message(FATAL_ERROR
            "${case_name} failed unexpectedly with exit code ${result}\n"
            "stdout:\n${stdout}\n"
            "stderr:\n${stderr}"
        )
    endif()
endfunction()

function(run_cli_failure case_name expected_text)
    execute_process(
        COMMAND "${CLI_EXECUTABLE}" ${ARGN}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE stdout
        ERROR_VARIABLE stderr
    )

    if(result EQUAL 0)
        message(FATAL_ERROR "${case_name} succeeded unexpectedly")
    endif()

    set(combined_output "${stdout}\n${stderr}")
    string(FIND "${combined_output}" "${expected_text}" expected_position)
    if(expected_position EQUAL -1)
        message(FATAL_ERROR
            "${case_name} did not report expected text: ${expected_text}\n"
            "stdout:\n${stdout}\n"
            "stderr:\n${stderr}"
        )
    endif()
endfunction()

file(REMOVE_RECURSE "${TEST_WORK_DIR}")
file(MAKE_DIRECTORY "${TEST_WORK_DIR}")

set(ais_path "${TEST_WORK_DIR}/config_test_ais.csv")
file(WRITE "${ais_path}"
"vessel_id,timestamp,latitude,longitude,speed_knots,course_deg\n"
"FV-CONFIG-001,2026-06-16T00:00:00Z,0.0,0.0,2.0,0.0\n"
"FV-CONFIG-001,2026-06-16T01:00:00Z,0.0,0.0,2.0,90.0\n"
"FV-CONFIG-001,2026-06-16T04:00:00Z,0.0,0.0,10.0,180.0\n"
)

set(no_config_csv "${TEST_WORK_DIR}/no_config.csv")
set(no_config_md "${TEST_WORK_DIR}/no_config.md")
run_cli_success(
    "CLI no-config run"
    --ais "${ais_path}"
    --output "${no_config_csv}"
    --markdown-output "${no_config_md}"
)

if(NOT EXISTS "${no_config_csv}")
    message(FATAL_ERROR "CLI no-config run did not create ${no_config_csv}")
endif()

set(default_config_csv "${TEST_WORK_DIR}/default_config.csv")
set(default_config_md "${TEST_WORK_DIR}/default_config.md")
run_cli_success(
    "CLI default-config run"
    --ais "${ais_path}"
    --config "${SOURCE_DIR}/configs/default_config.json"
    --output "${default_config_csv}"
    --markdown-output "${default_config_md}"
)

file(READ "${no_config_csv}" no_config_report)
file(READ "${default_config_csv}" default_config_report)
if(NOT no_config_report STREQUAL default_config_report)
    message(FATAL_ERROR
        "Default config output differs from no-config output\n"
        "no config:\n${no_config_report}\n"
        "default config:\n${default_config_report}"
    )
endif()

set(custom_config "${TEST_WORK_DIR}/custom_config.json")
file(WRITE "${custom_config}"
"{\n"
"  \"risk_scoring\": {\n"
"    \"loitering_weight\": 0.0,\n"
"    \"ais_gap_weight\": 0.0,\n"
"    \"low_speed_weight\": 0.0,\n"
"    \"turning_weight\": 0.0,\n"
"    \"route_anomaly_weight\": 1.0\n"
"  }\n"
"}\n"
)

set(custom_config_csv "${TEST_WORK_DIR}/custom_config.csv")
set(custom_config_md "${TEST_WORK_DIR}/custom_config.md")
run_cli_success(
    "CLI custom-config run"
    --ais "${ais_path}"
    --config "${custom_config}"
    --output "${custom_config_csv}"
    --markdown-output "${custom_config_md}"
)

file(READ "${custom_config_csv}" custom_config_report)
if(custom_config_report STREQUAL no_config_report)
    message(FATAL_ERROR "Custom config did not change CLI CSV output")
endif()

string(FIND "${custom_config_report}" "FV-CONFIG-001,85.000,Critical," custom_score_position)
if(custom_score_position EQUAL -1)
    message(FATAL_ERROR "Custom config did not produce the expected 85.000 Critical score:\n${custom_config_report}")
endif()

run_cli_failure(
    "CLI missing-config run"
    "Unable to open analysis config file"
    --ais "${ais_path}"
    --config "${TEST_WORK_DIR}/missing_config.json"
    --output "${TEST_WORK_DIR}/missing_config.csv"
)

set(malformed_config "${TEST_WORK_DIR}/malformed_config.json")
file(WRITE "${malformed_config}" "{\n  \"risk_scoring\": ")
run_cli_failure(
    "CLI malformed-config run"
    "Invalid analysis config JSON"
    --ais "${ais_path}"
    --config "${malformed_config}"
    --output "${TEST_WORK_DIR}/malformed_config.csv"
)

set(invalid_weights_config "${TEST_WORK_DIR}/invalid_weights_config.json")
file(WRITE "${invalid_weights_config}" "{ \"risk_scoring\": { \"route_anomaly_weight\": 0.5 } }\n")
run_cli_failure(
    "CLI invalid-weights config run"
    "risk_scoring weights must sum to 1.0"
    --ais "${ais_path}"
    --config "${invalid_weights_config}"
    --output "${TEST_WORK_DIR}/invalid_weights_config.csv"
)

set(invalid_threshold_config "${TEST_WORK_DIR}/invalid_threshold_config.json")
file(WRITE "${invalid_threshold_config}" "{ \"feature_extraction\": { \"high_turning_threshold_deg\": 181.0 } }\n")
run_cli_failure(
    "CLI invalid-threshold config run"
    "feature_extraction.high_turning_threshold_deg must be in [0, 180]"
    --ais "${ais_path}"
    --config "${invalid_threshold_config}"
    --output "${TEST_WORK_DIR}/invalid_threshold_config.csv"
)

set(invalid_bands_config "${TEST_WORK_DIR}/invalid_bands_config.json")
file(WRITE "${invalid_bands_config}" "{ \"risk_bands\": { \"medium_upper_bound\": 25.0 } }\n")
run_cli_failure(
    "CLI invalid-risk-band config run"
    "risk band upper bounds must be strictly increasing"
    --ais "${ais_path}"
    --config "${invalid_bands_config}"
    --output "${TEST_WORK_DIR}/invalid_bands_config.csv"
)
