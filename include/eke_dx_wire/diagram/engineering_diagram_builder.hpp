#pragma once

#include "eke_dx_wire/diagram/engineering_diagram.hpp"

namespace eke::dx::wire {

// AP-WIRE-026: assembles the read-only EngineeringDiagram projection from
// an already-complete WireModel. The builder is deterministic and
// read-only with respect to its input: it never mutates the WireModel it
// is given, and never performs OCR, fuzzy matching, component
// recognition, topology inference, wire reconstruction, terminal
// invention, or canonical-identity guessing - those are the
// responsibility of AP-WIRE-008/017/018/019/021/022/023/024, whose
// already-resolved output this builder only reads and cross-references.
class EngineeringDiagramBuilder {
public:
    [[nodiscard]] EngineeringDiagram build(const WireModel& model) const;
};

} // namespace eke::dx::wire
