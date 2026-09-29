# RexMix Node v0.2

A JUCE 8 CMake VST3 audio effect for Windows x64. It passes audio through unchanged with no added latency and displays host timing, per-channel RMS/peak, and a live FFT spectrum. It has no Master plugin, IPC, shared memory, or networking.

## Requirements

- Visual Studio 2022 Build Tools with the C++ workload and Windows SDK
- CMake 3.22 or newer
- JUCE 8 source tree

By default, CMake looks for JUCE at `%USERPROFILE%\source\JUCE`. Override it with `-DJUCE_SOURCE_DIR=...` if the JUCE checkout is elsewhere.

## Configure and build

Run these commands from a Visual Studio 2022 Developer PowerShell:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target RexMixNode_VST3
```

The VST3 bundle is generated at:

```text
build\RexMixNode_artefacts\Release\VST3\RexMix Node.vst3
```

The build does not copy or install the plugin into FL Studio.

## Load manually in FL Studio

1. Copy the entire generated `RexMix Node.vst3` bundle to FL Studio's VST3 scan directory, normally `%COMMONPROGRAMFILES%\VST3`. Copying may require permission to write to that directory.
2. In FL Studio, open **Options > Manage plugins** and scan for plugins.
3. Add **RexMix Node** as an effect on a mixer channel.

The plugin logs a summary about every 500 ms through JUCE's logger. Host time and sample position are shown as unavailable when the host does not provide them. The sample position remains the host-provided timeline position; it is intended to timestamp analysis frames and synchronize Node data with a future RexMix Master.

When the host reports that transport is stopped, RMS, peak, and spectrum analysis pause and the last measured values remain visible. The UI identifies the stopped/held state. While playing, RMS and peak are measured from every audio callback and the UI applies light attack/release ballistics for readability. The callback's instantaneous buffer size is intentionally not shown because hosts may split processing into short sub-blocks.
