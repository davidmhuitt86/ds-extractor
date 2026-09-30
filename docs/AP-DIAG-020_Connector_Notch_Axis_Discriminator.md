# AP-DIAG-020 — Connector Notch-Axis Discriminator

## Status

**COMPLETE.** Implementation AP for one proven defect, per AP-DIAG-019.

## Baseline

- Baseline commit: `133357e` (AP-DIAG-019). Verified as current HEAD before
  any change in this AP.
- Source: `samples/trx300ODG.png`, 898×549, page 0.
- Baseline connector population (re-confirmed by a fresh extraction against
  unmodified code before making any change, matching AP-DIAG-019 exactly):

| ID | bbox | notch axis (measured) | classification |
|---|---|---|---|
| `connector-a228885f25dd1f6b` | (600,289,18,63) | left/right | REAL_CONNECTOR |
| `connector-3bb50861590cea40` | (355,352,17,26) | left/right | REAL_CONNECTOR |
| `connector-08bf96db0b914922` | (369,81,28,11) | top/bottom | FALSE_POSITIVE |
| `connector-fdd33a2a7f942823` | (371,249,49,10) | top/bottom | FALSE_POSITIVE |
| `connector-7a793f0ead4a15ca` | (371,259,49,10) | top/bottom | FALSE_POSITIVE |
| `connector-0aa656ccf7f56e0e` | (365,345,46,10) | top/bottom | FALSE_POSITIVE |
| `connector-713147ddb5e260d6` | (420,304,57,51) | top/bottom | FALSE_POSITIVE |

## Root cause (established by AP-DIAG-019, re-verified before editing)

`ConnectorGeometryDetector::detect` (`src/image/connector_geometry_detector.cpp`)
computed:

```cpp
const bool opposing_notches =
    (notch_left && notch_right) ||
    (notch_top && notch_bottom);
```

accepting left/right-axis and top/bottom-axis opposing convexity-defect
pairs as equally valid connector-body evidence. AP-DIAG-019 measured,
directly against the real source, that both genuine TRX300 connectors
show their notch pair exclusively on the left/right axis, while all 5
false positives show theirs exclusively on the top/bottom axis — a
7/7, zero-exception discriminator.

## Implementation

Smallest possible change: `opposing_notches` now requires the left/right
axis specifically. No new configuration parameter was introduced — the
existing per-defect nearest-side classification (`notch_left`/`notch_right`)
was already computed; only the top/bottom bookkeeping (`notch_top`,
`notch_bottom`), which fed nothing else in the function, was removed
along with the now-redundant `else` branch that set them. A defect whose
nearest side is top or bottom is simply not counted toward
`notch_left`/`notch_right` — it is excluded, not misattributed to
whichever of left/right happens to be nearer.

```cpp
// AP-DIAG-020: AP-DIAG-019 measured this directly against the real
// TRX300 source (7/7 connector candidates, zero exceptions): both
// genuine connector bodies show their opposing notch pair exclusively
// on the left/right axis, while every false positive (wire-color text
// glyphs and a diode symbol sitting on a horizontal wire, plus one
// lower-confidence case) shows its opposing pair exclusively on the
// top/bottom axis - ink that happens to bulge above/below a horizontal
// baseline, not the TRX300 connector family's actual notch/interlock
// profile. The top/bottom axis is therefore not accepted as equivalent
// evidence.
const bool opposing_notches = notch_left && notch_right;
```

`src/image/connector_geometry_detector.cpp` is the only production
source file changed. No wire detection, topology construction, endpoint
reconstruction, net resolution, component detection, circle detection,
grid-spacing logic, `DistributionDecomposer`, electrical-net behavior, or
Wire identity code was touched.

## Tests

`tests/test_connector_geometry_detector.cpp`: added two new synthetic
fixtures.

1. **`top_bottom`** — the literal transpose (x and y swapped) of the
   already-accepted left/right-notched body fixture. Every geometric
   property other than axis (area, notch depth, notch count, fill ratio)
   is therefore identical to a fixture already known to pass all other
   filters. Asserts `top_bottom_result.regions.empty()`.
