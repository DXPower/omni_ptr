#include "DxPtr.hpp"
#include "Common.hpp"

using namespace DxPtr;

struct Aliaser {
    TickerInfo* info;
    Ticker ticker;
    std::string name;
    int arr[10];

    Aliaser(TickerInfo& aliaserInfo, TickerInfo& tickerInfo)
    : info(&aliaserInfo), ticker(tickerInfo, "Aliaser") {
       aliaserInfo.constructed++; 
    }

    ~Aliaser() {
        info->destroyed++;
    }
};

TEST_CASE("Aliasing constructor", "[advanced][lifetime]") {
    TickerInfo aliaserInfo, tickerInfo;

    auto* aliaser = new Aliaser(aliaserInfo, tickerInfo);
    omni_ptr<Aliaser> owner(aliaser);

    SECTION("Move alias") {
        omni_ptr<std::string> nameAlias(std::move(owner), &owner->name);

        REQUIRE(nameAlias.get() == &aliaser->name);
        REQUIRE(not owner);
        REQUIRE(nameAlias);
        REQUIRE(owner.use_count() == 0);
        REQUIRE(nameAlias.use_count() == 1);
        REQUIRE(aliaserInfo == constructed<>);
        REQUIRE(tickerInfo == constructed<>);
    }

    SECTION("Copy alias from owning") {
        omni_view<std::string> nameAlias(owner, &owner->name);

        REQUIRE(nameAlias.get() == &aliaser->name);
        REQUIRE(owner);
        REQUIRE(nameAlias);
        REQUIRE(owner.use_count() == 2);
        REQUIRE(nameAlias.use_count() == 2);
        REQUIRE(aliaserInfo == constructed<>);
        REQUIRE(tickerInfo == constructed<>);

        owner.reset();

        REQUIRE(not nameAlias);
        REQUIRE(nameAlias.expired());
        REQUIRE(owner.use_count() == 0);
        REQUIRE(nameAlias.use_count() == 1);
        REQUIRE(aliaserInfo == destroyed<>);
        REQUIRE(tickerInfo == destroyed<>);
    }

    SECTION("Copy alias from non-owning") {
        omni_view<Aliaser> view = owner;
        omni_view<std::string> nameAlias(view, &view->name);

        REQUIRE(nameAlias.get() == &aliaser->name);
        REQUIRE(owner);
        REQUIRE(nameAlias);
        REQUIRE(owner.use_count() == 3);
        REQUIRE(nameAlias.use_count() == 3);
        REQUIRE(aliaserInfo == constructed<>);
        REQUIRE(tickerInfo == constructed<>);

        owner.reset();

        REQUIRE(not nameAlias);
        REQUIRE(nameAlias.expired());
        REQUIRE(owner.use_count() == 0);
        REQUIRE(nameAlias.use_count() == 2);
        REQUIRE(aliaserInfo == destroyed<>);
        REQUIRE(tickerInfo == destroyed<>);
    }

    SECTION("Unbounded array alias") {
        omni_view<int[]> alias(owner, owner->arr);

        for (std::size_t i = 0; i < 10; i++) {
            REQUIRE(&alias[i] == &owner->arr[i]);
        }

        owner.reset();
    }

    SECTION("Bounded array alias") {
        omni_view<int[10]> alias(owner, owner->arr);

        for (std::size_t i = 0; i < 10; i++) {
            REQUIRE(&alias[i] == &owner->arr[i]);
        }

        owner.reset();
    }

    REQUIRE(aliaserInfo == destroyed<>);
    REQUIRE(tickerInfo == destroyed<>);
}