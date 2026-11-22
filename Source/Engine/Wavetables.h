//
//  Wavetables.h
//  Tonbox
//
//  Band-limited triangle and saw, as mipmapped wavetables.
//
//  Table m holds the first 2^m harmonics. A note reads the largest table
//  that fits under Nyquist at its pitch and the one below it, crossfaded by
//  how far the pitch is through the octave - so neither ever folds back, and
//  the top octave of the spectrum fades in and out instead of switching. The
//  top quarter of each table's harmonics is tapered as well. Together that
//  rolls the spectrum off above about 10 kHz.
//
//  Two-point polyBLEP was tried first: 13 to 29 dB more aliasing at 8 kHz,
//  and a triangle whose upper harmonics were several dB short. The saw still
//  uses it below 1 kHz (ToneEngine.cpp), where the saw is full-band and has
//  no Gibbs overshoot.
//

#pragma once

#include <array>
#include <cmath>
#include <vector>

namespace Wavetables
{
    constexpr int size = 4096;           // samples per cycle
    constexpr int levels = 12;           // 1 .. 2048 harmonics

    enum Shape { triangle, saw };

    struct Set
    {
        std::array<std::vector<float>, levels> tables;
    };

    inline Set build (Shape shape)
    {
        constexpr double pi = 3.14159265358979323846;
        Set set;

        for (int m = 0; m < levels; ++m)
        {
            auto& table = set.tables[(size_t) m];
            table.assign (size + 1, 0.0f);
            const auto harmonics = 1 << m;

            std::vector<double> sum (size, 0.0);

            for (int n = 1; n <= harmonics; ++n)
            {
                double amplitude;

                if (shape == triangle)
                {
                    if (n % 2 == 0)
                        continue;

                    // Peak at a quarter cycle, like the sine.
                    amplitude = 8.0 / (pi * pi) / (double) (n * n) * (((n - 1) / 2) % 2 == 0 ? 1.0 : -1.0);
                }
                else
                {
                    // Falling: 1 - 2 phase.
                    amplitude = 2.0 / pi / (double) n;
                }

                // Raised-cosine taper over the top quarter.
                const auto position = (double) n / (double) harmonics;
                if (m >= 2 && position > 0.75)
                    amplitude *= 0.5 + 0.5 * std::cos (pi * (position - 0.75) / 0.25);

                for (int i = 0; i < size; ++i)
                    sum[(size_t) i] += amplitude * std::sin (2.0 * pi * (double) n * (double) i / size);
            }

            for (int i = 0; i < size; ++i)
                table[(size_t) i] = (float) sum[(size_t) i];

            table[size] = table[0];   // so interpolation never wraps
        }

        return set;
    }

    inline const Set& get (Shape shape)
    {
        static const Set tri = build (triangle), sawtooth = build (saw);
        return shape == triangle ? tri : sawtooth;
    }

    inline float read (const std::vector<float>& table, double phase)
    {
        const auto position = phase * size;
        const auto i = (int) position;
        const auto f = (float) (position - i);
        return table[(size_t) i] + f * (table[(size_t) i + 1] - table[(size_t) i]);
    }

    /** dt is the phase increment per sample (frequency / sample rate). */
    inline float play (const Set& set, double phase, double dt)
    {
        const auto fit = std::log2 (0.5 / std::max (dt, 1.0e-9));   // log2 of the harmonics under Nyquist

        if (fit < 1.0)
            return read (set.tables[0], phase);

        if (fit >= levels)
            return read (set.tables[levels - 1], phase);

        const auto m = (int) fit;                // table m fits, table m - 1 fits with room
        const auto w = (float) (fit - m);
        const auto lower = read (set.tables[(size_t) m - 1], phase);
        const auto upper = read (set.tables[(size_t) m], phase);
        return lower + w * (upper - lower);
    }
}
