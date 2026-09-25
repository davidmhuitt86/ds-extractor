cmake_minimum_required(VERSION 3.24)

project(EKE_DX_WIRE
    VERSION 0.1.2
    DESCRIPTION "Deterministic engineering diagram wire reconstruction engine"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(DX_WIRE_BUILD_TESTS "Build unit tests" ON)
option(DX_WIRE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

find_package(OpenCV 5 REQUIRED COMPONENTS core imgproc imgcodecs highgui)
find_package(CURL REQUIRED)

add_library(eke_dx_wire
    src/core/geometry.cpp
    src/core/ids.cpp
    src/core/extraction_audit.cpp
    src/core/coverage_diagnostics.cpp
    src/core/base64.cpp
    src/core/http_transport.cpp
    src/image/image_loader.cpp
    src/image/normalizer.cpp
    src/image/morphology_detector.cpp
    src/image/conductor_normalizer.cpp
    src/image/conductor_evidence_evaluator.cpp
    src/image/rejected_geometry_classifier.cpp
    src/image/geometry_ownership_classifier.cpp
    src/image/shape_detector.cpp
    src/image/text_region_detector.cpp
    src/image/symbol_geometry_extractor.cpp
    src/topology/topology_reconstructor.cpp
    src/topology/topology_semantic_resolver.cpp
    src/topology/gap_interpreter.cpp
    src/topology/endpoint_reconstructor.cpp
    src/topology/wire_reconstructor.cpp
    src/topology/physical_wire_identity_reconstructor.cpp
    src/topology/distribution_decomposer.cpp
    src/topology/terminal_semantic_resolver.cpp
    src/topology/endpoint_semantic_reconstructor.cpp
    src/topology/terminal_semantic_evidence_builder.cpp
    src/topology/terminal_location_detector.cpp
    src/topology/terminal_recognizer.cpp
    src/topology/connector_terminal_model.cpp
    src/topology/conductor_boundary_resolver.cpp
    src/topology/component_symbol_recognizer.cpp
    src/topology/circuit_role_resolver.cpp
    src/topology/electrical_net_resolver.cpp
    src/topology/circuit_role_evidence_builder.cpp
    src/topology/semantic_evidence_associator.cpp
    src/topology/semantic_observation_resolver.cpp
    src/topology/engineering_object_semantic_resolver.cpp
    src/topology/engineering_object_semantic_applier.cpp
    src/topology/component_identity_evidence_builder.cpp
    src/topology/component_identity_resolver.cpp
    src/topology/component_identity_registry.cpp
    src/topology/text_evidence_interpreter.cpp
    src/topology/text_recognition_provider.cpp
    src/topology/json_text_recognition_provider.cpp
    src/topology/sidecar_text_recognition_provider.cpp
    src/topology/recognition_observation_parser.cpp
    src/topology/anthropic_vision_text_recognition_provider.cpp
    src/topology/wire_model_validator.cpp
    src/topology/wire_semantic_resolver.cpp
    src/topology/symbol_recognition_provider.cpp
    src/topology/symbol_family_recognizer.cpp
    src/diagram/engineering_diagram_builder.cpp
    src/export/engineering_diagram_exporter.cpp
    src/image/component_candidate_classifier.cpp
    src/image/diagram_furniture_classifier.cpp
    src/export/symbol_renderer.cpp
    src/export/structured_svg_exporter.cpp
    src/export/svg_structure_validator.cpp
    src/export/artifact_writer.cpp
    src/export/review_artifact_writer.cpp
    src/export/topology_exporter.cpp
    src/export/recognition_input_exporter.cpp
    src/pipeline/extraction_pipeline.cpp
)

target_include_directories(eke_dx_wire PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(eke_dx_wire PUBLIC opencv_core opencv_imgproc opencv_imgcodecs)
target_link_libraries(eke_dx_wire PRIVATE CURL::libcurl)

if(WIN32)
    target_link_libraries(eke_dx_wire PUBLIC runtimeobject)
endif()
target_compile_features(eke_dx_wire PUBLIC cxx_std_23)

if(MSVC)
    target_compile_options(eke_dx_wire PRIVATE /W4)
else()
    target_compile_options(eke_dx_wire PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(dx-extract src/app/main.cpp)
target_link_libraries(dx-extract PRIVATE eke_dx_wire)

if(WIN32)
    add_executable(dx-extractor-gui src/gui/main.cpp)
    target_link_libraries(dx-extractor-gui PRIVATE eke_dx_wire
        opencv_highgui opencv_imgproc opencv_imgcodecs)
    target_link_libraries(dx-extractor-gui PRIVATE comdlg32)
endif()

if(DX_WIRE_BUILD_TESTS)
    enable_testing()

    add_executable(dx-wire-test-geometry tests/test_geometry.cpp)
    target_link_libraries(dx-wire-test-geometry PRIVATE eke_dx_wire)
    add_test(NAME dx-wire-test-geometry COMMAND dx-wire-test-geometry)

    add_executable(dx-wire-test-ids tests/test_ids.cpp)
    target_link_libraries(dx-wire-test-ids PRIVATE eke_dx_wire)
    add_test(NAME dx-wire-test-ids COMMAND dx-wire-test-ids)

    add_executable(dx-wire-test-topology tests/test_topology.cpp)
    add_executable(dx-wire-test-topology-semantic-resolver tests/test_topology_semantic_resolver.cpp)
    target_link_libraries(dx-wire-test-topology-semantic-resolver PRIVATE eke_dx_wire)
    add_test(NAME dx-wire-test-topology-semantic-resolver COMMAND dx-wire-test-topology-semantic-resolver)
    target_link_libraries(dx-wire-test-topology PRIVATE eke_dx_wire)
    add_test(NAME dx-wire-test-topology COMMAND dx-wire-test-topology)

    add_executable(dx-wire-test-conductor-normalizer tests/test_conductor_normalizer.cpp)
    target_link_libraries(dx-wire-test-conductor-normalizer PRIVATE eke_dx_wire)
    add_test(NAME dx-wire-test-conductor-normalizer COMMAND dx-wire-test-conductor-normalizer)

    add_executable(dx-wire-test-conductor-evidence-evaluator tests/test_conductor_evidence_evaluator.cpp)
    target_link_libraries(dx-wire-test-conductor-evidence-evaluator PRIVATE eke_dx_wire)
    add_test(NAME dx-wire-test-conductor-evidence-evaluator COMMAND dx-wire-test-conductor-evidence-evaluator)

    add_executable(dx-wire-test-rejected-geometry-classifier tests/test_rejected_geometry_classifier.cpp)
    target_link_libraries(dx-wire-test-rejected-geometry-classifier PRIVATE eke_dx_wire)
    add_test(NAME dx-wire-test-rejected-geometry-classifier COMMAND dx-wire-test-rejected-geometry-classifier)

    add_executable(dx-wire-test-geometry-ownership-classifier tests/test_geometry_ownership_classifier.cpp)
    target_link_libraries(dx-wire-test-geometry-ownership-classifier PRIVATE eke_dx_wire)
    add_test(NAME dx-wire-test-geometry-ownership-classifier COMMAND dx-wire-test-geometry-ownership-classifier)

    add_executable(dx-wire-test-endpoint-reconstructor tests/test_endpoint_reconstructor.cpp)
    target_link_libraries(dx-wire-test-endpoint-reconstructor PRIVATE eke_dx_wire)
    add_test(NAME dx-wire-test-endpoint-reconstructor COMMAND dx-wire-test-endpoint-reconstructor)

    add_executable(dx-wire-test-wire-reconstructor tests/test_wire_reconstructor.cpp)
    target_link_libraries(dx-wire-test-wire-reconstructor PRIVATE eke_dx_wire)
    add_test(NAME dx-wire-test-wire-reconstructor COMMAND dx-wire-test-wire-reconstructor)

    add_executable(dx-wire-test-physical-wire-identity-reconstructor tests/test_physical_wire_identity_reconstructor.cpp)
    target_link_libraries(dx-wire-test-physical-wire-identity-reconstructor PRIVATE eke_dx_wire)
    add_test(NAME dx-wire-test-physical-wire-identity-reconstructor COMMAND dx-wire-test-physical-wire-identity-reconstructor)

    add_executable(dx-wire-test-distribution-decomposer tests/test_distribution_decomposer.cpp)
    target_link_libraries(dx-wire-test-distribution-decomposer PRIVATE eke_dx_wire)
    add_test(NAME dx-wire-test-distribution-decomposer COMMAND dx-wire-test-distribution-decomposer)

    add_executable(dx-wire-test-endpoint-semantic-reconstructor tests/test_endpoint_semantic_reconstructor.cpp)
    target_link_libraries(dx-wire-test-endpoint-semantic-reconstructor PRIVATE eke_dx_wire)
    add_test(NAME dx-wire-test-endpoint-semantic-reconstructor COMMAND dx-wire-test-endpoint-semantic-reconstructor)

    add_executable(dx-wire-test-terminal-semantic-resolver tests/test_terminal_semantic_resolver.cpp)
    target_link_libraries(dx-wire-test-terminal-semantic-resolver PRIVATE eke_dx_wire)
    add_test(NAME dx-wire-test-terminal-semantic-resolver COMMAND dx-wire-test-terminal-semantic-resolver)
    add_executable(dx-wire-test-terminal-semantic-evidence-builder tests/test_terminal_semantic_evidence_builder.cpp)
    target_link_libraries(dx-wire-test-terminal-semantic-evidence-builder PRIVATE eke_dx_wire)
    add_test(NAME dx-wire-test-terminal-semantic-evidence-builder COMMAND dx-wire-test-terminal-semantic-evidence-builder)

    add_executable(dx-wire-test-terminal-location-detector tests/test_terminal_location_detector.cpp)
    add_executable(dx-wire-test-terminal-recognizer tests/test_terminal_recognizer.cpp)
    add_executable(dx-wire-test-connector-terminal-model tests/test_connector_terminal_model.cpp)
    add_executable(dx-wire-test-circuit-role-resolver tests/test_circuit_role_resolver.cpp)
    add_executable(dx-wire-test-electrical-net-resolver tests/test_electrical_net_resolver.cpp)
    add_executable(dx-wire-test-circuit-role-evidence-builder tests/test_circuit_role_evidence_builder.cpp)
    add_executable(dx-wire-test-semantic-evidence-associator tests/test_semantic_evidence_associator.cpp)
    add_executable(dx-wire-test-text-evidence-interpreter tests/test_text_evidence_interpreter.cpp)
    add_executable(dx-wire-test-semantic-observation-resolver tests/test_semantic_observation_resolver.cpp)
    add_executable(dx-wire-test-engineering-object-semantic-resolver tests/test_engineering_object_semantic_resolver.cpp)
    add_executable(dx-wire-test-engineering-object-semantic-applier tests/test_engineering_object_semantic_applier.cpp)
    add_executable(dx-wire-test-component-identity-evidence-builder tests/test_component_identity_evidence_builder.cpp)
    add_executable(dx-wire-test-component-identity-resolver tests/test_component_identity_resolver.cpp)
    add_executable(dx-wire-test-component-identity-registry tests/test_component_identity_registry.cpp)
    target_link_libraries(dx-wire-test-circuit-role-resolver PRIVATE eke_dx_wire)
    target_link_libraries(dx-wire-test-electrical-net-resolver PRIVATE eke_dx_wire)
    add_test(NAME dx-wire-test-electrical-net-resolver COMMAND dx-wire-test-electrical-net-resolver)
    add_test(NAME dx-wire-test-circuit-role-resolver COMMAND dx-wire-test-circuit-role-resolver)
    target_link_libraries(dx-wire-test-circuit-role-evidence-builder PRIVATE eke_dx_wire)
    add_test(NAME dx-wire-test-circuit-role-evidence-builder COMMAND dx-wire-test-circuit-role-evidence-builder)
    target_link_libraries(dx-wire-test-semantic-evidence-associator PRIVATE eke_dx_wire)
    add_test(NAME dx-wire-test-semantic-evidence-associator COMMAND dx-wire-test-semantic-evidence-associator)
    target_link_libraries(dx-wire-test-text-evidence-interpreter PRIVATE eke_dx_wire)
    target_link_libraries(dx-wire-test-semantic-observation-resolver PRIVATE eke_dx_wire)
    target_link_libraries(dx-wire-test-engineering-object-semantic-resolver PRIVATE eke_dx_wire)
    target_link_libraries(dx-wire-test-engineering-object-semantic-applier PRIVATE eke_dx_wire)
    target_link_libraries(dx-wire-test-component-identity-evidence-builder PRIVATE eke_dx_wire)
    target_link_libraries(dx-wire-test-component-identity-resolver PRIVATE eke_dx_wire)
    target_link_libraries(dx-wire-test-component-identity-registry PRIVATE eke_dx_wire)
    add_test(NAME dx-wire-test-engineering-object-semantic-applier COMMAND dx-wire-test-engineering-object-semantic-applier)
    add_test(NAME dx-wire-test-component-identity-evidence-builder COMMAND dx-wire-test-component-identity-evidence-builder)
    add_test(NAME dx-wire-test-component-identity-resolver COMMAND dx-wire-test-component-identity-resolver)
    add_test(NAME dx-wire-test-component-identity-registry COMMAND dx-wire-test-component-identity-registry)
    add_test(NAME dx-wire-test-semantic-observation-resolver COMMAND dx-wire-test-semantic-observation-resolver)
    add_test(NAME dx-wire-test-engineering-object-semantic-resolver COMMAND dx-wire-test-engineering-object-semantic-resolver)
    add_test(NAME dx-wire-test-text-evidence-interpreter COMMAND dx-wire-test-text-evidence-interpreter)

    add_executable(dx-wire-test-text-recognition-provider tests/test_text_recognition_provider.cpp)
    add_executable(dx-wire-test-json-text-recognition-provider tests/test_json_text_recognition_provider.cpp)
    target_link_libraries(dx-wire-test-json-text-recognition-provider PRIVATE eke_dx_wire)
    add_test(NAME dx-wire-test-json-text-recognition-provider COMMAND dx-wire-test-json-text-recognition-provider)
    target_link_libraries(dx-wire-test-text-recognition-provider PRIVATE eke_dx_wire)
    add_test(NAME dx-wire-test-text-recognition-provider COMMAND dx-wire-test-text-recognition-provider)

    add_executable(dx-wire-test-sidecar-text-recognition-provider tests/test_sidecar_text_recognition_provider.cpp)
    target_link_libraries(dx-wire-test-sidecar-text-recognition-provider PRIVATE eke_dx_wire)
    add_test(NAME dx-wire-test-sidecar-text-recognition-provider COMMAND dx-wire-test-sidecar-text-recognition-provider)

    add_executable(dx-wire-test-recognition-observation-parser tests/test_recognition_observation_parser.cpp)
    target_link_libraries(dx-wire-test-recognition-observation-parser PRIVATE eke_dx_wire)
    add_test(NAME dx-wire-test-recognition-observation-parser COMMAND dx-wire-test-recognition-observation-parser)

    add_executable(dx-wire-test-anthropic-vision-text-recognition-provider tests/test_anthropic_vision_text_recognition_provider.cpp)
    target_link_libraries(dx-wire-test-anthropic-vision-text-recognition-provider PRIVATE eke_dx_wire)
    add_test(NAME dx-wire-test-anthropic-vision-text-recognition-provider COMMAND dx-wire-test-anthropic-vision-text-recognition-provider)

    add_executable(dx-wire-test-base64 tests/test_base64.cpp)
    target_link_libraries(dx-wire-test-base64 PRIVATE eke_dx_wire)
    add_test(NAME dx-wire-test-base64 COMMAND dx-wire-test-base64)
    target_link_libraries(dx-wire-test-terminal-location-detector PRIVATE eke_dx_wire)
    target_link_libraries(dx-wire-test-terminal-recognizer PRIVATE eke_dx_wire)
    target_link_libraries(dx-wire-test-connector-terminal-model PRIVATE eke_dx_wire)
    add_test(NAME dx-wire-test-terminal-location-detector COMMAND dx-wire-test-terminal-location-detector)
    add_test(NAME dx-wire-test-terminal-recognizer COMMAND dx-wire-test-terminal-recognizer)
    add_test(NAME dx-wire-test-connector-terminal-model COMMAND dx-wire-test-connector-terminal-model)

    add_executable(dx-wire-test-conductor-boundary-resolver tests/test_conductor_boundary_resolver.cpp)
    target_link_libraries(dx-wire-test-conductor-boundary-resolver PRIVATE eke_dx_wire)
    add_test(NAME dx-wire-test-conductor-boundary-resolver COMMAND dx-wire-test-conductor-boundary-resolver)

    add_executable(dx-wire-test-component-symbol-recognizer tests/test_component_symbol_recognizer.cpp)
    target_link_libraries(dx-wire-test-component-symbol-recognizer PRIVATE eke_dx_wire)
    add_test(NAME dx-wire-test-component-symbol-recognizer COMMAND dx-wire-test-component-symbol-recognizer)

    add_executable(dx-wire-test-component-candidate-classifier tests/test_component_candidate_classifier.cpp)
    target_link_libraries(dx-wire-test-component-candidate-classifier PRIVATE eke_dx_wire)
    add_test(NAME dx-wire-test-component-candidate-classifier COMMAND dx-wire-test-component-candidate-classifier)

    add_executable(dx-wire-test-diagram-furniture-classifier tests/test_diagram_furniture_classifier.cpp)
    target_link_libraries(dx-wire-test-diagram-furniture-classifier PRIVATE eke_dx_wire)
    add_test(NAME dx-wire-test-diagram-furniture-classifier COMMAND dx-wire-test-diagram-furniture-classifier)

    add_executable(dx-wire-test-gap-interpreter tests/test_gap_interpreter.cpp)
    target_link_libraries(dx-wire-test-gap-interpreter PRIVATE eke_dx_wire)
    add_test(NAME dx-wire-test-gap-interpreter COMMAND dx-wire-test-gap-interpreter)

    add_executable(dx-wire-test-shape-detector tests/test_shape_detector.cpp)
    target_link_libraries(dx-wire-test-shape-detector PRIVATE eke_dx_wire)
    add_test(NAME dx-wire-test-shape-detector COMMAND dx-wire-test-shape-detector)

    add_executable(dx-wire-test-text-region-detector tests/test_text_region_detector.cpp)
    target_link_libraries(dx-wire-test-text-region-detector PRIVATE eke_dx_wire)
    add_test(NAME dx-wire-test-text-region-detector COMMAND dx-wire-test-text-region-detector)
    
    add_executable(dx-wire-test-wire-model-validator tests/test_wire_model_validator.cpp)
    target_link_libraries(dx-wire-test-wire-model-validator PRIVATE eke_dx_wire)
    add_test(NAME dx-wire-test-wire-model-validator COMMAND dx-wire-test-wire-model-validator)

    add_executable(dx-wire-test-extraction-audit tests/test_extraction_audit.cpp)
    add_executable(dx-wire-test-recognition-input-exporter tests/test_recognition_input_exporter.cpp)
    target_compile_definitions(dx-wire-test-recognition-input-exporter PRIVATE DX_WIRE_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_link_libraries(dx-wire-test-recognition-input-exporter PRIVATE eke_dx_wire)
    add_test(NAME dx-wire-test-recognition-input-exporter COMMAND dx-wire-test-recognition-input-exporter)
    target_link_libraries(dx-wire-test-extraction-audit PRIVATE eke_dx_wire)
    add_test(NAME dx-wire-test-extraction-audit COMMAND dx-wire-test-extraction-audit)

    add_executable(dx-wire-test-coverage-diagnostics tests/test_coverage_diagnostics.cpp)
    target_link_libraries(dx-wire-test-coverage-diagnostics PRIVATE eke_dx_wire)
    add_test(NAME dx-wire-test-coverage-diagnostics COMMAND dx-wire-test-coverage-diagnostics)

    add_executable(dx-wire-test-symbol-geometry-extractor tests/test_symbol_geometry_extractor.cpp)
    target_link_libraries(dx-wire-test-symbol-geometry-extractor PRIVATE eke_dx_wire)
    add_test(NAME dx-wire-test-symbol-geometry-extractor COMMAND dx-wire-test-symbol-geometry-extractor)

    add_executable(dx-wire-test-wire-semantic-resolver tests/test_wire_semantic_resolver.cpp)
    target_link_libraries(dx-wire-test-wire-semantic-resolver PRIVATE eke_dx_wire)
    add_test(NAME dx-wire-test-wire-semantic-resolver COMMAND dx-wire-test-wire-semantic-resolver)

    add_executable(dx-wire-test-engineering-diagram-builder tests/test_engineering_diagram_builder.cpp)
    target_link_libraries(dx-wire-test-engineering-diagram-builder PRIVATE eke_dx_wire)
    add_test(NAME dx-wire-test-engineering-diagram-builder COMMAND dx-wire-test-engineering-diagram-builder)

    add_executable(dx-wire-test-symbol-family-recognizer tests/test_symbol_family_recognizer.cpp)
    target_link_libraries(dx-wire-test-symbol-family-recognizer PRIVATE eke_dx_wire)
    add_test(NAME dx-wire-test-symbol-family-recognizer COMMAND dx-wire-test-symbol-family-recognizer)

    add_executable(dx-wire-test-structured-svg-exporter tests/test_structured_svg_exporter.cpp)
    target_link_libraries(dx-wire-test-structured-svg-exporter PRIVATE eke_dx_wire)
    add_test(NAME dx-wire-test-structured-svg-exporter COMMAND dx-wire-test-structured-svg-exporter)

    add_executable(dx-wire-test-svg-structure-validator tests/test_svg_structure_validator.cpp)
    target_link_libraries(dx-wire-test-svg-structure-validator PRIVATE eke_dx_wire)
    add_test(NAME dx-wire-test-svg-structure-validator COMMAND dx-wire-test-svg-structure-validator)

    add_executable(dx-wire-test-assertions-enabled tests/test_assertions_enabled.cpp)
    add_test(NAME dx-wire-test-assertions-enabled COMMAND dx-wire-test-assertions-enabled)

    # AP-TEST-FIX-001: CMake's Release configuration defines NDEBUG,
    # which compiles every assert() in these test executables into a
    # no-op - CTest would then only prove a test binary didn't crash,
    # never that any assertion it contains actually held. Every test
    # executable's own translation unit (never eke_dx_wire, dx-extract,
    # or dx-extractor-gui - production Release behavior is untouched)
    # gets NDEBUG removed again in Release-family configurations, so
    # assert() is real wherever this suite uses it. The library itself
    # contains no assert() calls, so this needs no library-side change.
    get_property(_dx_wire_test_names DIRECTORY PROPERTY TESTS)
    foreach(_dx_wire_test_name ${_dx_wire_test_names})
        if(TARGET ${_dx_wire_test_name})
            if(MSVC)
                target_compile_options(${_dx_wire_test_name} PRIVATE
                    $<$<CONFIG:Release>:/UNDEBUG>)
            else()
                target_compile_options(${_dx_wire_test_name} PRIVATE
                    $<$<CONFIG:Release>:-UNDEBUG>)
            endif()
        endif()
    endforeach()
endif()

install(TARGETS dx-extract RUNTIME DESTINATION bin)
