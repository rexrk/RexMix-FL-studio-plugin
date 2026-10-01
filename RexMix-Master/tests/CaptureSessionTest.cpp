#include "CaptureSession.h"

#include <iostream>

namespace
{
bool check (bool condition, const char* message)
{
    if (! condition)
        std::cerr << "FAIL: " << message << '\n';

    return condition;
}

rexmix::NodeSlot makeFrame (std::uint64_t nodeId,
                            std::uint64_t frameSequence,
                            rexmix::NodeType type,
                            std::int64_t samplePosition)
{
    rexmix::NodeSlot frame {};
    frame.active = 1;
    frame.nodeId = nodeId;
    frame.frameSequence = frameSequence;
    frame.nodeType = type;
    frame.samplePosition = samplePosition;
    frame.timestampNs = frameSequence * 1000;
    frame.sampleRate = 48000.0f;
    frame.rms[0] = 0.25f;
    frame.rms[1] = 0.20f;
    frame.peak[0] = 0.8f;
    frame.peak[1] = 0.7f;
    frame.stereoWidth = 0.35f;
    frame.correlation = 0.92f;
    frame.pitchHz = 440.0f;
    frame.transientStrength = 0.12f;
    frame.spectrum[63] = 0.5f;
    return frame;
}
}

int main()
{
    bool passed = true;
    rexmix::CaptureSession capture;
    rexmix::HostTransportSnapshot transport;

    transport.playing = true;
    transport.playbackGeneration = 1;
    transport.playbackStartGeneration = 1;
    transport.ppqAvailable = true;
    transport.ppqPosition = 128.0;
    transport.samplePositionAvailable = true;
    transport.samplePosition = 100000;
    transport.sampleRate = 48000.0;
    transport.bpm = 120.0;
    transport.timeSignatureNumerator = 4;
    transport.timeSignatureDenominator = 4;
    transport.startPpqAvailable = true;
    transport.startPpqPosition = 128.0;
    transport.startSamplePositionAvailable = true;
    transport.startSamplePosition = 100000;
    transport.startTimeSecondsAvailable = true;
    transport.startTimeSeconds = 2.0;
    transport.startSampleRate = 48000.0;
    transport.startBpm = 120.0;
    transport.startTimeSignatureNumerator = 4;
    transport.startTimeSignatureDenominator = 4;
    capture.updateTransport (transport);
    passed &= check (capture.getState() == rexmix::CaptureState::capturing
                         && capture.getCurrentBar() == 1,
                     "new playback begins a capture at bar one");

    const auto kick = makeFrame (101, 1, rexmix::NodeType::kick, 100000);
    const auto bass = makeFrame (202, 1, rexmix::NodeType::bass808, 100000);
    passed &= check (capture.appendNodeFrame (0, kick),
                     "latest Node frame is copied into Master RAM");
    passed &= check (capture.appendNodeFrame (1, bass),
                     "a second Node is captured independently");
    passed &= check (! capture.appendNodeFrame (0, kick),
                     "duplicate latest frame is not stored twice");
    passed &= check (capture.getCapturedNodeCount() == 2
                         && capture.getCapturedFrameCount() == 2,
                     "capture reports two independent Nodes and frames");
    passed &= check (capture.getTrack (0).frames[0].frame.nodeId == 101
                         && capture.getTrack (1).frames[0].frame.nodeType
                             == rexmix::NodeType::bass808,
                     "captured RAM retains Node identity and selected type");
    passed &= check (capture.appendNodeFrame (
                         0, makeFrame (303, 1, rexmix::NodeType::leadVocal, 120000))
                         && capture.getCapturedNodeCount() == 3
                         && capture.getCapturedFrameCount() == 3,
                     "a replacement Node in a reused slot gets a separate capture track");

    transport.playing = false;
    transport.ppqPosition = 136.0;
    transport.samplePosition = 196000;
    capture.updateTransport (transport);
    passed &= check (capture.getState() == rexmix::CaptureState::partial
                         && capture.canAnalyze()
                         && capture.getCapturedFrameCount() == 3
                         && capture.getCapturedBarCount() == 2,
                     "stopping early retains the partial capture for analysis");

    transport.playing = true;
    transport.playbackGeneration = 2;
    transport.playbackStartGeneration = 2;
    transport.ppqPosition = 512.0;
    transport.samplePosition = 300000;
    transport.startPpqAvailable = true;
    transport.startPpqPosition = 512.0;
    transport.startSamplePosition = 300000;
    transport.startTimeSeconds = 6.0;
    capture.updateTransport (transport);
    passed &= check (capture.getState() == rexmix::CaptureState::capturing
                         && capture.getCapturedFrameCount() == 0
                         && capture.getCapturedNodeCount() == 0,
                     "new playback discards the previous partial capture");

    passed &= check (capture.appendNodeFrame (
                         0, makeFrame (303, 1, rexmix::NodeType::leadVocal, 300000)),
                     "new capture accepts current Node frames");
    transport.ppqPosition = 576.0;
    capture.updateTransport (transport);
    passed &= check (capture.getState() == rexmix::CaptureState::complete
                         && capture.getCurrentBar() == rexmix::captureBarCount
                         && capture.canAnalyze(),
                     "capture completes at exactly 16 PPQ-measured bars");
    capture.analyze();
    passed &= check (capture.getState() == rexmix::CaptureState::analyzed,
                     "Analyze Mix placeholder accepts a completed capture");

    transport.playbackGeneration = 3;
    transport.playbackStartGeneration = 3;
    transport.ppqPosition = 700.0;
    transport.startPpqPosition = 700.0;
    capture.updateTransport (transport);
    passed &= check (capture.getCapturedFrameCount() == 0
                         && capture.getCapturedNodeCount() == 0,
                     "only the latest capture is retained");

    rexmix::CaptureSession fallbackCapture;
    transport = {};
    transport.playing = true;
    transport.playbackGeneration = 1;
    transport.samplePositionAvailable = true;
    transport.samplePosition = 50000;
    transport.sampleRate = 48000.0;
    transport.bpm = 120.0;
    transport.timeSignatureNumerator = 3;
    transport.timeSignatureDenominator = 4;
    transport.playbackStartGeneration = 1;
    transport.startSamplePositionAvailable = true;
    transport.startSamplePosition = 50000;
    transport.startSampleRate = 48000.0;
    transport.startBpm = 120.0;
    transport.startTimeSignatureNumerator = 3;
    transport.startTimeSignatureDenominator = 4;
    fallbackCapture.updateTransport (transport);
    transport.samplePosition += static_cast<std::int64_t> (23.0 * transport.sampleRate);
    fallbackCapture.updateTransport (transport);
    passed &= check (fallbackCapture.getState() == rexmix::CaptureState::capturing,
                     "sample-rate/BPM fallback respects the 3/4 bar length");
    transport.samplePosition += static_cast<std::int64_t> (transport.sampleRate);
    fallbackCapture.updateTransport (transport);
    passed &= check (fallbackCapture.getState() == rexmix::CaptureState::complete,
                     "sample-position fallback completes after 16 bars");

    rexmix::CaptureSession delayedStartCapture;
    transport = {};
    transport.playing = true;
    transport.playbackGeneration = 1;
    transport.playbackStartGeneration = 1;
    transport.ppqAvailable = true;
    transport.ppqPosition = 140.0;
    transport.startPpqAvailable = true;
    transport.startPpqPosition = 128.0;
    transport.timeSignatureNumerator = 4;
    transport.timeSignatureDenominator = 4;
    delayedStartCapture.updateTransport (transport);
    passed &= check (delayedStartCapture.getCurrentBar() == 4,
                     "capture range is anchored to the audio-thread playback-start position");

    rexmix::CaptureSession boundedCapture;
    transport = {};
    transport.playing = true;
    transport.playbackGeneration = 1;
    transport.ppqAvailable = true;
    transport.ppqPosition = 0.0;
    transport.timeSignatureNumerator = 4;
    transport.timeSignatureDenominator = 4;
    boundedCapture.updateTransport (transport);
    bool allBoundedFramesStored = true;
    for (std::uint64_t sequence = 1;
         sequence <= rexmix::maxCapturedFramesPerNode;
         ++sequence)
    {
        allBoundedFramesStored &= boundedCapture.appendNodeFrame (
            0, makeFrame (404, sequence, rexmix::NodeType::snare,
                          static_cast<std::int64_t> (sequence)));
    }
    passed &= check (allBoundedFramesStored
                         && ! boundedCapture.appendNodeFrame (
                             0, makeFrame (404, rexmix::maxCapturedFramesPerNode + 1,
                                           rexmix::NodeType::snare, 0))
                         && boundedCapture.getCapturedFrameCount()
                             == rexmix::maxCapturedFramesPerNode,
                     "per-Node capture storage enforces its fixed frame bound");

    if (! passed)
        return 1;

    std::cout << "Capture lifecycle, bounded latest-frame storage, multi-Node capture, "
                 "partial retention, PPQ/fallback timing, and latest-only behavior passed.\n";
    return 0;
}
