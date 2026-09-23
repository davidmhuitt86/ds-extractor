#include "eke_dx_wire/core/base64.hpp"

#include <cassert>
#include <string>
#include <vector>

using namespace eke::dx::wire;

namespace {

std::vector<uint8_t> bytes(const std::string& s) {
    return std::vector<uint8_t>(s.begin(), s.end());
}

} // namespace

int main() {
    // RFC 4648 test vectors.
    assert(base64_encode({}) == "");
    assert(base64_encode(bytes("f")) == "Zg==");
    assert(base64_encode(bytes("fo")) == "Zm8=");
    assert(base64_encode(bytes("foo")) == "Zm9v");
    assert(base64_encode(bytes("foob")) == "Zm9vYg==");
    assert(base64_encode(bytes("fooba")) == "Zm9vYmE=");
    assert(base64_encode(bytes("foobar")) == "Zm9vYmFy");

    return 0;
}
