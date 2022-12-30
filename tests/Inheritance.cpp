#include "DxPtr.hpp"
#include "Common.hpp"

using namespace DxPtr;

TEMPLATE_TEST_CASE("Inheritance basic use", "[inheritance][basic]", omni_ptr<Derived>, omni_ptr<Base>) {
    TickerInfo infoD, infoB;

    TestType derived = make_omni<Derived>(infoD, infoB);

    REQUIRE(infoD == constructed<>);
    REQUIRE(infoB == constructed<>);

    omni_view<Base> base = derived;


    REQUIRE(derived == base);
    REQUIRE(infoD == constructed<>);
    REQUIRE(infoB == constructed<>);

    std::string resD, resB;

    derived->Write(resD);
    base->Write(resB);

    REQUIRE(resD == "BaseDerived");
    REQUIRE(resB == "BaseDerived");

    derived.reset();

    REQUIRE(base.expired());
    REQUIRE(infoD == destroyed<>);
    REQUIRE(infoB == destroyed<>);
}


TEST_CASE("Inheritance from weaks", "[inheritance][basic]") {
    TickerInfo infoD, infoB;

    auto derived = make_omni<Derived>(infoD, infoB);

    REQUIRE(infoD == constructed<>);
    REQUIRE(infoB == constructed<>);

    omni_ref<Derived> weakD = derived;
    omni_view<Base> weakB = weakD;

    REQUIRE(weakD == weakB);
    REQUIRE(derived == weakD);
    REQUIRE(derived == weakB);
    REQUIRE(derived.use_count() == 3);
    REQUIRE(weakD.use_count() == 3);
    REQUIRE(weakB.use_count() == 3);
    REQUIRE(infoD == constructed<>);
    REQUIRE(infoB == constructed<>);

    std::string resD, resWD, resWB;

    derived->Write(resD);
    weakD->Write(resWD);
    weakB->Write(resWB);

    REQUIRE(resD == "BaseDerived");
    REQUIRE(resWD == "BaseDerived");
    REQUIRE(resWB == "BaseDerived");

    derived.reset();

    REQUIRE(weakD.expired());
    REQUIRE(weakB.expired());
    REQUIRE(derived.use_count() == 0);
    REQUIRE(weakD.use_count() == 2);
    REQUIRE(weakB.use_count() == 2);
    REQUIRE(infoD == destroyed<>);
    REQUIRE(infoB == destroyed<>);

    weakB.reset();
    REQUIRE(weakD.use_count() == 1);
    REQUIRE(weakB.use_count() == 0);
    REQUIRE(infoD == destroyed<>);
    REQUIRE(infoB == destroyed<>);

    weakD.reset();
    REQUIRE(weakD.use_count() == 0);
    REQUIRE(weakB.use_count() == 0);
    REQUIRE(infoD == destroyed<>);
    REQUIRE(infoB == destroyed<>);
}