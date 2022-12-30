#pragma once

#include "DxPtr.hpp"
#include <compare>
#include <crtdbg.h>
#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>
#include <new>
#include <cstdlib>
#include <iostream>
#include <concepts>
#include <compare>
#include <iosfwd>
#include <functional>

// #ifdef _MSC_VER
// #include <malloc.h>
// #define DXPTR_NOMSVC_CONSTEXPR
// #else
// #define DXPTR_NOMSVC_CONSTEXPR constexpr
// #endif

#define OMNI_PTR_ERROR_COPY_CONVERT_OWNING \
    "Can only use copying conversion functions with non-owning omni_ptrs. " \
    "(did you mean to use std::move or omni_view/omni_ref?)"

namespace DxPtr {
    namespace detail {
        template<typename T, typename... Args>
        concept correct_constructor_args = std::constructible_from<T, Args...>;
        
        constexpr std::size_t round_up_to_nearest_multiple(std::size_t toRound, std::size_t multiple) {
            std::size_t remainder = toRound % multiple;

            if (remainder == 0)
                return toRound;

            return toRound + multiple - remainder;
        }
    }

    namespace AlignmentPolicy {
        template<typename Stored, typename P>
        concept interface = requires(P& policy) {
            { policy.template get_alignment<Stored>() } -> std::convertible_to<std::align_val_t>;
        };

        class Default {
            public:
            constexpr Default() = default;

            template<typename T>
            constexpr std::align_val_t get_alignment() const {
                return static_cast<std::align_val_t>(alignof(T));
            }
        };

        template<std::size_t Alignment>
        class Overaligned {
            public:
            constexpr Overaligned() = default;

            template<typename T>
            requires (Alignment >= alignof(T))
            constexpr std::align_val_t get_alignment() const {
                return static_cast<std::align_val_t>(Alignment);
            }
        };

        template<typename T, typename Policy>
        requires interface<T, Policy>
        std::size_t get_stored_size() = delete;

        template<typename T, typename Policy>
        requires (interface<T, Policy> and not std::is_unbounded_array_v<T>)
        consteval std::size_t get_stored_size() {
            using namespace detail;

            if constexpr (std::is_bounded_array_v<T>)
                return std::extent_v<T> * round_up_to_nearest_multiple(
                      sizeof(std::remove_extent_t<T>)
                    , Policy{}.get_alignment()
                );
            else
                return sizeof(T);
        }

        template<typename T, typename Policy>
        requires interface<T, Policy> and std::is_unbounded_array_v<T>
        consteval std::size_t get_stored_size(std::size_t num) {
            using namespace detail;
            return num * round_up_to_nearest_multiple(
                  sizeof(std::remove_extent_t<T>)
                , Policy{}.get_alignment()
            );
        }
    }

    template<typename T, typename AP>
    class omni_ptr;

    template<typename T, typename AP, typename... Args>
    requires DxPtr::detail::correct_constructor_args<T, Args...>
    omni_ptr<T, AP> make_omni(Args&&... args);

    namespace detail { 
        class omni_block_base {
            protected:
            uintptr_t originalPointer;
            std::size_t refCount = 1;
            bool isExpired = false;

            public:
            omni_block_base(uintptr_t original) : originalPointer(original) { }
            
            protected:
            virtual ~omni_block_base() noexcept = default;

            public:
            void increment() { refCount++; }
            
            // Does not call expire(), as that is the role of the owning omni_ptr
            void decrement() noexcept {
                refCount--;

                if (refCount == 0)
                    delete this;
            }

            void release() noexcept {
                originalPointer = reinterpret_cast<uintptr_t>(nullptr);
                isExpired = true;
                decrement();
            }

            void expire() noexcept {
                call_deleter();
                isExpired = true;
            }

            bool is_expired() noexcept {
                return isExpired;
            }

            std::size_t use_count() noexcept {
                return refCount;
            }

            virtual void call_deleter() noexcept = 0;
            virtual void delete_allocation() noexcept = 0;

            void operator delete(omni_block_base* alloc, std::destroying_delete_t) noexcept {
                alloc->delete_allocation();
            }
        };

        template<
              typename T
            , bool IsConjoined = false
            , typename Deleter = std::default_delete<T>
            , typename AP = AlignmentPolicy::Default
        >
        requires 
                AlignmentPolicy::interface<T, AP>
            and (std::is_same_v<Deleter, std::default_delete<T>> or std::is_rvalue_reference_v<Deleter>)
        class omni_block final : Deleter, public omni_block_base {
            static constexpr bool IsDefaultDeleter = std::is_same_v<Deleter, std::default_delete<T>>;

