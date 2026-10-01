# RexMix Master v0.1

A JUCE 8 CMake VST3 effect for Windows x64. The Master creates and owns a versioned, fixed-size shared-memory registry for future RexMix Nodes. It passes audio through unchanged and adds no processing latency. Node publishing is not implemented in this version.

## Requirements

- Visual Studio 2022 Build Tools with the C++ workload and Windows SDK
- CMake 3.22 or newer
- JUCE 8 source tree

By default, CMake looks for JUCE at `%USERPROFILE%\source\JUCE`. Override it with `-DJUCE_SOURCE_DIR=...` if needed.

## Configure, test, and build

Run from a Visual Studio 2022 Developer PowerShell:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target RexMixMaster_VST3 RexMixSharedMemoryTest
ctest --test-dir build -C Release --output-on-failure
```

The VST3 bundle is generated at:

```text
build\RexMixMaster_artefacts\Release\VST3\RexMix Master.vst3
```

The plugin is not automatically installed into FL Studio. Copy the complete bundle directory to `%COMMONPROGRAMFILES%\VST3`, scan via **Options > Manage plugins**, then add **RexMix Master** to the Master mixer channel.

## Shared-memory protocol

- Named object: `Local\RexMix_SharedMemory_v1` (per interactive Windows session).
- Protocol version 1, structure version 1, maximum 64 fixed Node slots.
- Region size: 23,104 bytes (64-byte header plus 64 × 360-byte slots).
- Header includes the `REXMIXSM` signature, versions, sizes, capacity, active count, initialization state, and reserved fields.
- Each slot contains an active flag, node ID/type, registration session ID, host sample position, timestamp in Unix-epoch nanoseconds, frame sequence, sample rate, left/right RMS and peak, stereo width, correlation, pitch, transient strength, 64 spectrum magnitudes, and reserved fields.
- The 64 spectrum magnitudes are intended as logarithmically spaced values from 20 Hz to 20 kHz; no raw audio is stored.
- Slot sequence counters reserve an odd/even publication protocol so future publishers and readers can avoid accepting a partially written frame.

The Master initializes a newly created region, serializing initialization with a named mutex outside the audio callback. Further Master instances attach without clearing an already-valid region. The OS removes the named mapping after the final handle closes; the Master owns no files and has no continuous network or file activity.

## Current limitations

RexMix Node v0.2 does not yet publish into this mapping, so the registry starts with zero active Nodes. This version does not implement slot registration, heartbeat/offline tracking, session capture, or shared-memory writing. It should be loaded in the same interactive Windows session as future Node instances.
