#include "eke_dx_wire/image/circle_context_classifier.hpp"

#include <cassert>
#include <vector>

using namespace eke::dx::wire;

namespace {

ComponentCandidate circle(double x, double y) {
    ComponentCandidate c;
    c.kind = ComponentCandidateKind::CircularSymbol;
    c.bounds = {static_cast<int>(x), static_cast<int>(y), 10, 10};
    c.circle_probe_max_run_fraction = 0.10;
    return c;
}

ConductorSegment segment(double x1, double y1, double x2, double y2) {
    ConductorSegment s;
    s.geometry = {{x1, y1}, {x2, y2}};
    return s;
}

} // namespace

int main() {
    CircleContextClassifier classifier;

    {
        auto c = circle(100, 100);
        c.circle_probe_max_run_fraction = 1.0;
        const auto out = classifier.classify({c}, {
            segment(105, 40, 105, 160)});
        assert(out.empty());
    }

    {
        const auto c = circle(100, 100);
        const auto out = classifier.classify({c}, {
            segment(105, 40, 105, 100)});
        assert(out.size() == 1);
    }

    {
        const auto c = circle(100, 100);
        const auto out = classifier.classify({c}, {
            segment(97, 40, 97, 97)});
        assert(out.size() == 1);
    }

    {
        const auto c = circle(100, 100);
        const auto out = classifier.classify({c}, {
            segment(40, 80, 60, 80)});
        assert(out.empty());
    }

    {
        ComponentCandidate rectangle;
        rectangle.kind = ComponentCandidateKind::Enclosure;
        rectangle.bounds = {100, 100, 20, 20};
        const auto out = classifier.classify({rectangle}, {});
        assert(out.size() == 1);
        assert(out.front().kind == ComponentCandidateKind::Enclosure);
    }

    return 0;
}
