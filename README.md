# The Freak

Dynamic EQ by Castle Danger Music Company — six bands (bell / low shelf / high shelf), each with a static gain plus a
depth that cuts or lifts when its range strays from its own recent average. Part of the Mix System: The Master
prescribes and sets it over Castle Danger Link.

- JUCE 8.0.15, VST3 / AU / Standalone, universal macOS (arm64 + x86_64, macOS 11+) and Windows
- `controls.txt` — every control explained; `readme.txt` — installing by hand
- CI builds on every push to `main`; release zips land on the `downloads-macos` / `downloads-windows` branches
- Local headless checks: `cmake -B build -DFREAK_TESTS_ONLY=ON && cmake --build build --target TheFreakSnapshot && ./build/TheFreakSnapshot_artefacts/Release/TheFreakSnapshot snaps`
