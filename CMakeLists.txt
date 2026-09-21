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

find_package(OpenCV 5 REQUIRED COMPONENTS core imgproc imgcodecs)

add_library(eke_dx_wire
    src/core/geometry.cpp
    src/core/ids.cpp
    src/image/image_loader.cpp
    src/image/normalizer.cpp
    src/image/morphology_detector.cpp
    src/topology/topology_reconstructor.cpp
    src/export/svg_exporter.cpp
    src/export/topology_exporter.cpp
    src/pipeline/extraction_pipeline.cpp
)

target_include_directories(eke_dx_wire PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(eke_dx_wire PUBLIC opencv_core opencv_imgproc opencv_imgcodecs)
target_compile_features(eke_dx_wire PUBLIC cxx_std_23)

if(MSVC)
    target_compile_options(eke_dx_wire PRIVATE /W4)
else()
    target_compile_options(eke_dx_wire PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(dx-extract src/app/main.cpp)
target_link_libraries(dx-extract PRIVATE eke_dx_wire)

if(DX_WIRE_BUILD_TESTS)
    enable_testing()

    add_executable(dx-wire-test-geometry tests/test_geometry.cpp)
    target_link_libraries(dx-wire-test-geometry PRIVATE eke_dx_wire)
    add_test(NAME dx-wire-test-geometry COMMAND dx-wire-test-geometry)

    add_executable(dx-wire-test-ids tests/test_ids.cpp)
    target_link_libraries(dx-wire-test-ids PRIVATE eke_dx_wire)
    add_test(NAME dx-wire-test-ids COMMAND dx-wire-test-ids)

    add_executable(dx-wire-test-topology tests/test_topology.cpp)
    target_link_libraries(dx-wire-test-topology PRIVATE eke_dx_wire)
    add_test(NAME dx-wire-test-topology COMMAND dx-wire-test-topology)
endif()

install(TARGETS dx-extract RUNTIME DESTINATION bin)
