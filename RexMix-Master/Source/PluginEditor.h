#pragma once

#include "PluginProcessor.h"

#include <array>

class RexMixNodeRegistryView final : public juce::Component
{
public:
    RexMixNodeRegistryView();
    void setNodes (const std::array<rexmix::NodeSlot, rexmix::maxNodes>& nodes);
    void paint (juce::Graphics&) override;

private:
    std::array<rexmix::NodeSlot, rexmix::maxNodes> nodeSlots {};
};

class RexMixMasterAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                               private juce::Timer
{
public:
    explicit RexMixMasterAudioProcessorEditor (RexMixMasterAudioProcessor&);
    ~RexMixMasterAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshRegistry();

    RexMixMasterAudioProcessor& processor;
    juce::Viewport registryViewport;
    RexMixNodeRegistryView registryView;
    std::array<rexmix::NodeSlot, rexmix::maxNodes> nodeSlots {};
    std::uint32_t activeNodeCount = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RexMixMasterAudioProcessorEditor)
};
