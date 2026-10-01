# RexMix Node v0.4.1

A JUCE 8 CMake VST3 audio effect for Windows x64. It passes audio through unchanged with no added latency and displays host timing, per-channel RMS/peak, and a live FFT spectrum. When RexMix Master is available, it registers in the existing shared-memory registry, publishes the latest analysis frame, and sends the selected Audio Type.

## Requirements

- Visual Studio 2022 Build Tools with the C++ workload and Windows SDK
- CMake 3.22 or newer
- JUCE 8 source tree

By default, CMake looks for JUCE at `%USERPROFILE%\source\JUCE`. Override it with `-DJUCE_SOURCE_DIR=...` if the JUCE checkout is elsewhere.

## Configure and build

Run these commands from a Visual Studio 2022 Developer PowerShell:

```powershell
cmake -S . -B build-v0.4.1 -G "Visual Studio 17 2022" -A x64
cmake --build build-v0.4.1 --config Release --target RexMixNode_VST3 RexMixNodeSharedMemoryTest
ctest --test-dir build-v0.4.1 -C Release --output-on-failure
```

The VST3 bundle is generated at:

```text
build-v0.4.1\RexMixNode_artefacts\Release\VST3\RexMix Node.vst3
```

The build does not copy or install the plugin into FL Studio.

## Load manually in FL Studio

1. Copy the entire generated `RexMix Node.vst3` bundle to FL Studio's VST3 scan directory, normally `%COMMONPROGRAMFILES%\VST3`. Copying may require permission to write to that directory.
2. In FL Studio, open **Options > Manage plugins** and scan for plugins.
3. Add **RexMix Node** as an effect on a mixer channel.

The plugin logs a summary about every 500 ms through JUCE's logger. Host time and sample position are shown as unavailable when the host does not provide them. The sample position remains the host-provided timeline position and is published with each analysis frame.

When the host reports that transport is stopped, RMS, peak, and spectrum analysis pause and the last measured values remain visible. The UI identifies the stopped/held state. While playing, RMS and peak are measured from every audio callback and the UI applies light attack/release ballistics for readability. The callback's instantaneous buffer size is intentionally not shown because hosts may split processing into short sub-blocks.

## Master shared memory

The Node uses the exact RexMix Master protocol: `Local\RexMix_SharedMemory_v1`, protocol/structure version 1, 64-byte header, 64 slots of 360 bytes, and 23,104 bytes total. Header reserved word 0 is the Master-presence flag (`1` while Master owns the mapping, `0` during normal unload); its use leaves all structure sizes and slot offsets unchanged. The Node only calls `OpenFileMappingW`; it never creates or initializes the region. If Master is not present, analysis continues locally and the UI reports **MASTER MEMORY: NOT FOUND**. The existing message-thread timer checks presence and retries connection every 500 ms; connection checks/reconnection are never performed from `processBlock()`.

Each instance claims an available slot using a one-shot interlocked sequence-counter compare/exchange. It registers a per-instance Node ID and session ID, then publishes a latest-frame snapshot while playing. Frames include the host sample position (or -1 when unavailable), timestamp in Unix-epoch nanoseconds, monotonically increasing per-instance frame sequence, sample rate, linear RMS/peak, stereo width/correlation, and 64 logarithmically spaced FFT magnitudes. Pitch and transient strength remain zero because the Node does not calculate those metrics. The Node publishes no raw audio or history.

The Audio Type control uses separate Category and Type dropdowns; the specific type remains selected per category while the instance/editor is open. The default is **Other > Other**. Stable `NodeType` IDs are 100–129, in the order: Kick, Snare, Hi-Hat, Clap, Tom, Percussion, 808, Synth Bass, Bass Guitar, Sub Bass, Piano, Guitar, Acoustic Guitar, Electric Guitar, Synth, Lead, Pad, Strings, Keys, Lead Vocal, Backing Vocal, Vocal Chop, Spoken, Impact, Risers, Sweep, Texture, FX Other, Ambience, Other. The chosen type is written to the existing `NodeSlot::nodeType`; selecting a type attempts a single non-blocking slot update immediately and later frames also carry the current selection.

The CTest fixture creates an isolated Master-compatible mapping for tests. It checks missing/available Master status, presence loss and restoration, reconnection, multiple independent Node slots and Audio Type changes, frame publication, and slot cleanup. Audio processing code remains independent of connection state; FL Studio/Master GUI behavior still requires both plugins to be exercised in the DAW.
