#pragma once

#include "CaptureSession.h"
#include "RexMixSharedMemory.h"

#include <juce_audio_utils/juce_audio_utils.h>

#include <atomic>

static_assert (std::atomic<double>::is_always_lock_free,
               "Host timing atomics must be lock-free on the x64 audio thread");
static_assert (std::atomic<std::uint64_t>::is_always_lock_free,
               "Playback generation atomics must be lock-free on the x64 audio thread");

class RexMixMasterAudioProcessor final : public juce::AudioProcessor,
                                         private juce::Timer
{
public:
    RexMixMasterAudioProcessor();
    ~RexMixMasterAudioProcessor() override;

    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;
    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;
    void getStateInformation(juce::MemoryBlock& destinationData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    const rexmix::SharedMemory& getSharedMemory() const noexcept;
    const rexmix::CaptureSession& getCaptureSession() const noexcept;
    void analyzeMix() noexcept;

private:
    void timerCallback() override;
    bool readTransportSnapshot (rexmix::HostTransportSnapshot& snapshot) const noexcept;

    rexmix::SharedMemory sharedMemory;
    rexmix::CaptureSession captureSession;
    std::atomic<std::uint64_t> transportSnapshotSequence { 0 };
    std::atomic<bool> transportPlaying { false };
    std::atomic<std::uint64_t> playbackGeneration { 0 };
    std::atomic<bool> ppqAvailable { false };
    std::atomic<double> ppqPosition { 0.0 };
    std::atomic<bool> samplePositionAvailable { false };
    std::atomic<std::int64_t> samplePosition { 0 };
    std::atomic<bool> timeSecondsAvailable { false };
    std::atomic<double> timeSeconds { 0.0 };
    std::atomic<double> hostSampleRate { 0.0 };
    std::atomic<double> hostBpm { 0.0 };
    std::atomic<int> timeSignatureNumerator { 0 };
    std::atomic<int> timeSignatureDenominator { 0 };
    std::atomic<std::uint64_t> playbackStartGeneration { 0 };
    std::atomic<bool> startPpqAvailable { false };
    std::atomic<double> startPpqPosition { 0.0 };
    std::atomic<bool> startSamplePositionAvailable { false };
    std::atomic<std::int64_t> startSamplePosition { 0 };
    std::atomic<bool> startTimeSecondsAvailable { false };
    std::atomic<double> startTimeSeconds { 0.0 };
    std::atomic<double> startSampleRate { 0.0 };
    std::atomic<double> startBpm { 0.0 };
    std::atomic<int> startTimeSignatureNumerator { 0 };
    std::atomic<int> startTimeSignatureDenominator { 0 };
    std::atomic<bool> previousAudioThreadPlaying { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RexMixMasterAudioProcessor)
};
