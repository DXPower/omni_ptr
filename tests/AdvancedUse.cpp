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

template<typename T, bool Deallocate = false>
struct CustomDeleter {
    const T** deleted;
    int* count;

    void operator()(T* ptr) noexcept {
        *deleted = ptr;
        (*count)++;

        if constexpr (Deallocate)
            delete ptr;
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

TEMPLATE_TEST_CASE("Custom deleter", "[advanced][lifetime][deleter]", int, std::string) {
    int count = 0;

    SECTION("Generic types") {
        TestType* ptr = new TestType();
        const TestType* deleted = nullptr;

        omni_ptr<TestType> owner(ptr, CustomDeleter<TestType, true>{ &deleted, &count });

        REQUIRE(owner.get() == ptr);
        REQUIRE(owner.use_count() == 1);

        owner.reset();

        REQUIRE(ptr == deleted);
        REQUIRE(count == 1);
    }

    SECTION("Ticker") {
        TickerInfo info{};
        Ticker* ptr = new Ticker(info, "A");
        const Ticker* deleted = nullptr;

        omni_ptr<Ticker> owner(ptr, CustomDeleter<Ticker, true>{ &deleted, &count });

        REQUIRE(owner.get() == ptr);
        REQUIRE(owner.use_count() == 1);
        REQUIRE(info == constructed<>);

        owner.reset();

        REQUIRE(ptr == deleted);
        REQUIRE(count == 1);
        REQUIRE(info == destroyed<>);
    }
}