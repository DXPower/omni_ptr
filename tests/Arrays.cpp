#include "DxPtr.hpp"
#include "Common.hpp"

using namespace DxPtr;

TEST_CASE("Array construction and destruction", "[basic][arrays]") {
    ArrayTicker::Reset();

    SECTION("Created via constructor") {
        ArrayTicker* ptr = new ArrayTicker[10];

        REQUIRE(ArrayTicker::info == constructed<10>);

        omni_ptr<ArrayTicker[]> owner(ptr);

        REQUIRE(ArrayTicker::info == constructed<10>);
        REQUIRE(owner.get() == ptr);
    }


    SECTION("Created via make_omni") {
        auto owner = make_omni<ArrayTicker[]>(10);

        REQUIRE(ArrayTicker::info == constructed<10>);
    }

    REQUIRE(ArrayTicker::info == destroyed<10>);
}

TEST_CASE("Array indexing", "[basic][arrays") {
    ArrayTicker::Reset();

    ArrayTicker* ptr = new ArrayTicker[10];
    omni_ptr<ArrayTicker[]> owner(ptr);

    for (std::size_t i = 0; i < 10; i++) {
        REQUIRE(&ptr[i] == &owner[i]);
    }

    owner.reset();

    REQUIRE(ArrayTicker::info == destroyed<10>);

}