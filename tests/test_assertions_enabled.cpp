// AP-TEST-FIX-001: proves that this test target's own translation unit is
// compiled WITHOUT NDEBUG, i.e. that assert() below is a real runtime
// check rather than a no-op. CMake's default Release configuration
// defines NDEBUG project-wide; without the CMakeLists.txt correction
// this task adds, every assert()-based test in this suite would compile
// clean and "pass" regardless of what it actually checks.
//
// The #error below turns "NDEBUG leaked back into a test target" into a
// build failure, which is a stronger, earlier signal than a runtime
// check could give: if the test-target configuration ever regresses,
// this file simply stops compiling instead of silently passing.
#include <cassert>

#ifdef NDEBUG
#error "NDEBUG is defined for this test target - assert() would be compiled out. See the AP-TEST-FIX-001 test-target compile-definitions block in CMakeLists.txt."
#endif

int main() {
    int probe = 0;
    assert(++probe == 1);
    assert(probe == 1);

    // A deliberately false condition, wrapped so it is never evaluated,
    // documents what an actually-disabled assert() would have let
    // through - kept here only as a comment, never compiled, so this
    // test itself always stays green:
    //   assert(probe == 999); // would abort if assertions are real

    return 0;
}
