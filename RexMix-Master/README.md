# RexMix Master v0.3

A JUCE 8 CMake VST3 effect for Windows x64. The Master creates and owns the RexMix Node shared-memory registry, displays each active Node's selected Audio Type and current sample position, and captures up to the first 16 bars of playback in RAM. It passes audio through unchanged and adds no processing latency.

## Requirements

- Visual Studio 2022 Build Tools with the C++ workload and Windows SDK
- CMake 3.22 or newer
- JUCE 8 source tree

By default, CMake looks for JUCE at `%USERPROFILE%\source\JUCE`. Override it with `-DJUCE_SOURCE_DIR=...` if needed.

## Configure, test, and build

Run from a Visual Studio 2022 Developer PowerShell:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target RexMixMaster_VST3 RexMixSharedMemoryTest RexMixCaptureSessionTest
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
- Header reserved word 0 is the Master-presence flag (`1` while the Master owns the mapping, cleared on normal unload). This uses existing reserved space and does not change the shared-memory layout, size, offsets, or protocol versions.
- Each slot contains an active flag, node ID/type, registration session ID, host sample position, timestamp in Unix-epoch nanoseconds, frame sequence, sample rate, left/right RMS and peak, stereo width, correlation, pitch, transient strength, 64 spectrum magnitudes, and reserved fields. The registry UI presents only Audio Type and Sample Position for active slots; it does not display Node ID or frame sequence.
- Node type numeric values 100–129 identify RexMix Node audio types: Kick, Snare, Hi-Hat, Clap, Tom, Percussion, 808, Synth Bass, Bass Guitar, Sub Bass, Piano, Guitar, Acoustic Guitar, Electric Guitar, Synth, Lead, Pad, Strings, Keys, Lead Vocal, Backing Vocal, Vocal Chop, Spoken, Impact, Risers, Sweep, Texture, FX Other, Ambience, and Other. These extend the existing 32-bit NodeType field without changing the shared-memory layout or protocol versions.
- The 64 spectrum magnitudes are intended as logarithmically spaced values from 20 Hz to 20 kHz; no raw audio is stored.
- Slot sequence counters reserve an odd/even publication protocol so future publishers and readers can avoid accepting a partially written frame.

The Master initializes a newly created region, serializing initialization with a named mutex outside the audio callback. Further Master instances attach without clearing an already-valid region. The OS removes the named mapping after the final handle closes; the Master owns no files and has no continuous network or file activity.

## Current limitations

The Master polls JUCE host transport information on the audio callback and transfers a small atomic snapshot to a 20 Hz message-thread capture timer. Capture duration is measured from the detected playback start using host PPQ, time signature, and the first 16 bars. If PPQ is unavailable, it falls back to host sample position and sample rate with BPM/time signature, then host seconds when sample position is unavailable. If time signature or BPM is unavailable, the fallback defaults to 4/4 and 120 BPM.

The message-thread timer copies new latest Node frames (deduplicated by per-Node frame sequence) into bounded Master-owned RAM. Each Node track is capped at 4,096 frames; at most 64 distinct Node IDs are stored per capture, and only Nodes that publish during capture allocate frame storage (under 94 MB at the theoretical maximum). Captured records preserve the existing Node slot values and sample positions for later alignment. No raw audio or disk data is stored. A new playback start clears all previous capture data; stopping early retains the partial capture. The Analyze Mix button becomes available for completed captures and partial captures containing Node data, and currently only changes the UI state to “Capture ready for analysis.”

Node ID and frame sequence remain internal capture keys and are not displayed. The protocol's registration-session field is not used for playback history. `processBlock()` only copies host timing values to lock-free atomic fields: it performs no shared-memory reads, capture allocation, or analysis, and leaves audio unchanged. FL Studio end-to-end capture should be verified with the Master and Nodes loaded together.
