#include "CaptureSession.h"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace rexmix
{
void CaptureSession::updateTransport (const HostTransportSnapshot& transport)
{
    if (transport.playing
        && (! previousTransportPlaying
            || transport.playbackGeneration != playbackGeneration))
    {
        beginCapture (transport);
        previousTransportPlaying = true;
        maxElapsedBars = elapsedBars (transport);
        if (maxElapsedBars >= captureBarCount)
        {
            currentBar = captureBarCount;
            state = CaptureState::complete;
        }
        else
        {
            currentBar = static_cast<int> (std::floor (maxElapsedBars)) + 1;
        }
        return;
    }

    previousTransportPlaying = transport.playing;
    if (state != CaptureState::capturing)
        return;

    maxElapsedBars = std::max (maxElapsedBars, elapsedBars (transport));
    if (maxElapsedBars >= captureBarCount)
    {
        currentBar = captureBarCount;
        state = CaptureState::complete;
    }
    else
    {
        currentBar = static_cast<int> (std::floor (maxElapsedBars)) + 1;
        if (! transport.playing)
            state = CaptureState::partial;
    }
}

bool CaptureSession::appendNodeFrame (std::uint32_t slotIndex, const NodeSlot& frame)
{
    if (state != CaptureState::capturing || slotIndex >= maxNodes
        || frame.active != 1 || frame.nodeId == 0 || frame.frameSequence == 0)
        return false;

    auto trackIt = std::find_if (tracks.begin(), tracks.end(), [&frame] (const CaptureTrack& track)
    {
        return track.nodeId == frame.nodeId;
    });

    if (trackIt == tracks.end())
    {
        trackIt = std::find_if (tracks.begin(), tracks.end(), [] (const CaptureTrack& track)
        {
            return track.nodeId == 0;
        });
        if (trackIt == tracks.end())
            return false;

        trackIt->nodeId = frame.nodeId;
        trackIt->lastFrameSequence = 0;
    }

    auto& track = *trackIt;
    if (track.lastFrameSequence == frame.frameSequence)
        return false;

    if (track.frames.capacity() < maxCapturedFramesPerNode)
        track.frames.reserve (maxCapturedFramesPerNode);

    if (track.frames.size() >= maxCapturedFramesPerNode)
        return false;

    track.frames.push_back ({ frame });
    track.lastFrameSequence = frame.frameSequence;
    return true;
}

void CaptureSession::analyze() noexcept
{
    if (canAnalyze())
        state = CaptureState::analyzed;
}

CaptureState CaptureSession::getState() const noexcept
{
    return state;
}

int CaptureSession::getCurrentBar() const noexcept
{
    return currentBar;
}

int CaptureSession::getCapturedBarCount() const noexcept
{
    if (state == CaptureState::complete || state == CaptureState::analyzed)
        return captureBarCount;

    return std::clamp (static_cast<int> (std::floor (maxElapsedBars)),
                       0,
                       captureBarCount);
}

std::uint32_t CaptureSession::getCapturedNodeCount() const noexcept
{
    return static_cast<std::uint32_t> (std::count_if (
        tracks.begin(), tracks.end(), [] (const CaptureTrack& track)
        {
            return ! track.frames.empty();
        }));
}

std::size_t CaptureSession::getCapturedFrameCount() const noexcept
{
    std::size_t count = 0;
    for (const auto& track : tracks)
        count += track.frames.size();

    return count;
}

bool CaptureSession::canAnalyze() const noexcept
{
    return state == CaptureState::complete
        || (state == CaptureState::partial && getCapturedFrameCount() != 0)
        || state == CaptureState::analyzed;
}

const CaptureTrack& CaptureSession::getTrack (std::uint32_t slotIndex) const noexcept
{
    assert (slotIndex < maxNodes);
    return tracks[slotIndex];
}

void CaptureSession::beginCapture (const HostTransportSnapshot& transport)
{
    clearCapture();
    state = CaptureState::capturing;
    playbackGeneration = transport.playbackGeneration;
    const auto hasStartPosition = transport.playbackStartGeneration
                               == transport.playbackGeneration;
    startSamplePosition = hasStartPosition ? transport.startSamplePosition
                                           : transport.samplePosition;
    startSampleAvailable = hasStartPosition ? transport.startSamplePositionAvailable
                                            : transport.samplePositionAvailable;
    startTimeSeconds = hasStartPosition ? transport.startTimeSeconds
                                        : transport.timeSeconds;
    startTimeAvailable = hasStartPosition ? transport.startTimeSecondsAvailable
                                          : transport.timeSecondsAvailable;
    startPpqPosition = hasStartPosition ? transport.startPpqPosition
                                        : transport.ppqPosition;
    startPpqAvailable = (hasStartPosition ? transport.startPpqAvailable
                                          : transport.ppqAvailable)
                     && std::isfinite (startPpqPosition);
    const auto bpmAtStart = hasStartPosition ? transport.startBpm : transport.bpm;
    auto startTimeSignature = transport;
    if (hasStartPosition)
    {
        startTimeSignature.timeSignatureNumerator = transport.startTimeSignatureNumerator;
        startTimeSignature.timeSignatureDenominator = transport.startTimeSignatureDenominator;
    }
    startBeatsPerBar = beatsPerBar (startTimeSignature);
    startBpm = std::isfinite (bpmAtStart)
            && bpmAtStart >= 20.0
            && bpmAtStart <= 400.0
             ? bpmAtStart
             : 120.0;
    const auto sampleRate = hasStartPosition ? transport.startSampleRate
                                             : transport.sampleRate;
    startSampleRate = std::isfinite (sampleRate)
                   && sampleRate > 0.0
                    ? sampleRate
                    : 0.0;
    maxElapsedBars = 0.0;
    currentBar = 1;
}

double CaptureSession::elapsedBars (const HostTransportSnapshot& transport) const noexcept
{
    if (startPpqAvailable && transport.ppqAvailable
        && std::isfinite (transport.ppqPosition))
    {
        const auto ppqDelta = transport.ppqPosition - startPpqPosition;
        if (std::isfinite (ppqDelta))
            return std::max (0.0, ppqDelta / startBeatsPerBar);
    }

    const auto sampleRate = transport.sampleRate > 0.0
                         && std::isfinite (transport.sampleRate)
                          ? transport.sampleRate
                          : startSampleRate;
    if (startSampleAvailable && transport.samplePositionAvailable && sampleRate > 0.0)
    {
        const auto elapsedSeconds = (static_cast<double> (transport.samplePosition)
                                   - static_cast<double> (startSamplePosition)) / sampleRate;
        if (std::isfinite (elapsedSeconds))
            return std::max (0.0, elapsedSeconds * startBpm / 60.0 / startBeatsPerBar);
    }

    if (startTimeAvailable && transport.timeSecondsAvailable
        && std::isfinite (transport.timeSeconds))
    {
        const auto elapsedSeconds = transport.timeSeconds - startTimeSeconds;
        if (std::isfinite (elapsedSeconds))
            return std::max (0.0, elapsedSeconds * startBpm / 60.0 / startBeatsPerBar);
    }

    return maxElapsedBars;
}

double CaptureSession::beatsPerBar (const HostTransportSnapshot& transport) const noexcept
{
    const auto numerator = transport.timeSignatureNumerator;
    const auto denominator = transport.timeSignatureDenominator;
    if (numerator <= 0 || numerator > 32 || denominator <= 0 || denominator > 32)
        return 4.0;

    return static_cast<double> (numerator) * 4.0 / denominator;
}

void CaptureSession::clearCapture()
{
    for (auto& track : tracks)
    {
        std::fill (track.frames.begin(), track.frames.end(), CapturedNodeFrame {});
        track.nodeId = 0;
        track.lastFrameSequence = 0;
        track.frames.clear();
    }
}
}
