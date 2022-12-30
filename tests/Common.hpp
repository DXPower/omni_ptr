#pragma once

#include <string>
#include <iostream>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_template_test_macros.hpp>

struct TickerInfo {
    int constructed = 0;
    int copyConstructed = 0;
    int copyAssigned = 0;
    int moveConstructed = 0;
    int moveAssigned = 0;
    int copiedFrom = 0;
    int movedFrom = 0;
    int destroyed = 0;

    bool operator==(const TickerInfo&) const = default;

    friend std::ostream& operator<<(std::ostream& out, const TickerInfo& info) {
        out <<   "C : " << info.constructed
            << "\nCC: " << info.copyConstructed
            << "\nCA: " << info.copyAssigned
            << "\nMC: " << info.moveConstructed
            << "\nMA: " << info.moveAssigned
            << "\nCF: " << info.copiedFrom
            << "\nMF: " << info.movedFrom
            << "\nD : " << info.destroyed;

        return out;
    }
};

struct Ticker {
    TickerInfo* info;
    std::string str;

    Ticker(TickerInfo& info, std::string str) : info(&info), str(std::move(str)) {
        this->info->constructed++;
    }

    ~Ticker() {
        info->destroyed++;
    }

    Ticker(const Ticker& copy) : info(copy.info), str(copy.str) {
        info->copyConstructed++;
    }

    Ticker(Ticker&& move) : info(move.info), str(move.str) {
        info->moveConstructed++;
    }

    Ticker& operator=(const Ticker& copy) {
        str = copy.str;
        info->copyAssigned++;
        copy.info->copiedFrom++;
        return *this;
    }

    Ticker& operator=(Ticker&& copy) {
        str = copy.str;
        info->moveAssigned++;
        copy.info->movedFrom++;
        return *this;
    }
};

struct Base {
    TickerInfo* info;

    Base(TickerInfo& info) : info(&info) {
        info.constructed++;
    }

    virtual ~Base() {
        info->destroyed++;
    }

    virtual void Write(std::string& out) const {
        out += "Base";
    }
};

struct Derived final : Base {
    TickerInfo* info;

    Derived(TickerInfo& derivedInfo, TickerInfo& baseInfo) : Base(baseInfo), info(&derivedInfo) {
        derivedInfo.constructed++;
    }

    virtual ~Derived() {
        info->destroyed++;
    }

    void Write(std::string& out) const override {
        Base::Write(out);
        out += "Derived";
    }
};

struct Unrelated { };

template<int N = 1>
constexpr TickerInfo constructed {
    .constructed = N
};

template<int N = 1>
constexpr TickerInfo destroyed {
      .constructed = N
    , .destroyed = N
};
