#include <type_traits>

#include "DxPtr.hpp"
#include "Common.hpp"

using namespace DxPtr;

struct BasicData {
    int x, y;
};

TEST_CASE("Scope-bound lifetime of omni_ptr", "[ownership][scope][basic]") {
    TickerInfo info{};

    SECTION("Created via ctor") {
        {
            Ticker* foo = new Ticker(info, "A");

            omni_ptr ptr(foo);

            REQUIRE(foo == ptr.get());
            
            REQUIRE(info == TickerInfo{
                  .constructed = 1
                , .destroyed = 0
            });
        }
            
        REQUIRE(info == TickerInfo{
              .constructed = 1
            , .destroyed = 1
        });
    }
    
    SECTION("Created via make_omni") {
        {
            auto ptr = make_omni<Ticker>(info, "B");

            REQUIRE(info == TickerInfo{
                  .constructed = 1
                , .destroyed = 0
            });
        }
            
        REQUIRE(info == TickerInfo{
              .constructed = 1
            , .destroyed = 1
        });
    }

    // https://bugs.llvm.org/show_bug.cgi?id=24137
    SECTION("Const class and make_omni") {
        auto omni_ptr = make_omni<const Ticker>(info, "0");

        REQUIRE(info == constructed<>);
    }
}

TEST_CASE("Move semantics of omni_ptr", "[ownership][move]") {
    TickerInfo info{};

    {
        auto movedFrom = make_omni<Ticker>(info, "C");

        REQUIRE(info == TickerInfo{
            .constructed = 1
        });

        SECTION("Move-constructed-from state") {
            auto movedTo = std::move(movedFrom);

            REQUIRE(movedFrom.get() == nullptr);
            REQUIRE(info == TickerInfo{
                .constructed = 1
            });
        }

        SECTION("Move-assigned-from state") {
            omni_ptr<Ticker> movedTo;

            REQUIRE(movedTo.get() == nullptr);

            movedTo = std::move(movedFrom);

            REQUIRE(movedFrom.get() == nullptr);
            REQUIRE(info == TickerInfo{
                .constructed = 1
            });
        }

        REQUIRE(movedFrom.get() == nullptr);
        REQUIRE(info == TickerInfo{
              .constructed = 1
            , .destroyed = 1
        });
    }
    
    REQUIRE(info == TickerInfo{
          .constructed = 1
        , .destroyed = 1
    });
}

TEST_CASE("Uncopyability", "[use][copy]") {
    REQUIRE(not std::is_copy_constructible_v<omni_ptr<Ticker>>);
    REQUIRE(not std::is_copy_assignable_v<omni_ptr<Ticker>>);
}