            struct allocation {
                std::align_val_t alignment;
                std::size_t controlRegionSize, storedRegionSize;
                std::ptrdiff_t offsetControl, offsetStored;

                constexpr std::size_t get_total_size() const {
                    return controlRegionSize + storedRegionSize;
                }

                constexpr std::size_t get_control_size() const { 
                    return sizeof(omni_block);
                }

                constexpr std::align_val_t get_control_alignment() const {
                    return static_cast<std::align_val_t>(alignof(omni_block));
                }

                constexpr std::size_t get_stored_size() const { 
                    return AlignmentPolicy::get_stored_size<T, AP>();
                }

                constexpr std::align_val_t get_stored_alignment() const {
                    return AP{}.template get_alignment<T>();
                }

                #ifdef _DEBUG
                friend std::ostream& operator<<(std::ostream& out, const allocation& info) {
                    out << "Size:\t" << info.get_total_size()
                        << "\nAlign:\t" << static_cast<std::size_t>(info.alignment)
                        << "\nSizeControl:\t" << info.get_control_size()
                        << "\nAlignControl:\t" << static_cast<std::size_t>(info.get_control_alignment())
                        << "\nOffsetControl:\t" << info.offsetControl
                        << "\nSizeStored:\t" << info.get_stored_size()
                        << "\nAlignStored:\t" << static_cast<std::size_t>(info.get_stored_alignment())
                        << "\nOffsetStored:\t" << info.offsetStored;

                    return out;
                }
                #endif
            };

            public:
            omni_block(T* ptr) 
            requires IsDefaultDeleter
            : omni_block_base(reinterpret_cast<uintptr_t>(ptr)) { }

            omni_block(T* ptr, Deleter deleter)
            requires (not IsDefaultDeleter)
            : Deleter(std::move(deleter)), omni_block_base(ptr) { }
          
            T* get() const noexcept { return reinterpret_cast<T*>(originalPointer); }

            void call_deleter() noexcept override {
                // If conjoined, stored T is a part of this allocation
                // and we cannot call a regular delete on it.
                // Must call dtor manually.
                if constexpr (IsConjoined)
                    get()->~T();
                else
                    Deleter::operator()(reinterpret_cast<T*>(originalPointer));
            }

            // Buffer looks like [Control Padding Stored] or [Stored Padding Control],
            // depending on alignment
            static constexpr allocation get_conjoined_buffer_info() {
                auto alignControl = static_cast<std::align_val_t>(alignof(omni_block));
                auto alignT = AP{}.template get_alignment<T>();
                auto alignTarget = std::max(alignControl, alignT);

                std::size_t controlRegionSize, storedRegionSize;
                std::ptrdiff_t offsetControl, offsetStored;

                if (alignControl >= alignT) {
                    // Case 1: Region 1 is Control block
                    storedRegionSize = AlignmentPolicy::get_stored_size<T, AP>();
                    controlRegionSize = round_up_to_nearest_multiple(
                          sizeof(omni_block)
                        , (std::size_t) alignT
                    );

                    offsetControl = 0;
                    offsetStored = controlRegionSize;
                } else {
                    // Case 2: Region 1 is Stored block
                    // get_stored_size may add padding if the stored type
                    // is an array
                    storedRegionSize = round_up_to_nearest_multiple(
                          AlignmentPolicy::get_stored_size<T, AP>()
                        , (std::size_t) alignControl
                    );
                    controlRegionSize = sizeof(omni_block);

                    offsetControl = storedRegionSize;
                    offsetStored = 0;
                }

                return allocation{
                      .alignment = alignTarget
                    , .controlRegionSize = controlRegionSize
                    , .storedRegionSize = storedRegionSize
                    , .offsetControl = offsetControl
                    , .offsetStored = offsetStored
                };
            }

            template<typename... Args>
            requires IsConjoined
            static omni_block* make_conjoined(Args&&... args) {
                constexpr allocation info = get_conjoined_buffer_info();
                
                std::byte* buffer = new (info.alignment) std::byte[info.get_total_size()];

                // Create our objects in the proper locations
                T* stored = new(buffer + info.offsetStored) T(std::forward<Args>(args)...);
                omni_block* control = new(buffer + info.offsetControl) omni_block(stored);

                // std::cout << "Created conjoined omni_block at " << control << "\n";
                // std::cout << "Buffer location: " << (void*) buffer << "\n";
                // std::cout << info << "\n";

                return control;
            }

