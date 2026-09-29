#pragma once

#include "PluginProcessor.h"

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
    void drawMetricCard (juce::Graphics&,
                         juce::Rectangle<float>,
                         const juce::String& title,
                         const juce::String& value) const;
    void drawSpectrum (juce::Graphics&, juce::Rectangle<float>) const;
    juce::String formatLevel (float decibels, bool channelAvailable) const;
    static float applyBallistics (float displayedDb, float measuredDb) noexcept;

    RexMixAudioProcessor& processor;
    std::array<float, 2> displayedRmsDb { -100.0f, -100.0f };
    std::array<float, 2> displayedPeakDb { -100.0f, -100.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RexMixAudioProcessorEditor)
};
