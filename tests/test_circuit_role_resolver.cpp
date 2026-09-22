#include "eke_dx_wire/topology/circuit_role_resolver.hpp"

#include <cassert>

using namespace eke::dx::wire;

static EndpointCandidate endpoint(
    const char* id,
    EndpointKind kind = EndpointKind::ComponentTerminal,
    TerminalRole role = TerminalRole::Unknown,
    ConfidenceClass confidence = ConfidenceClass::High) {
    EndpointCandidate e;
    e.id = id;
    e.kind = kind;
    e.terminal_role = role;
    e.confidence = confidence;
    return e;
}

static ElectricalNet net(const char* id, std::initializer_list<const char*> ids) {
    ElectricalNet n;
    n.id = id;
    for (const auto* id_value : ids) {
        n.endpoint_ids.emplace_back(id_value);
    }
    return n;
}

int main() {
    {
        auto n = net("ground-net", {"G", "A", "B"});
        auto g = endpoint(
            "G",
            EndpointKind::Ground,
            TerminalRole::GroundTerminal);

        const auto result = CircuitRoleResolver().resolve(
            {n},
            {g, endpoint("A"), endpoint("B")},
            {});

        assert(result.nets.size() == 1);
        assert(result.nets.front().role == DistributionRole::Ground);
        assert(result.nets.front().anchor_endpoint == "G");
        assert(result.unresolved_net_ids.empty());
    }

    {
        auto n = net("power-net", {"P", "A", "B"});
        auto p = endpoint(
            "P",
            EndpointKind::ExternalConnection,
            TerminalRole::PowerSource,
            ConfidenceClass::High);

        const auto result = CircuitRoleResolver().resolve(
            {n},
            {p, endpoint("A"), endpoint("B")},
            {});

        assert(result.nets.front().role == DistributionRole::PowerFeed);
        assert(result.nets.front().anchor_endpoint == "P");
    }

    {
        auto n = net("function-net", {"S", "A", "B"});
        const auto result = CircuitRoleResolver().resolve(
            {n},
            {endpoint("S"), endpoint("A"), endpoint("B")},
            {{"S", DistributionRole::SharedFunctionFeed,
              ConfidenceClass::High, "reference-graph"}});

        assert(result.nets.front().role ==
               DistributionRole::SharedFunctionFeed);
        assert(result.nets.front().anchor_endpoint == "S");
    }

    {
        auto n = net("conflict-net", {"A", "B"});
        const auto result = CircuitRoleResolver().resolve(
            {n},
            {endpoint("A"), endpoint("B")},
            {{"A", DistributionRole::PowerFeed,
              ConfidenceClass::High, "source-a"},
             {"B", DistributionRole::Ground,
              ConfidenceClass::High, "source-b"}});

        assert(result.nets.front().role == DistributionRole::Unknown);
        assert(result.nets.front().confidence ==
               ConfidenceClass::Unresolved);
        assert(result.unresolved_net_ids.size() == 1);
    }

    {
        // A weak assertion must not override stronger semantic evidence.
        auto n = net("priority-net", {"P", "A"});
        const auto result = CircuitRoleResolver().resolve(
            {n},
            {endpoint(
                "P",
                EndpointKind::ExternalConnection,
                TerminalRole::PowerSource,
                ConfidenceClass::High),
             endpoint("A")},
            {{"P", DistributionRole::Ground,
              ConfidenceClass::Low, "weak-source"}});

        assert(result.nets.front().role == DistributionRole::PowerFeed);
    }

    return 0;
}
