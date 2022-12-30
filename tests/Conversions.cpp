#include "DxPtr.hpp"
#include "Common.hpp"

using namespace DxPtr;

TEST_CASE("Conversions", "[inheritance][advanced][conversion]") {
    TickerInfo infoB, infoD;

    omni_ptr<Base> owner = make_omni<Derived>(infoD, infoB);
    omni_view<Base> source = owner;

    auto copyCastCheck = [&](const auto& derived) {
        REQUIRE(owner == derived);
        REQUIRE(source == derived);
        REQUIRE(owner.use_count() == 3);
        REQUIRE(source.use_count() == 3);
        REQUIRE(derived.use_count() == 3);
        REQUIRE(infoB == constructed<>);
        REQUIRE(infoD == constructed<>);
    };

    auto moveCastCheck = [&](const auto& derived, const auto& movedTo) {
        REQUIRE(owner == nullptr);
        REQUIRE(owner != source);
        REQUIRE(movedTo == source);
        REQUIRE(movedTo == derived);
        REQUIRE(source == derived);
        REQUIRE(not source.expired());
        REQUIRE(not derived.expired());
        REQUIRE(owner.use_count() == 0);
        REQUIRE(source.use_count() == 3);
        REQUIRE(derived.use_count() == 3);
        REQUIRE(infoB == constructed<>);
        REQUIRE(infoD == constructed<>);
    };

    auto destroyedCheck = [&](const auto& derived) {
        REQUIRE(owner == nullptr);
        REQUIRE(source == nullptr);
        REQUIRE(derived == nullptr);
        REQUIRE(source.expired());
        REQUIRE(derived.expired());
        REQUIRE(source.expired());
        REQUIRE(derived.expired());
        REQUIRE(source.use_count() == 2);
        REQUIRE(derived.use_count() == 2);
        REQUIRE(infoB == destroyed<>);
        REQUIRE(infoD == destroyed<>);
    };

    REQUIRE(owner == source);
    REQUIRE(infoB == constructed<>);
    REQUIRE(infoD == constructed<>);

    SECTION("Static cast") {
        omni_view<Derived> derived = static_pointer_cast<Derived>(source);

        copyCastCheck(derived);

        {
            auto movedTo = static_pointer_cast<Derived>(std::move(owner));

            moveCastCheck(derived, movedTo);
        }

        destroyedCheck(derived);
    }

    SECTION("Dynamic cast") {
        omni_view<Derived> derived = dynamic_pointer_cast<Derived>(source);

        copyCastCheck(derived);

        {
            auto movedTo = dynamic_pointer_cast<Derived>(std::move(owner));

            moveCastCheck(derived, movedTo);
        }

        destroyedCheck(derived);
    }

    SECTION("Const cast") {
        omni_view<Base> derived = const_pointer_cast<const Base>(source);

        copyCastCheck(derived);

        {
            auto movedTo = const_pointer_cast<const Base>(std::move(owner));

            moveCastCheck(derived, movedTo);
        }

        destroyedCheck(derived);
    }

    SECTION("Reinterpret cast") {
        omni_view<Derived> derived = reinterpret_pointer_cast<Derived>(source);

        copyCastCheck(derived);

        {
            auto movedTo = reinterpret_pointer_cast<Derived>(std::move(owner));

            moveCastCheck(derived, movedTo);
        }

        destroyedCheck(derived);
    }
    
    source.reset();

    REQUIRE(owner == source);
    REQUIRE(owner == nullptr);
    REQUIRE(source == nullptr);
    REQUIRE(source.expired());
    REQUIRE(source.use_count() == 0);
    REQUIRE(infoB == destroyed<>);
    REQUIRE(infoD == destroyed<>);
}

