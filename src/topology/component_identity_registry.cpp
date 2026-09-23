#include "eke_dx_wire/topology/component_identity_registry.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <utility>

namespace eke::dx::wire {
namespace {

std::string normalize_identity(const std::string& value) {
    std::string normalized;
    normalized.reserve(value.size());

    for (const unsigned char ch : value) {
        if (std::isspace(ch) || ch == '-' || ch == '_') {
            continue;
        }
        normalized.push_back(
            static_cast<char>(std::toupper(ch)));
    }

    return normalized;
}

} // namespace

std::optional<ComponentIdentityRegistryEntry>
NullComponentIdentityRegistry::lookup(
    const std::string&) const {
    return std::nullopt;
}

StaticComponentIdentityRegistry::StaticComponentIdentityRegistry(
    std::vector<ComponentIdentityRegistryEntry> entries) {

    std::map<std::string, std::set<std::string>> alias_owners;
    std::map<std::string, ComponentIdentityRegistryEntry> canonical_entries;

    for (auto& entry : entries) {
        if (entry.canonical_id.empty() ||
            entry.canonical_name.empty()) {
            continue;
        }

        std::sort(entry.aliases.begin(), entry.aliases.end());
        entry.aliases.erase(
            std::unique(entry.aliases.begin(), entry.aliases.end()),
            entry.aliases.end());

        const std::string canonical_key =
            normalize_identity(entry.canonical_name);

        if (!canonical_key.empty()) {
            entry.aliases.push_back(entry.canonical_name);
        }

        std::sort(entry.aliases.begin(), entry.aliases.end());
        entry.aliases.erase(
            std::unique(entry.aliases.begin(), entry.aliases.end()),
            entry.aliases.end());

        canonical_entries[entry.canonical_id] = entry;

        for (const auto& alias : entry.aliases) {
            const std::string key = normalize_identity(alias);
            if (!key.empty()) {
                alias_owners[key].insert(entry.canonical_id);
            }
        }
    }

    for (const auto& [alias, owners] : alias_owners) {
        if (owners.size() != 1) {
            continue;
        }

        const auto owner = canonical_entries.find(*owners.begin());
        if (owner != canonical_entries.end()) {
            entries_[alias] = owner->second;
        }
    }
}

std::optional<ComponentIdentityRegistryEntry>
StaticComponentIdentityRegistry::lookup(
    const std::string& normalized_identity) const {
    const auto it = entries_.find(normalize_identity(normalized_identity));
    if (it == entries_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::vector<ComponentIdentityCanonicalization>
ComponentIdentityCanonicalizer::canonicalize(
    const std::vector<ComponentIdentityResolution>& resolutions,
    const ComponentIdentityRegistry& registry) const {

    std::vector<ComponentIdentityCanonicalization> result;
    result.reserve(resolutions.size());

    for (const auto& resolution : resolutions) {
        ComponentIdentityCanonicalization artifact;
        artifact.id =
            "component-identity-canonicalization-" + resolution.id;
        artifact.component_id = resolution.component_id;
        artifact.source_resolution_id = resolution.id;
        artifact.source_identity = resolution.identity;

        if (resolution.status == ComponentIdentityResolutionStatus::Conflicted) {
            artifact.status = ComponentIdentityCanonicalizationStatus::Conflicted;
            result.push_back(std::move(artifact));
            continue;
        }

        if (resolution.status != ComponentIdentityResolutionStatus::Resolved ||
            resolution.identity.empty()) {
            artifact.status = ComponentIdentityCanonicalizationStatus::NotFound;
            result.push_back(std::move(artifact));
            continue;
        }

        const auto entry = registry.lookup(resolution.identity);
        if (!entry.has_value()) {
            artifact.status = ComponentIdentityCanonicalizationStatus::NotFound;
            result.push_back(std::move(artifact));
            continue;
        }

        artifact.canonical_id = entry->canonical_id;
        artifact.canonical_name = entry->canonical_name;
        artifact.confidence = resolution.confidence;
        artifact.status = ComponentIdentityCanonicalizationStatus::Resolved;
        result.push_back(std::move(artifact));
    }

    std::sort(
        result.begin(),
        result.end(),
        [](const auto& a, const auto& b) {
            return a.id < b.id;
        });

    return result;
}

} // namespace eke::dx::wire
