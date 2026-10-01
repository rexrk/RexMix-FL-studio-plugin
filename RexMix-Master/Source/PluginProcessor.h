#pragma once

#include "RexMixSharedMemory.h"

#include <juce_audio_utils/juce_audio_utils.h>

class RexMixMasterAudioProcessor final : public juce::AudioProcessor
{
public:
    RexMixMasterAudioProcessor();
    ~RexMixMasterAudioProcessor() override = default;

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

private:
    rexmix::SharedMemory sharedMemory;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RexMixMasterAudioProcessor)
};