            // This does not call the destructor for the stored T,
            // as that is done by the owning omni_ptr by calling expire()
            void delete_allocation() noexcept override {
                omni_block* alloc = this;

                alloc->~omni_block();
                
                if (not IsConjoined) {
                    // std::cout << "Deleting non-conjoined omni_block at " << alloc << "\n";
                    ::operator delete(alloc, sizeof(omni_block));
                } else {
                    constexpr allocation info = get_conjoined_buffer_info();
                    
                    std::byte* blockLoc = reinterpret_cast<std::byte*>(alloc);
                    std::byte* bufferBegin = info.offsetControl == 0 ? blockLoc : blockLoc - info.offsetControl;

                    // std::cout << "Deleting conjoined omni_block at " << alloc << "\n";
                    // std::cout << "Buffer location: " << (void*) bufferBegin << "\n";
                    // std::cout << info << "\n";

                    ::operator delete(bufferBegin, info.get_total_size(), info.alignment);
                }
            }
        };

        template<bool F, typename T>
        struct weak_type_alias_provider;

        template<typename T>
        struct weak_type_alias_provider<true, T> {
            using weak_type = T;
        };

        template<typename T>
        struct weak_type_alias_provider<false, T> { };

        template<typename T, bool IsOwning, typename AP = AlignmentPolicy::Default>
        requires AlignmentPolicy::interface<T, AP>
        class omni_ptr : public weak_type_alias_provider<IsOwning, omni_ptr<T, false, AP>> {
            public:
            using pointer = T*;
            using element_type = std::remove_extent_t<T>;
            constexpr static bool is_owning = IsOwning;

            private:
            using control_base_t = omni_block_base;

            template<bool IsConjoined = false, typename Deleter = std::default_delete<T>>
            using control_t = omni_block<T, IsConjoined, Deleter, AP>;

            T* data;
            control_base_t* control;

            // Basic constructor
            protected:
            constexpr omni_ptr(T* ptr, control_base_t* control) : data(ptr), control(control) {
                // log_creation();
            }

            public:
            // Default/nullptr constructors
            constexpr omni_ptr() noexcept : omni_ptr(nullptr, nullptr) { }
            constexpr omni_ptr(std::nullptr_t) noexcept : omni_ptr() { }

            // Raw pointer constructor
            explicit omni_ptr(T* ptr) 
            requires IsOwning
            : omni_ptr(ptr, new control_t<>(ptr)) {
                // std::cout << "Created solo control block at " << control << "\n";
            }

            // Raw pointer converting constructor
            template<typename Y>
            requires IsOwning and std::convertible_to<Y*, T*>
            explicit omni_ptr(Y* ptr) : omni_ptr(static_cast<T*>(ptr)) { }

            // Copy constructor
            omni_ptr(const omni_ptr& copy) noexcept
            requires (not IsOwning)
            : omni_ptr(copy.data, copy.control) {
                if (control != nullptr)
                    control->increment();
            }
            
            // Move constructor
            omni_ptr(omni_ptr&& move) noexcept : omni_ptr(move.data, move.control) {
                move.data = nullptr;
                move.control = nullptr;
            }

            // Copy assignment
            omni_ptr& operator=(const omni_ptr& copy) noexcept
            requires (not IsOwning) {
                if (this == &copy)
                    return *this;

                if (control != nullptr)
                    control->decrement();

                data = copy.data;
                control = copy.control;

                if (control != nullptr)
                    control->increment();

                return *this;
            }

            // Move assignment
            omni_ptr& operator=(omni_ptr&& move) noexcept {
                if (this == &move)
                    return *this;

                swap(move);

                return *this;
            };

            // Destructor
            ~omni_ptr() {
                if (control == nullptr)
                    return;

                if constexpr (IsOwning)
                    reset();
                else
                    control->decrement();
            }

            // Non-owning copy construct from owner or convertible pointer
            template<typename Y, bool IsOwning2, typename AP2>
            requires (not IsOwning and std::convertible_to<Y*, T*>)
            omni_ptr(const omni_ptr<Y, IsOwning2, AP2>& copy) noexcept
            : omni_ptr(copy.data, copy.control) {
                if (control != nullptr)
                    control->increment();
            }

