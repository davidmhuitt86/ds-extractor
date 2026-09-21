#include "eke_dx_wire/core/ids.hpp"

#include <cassert>

using namespace eke::dx::wire;

int main() {
    const auto a = stable_id("wire-segment", "source:0:1,2-3,4");
    const auto b = stable_id("wire-segment", "source:0:1,2-3,4");
    const auto c = stable_id("wire-segment", "source:0:1,2-3,5");

    assert(a == b);
    assert(a != c);
    return 0;
}
