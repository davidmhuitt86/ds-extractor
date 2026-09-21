#include "eke_dx_wire/image/conductor_normalizer.hpp"

#include "eke_dx_wire/core/ids.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>
#include <vector>

namespace eke::dx::wire {
namespace {

double confidence_rank(ConfidenceClass value) {
    switch (value) {
    case ConfidenceClass::High: return 4.0;
    case ConfidenceClass::Medium: return 3.0;
    case ConfidenceClass::Low: return 2.0;
    case ConfidenceClass::Unresolved: return 1.0;
    }
    return 0.0;
}

bool same_geometry(
    const Segment2D& a,
    const Segment2D& b,
    double tolerance) {

    const bool same_forward =
        distance(a.a, b.a) <= tolerance &&
        distance(a.b, b.b) <= tolerance;

    const bool same_reverse =
        distance(a.a, b.b) <= tolerance &&
        distance(a.b, b.a) <= tolerance;

    return same_forward || same_reverse;
}

bool horizontal(const ConductorSegment& segment, double tolerance) {
    return segment.geometry.is_horizontal(tolerance);
}

bool vertical(const ConductorSegment& segment, double tolerance) {
    return segment.geometry.is_vertical(tolerance);
}

bool mergeable(
    const ConductorSegment& a,
    const ConductorSegment& b,
    double tolerance) {

    if (horizontal(a, tolerance) && horizontal(b, tolerance)) {
        if (std::abs(a.geometry.a.y - b.geometry.a.y) > tolerance)
            return false;

        const double a0 = std::min(a.geometry.a.x, a.geometry.b.x);
        const double a1 = std::max(a.geometry.a.x, a.geometry.b.x);
        const double b0 = std::min(b.geometry.a.x, b.geometry.b.x);
        const double b1 = std::max(b.geometry.a.x, b.geometry.b.x);

        return b0 <= a1 + tolerance && a0 <= b1 + tolerance;
    }

    if (vertical(a, tolerance) && vertical(b, tolerance)) {
        if (std::abs(a.geometry.a.x - b.geometry.a.x) > tolerance)
            return false;

        const double a0 = std::min(a.geometry.a.y, a.geometry.b.y);
        const double a1 = std::max(a.geometry.a.y, a.geometry.b.y);
        const double b0 = std::min(b.geometry.a.y, b.geometry.b.y);
        const double b1 = std::max(b.geometry.a.y, b.geometry.b.y);

        return b0 <= a1 + tolerance && a0 <= b1 + tolerance;
    }

    return false;
}

ConductorSegment merge_pair(
    const ConductorSegment& a,
    const ConductorSegment& b,
    double tolerance) {

    ConductorSegment merged = a;

    if (horizontal(a, tolerance)) {
        const double y = (a.geometry.a.y + b.geometry.a.y) * 0.5;
        const double x0 = std::min({
            a.geometry.a.x, a.geometry.b.x,
            b.geometry.a.x, b.geometry.b.x});
        const double x1 = std::max({
            a.geometry.a.x, a.geometry.b.x,
            b.geometry.a.x, b.geometry.b.x});
        merged.geometry = {{x0, y}, {x1, y}};
    } else {
        const double x = (a.geometry.a.x + b.geometry.a.x) * 0.5;
        const double y0 = std::min({
            a.geometry.a.y, a.geometry.b.y,
            b.geometry.a.y, b.geometry.b.y});
        const double y1 = std::max({
            a.geometry.a.y, a.geometry.b.y,
            b.geometry.a.y, b.geometry.b.y});
        merged.geometry = {{x, y0}, {x, y1}};
    }

    merged.thickness_px = std::max(a.thickness_px, b.thickness_px);
    merged.heavy_cable = a.heavy_cable || b.heavy_cable;
    if (confidence_rank(b.confidence) > confidence_rank(a.confidence))
        merged.confidence = b.confidence;

    const int x0 = static_cast<int>(std::floor(std::min(
        a.provenance.source_region.x,
        b.provenance.source_region.x)));
    const int y0 = static_cast<int>(std::floor(std::min(
        a.provenance.source_region.y,
        b.provenance.source_region.y)));
    const int x1 = static_cast<int>(std::ceil(std::max(
        a.provenance.source_region.x + a.provenance.source_region.width,
        b.provenance.source_region.x + b.provenance.source_region.width)));
    const int y1 = static_cast<int>(std::ceil(std::max(
        a.provenance.source_region.y + a.provenance.source_region.height,
        b.provenance.source_region.y + b.provenance.source_region.height)));

    merged.provenance.source_region = {
        x0, y0, x1 - x0, y1 - y0};
    merged.provenance.stage = "geometry.normalized";
    return merged;
}

std::string canonical_geometry(const ConductorSegment& segment) {
    std::ostringstream out;
    out.precision(12);
    out << segment.geometry.a.x << ',' << segment.geometry.a.y
        << '-' << segment.geometry.b.x << ',' << segment.geometry.b.y;
    return out.str();
}

} // namespace

ConductorNormalizer::ConductorNormalizer(
    GeometryNormalizationConfig config)
    : config_(config) {}

std::vector<ConductorSegment> ConductorNormalizer::normalize(
    const std::vector<ConductorSegment>& raw_segments) const {

    std::vector<ConductorSegment> working;
    working.reserve(raw_segments.size());

    // Stage 1: suppress duplicate geometry while retaining the strongest
    // evidence and preserving the source provenance region.
    for (const auto& candidate : raw_segments) {
        auto duplicate = std::find_if(
            working.begin(), working.end(),
            [&](const ConductorSegment& existing) {
                return same_geometry(
                    existing.geometry, candidate.geometry,
                    config_.duplicate_tolerance);
            });

        if (duplicate == working.end()) {
            working.push_back(candidate);
            continue;
        }

        if (confidence_rank(candidate.confidence) >
            confidence_rank(duplicate->confidence)) {
            *duplicate = candidate;
        }

        duplicate->thickness_px =
            std::max(duplicate->thickness_px, candidate.thickness_px);
        duplicate->heavy_cable =
            duplicate->heavy_cable || candidate.heavy_cable;
        duplicate->provenance.stage = "geometry.normalized";
    }

    // Stage 2: repeatedly merge overlapping or endpoint-touching collinear
    // fragments. A gap is never bridged merely because two segments are
    // close; this prevents separate parallel conductors from being invented
    // into one conductor.
    bool changed = true;
    while (changed) {
        changed = false;

        for (std::size_t i = 0; i < working.size() && !changed; ++i) {
            for (std::size_t j = i + 1; j < working.size(); ++j) {
                if (!mergeable(
                        working[i], working[j],
                        config_.collinear_tolerance)) {
                    continue;
                }

                working[i] = merge_pair(
                    working[i], working[j], config_.collinear_tolerance);
                working.erase(working.begin() +
                              static_cast<std::ptrdiff_t>(j));
                changed = true;
                break;
            }
        }
    }

    // Stage 3: assign deterministic IDs only after geometry has stabilized.
    for (auto& segment : working) {
        segment.id = stable_id(
            "normalized-conductor-segment",
            canonical_geometry(segment) + ":" +
            segment.provenance.source_id + ":" +
            std::to_string(segment.provenance.page));
        segment.provenance.stage = "geometry.normalized";
    }

    std::sort(
        working.begin(), working.end(),
        [](const ConductorSegment& a, const ConductorSegment& b) {
            return a.id < b.id;
        });

    return working;
}

} // namespace eke::dx::wire
