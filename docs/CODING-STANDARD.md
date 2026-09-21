# EKE-DX-WIRE Coding Standard

## Language

C++23.

## General rules

- Prefer value semantics for domain objects.
- Use `std::string`, `std::vector`, `std::optional`, `std::span` and standard
  containers unless a specialized structure is justified.
- Do not expose OpenCV types from domain headers.
- Keep image-processing implementation in image/algorithm modules.
- No global mutable state.
- No hidden configuration.
- No stage may silently mutate an earlier artifact.

## Naming

- Types: `PascalCase`
- functions/methods: `snake_case`
- variables: `snake_case`
- constants: `UPPER_SNAKE_CASE` only when they are compile-time constants with
  semantic significance.
- IDs use explicit prefixes such as `wire-segment-...`.

## Geometry

- Source image coordinates use pixel-space origin `(0,0)` at the top-left.
- Floating-point coordinates are used internally.
- Integer conversion is performed only at raster boundaries.
- Tolerances are configuration values, never magic constants.

## Logging

Every pipeline stage should report:

- input object count
- output object count
- rejected object count
- unresolved object count
- elapsed time
- configuration identity

## Testing

Every algorithmic stage requires:

1. unit tests,
2. deterministic regression tests,
3. at least one visual/reference fixture when geometry is involved.

## Prohibited patterns

- edge-pair tracing as the primary conductor representation;
- hard-coded coordinates in production algorithms;
- automatic junction inference from intersection alone;
- embedding raster images as a substitute for vector reconstruction;
- GUI-specific extraction logic.
