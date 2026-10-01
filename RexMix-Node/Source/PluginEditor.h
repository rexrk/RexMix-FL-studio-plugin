#pragma once

#include "PluginProcessor.h"

#include <array>

class RexMixAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                         private juce::Timer
{
public:
    explicit RexMixAudioProcessorEditor (RexMixAudioProcessor&);
    ~RexMixAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void categoryChanged();
    void audioTypeChanged();
    void populateAudioTypes (int categoryId);
    int getCategoryId (rexmix::NodeType type) const noexcept;
    void drawMetricCard (juce::Graphics&,
                         juce::Rectangle<float>,
                         const juce::String& title,
                         const juce::String& value) const;
    void drawSpectrum (juce::Graphics&, juce::Rectangle<float>) const;
    juce::String formatLevel (float decibels, bool channelAvailable) const;
    static float applyBallistics (float displayedDb, float measuredDb) noexcept;

    RexMixAudioProcessor& processor;
    juce::Label audioTypeLabel;
    juce::ComboBox categoryBox;
    juce::Label categorySeparator;
    juce::ComboBox audioTypeBox;
    std::array<rexmix::NodeType, 6> rememberedTypes {
        rexmix::NodeType::kick,
        rexmix::NodeType::bass808,
        rexmix::NodeType::piano,
        rexmix::NodeType::leadVocal,
        rexmix::NodeType::impact,
        rexmix::NodeType::other
    };
    bool updatingAudioTypeControls = false;
    std::array<float, 2> displayedRmsDb { -100.0f, -100.0f };
    std::array<float, 2> displayedPeakDb { -100.0f, -100.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RexMixAudioProcessorEditor)
};
