#include "DxPtr.hpp"
#include "Common.hpp"

using namespace DxPtr;

TEST_CASE("Construction", "[basic][use]") {
    SECTION("Defaults") {
        SECTION("omni_view") {
            omni_view<int> ptr;
            omni_view<Ticker> ptr2(nullptr);

            REQUIRE(ptr.get() == nullptr);
            REQUIRE(ptr2.get() == nullptr);

            REQUIRE(std::is_const_v<std::remove_pointer_t<decltype(ptr.get())>>);
            REQUIRE(std::is_const_v<std::remove_pointer_t<decltype(ptr2.get())>>);
        }

        SECTION("omni_ref") {
            omni_ref<int> ptr;
            omni_ref<Ticker> ptr2(nullptr);

            REQUIRE(ptr.get() == nullptr);
            REQUIRE(ptr2.get() == nullptr);

            REQUIRE(not std::is_const_v<std::remove_pointer_t<decltype(ptr.get())>>);
            REQUIRE(not std::is_const_v<std::remove_pointer_t<decltype(ptr2.get())>>);
        }
    }

    SECTION("From owning") {
        omni_ptr<Ticker> owner;

        SECTION("nullptr") {
            omni_view v = owner;
            REQUIRE(v.get() == owner.get());
            REQUIRE(v.get() == nullptr);

            omni_ref r = owner;
            REQUIRE(r.get() == owner.get());
            REQUIRE(r.get() == nullptr);

            REQUIRE(v == r);
        }

        SECTION("With data") {
            TickerInfo info{};
            owner = make_omni<Ticker>(info, "A");

            omni_view v = owner;
            REQUIRE(v.get() == owner.get());

            omni_ref r = owner;
            REQUIRE(r.get() == owner.get());

            REQUIRE(v == r);

            REQUIRE(owner.use_count() == 3);
            REQUIRE(v.use_count() == 3);
            REQUIRE(r.use_count() == 3);
        }
    }
}

TEMPLATE_PRODUCT_TEST_CASE("Scope-bound lifetime of weaks", "[ownership][scope][basic]", (omni_view, omni_ref), (Ticker)) {
    using weak_t = TestType;
    TickerInfo info{};
    
    SECTION("Created via ctor") {
        Ticker* ticker = new Ticker(info, "B");

        REQUIRE(info == constructed<>);

        SECTION("Weak destroyed first") {
            omni_ptr owner(ticker);

            REQUIRE(info == constructed<>);

            {
                weak_t ptr = owner;

                REQUIRE(ptr.get() == ticker);
                REQUIRE(owner == ptr);
                REQUIRE(not ptr.expired());
                REQUIRE(owner.use_count() == 2);
                REQUIRE(ptr.use_count() == 2);
                REQUIRE(info == constructed<>);
            }

            REQUIRE(info == constructed<>);
        }

        SECTION("Owner destroyed first") {
            weak_t ptr;

            {
                omni_ptr owner(ticker);
                ptr = owner;
                
                REQUIRE(ptr == owner);
                REQUIRE(ptr.get() == ticker);
                REQUIRE(not ptr.expired());
                REQUIRE(owner.use_count() == 2);
                REQUIRE(ptr.use_count() == 2);
                REQUIRE(info == constructed<>);
            }

            REQUIRE(ptr.expired());
            REQUIRE(info == destroyed<>);
        }

        REQUIRE(info == destroyed<>);
    }
    
    SECTION("Created via make_omni") {
        SECTION("Weak destroyed first") {
            auto owner = make_omni<Ticker>(info, "C");

            REQUIRE(info == constructed<>);

            {
                weak_t ptr = owner;

                REQUIRE(owner == ptr);
                REQUIRE(not ptr.expired());
                REQUIRE(owner.use_count() == 2);
                REQUIRE(ptr.use_count() == 2);
                REQUIRE(info == constructed<>);
            }

            REQUIRE(info == constructed<>);
        }

        SECTION("Owner destroyed first") {
            weak_t ptr;

            {
                auto owner = make_omni<Ticker>(info, "C");
                ptr = owner;
                
                REQUIRE(ptr == owner);
                REQUIRE(not ptr.expired());
                REQUIRE(owner.use_count() == 2);
                REQUIRE(ptr.use_count() == 2);
                REQUIRE(info == constructed<>);
            }

            REQUIRE(ptr.expired());
            REQUIRE(info == destroyed<>);
        }

        REQUIRE(info == destroyed<>);
    }
}

