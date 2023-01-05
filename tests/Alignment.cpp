#include "DxPtr.hpp"
#include "Common.hpp"

using namespace DxPtr;

template<std::size_t A>
struct alignas(A) Aligner {
    static constexpr std::size_t alignment = A;
    char c;

    Aligner(char c) : c(c) { }
};

bool IsAligned(void* ptr, std::size_t alignment) {
    return (reinterpret_cast<uintptr_t>(ptr) % alignment) == 0;
}

TEMPLATE_TEST_CASE_SIG("Basic alignment checks", "[alignment]", ((std::size_t A), A), 1, 2, 4, 8, 16, 32, 64) {
    SECTION("Default alignment") {
        auto omni = make_omni<Aligner<A>>('A');

        REQUIRE(IsAligned(omni.get(), A));
    }

    SECTION("Overaligned") {
        auto omni = make_omni<Aligner<1>, AlignmentPolicy::Overaligned<A>>('B');

        REQUIRE(IsAligned(omni.get(), A));
    }
    
}