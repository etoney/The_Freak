#pragma once
#include <juce_core/juce_core.h>
#include <vector>
#include <utility>

namespace freak
{
struct Preset { juce::String name; std::vector<std::pair<juce::String, float>> values; };

// Band ids: b<n>on, b<n>type (0 Bell, 1 Low Shelf, 2 High Shelf), b<n>f (Hz), b<n>q, b<n>g (static dB),
// b<n>d (dynamic depth dB: negative cuts when the band jumps, positive lifts when it sinks), b<n>t (threshold dB)
// Anything not listed stays at its default (all bands off).
inline const std::vector<Preset>& factoryPresets()
{
    static const std::vector<Preset> p {
        { "Init (All Bands Off)", { } },
        { "Master: Polish", { { "b1on", 1 }, { "b1f", 90 },   { "b1d", -4 }, { "b1t", 4 },
                              { "b3on", 1 }, { "b3f", 450 },  { "b3q", 1.2f }, { "b3d", -3 }, { "b3t", 4 },
                              { "b5on", 1 }, { "b5f", 3800 }, { "b5q", 1.6f }, { "b5d", -4 }, { "b5t", 3 },
                              { "b6on", 1 }, { "b6f", 9000 }, { "b6d", 3 }, { "b6t", 4 } } },
        { "Tame The Low End", { { "b1on", 1 }, { "b1f", 110 }, { "b1d", -6 }, { "b1t", 3 },
                                { "b2on", 1 }, { "b2f", 60 }, { "b2type", 0 }, { "b2q", 1.4f }, { "b2d", -4 }, { "b2t", 4 } } },
        { "De-Mud", { { "b2on", 1 }, { "b2f", 280 }, { "b2q", 1.2f }, { "b2d", -5 }, { "b2t", 3 },
                      { "b3on", 1 }, { "b3f", 500 }, { "b3q", 1.5f }, { "b3d", -3 }, { "b3t", 4 } } },
        { "Harshness Tamer", { { "b4on", 1 }, { "b4f", 2800 }, { "b4q", 2.0f }, { "b4d", -5 }, { "b4t", 3 },
                               { "b5on", 1 }, { "b5f", 4800 }, { "b5q", 2.0f }, { "b5d", -6 }, { "b5t", 3 } } },
        { "Cymbal Tamer", { { "b6on", 1 }, { "b6f", 7500 }, { "b6d", -6 }, { "b6t", 3 } } },
        { "Kick & Bass Control", { { "b1on", 1 }, { "b1type", 0 }, { "b1f", 55 },  { "b1q", 1.5f }, { "b1d", -6 }, { "b1t", 3 },
                                   { "b2on", 1 }, { "b2f", 120 }, { "b2q", 1.4f }, { "b2d", -4 }, { "b2t", 3 } } },
        { "Vocal Smoother", { { "b2on", 1 }, { "b2f", 250 }, { "b2q", 1.2f }, { "b2d", -4 }, { "b2t", 3 },
                              { "b4on", 1 }, { "b4f", 3000 }, { "b4q", 2.5f }, { "b4d", -5 }, { "b4t", 3 },
                              { "b6on", 1 }, { "b6f", 10000 }, { "b6d", 2 }, { "b6t", 5 } } },
        { "Guitar Fizz Control", { { "b4on", 1 }, { "b4f", 2200 }, { "b4q", 1.5f }, { "b4d", -4 }, { "b4t", 3 },
                                   { "b6on", 1 }, { "b6f", 6000 }, { "b6d", -6 }, { "b6t", 2 } } },
        { "Keep It Bright", { { "b5on", 1 }, { "b5f", 5000 }, { "b5q", 1.0f }, { "b5d", 3 }, { "b5t", 3 },
                              { "b6on", 1 }, { "b6f", 11000 }, { "b6d", 4 }, { "b6t", 3 } } },
    };
    return p;
}
} // namespace freak