TEMPLATE_PRODUCT_TEST_CASE("Copy semantics of weaks", "[ownership][copy]", (omni_view, omni_ref), (Ticker)) {
    using weak_t = TestType;
    TickerInfo info{};

    SECTION("Weaks destroyed first") {
        auto owner = make_omni<Ticker>(info, "F");

        {
            weak_t ptr1 = owner;
            weak_t ptr2;

            {
                weak_t ptr3 = ptr1;
                ptr2 = ptr1;

                REQUIRE(owner == ptr1);
                REQUIRE(owner == ptr2);
                REQUIRE(owner == ptr3);
                REQUIRE(ptr1 == ptr2);
                REQUIRE(ptr1 == ptr3);
                REQUIRE(ptr2 == ptr3);

                REQUIRE(owner.use_count() == 4);
                REQUIRE(ptr1.use_count() == 4);
                REQUIRE(ptr2.use_count() == 4);
                REQUIRE(ptr3.use_count() == 4);

                REQUIRE(info == constructed<>);
            }

            REQUIRE(owner == ptr1);
            REQUIRE(owner == ptr2);
            REQUIRE(ptr1 == ptr2);
            REQUIRE(ptr1 == ptr2);

            REQUIRE(owner.use_count() == 3);
            REQUIRE(ptr1.use_count() == 3);
            REQUIRE(ptr2.use_count() == 3);

            REQUIRE(info == constructed<>);
        }

        REQUIRE(owner.use_count() == 1);
    }

    SECTION("Owner destroyed first") {
        weak_t ptr;
        weak_t ptr2 = ptr;
        weak_t ptr3;

        REQUIRE(ptr.expired());
        REQUIRE(ptr2.expired());
        REQUIRE(ptr3.expired());
        REQUIRE(ptr == nullptr);
        REQUIRE(ptr2 == nullptr);
        REQUIRE(ptr3 == nullptr);
        REQUIRE(ptr == ptr2);
        REQUIRE(ptr == ptr3);
        REQUIRE(ptr2 == ptr3);

        {
            auto owner = make_omni<Ticker>(info, "F");

            weak_t ptr4 = owner;

            REQUIRE(ptr4 == owner);
            REQUIRE(ptr2 == nullptr);
            REQUIRE(ptr3.expired());
            REQUIRE(owner.use_count() == 2);

            REQUIRE(info == constructed<>);

            ptr = owner;
            ptr2 = ptr;
            ptr3 = ptr4;

            REQUIRE(ptr == ptr4);
            REQUIRE(ptr2 == ptr3);
            REQUIRE(ptr3 == ptr4);
            REQUIRE(ptr3 == owner);

            REQUIRE(owner.use_count() == 5);
            REQUIRE(ptr.use_count() == 5);
            REQUIRE(ptr2.use_count() == 5);
            REQUIRE(ptr3.use_count() == 5);
            REQUIRE(ptr4.use_count() == 5);

        }

        REQUIRE(ptr.expired());
        REQUIRE(ptr2.expired());
        REQUIRE(ptr3.expired());
        REQUIRE(ptr.use_count() == 3);
        REQUIRE(ptr2.use_count() == 3);
        REQUIRE(ptr3.use_count() == 3);
        REQUIRE(info == destroyed<>);
    }

    REQUIRE(info == destroyed<>);
}

TEMPLATE_PRODUCT_TEST_CASE("Move semantics of weaks", "[ownership][move]", (omni_view, omni_ref), (Ticker)) {
    using weak_t = TestType;
    TickerInfo info{};

    SECTION("Moving owner no effect") {
        auto movedFrom = make_omni<Ticker>(info, "D");
        weak_t ptr = movedFrom;

        auto movedTo = std::move(movedFrom);

        REQUIRE(ptr == movedTo);
        REQUIRE(movedFrom == nullptr);
        REQUIRE(not ptr.expired());
        REQUIRE(ptr.use_count() == 2);
        REQUIRE(info == constructed<>);
    }

    SECTION("Creating weak from temporary") {
        weak_t ptr = omni_ptr<Ticker>(new Ticker(info, "E"));

        REQUIRE(ptr.expired());
    }

    SECTION("Moving weaks") {
        auto owner = make_omni<Ticker>(info, "D");
        weak_t movedFrom = owner;
        
        SECTION("Move construct") {
            weak_t movedTo = std::move(movedFrom);
            
            REQUIRE(movedFrom.expired());
            REQUIRE(owner == movedTo);
            REQUIRE(owner.use_count() == 2);
            REQUIRE(info == constructed<>);
        }
        
        SECTION("Move assign") {
            weak_t movedTo;
            movedTo = std::move(movedFrom);
            
            REQUIRE(movedFrom.expired());
            REQUIRE(owner == movedTo);
            REQUIRE(owner.use_count() == 2);
            REQUIRE(info == constructed<>);
        }

        REQUIRE(owner.use_count() == 1);
        REQUIRE(movedFrom == nullptr);
    }

    REQUIRE(info == destroyed<>);
}