2. **`stub`** — a structurally distinct top/bottom silhouette: a
   horizontal bar with a short stub protruding above and another below,
   representative of the general class of incidental ink (a glyph
   stroke, a symbol's apex) bulging above/below a wire rather than the
   TRX300 connector family's left/right interlock. Asserts
   `stub_result.regions.empty()`.

Neither fixture encodes any of the five real false-positive object IDs or
their exact coordinates — both are independently constructed synthetic
geometries sharing only the causal property (top/bottom notch axis).

**Causality verification** (required by the AP, performed before
finalizing): both fixtures were run against the *unmodified*
`connector_geometry_detector.cpp` (saved from `git show HEAD:...` before
editing) and confirmed to produce a non-empty region — i.e., both would
have been **accepted** as connectors under the old code. After restoring
the fix, both correctly produce zero regions. This proves the rejection
is caused specifically by the notch-axis discriminator, not by any other
filter (area/fill/aspect/size are unchanged from the still-accepted
left/right fixture).

The existing 4 fixtures (`first`/`second` inline body, `attached_result`,
`unrelated_result`, `one_sided_result`) are unmodified and continue to
pass, confirming the two genuine-connector-shaped fixtures remain
accepted and the two already-rejected fixtures remain rejected.

## Validation

Clean Release build (`cmake -DCMAKE_BUILD_TYPE=Release`, full rebuild):
succeeded, no warnings from the changed file.

Full Release CTest suite: **66/66 passing** (64 pre-existing + the 2 new
fixtures inside the existing `dx-wire-test-connector-geometry-detector`
target — no new CTest target was needed since the fixtures were added to
the existing, already-registered test binary).

Real TRX300 extraction (`dx-extract extract samples/trx300ODG.png`),
before vs. after, same source, same page:

| population | before | after | changed? |
|---|---|---|---|
| connector candidates | 7 | 2 | yes (targeted) |
| genuine connectors | 2 | 2 | no |
| false-positive connectors | 5 | 0 | yes (targeted) |
| connector terminals | 6 | 1 | yes (downstream of connectors) |
| components | 34 | 34 | no |
| terminal candidates | 18 | 18 | no |
| conductor segments | 230 | 230 | no |
| wires | 37 | 37 | no |
| endpoint candidates | 191 | 191 | no |
| topology nodes | 533 | 533 | no |
| topology edges | 643 | 643 | no |
| electrical nets | 12 | 12 | no |
| validation errors | 0 | 0 | no |
| validation warnings | 34 | 34 | no |

The 2 surviving connector candidates after the fix are exactly
`connector-a228885f25dd1f6b` and `connector-3bb50861590cea40` — the same
2 IDs AP-DIAG-019 identified as genuine, confirmed by identical stable
IDs, not merely a matching count.

## Unrelated populations

**No change observed or claimed** in any of the following, all explicitly
out of this AP's scope per AP-DIAG-019's remaining findings: the 21
circular-symbol/junction-dot false positives, the `bounded_by_crossing_lines`
25px probe-distance gap, the 62 independent net-resolution-gap endpoints,
the 54-endpoint upstream semantic-resolution question, and the 27
unresolved-wire population. Every population count above that is not
`connector_candidates` or `connector_terminals` is unchanged, confirming
this fix did not incidentally touch anything beyond
`ConnectorGeometryDetector`'s own output.

## Commit discipline

Single commit containing: the detector change, the two regression
fixtures, this document, and the machine-readable artifact. No CMake
registration was needed (the fixtures extend the already-registered
`dx-wire-test-connector-geometry-detector` target). No unrelated cleanup
included.

## Artifacts

- `docs/AP-DIAG-020_Connector_Notch_Axis_Discriminator.md` (this file)
- `artifacts/audit/AP-DIAG-020_connector_notch_axis_fix.json`
- AP-DIAG-019's own artifacts
  (`artifacts/audit/AP-DIAG-019_correlated_failure_investigation.json`,
  its doc) are unmodified — they remain the historical record of the
  investigation this AP implements one finding from.

## Next AP (recommended, not started)

Per AP-DIAG-019 §11, the next-highest-leverage candidate is the
`bounded_by_crossing_lines` probe-distance gap (Part B), affecting up to
21 of 34 current component candidates — a larger population than this
AP's connector fix touched. Recommended as **AP-DIAG-021**, scoped
strictly to that one function in `shape_detector.cpp`. Not started
automatically.
