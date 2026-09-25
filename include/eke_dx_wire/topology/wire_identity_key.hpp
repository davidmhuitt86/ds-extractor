#pragma once

#include "eke_dx_wire/core/model.hpp"

#include <string>

namespace eke::dx::wire {

// AP-DIAG-FIX-004: a canonical, traversal-direction-independent identity
// key for a Wire record, used at the pipeline's Wire-population merge
// boundary (src/pipeline/extraction_pipeline.cpp) to detect when two
// Wire records - produced by different upstream reconstructors that use
// different endpoint-ordering conventions - represent the exact same
// physical wire.
//
// Background: PhysicalWireIdentityReconstructor orders a wire's endpoints
// by "smaller endpoint id first"; DistributionDecomposer (invoked from
// within ElectricalNetResolver) always orders them "anchor endpoint
// first", regardless of the anchor id's lexicographic relationship to
// the other endpoint. When both reconstructors independently discover
// the same physical wire, their Wire::id fields differ (Wire::id is
// itself derived from source+page+start+end, so reversing start/end
// changes the hash input) even though the wire is identical - which is
// exactly what let two duplicate Wire-record pairs through the
// pre-existing (AP-WIRE-004) id-based merge check that AP-DIAG-AUDIT-003
// found. See docs/AP-DIAG-FIX-004_Physical_Wire_Record_Deduplication.md
// for the full forensic trace.
//
// The key intentionally normalizes ONLY endpoint ordering (matching the
// documented failure exactly: "same endpoint pair + same topology path +
// same conductor path + reversed ordering") - it does not collapse two
// wires that merely share an endpoint or a conductor segment with each
// other, since AP-WIRE-029 permits multiple distinct physical wires to
// share conductor geometry (one net != one wire, one wire != one net).
// topology_edges/conductor_segments are included (sorted, since reversing
// traversal direction also reverses path order) so that two wires with
// the same endpoint pair but a genuinely different physical path are
// never merged into one key.
std::string canonical_wire_identity_key(const Wire& wire);

} // namespace eke::dx::wire