            // Non-owning copy assignment from owner or convertible pointer
            template<typename Y, bool IsOwning2, typename AP2>
            requires (not IsOwning and std::convertible_to<Y*, T*>)
            omni_ptr& operator=(const omni_ptr<Y, IsOwning2, AP2>& copy) {
                if (this == &copy)
                    return *this;

                if (data != nullptr)
                    control->decrement();

                data = copy.data;
                control = copy.control;

                if (control != nullptr)
                    control->increment();

                return *this;
            }

            // Move construct owning from owning convertible omni_ptr
            template<typename Y, typename AP2>
            requires (IsOwning and std::convertible_to<Y*, T*>)
            omni_ptr(omni_ptr<Y, true, AP2>&& move) noexcept
            : omni_ptr(move.data, move.control) {
                move.data = nullptr;
                move.control = nullptr;
            }

            // Move assign owning from owning convertible omni_ptr
            template<typename Y, typename AP2>
            requires (IsOwning and std::convertible_to<Y*, T*>)
            omni_ptr& operator=(omni_ptr<Y, true, AP2>&& move) noexcept {
                if (this == &move)
                    return *this;

                swap(move);

                return *this;
            }

            #ifdef _DEBUG
            void log_creation() const {
                // std::cout << "created omni_ptr " << (IsOwning ? "(owning)" : "(not owning)")
                //     << " with:\nT*\t: " << data << "\nC*:\t " << control << "\n";
            }
            #endif

            T* get() const noexcept {
                if constexpr (IsOwning)
                    return data;
                else if (not expired())
                    return data;
                else
                    return nullptr;
            }

            void reset() noexcept {
                #ifdef _DEBUG
                // std::cout << "reset omni_ptr " << (IsOwning ? "(owning)" : "(not owning)")
                    // << " with:\nT*\t: " << data << "\nC*:\t " << control << "\n";
                #endif 

                if (control == nullptr)
                    return;

                if constexpr (IsOwning)
                    control->expire();
                
                control->decrement();

                data = nullptr;
                control = nullptr;
            }

            void reset(T* other)
            requires IsOwning {
                omni_ptr(other).swap(*this);
            }

            T* release() noexcept
            requires IsOwning {
                if (control == nullptr)
                    return nullptr;

                control->release();
                control = nullptr;

                return std::exchange(data, nullptr);
            }

            bool expired() const noexcept
            requires (not IsOwning) {
                return control == nullptr || control->is_expired();
            }

            long use_count() const noexcept {
                if (data == nullptr)
                    return 0;

                // Long return type to match std::shared_ptr
                return static_cast<long>(control->use_count());
            }

            void swap(omni_ptr& other) noexcept {
                std::swap(data, other.data);
                std::swap(control, other.control);
            }

            // Pointer operator overloads
            T& operator*() const noexcept {
                return *data;
            }

            T* operator->() const noexcept {
                return data;
            }

            operator bool() const noexcept {
                return data != nullptr;
            }

            template<typename T2, bool IsOwning2, typename AP2>
            requires AlignmentPolicy::interface<T2, AP2>
            friend class detail::omni_ptr;

            template<typename T2, typename AP2, typename... Args>
            requires detail::correct_constructor_args<T2, Args...>
            friend DxPtr::omni_ptr<T2, AP2> DxPtr::make_omni(Args&&... args);

            template<typename Ptr, typename T2>
            friend Ptr make_omni_ptr_raw(T2*, omni_block_base*);
            
            template<typename Ptr>
            friend void set_omni_ptr_null_raw(Ptr& omni);

            template<typename T2, bool O2, typename AP2>
            friend control_base_t* get_control_block(const detail::omni_ptr<T2, O2, AP2>&);
        };

        template<typename U, bool O1, typename A, typename V, bool O2, typename B>
        constexpr bool operator==(const omni_ptr<U, O1, A>& lhs, const omni_ptr<V, O2, B>& rhs) noexcept {
            return lhs.get() == rhs.get();
        }

        template<typename U, bool O, typename A>
        constexpr bool operator==(const omni_ptr<U, O, A>& lhs, std::nullptr_t) noexcept {
            return lhs.get() == nullptr;
        }

        template<typename U, bool O, typename A>
        constexpr bool operator==(std::nullptr_t, const omni_ptr<U, O, A>& rhs) noexcept {
            return nullptr == rhs.get();
        }

        template<typename U, bool O1, typename A, typename V, bool O2, typename B>
        constexpr std::strong_ordering operator<=>(const omni_ptr<U, O1, A>& lhs, const omni_ptr<V, O2, B>& rhs) noexcept {
            return std::compare_three_way{}(lhs.get(), rhs.get());
        }