TEMPLATE_PRODUCT_TEST_CASE("Null conversions", "[inheritance][advanced][conversions]", (omni_ptr, omni_ref, omni_view), (Base, Derived)) {
    TestType ptr{};

    SECTION("Static cast") {
        if constexpr (not decltype(ptr)::is_owning)
            REQUIRE(static_pointer_cast<Base>(ptr) == nullptr);

        REQUIRE(static_pointer_cast<Base>(std::move(ptr)) == nullptr);
    }

    SECTION("Dynamic cast") {
        if constexpr (not decltype(ptr)::is_owning)
            REQUIRE(dynamic_pointer_cast<Base>(ptr) == nullptr);

        REQUIRE(dynamic_pointer_cast<Base>(std::move(ptr)) == nullptr);
    }

    SECTION("Const cast") {
        if constexpr (not decltype(ptr)::is_owning)
            REQUIRE(const_pointer_cast<const typename TestType::element_type>(ptr) == nullptr);

        REQUIRE(const_pointer_cast<const typename TestType::element_type>(std::move(ptr)) == nullptr);
    }

    SECTION("Reinterpret cast") {
        if constexpr (not decltype(ptr)::is_owning)
            REQUIRE(reinterpret_pointer_cast<std::byte>(ptr) == nullptr);

        REQUIRE(reinterpret_pointer_cast<std::byte>(std::move(ptr)) == nullptr);
    }

    REQUIRE(ptr.use_count() == 0);
    REQUIRE(ptr == nullptr);
}

TEMPLATE_TEST_CASE("Dynamic cast failure", "[inheritance][advanced][conversion]", Derived, Unrelated) {
    TickerInfo infoB;

    Base* base = new Base(infoB); 
    omni_ptr<Base> owner(base);

    SECTION("Copy convert") {
        const omni_view view = owner;

        auto converted = dynamic_pointer_cast<TestType>(view);

        REQUIRE(converted == nullptr);
        REQUIRE(not view.expired());
        REQUIRE(owner == view);
        REQUIRE(owner.get() == base);
        REQUIRE(view.get() == base);
        REQUIRE(owner.use_count() == 2);
        REQUIRE(view.use_count() == 2);
    }

    SECTION("Move convert owning") {
        auto converted = dynamic_pointer_cast<TestType>(std::move(owner));

        REQUIRE(converted == nullptr);
        REQUIRE(owner.get() == base);
        REQUIRE(owner.use_count() == 1);
    }

    SECTION("Move convert non-owning") {
        const omni_view view = owner;

        auto converted = dynamic_pointer_cast<TestType>(std::move(owner));

        REQUIRE(converted == nullptr);
        REQUIRE(not view.expired());
        REQUIRE(owner == view);
        REQUIRE(owner.get() == base);
        REQUIRE(view.get() == base);
        REQUIRE(owner.use_count() == 2);
        REQUIRE(view.use_count() == 2);
    }
    
    REQUIRE(owner.use_count() == 1);
    REQUIRE(infoB == constructed<>);
}

TEMPLATE_TEST_CASE_SIG (
      "Const cast", "[inheritance][advanced][conversion]"
    , ((template<typename, typename> typename Ptr, template<typename, typename> typename PtrOpp, typename T), Ptr, PtrOpp, T)
    , (omni_view, omni_ref, const Ticker), (omni_ref, omni_view, Ticker)) {
    
    TickerInfo info{};
    auto owner = make_omni<Ticker>(info, "A");

    SECTION("Copy conversion") {
        PtrOpp<Ticker, AlignmentPolicy::Default> weak = owner;

        Ptr<std::remove_const_t<T>, AlignmentPolicy::Default> converted = const_pointer_cast<T>(weak);

        REQUIRE(weak == owner);
        REQUIRE(converted == weak);
        REQUIRE(converted == owner);
        REQUIRE(weak.use_count() == 3);
        REQUIRE(owner.use_count() == 3);
        REQUIRE(converted.use_count() == 3);
    }

    SECTION("Move conversion from owner") {
        omni_ptr<T> converted = const_pointer_cast<T>(std::move(owner));
        
        REQUIRE(converted != nullptr);
        REQUIRE(not owner);
        REQUIRE(owner.use_count() == 0);
        REQUIRE(converted.use_count() == 1);
    }

    SECTION("Move conversion from non-owner") {
        PtrOpp<Ticker, AlignmentPolicy::Default> weak = owner;

        Ptr<std::remove_const_t<T>, AlignmentPolicy::Default> converted = const_pointer_cast<T>(std::move(weak));

        REQUIRE(weak != owner);
        REQUIRE(weak == nullptr);
        REQUIRE(converted == owner);
        REQUIRE(weak.use_count() == 0);
        REQUIRE(owner.use_count() == 2);
        REQUIRE(converted.use_count() == 2);
    }
}