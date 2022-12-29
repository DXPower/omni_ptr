#include <ios>
#include <iostream>
#include <thread>
#include <chrono>
#include <iomanip>

#include "DxPtr.hpp"

using namespace std::chrono_literals;
using namespace DxPtr;

struct Foo {
    Foo() {
        std::cout << "Foo()\n"; 
        std::cout << "Addr: " << this << "\n"; 
    }
    
    ~Foo() {
        std::cout << "~Foo()\n"; 
        std::cout << "Addr: " << this << "\n"; 
    }
};

int main() {
    // constexpr auto align = static_cast<std::align_val_t>(alignof(Foo));

    // auto* control = detail::omni_block<Foo, AlignmentPolicy::Overaligned<32>>::make_conjoined<>();

    // std::cout << "Control: " << control << "\n";

    // delete control;

    // Foo* f = new Foo();
    // auto omni = omni_ptr<Foo>(f);

    omni_ptr<Foo, AlignmentPolicy::Overaligned<32>>::weak_type v;

    {
        auto omni = make_omni<Foo, AlignmentPolicy::Overaligned<32>>();
        v = omni;

        std::cout << std::boolalpha << v.expired() << "\n";
    }

    std::cout << v.expired() << "\n";
}