        template<typename U, bool O, typename A>
        constexpr std::strong_ordering operator<=>(const omni_ptr<U, O, A>& lhs, std::nullptr_t) noexcept {
            return std::compare_three_way{}(lhs.get(), (U*) nullptr);
        }

        template<typename U, bool O, typename A>
        constexpr std::strong_ordering operator<=>(std::nullptr_t, const omni_ptr<U, O, A>& rhs) noexcept {
            return std::compare_three_way{}((U*) nullptr, rhs.get());
        }

        template<typename U, bool O, typename A>
        std::ostream& operator<<(std::ostream& out, const omni_ptr<U, O, A>& rhs) {
            out << rhs.get();
            return out;
        }

        template<typename Ptr, typename T>
        Ptr make_omni_ptr_raw(T* ptr, omni_block_base* control) {
            return Ptr(ptr, control);
        }

        template<typename Ptr>
        void set_omni_ptr_null_raw(Ptr& omni) {
            omni.data = nullptr;
            omni.control = nullptr;
        }

        template<typename T, bool O, typename AP>
        omni_block_base* get_control_block(const detail::omni_ptr<T, O, AP>& omni) {
            return omni.control;
        }
    }

    template<typename T, typename AP = AlignmentPolicy::Default>
    class omni_ptr : public detail::omni_ptr<T, true, AP> {
        using detail::omni_ptr<T, true, AP>::omni_ptr;
    };

    template<typename T>
    omni_ptr(T*) -> omni_ptr<T>;

    template<typename T, typename AP = AlignmentPolicy::Default>
    class omni_view : public detail::omni_ptr<const T, false, AP> {
        using detail::omni_ptr<const T, false, AP>::omni_ptr;
    };

    template<typename T, typename AP = AlignmentPolicy::Default>
    omni_view(omni_ptr<T, AP>) -> omni_view<T, AP>;

    template<typename T, typename AP = AlignmentPolicy::Default>
    class omni_ref : public detail::omni_ptr<T, false, AP> {
        using detail::omni_ptr<T, false, AP>::omni_ptr;
    };

    template<typename T, typename AP = AlignmentPolicy::Default>
    omni_ref(omni_ptr<T, AP>) -> omni_ref<T, AP>;
    
    namespace detail {
        template<template<typename, typename> typename Ptr, typename T, typename AP>
        concept IsOmni = 
               std::is_same_v<Ptr<T, AP>, DxPtr::omni_ptr<T, AP>> 
            or std::is_same_v<Ptr<T, AP>, DxPtr::omni_view<T, AP>> 
            or std::is_same_v<Ptr<T, AP>, DxPtr::omni_ref<T, AP>>;

        template<template<typename, typename> typename Ptr, typename T, typename AP>
        concept IsWeakOmni = 
               std::is_same_v<Ptr<T, AP>, DxPtr::omni_view<T, AP>> 
            or std::is_same_v<Ptr<T, AP>, DxPtr::omni_ref<T, AP>>;

    }

    template<typename T, typename AP = AlignmentPolicy::Default, typename... Args>
    requires detail::correct_constructor_args<T, Args...>
    omni_ptr<T, AP> make_omni(Args&&... args) {
        using omni_ptr_t = omni_ptr<T, AP>;
        using control_t = typename omni_ptr_t::template control_t<true>;

        auto* omni_block = control_t::template make_conjoined(std::forward<Args>(args)...);

        return omni_ptr_t(omni_block->get(), omni_block);
    }

    template<typename T, template<typename, typename> typename Ptr, typename U, typename AP>
    Ptr<T, AP> static_pointer_cast(const Ptr<U, AP>& other) {
        static_assert(detail::IsWeakOmni<Ptr, U, AP>, OMNI_PTR_ERROR_COPY_CONVERT_OWNING);

        if (not other)
            return {};

        auto* ptr = static_cast<typename Ptr<T, AP>::element_type*>(other.get());
        auto* control = get_control_block(other);

        control->increment();

        return detail::make_omni_ptr_raw<Ptr<T, AP>>(ptr, control);
    }

