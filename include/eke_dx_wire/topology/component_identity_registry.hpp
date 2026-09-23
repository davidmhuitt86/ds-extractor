#pragma once

#include "eke_dx_wire/core/model.hpp"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace eke::dx::wire {

struct ComponentIdentityRegistryEntry {
    std::string canonical_id;
    std::string canonical_name;
    std::vector<std::string> aliases;
};

class ComponentIdentityRegistry {
public:
    virtual ~ComponentIdentityRegistry() = default;

    [[nodiscard]] virtual std::optional<ComponentIdentityRegistryEntry> lookup(
        const std::string& normalized_identity) const = 0;
};

class NullComponentIdentityRegistry final : public ComponentIdentityRegistry {
public:
    [[nodiscard]] std::optional<ComponentIdentityRegistryEntry> lookup(
        const std::string& normalized_identity) const override;
};

class StaticComponentIdentityRegistry final : public ComponentIdentityRegistry {
public:
    explicit StaticComponentIdentityRegistry(
        std::vector<ComponentIdentityRegistryEntry> entries);

    [[nodiscard]] std::optional<ComponentIdentityRegistryEntry> lookup(
        const std::string& normalized_identity) const override;

private:
    std::unordered_map<std::string, ComponentIdentityRegistryEntry> entries_;
};

class ComponentIdentityCanonicalizer {
public:
    [[nodiscard]] std::vector<ComponentIdentityCanonicalization> canonicalize(
        const std::vector<ComponentIdentityResolution>& resolutions,
        const ComponentIdentityRegistry& registry) const;
};

} // namespace eke::dx::wire
