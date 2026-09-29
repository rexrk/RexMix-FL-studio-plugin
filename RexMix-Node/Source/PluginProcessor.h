#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <cstdint>

class RexMixAudioProcessor final : public juce::AudioProcessor,
                                   private juce::Timer
{
public:
    enum class TransportState : std::uint8_t
    {
        unknown,
        stopped,
        playing
    };

    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int spectrumBinCount = fftSize / 2 + 1;

    RexMixAudioProcessor();
    ~RexMixAudioProcessor() override;

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

    double getHostSampleRate() const noexcept;
    TransportState getTransportState() const noexcept;
    double getPlaybackTimeSeconds() const noexcept;
    bool hasPlaybackTime() const noexcept;
    std::int64_t getSamplePosition() const noexcept;
    bool hasSamplePosition() const noexcept;
    int getMeasuredChannelCount() const noexcept;
    float getRmsDb(int channel) const noexcept;
    float getPeakDb(int channel) const noexcept;
    float getSpectrumDb(int bin) const noexcept;

private:
    void timerCallback() override;
    void pushSpectrumSample(float sample) noexcept;
    void calculateSpectrum() noexcept;

    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window {
        static_cast<size_t> (fftSize),
        juce::dsp::WindowingFunction<float>::hann
    };

    std::array<float, fftSize> fftHistory {};
    std::array<float, fftSize * 2> fftData {};
    int fftWritePosition = 0;
    int fftSamplesSinceTransform = 0;
    int fftSamplesCollected = 0;

    // These atomics are the future Node-to-RexMix-Master telemetry surface; v0.2 keeps it local.
    std::atomic<double> hostSampleRate { 0.0 };
    std::atomic<TransportState> transportState { TransportState::unknown };
    std::atomic<double> playbackTimeSeconds { 0.0 };
    std::atomic<bool> playbackTimeAvailable { false };
    std::atomic<std::int64_t> samplePosition { 0 };
    std::atomic<bool> samplePositionAvailable { false };
    std::atomic<int> measuredChannelCount { 0 };
    std::array<std::atomic<float>, 2> rmsDb {};
    std::array<std::atomic<float>, 2> peakDb {};
    std::array<std::atomic<float>, spectrumBinCount> spectrumDb {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RexMixAudioProcessor)
};
