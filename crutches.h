#pragma once


#include <utility>
#include <random>
#include <array>

namespace Pepega {
    constexpr std::array<std::pair<int, int>, 4> deltas{{{-1, 0}, {1, 0}, {0, -1}, {0, 1}}};

    template<typename T>
    T g() { return 0.1; };

    std::mt19937 rnd(1337);

    template<typename T>
    T random01() {
        if constexpr (std::is_same_v<T, float> || std::is_same_v<T, double>) {
            return T(rnd()) / T(std::mt19937::max());
        } else {
            return T::from_raw((rnd() & ((1ll << T::k) - 1ll)));
        }
    }
}
