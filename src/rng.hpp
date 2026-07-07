// Copyright 2017 Stephen L. Smith and Frank Imeson
// Licensed under the Apache License, Version 2.0.  See LICENSE.

#pragma once

#include <cstdint>
#include <vector>

namespace glns {
namespace detail {

// xoshiro256++ (Blackman & Vigna, public domain), seeded via splitmix64.
// Same generator family as Julia's default task-local RNG.
class Rng {
public:
    explicit Rng(std::uint64_t seed) {
        std::uint64_t x = seed;
        for (auto& si : s_) {
            x += 0x9e3779b97f4a7c15ULL;
            std::uint64_t z = x;
            z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
            z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
            si = z ^ (z >> 31);
        }
    }

    std::uint64_t next_u64() {
        const std::uint64_t result = rotl(s_[0] + s_[3], 23) + s_[0];
        const std::uint64_t t = s_[1] << 17;
        s_[2] ^= s_[0];
        s_[3] ^= s_[1];
        s_[1] ^= s_[2];
        s_[0] ^= s_[3];
        s_[2] ^= t;
        s_[3] = rotl(s_[3], 45);
        return result;
    }

    // uniform in [0, 1), like Julia's rand()
    double next_double() { return static_cast<double>(next_u64() >> 11) * 0x1.0p-53; }

    // uniform in [0, n); unbiased via rejection
    std::uint64_t bounded(std::uint64_t n) {
        const std::uint64_t threshold = (0 - n) % n;
        for (;;) {
            const std::uint64_t r = next_u64();
            if (r >= threshold) return r % n;
        }
    }

    // uniform integer in [lo, hi] inclusive, like Julia's rand(lo:hi)
    int rand_int(int lo, int hi) {
        return lo + static_cast<int>(bounded(static_cast<std::uint64_t>(hi - lo + 1)));
    }

    // random element of a non-empty vector, like Julia's rand(v)
    template <class T>
    const T& pick(const std::vector<T>& v) {
        return v[bounded(v.size())];
    }

    // Fisher-Yates shuffle, like Julia's shuffle!
    template <class T>
    void shuffle(std::vector<T>& v) {
        for (std::size_t i = v.size(); i > 1; --i) {
            const std::size_t j = bounded(i);
            std::swap(v[i - 1], v[j]);
        }
    }

private:
    static std::uint64_t rotl(std::uint64_t x, int k) {
        return (x << k) | (x >> (64 - k));
    }
    std::uint64_t s_[4];
};

}  // namespace detail
}  // namespace glns
