#include "eke_dx_wire/topology/recognition_observation_parser.hpp"

#include <cassert>
#include <stdexcept>
#include <string>

using namespace eke::dx::wire;

int main() {
    const std::string document =
        "{\n"
        "  \"observations\": [\n"
        "    {\"text_region_id\": \"r1\", \"raw_text\": \"GND\", \"confidence\": \"high\"},\n"
        "    {\"text_region_id\": \"r2\", \"raw_text\": \"B+\", \"confidence\": \"medium\"},\n"
        "    {\"text_region_id\": \"r3\", \"raw_text\": \"IGN\", \"confidence\": \"unresolved\"},\n"
        "    {\"text_region_id\": \"\", \"raw_text\": \"ignored\", \"confidence\": \"high\"}\n"
        "  ]\n}\n";

    const auto evidence = parse_recognition_observations(document, "test-provider");

    assert(evidence.size() == 2);
    assert(evidence[0].text_region_id == "r1");
    assert(evidence[0].raw_text == "GND");
    assert(evidence[0].confidence == ConfidenceClass::High);
    assert(evidence[0].provider == "test-provider");
    assert(evidence[1].text_region_id == "r2");
    assert(evidence[1].confidence == ConfidenceClass::Medium);

    bool threw = false;
    try {
        const auto unused = parse_recognition_observations("{\"not_observations\": []}", "test-provider");
        (void)unused;
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw);

    return 0;
}