    template<typename T, template<typename, typename> typename Ptr, typename U, typename AP>
    requires detail::IsOmni<Ptr, U, AP>
    Ptr<T, AP> static_pointer_cast(Ptr<U, AP>&& other) {
        if (not other)
            return {};

        auto* ptr = static_cast<typename Ptr<T, AP>::element_type*>(other.get());
        auto* control = detail::get_control_block(other);

        detail::set_omni_ptr_null_raw(other);

        return detail::make_omni_ptr_raw<Ptr<T, AP>>(ptr, control);
    }

    template<typename T, template<typename, typename> typename Ptr, typename U, typename AP>
    Ptr<T, AP> dynamic_pointer_cast(const Ptr<U, AP>& other) {
        static_assert(detail::IsWeakOmni<Ptr, U, AP>, OMNI_PTR_ERROR_COPY_CONVERT_OWNING);

        if constexpr (not std::is_class_v<T>) 
            return {};
        else {
            auto* ptr = dynamic_cast<typename Ptr<T, AP>::element_type*>(other.get());

            if (ptr == nullptr)
                return {};

            auto* control = detail::get_control_block(other);

            control->increment();
            
            return detail::make_omni_ptr_raw<Ptr<T, AP>>(ptr, control);
        }
    }

    template<typename T, template<typename, typename> typename Ptr, typename U, typename AP>
    requires detail::IsOmni<Ptr, U, AP>
    Ptr<T, AP> dynamic_pointer_cast(Ptr<U, AP>&& other) {
        if constexpr (not std::is_class_v<T>) 
            return {};
        else {
            auto* ptr = dynamic_cast<typename Ptr<T, AP>::element_type*>(other.get());

            if (ptr == nullptr)
                return {};

            auto* control = detail::get_control_block(other);

            detail::set_omni_ptr_null_raw(other);
            
            return detail::make_omni_ptr_raw<Ptr<T, AP>>(ptr, control);
        }
    }

    template<typename T, template<typename, typename> typename Ptr, typename U, typename AP>
    auto const_pointer_cast(const Ptr<U, AP>& other) {
        static_assert(detail::IsWeakOmni<Ptr, U, AP>, OMNI_PTR_ERROR_COPY_CONVERT_OWNING);
        
        using ResultPtr = std::conditional_t<
              std::is_const_v<T>
            , omni_view<std::remove_const_t<T>>
            , omni_ref<T>
        >;
        
        if (not other)
            return ResultPtr{};

        auto* ptr = const_cast<typename ResultPtr::element_type*>(other.get());
        auto* control = get_control_block(other);

        control->increment();

        return detail::make_omni_ptr_raw<ResultPtr>(ptr, control);
    }

    template<typename T, template<typename, typename> typename Ptr, typename U, typename AP>
    requires detail::IsOmni<Ptr, U, AP>
    auto const_pointer_cast(Ptr<U, AP>&& other) {
        using ResultPtr = std::conditional_t<
              detail::IsWeakOmni<Ptr, U, AP>
            , std::conditional_t<
                  std::is_const_v<T>
                , omni_view<std::remove_const_t<T>>
                , omni_ref<T>
            >
            , omni_ptr<T>
        >;

        if (not other)
            return ResultPtr{};

        auto* ptr = const_cast<typename ResultPtr::element_type*>(other.get());
        auto* control = detail::get_control_block(other);

        detail::set_omni_ptr_null_raw(other);

        return detail::make_omni_ptr_raw<ResultPtr>(ptr, control);
    }

    template<typename T, template<typename, typename> typename Ptr, typename U, typename AP>
    Ptr<T, AP> reinterpret_pointer_cast(const Ptr<U, AP>& other) {
        static_assert(detail::IsWeakOmni<Ptr, U, AP>, OMNI_PTR_ERROR_COPY_CONVERT_OWNING);

        if (not other)
            return {};

        auto* ptr = reinterpret_cast<typename Ptr<T, AP>::element_type*>(other.get());
        auto* control = get_control_block(other);

        control->increment();

        return detail::make_omni_ptr_raw<Ptr<T, AP>>(ptr, control);
    }

    template<typename T, template<typename, typename> typename Ptr, typename U, typename AP>
    requires detail::IsOmni<Ptr, U, AP>
    Ptr<T, AP> reinterpret_pointer_cast(Ptr<U, AP>&& other) {
        if (not other)
            return {};
        
        auto* ptr = reinterpret_cast<typename Ptr<T, AP>::element_type*>(other.get());
        auto* control = detail::get_control_block(other);

        detail::set_omni_ptr_null_raw(other);

        return detail::make_omni_ptr_raw<Ptr<T, AP>>(ptr, control);
    }
}