TEMPLATE_PRODUCT_TEST_CASE("Member Functions", "[use][basic]", (omni_ref, omni_view), (Ticker)) {
    using weak_t = TestType;

    TickerInfo info{};
    
    omni_ptr<Ticker> owner;
    weak_t ptr;

    SECTION("get") {
        REQUIRE(owner.get() == nullptr);
        REQUIRE(ptr.get() == nullptr);

        Ticker* ticker = new Ticker(info, "D");
        
        owner = omni_ptr(ticker);
        ptr = owner;

        REQUIRE(owner.get() == ticker);
        REQUIRE(ptr.get() == ticker);
    }

    SECTION("reset") {
        Ticker* t1 = new Ticker(info, "E");
        
        owner.reset(t1);
        ptr = owner;

        REQUIRE(owner.get() == t1);
        REQUIRE(ptr.get() == t1);
        REQUIRE(info == constructed<>);

        TickerInfo info2{};
        Ticker* t2 = new Ticker(info2, "F");

        owner.reset(t2);

        REQUIRE(owner.get() == t2);
        REQUIRE(ptr.get() == nullptr);
        REQUIRE(ptr.expired());
        REQUIRE(info == destroyed<>);
        REQUIRE(info2 == constructed<>);

        ptr = owner;
        REQUIRE(ptr.get() == t2);

        REQUIRE(info == destroyed<>);
        REQUIRE(info2 == constructed<>);

        owner.reset();

        REQUIRE(owner.get() == nullptr);
        REQUIRE(ptr.get() == nullptr);
        REQUIRE(ptr.expired());
        REQUIRE(info == destroyed<>);
        REQUIRE(info2 == destroyed<>);
    }

    SECTION("release") {
        Ticker* ticker = new Ticker(info, "G");

        owner = omni_ptr(ticker);
        ptr = owner;

        Ticker* ret = owner.release();

        REQUIRE(ticker == ret);
        REQUIRE(owner.get() == nullptr);
        REQUIRE(ptr.expired());
        REQUIRE(info == constructed<>);

        delete ret;
        
        REQUIRE(info == destroyed<>);
    }

    SECTION("use_count") {
        REQUIRE(owner.use_count() == 0);

        owner = omni_ptr(new Ticker(info, "H"));
        REQUIRE(owner.use_count() == 1);

        ptr = owner;
        REQUIRE(owner.use_count() == 2);
        REQUIRE(ptr.use_count() == 2);

        owner.reset();

        REQUIRE(owner.use_count() == 0);
        REQUIRE(ptr.use_count() == 1);
        REQUIRE(owner == nullptr);
        REQUIRE(ptr == nullptr);
        REQUIRE(ptr.expired());
        REQUIRE(info == destroyed<>);

        ptr.reset();

        REQUIRE(owner.use_count() == 0);
        REQUIRE(ptr.use_count() == 0);

        REQUIRE(info == destroyed<>);
    }

    SECTION("expired") {
        weak_t ptr;

        {
            auto owner = make_omni<Ticker>(info, "I");
            ptr = owner;

            REQUIRE(not ptr.expired());
        }

        REQUIRE(ptr.expired());

        {
            auto owner = make_omni<Ticker>(info, "I");
            ptr = owner;

            REQUIRE(not ptr.expired());

            owner.reset();

            REQUIRE(ptr.expired());
        }

        REQUIRE(ptr.expired());
    }
}

TEST_CASE("Operator overloads", "[use][basic]") {
    // auto ptr = make_omni<BasicData>(3, 4);
    auto ptr = make_omni<BasicData>(BasicData{3, 4});

    SECTION("Dereference and indirection") {
        BasicData& data = *ptr;

        REQUIRE(&data == ptr.get());
        REQUIRE(ptr->x == 3);
        REQUIRE((*ptr).y == 4);
    }

    SECTION("Equality") {
        BasicData* data = new BasicData{3, 4};
        
        omni_ptr ptr2(data);
        omni_ptr ptr3(data);

        REQUIRE(ptr2 == ptr3);
        REQUIRE(ptr3 == ptr2);
        REQUIRE(ptr  != ptr2);
        REQUIRE(ptr2 != ptr);
        REQUIRE(ptr  != ptr3);
        REQUIRE(ptr3 != ptr);

        ptr2.release();
        REQUIRE(ptr2 != ptr3);
        ptr3.release();
        REQUIRE(ptr2 == ptr3);

        REQUIRE(omni_ptr<BasicData>() == omni_ptr<BasicData>());
        REQUIRE(omni_ptr<BasicData>() == nullptr);
        REQUIRE(nullptr == omni_ptr<BasicData>());
        REQUIRE(ptr != nullptr);

        delete data;
    }

    SECTION("Relational") {
        REQUIRE(ptr > nullptr);
        REQUIRE(nullptr < ptr);
        REQUIRE(ptr <= ptr);
        REQUIRE(ptr >= ptr);
        
        std::vector<BasicData> vec(2, BasicData{3, 4});

        omni_ptr ptr1(&vec[0]);
        omni_ptr ptr2(&vec[1]);

        REQUIRE(ptr1 < ptr2);
        REQUIRE(ptr2 > ptr1);

        ptr1.release();
        ptr2.release();
    }

    SECTION("Boolean conversion") {
        REQUIRE(ptr);
        REQUIRE(not omni_ptr<BasicData>());
    }
}