#pragma once

#include "RexMixSharedMemory.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace rexmix
{
inline constexpr int captureBarCount = 16;
inline constexpr std::size_t maxCapturedFramesPerNode = 4096;

struct HostTransportSnapshot
{
    bool playing = false;
    std::uint64_t playbackGeneration = 0;
    bool ppqAvailable = false;
    double ppqPosition = 0.0;
    bool samplePositionAvailable = false;
    std::int64_t samplePosition = 0;
    bool timeSecondsAvailable = false;
    double timeSeconds = 0.0;
    double sampleRate = 0.0;
    double bpm = 0.0;
    int timeSignatureNumerator = 0;
    int timeSignatureDenominator = 0;
    std::uint64_t playbackStartGeneration = 0;
    bool startPpqAvailable = false;
    double startPpqPosition = 0.0;
    bool startSamplePositionAvailable = false;
    std::int64_t startSamplePosition = 0;
    bool startTimeSecondsAvailable = false;
    double startTimeSeconds = 0.0;
    double startSampleRate = 0.0;
    double startBpm = 0.0;
    int startTimeSignatureNumerator = 0;
    int startTimeSignatureDenominator = 0;
};

enum class CaptureState : std::uint8_t
{
    ready,
    capturing,
    complete,
    partial,
    analyzed
};

struct CapturedNodeFrame
{
    NodeSlot frame {};
};

struct CaptureTrack
{
    std::uint64_t nodeId = 0;
    std::uint64_t lastFrameSequence = 0;
    std::vector<CapturedNodeFrame> frames;
};

class CaptureSession final
{
public:
    void updateTransport (const HostTransportSnapshot& transport);
    bool appendNodeFrame (std::uint32_t slotIndex, const NodeSlot& frame);
    void analyze() noexcept;

    CaptureState getState() const noexcept;
    int getCurrentBar() const noexcept;
    int getCapturedBarCount() const noexcept;
    std::uint32_t getCapturedNodeCount() const noexcept;
    std::size_t getCapturedFrameCount() const noexcept;
    bool canAnalyze() const noexcept;
    const CaptureTrack& getTrack (std::uint32_t slotIndex) const noexcept;

private:
    void beginCapture (const HostTransportSnapshot& transport);
    double elapsedBars (const HostTransportSnapshot& transport) const noexcept;
    double beatsPerBar (const HostTransportSnapshot& transport) const noexcept;
    void clearCapture();

    std::array<CaptureTrack, maxNodes> tracks {};
    CaptureState state = CaptureState::ready;
    std::uint64_t playbackGeneration = 0;
    bool previousTransportPlaying = false;
    std::int64_t startSamplePosition = 0;
    double startTimeSeconds = 0.0;
    double startPpqPosition = 0.0;
    double startBeatsPerBar = 4.0;
    double startBpm = 120.0;
    double startSampleRate = 0.0;
    double maxElapsedBars = 0.0;
    int currentBar = 0;
    bool startSampleAvailable = false;
    bool startTimeAvailable = false;
    bool startPpqAvailable = false;
};
}
