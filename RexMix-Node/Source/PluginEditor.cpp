#include "PluginEditor.h"

#include <array>
#include <cmath>

namespace
{
const juce::Colour backgroundColour { 0xff101318 };
const juce::Colour panelColour { 0xff191e26 };
const juce::Colour accentColour { 0xff4de0b5 };
const juce::Colour primaryTextColour { 0xffedf2f7 };
const juce::Colour secondaryTextColour { 0xff8d98a8 };
const juce::Colour gridColour { 0xff303844 };
}

RexMixAudioProcessorEditor::RexMixAudioProcessorEditor (RexMixAudioProcessor& audioProcessor)
    : AudioProcessorEditor (audioProcessor), processor (audioProcessor)
{
    setSize (760, 500);
    startTimerHz (15);
}

RexMixAudioProcessorEditor::~RexMixAudioProcessorEditor()
{
    stopTimer();
}

void RexMixAudioProcessorEditor::paint (juce::Graphics& graphics)
{
    graphics.fillAll (backgroundColour);

    graphics.setColour (accentColour);
    graphics.fillRoundedRectangle (22.0f, 22.0f, 4.0f, 29.0f, 2.0f);

    graphics.setColour (primaryTextColour);
    graphics.setFont (juce::Font (juce::FontOptions { 22.0f, juce::Font::bold }));
    graphics.drawText ("RexMix Node", 38, 20, 230, 28, juce::Justification::centredLeft);

    graphics.setColour (secondaryTextColour);
    graphics.setFont (juce::Font (juce::FontOptions { 12.0f }));
    graphics.drawText ("VST3  /  LIVE CHANNEL ANALYZER", 39, 47, 290, 18,
                       juce::Justification::centredLeft);

    const auto cardWidth = (static_cast<float> (getWidth()) - 54.0f) * 0.5f;
    constexpr float cardHeight = 54.0f;
    constexpr float firstRowY = 78.0f;
    constexpr float rowGap = 9.0f;
    const auto leftX = 22.0f;
    const auto rightX = leftX + cardWidth + 10.0f;

    const auto playbackText = processor.hasPlaybackTime()
                            ? juce::String (processor.getPlaybackTimeSeconds(), 3) + " s"
                            : "--  (host timing unavailable)";
    const auto sampleText = processor.hasSamplePosition()
                          ? juce::String (static_cast<juce::int64> (processor.getSamplePosition()))
                          : "--  (not provided by host)";

    drawMetricCard (graphics, { leftX, firstRowY, cardWidth, cardHeight },
                    "HOST SAMPLE RATE",
                    processor.getHostSampleRate() > 0.0
                        ? juce::String (processor.getHostSampleRate(), 0) + " Hz"
                        : "--");
    const auto transportState = processor.getTransportState();
    const auto transportText = transportState == RexMixAudioProcessor::TransportState::stopped
                             ? "STOPPED  /  VALUES HELD"
                             : transportState == RexMixAudioProcessor::TransportState::playing
                                 ? "PLAYING  /  ANALYZING"
                                 : "ANALYZING  /  HOST STATE UNKNOWN";

    drawMetricCard (graphics, { rightX, firstRowY, cardWidth, cardHeight },
                    "TRANSPORT",
                    transportText);

    drawMetricCard (graphics, { leftX, firstRowY + cardHeight + rowGap, cardWidth, cardHeight },
                    "PLAYBACK POSITION",
                    playbackText);
    drawMetricCard (graphics, { rightX, firstRowY + cardHeight + rowGap, cardWidth, cardHeight },
                    "SAMPLE POSITION",
                    sampleText);

    const auto channelCount = processor.getMeasuredChannelCount();
    const auto rmsText = "L  " + formatLevel (displayedRmsDb[0], channelCount > 0)
                       + "     R  " + formatLevel (displayedRmsDb[1], channelCount > 1);
    const auto peakText = "L  " + formatLevel (displayedPeakDb[0], channelCount > 0)
                        + "     R  " + formatLevel (displayedPeakDb[1], channelCount > 1);

    drawMetricCard (graphics, { leftX, firstRowY + 2.0f * (cardHeight + rowGap), cardWidth, cardHeight },
                    "RMS LEVEL",
                    rmsText);
    drawMetricCard (graphics, { rightX, firstRowY + 2.0f * (cardHeight + rowGap), cardWidth, cardHeight },
                    "PEAK LEVEL",
                    peakText);

    drawSpectrum (graphics, { 22.0f, 281.0f, static_cast<float> (getWidth()) - 44.0f, 197.0f });
}

void RexMixAudioProcessorEditor::resized()
{
}

void RexMixAudioProcessorEditor::timerCallback()
{
    for (int channel = 0; channel < 2; ++channel)
    {
        displayedRmsDb[static_cast<size_t> (channel)] = applyBallistics (
            displayedRmsDb[static_cast<size_t> (channel)], processor.getRmsDb (channel));
        displayedPeakDb[static_cast<size_t> (channel)] = applyBallistics (
            displayedPeakDb[static_cast<size_t> (channel)], processor.getPeakDb (channel));
    }

    repaint();
}

void RexMixAudioProcessorEditor::drawMetricCard (juce::Graphics& graphics,
                                                  juce::Rectangle<float> bounds,
                                                  const juce::String& title,
                                                  const juce::String& value) const
{
    graphics.setColour (panelColour);
    graphics.fillRoundedRectangle (bounds, 7.0f);

    graphics.setColour (secondaryTextColour);
    graphics.setFont (juce::Font (juce::FontOptions { 10.0f, juce::Font::bold }));
    graphics.drawText (title, bounds.withTrimmedLeft (13.0f).withTrimmedTop (7.0f)
                              .withHeight (14.0f),
                       juce::Justification::centredLeft);

    graphics.setColour (primaryTextColour);
    graphics.setFont (juce::Font (juce::FontOptions { 15.0f, juce::Font::bold }));
    graphics.drawFittedText (value,
                             bounds.withTrimmedLeft (13.0f).withTrimmedRight (9.0f)
                                   .withTrimmedTop (24.0f).withTrimmedBottom (5.0f)
                                   .toNearestInt(),
                             juce::Justification::centredLeft, 1);
}

void RexMixAudioProcessorEditor::drawSpectrum (juce::Graphics& graphics,
                                                juce::Rectangle<float> bounds) const
{
    graphics.setColour (panelColour);
    graphics.fillRoundedRectangle (bounds, 7.0f);

    graphics.setColour (primaryTextColour);
    graphics.setFont (juce::Font (juce::FontOptions { 11.0f, juce::Font::bold }));
    const auto spectrumTitle = processor.getTransportState()
                             == RexMixAudioProcessor::TransportState::stopped
                             ? "SPECTRUM  /  HELD"
                             : "LIVE SPECTRUM";
    graphics.drawText (spectrumTitle, bounds.withTrimmedLeft (12.0f).withTrimmedTop (7.0f)
                                          .withHeight (15.0f),
                       juce::Justification::centredLeft);

    auto plot = bounds.withTrimmedLeft (43.0f).withTrimmedRight (12.0f)
                      .withTrimmedTop (28.0f).withTrimmedBottom (22.0f);

    const auto sampleRate = processor.getHostSampleRate() > 0.0
                          ? processor.getHostSampleRate()
                          : 44100.0;
    const auto maximumFrequency = juce::jmin (20000.0, sampleRate * 0.5);
    constexpr double minimumFrequency = 20.0;

    graphics.setFont (juce::Font (juce::FontOptions { 9.0f }));
    for (const auto level : { 0, -30, -60, -90 })
    {
        const auto y = plot.getY()
                     + plot.getHeight() * (static_cast<float> (100 + level) / 100.0f);
        graphics.setColour (gridColour);
        graphics.drawHorizontalLine (juce::roundToInt (y), plot.getX(), plot.getRight());
        graphics.setColour (secondaryTextColour);
        graphics.drawText (juce::String (level) + " dB",
                           juce::Rectangle<int> (juce::roundToInt (bounds.getX() + 5.0f),
                                                 juce::roundToInt (y - 6.0f),
                                                 34, 12),
                           juce::Justification::centredRight);
    }

    const std::array<double, 5> frequencyMarkers { 20.0, 100.0, 1000.0, 10000.0, 20000.0 };
    for (const auto frequency : frequencyMarkers)
    {
        if (frequency < minimumFrequency || frequency > maximumFrequency)
            continue;

        const auto proportion = std::log (frequency / minimumFrequency)
                              / std::log (maximumFrequency / minimumFrequency);
        const auto x = plot.getX() + plot.getWidth() * static_cast<float> (proportion);
        graphics.setColour (gridColour);
        graphics.drawVerticalLine (juce::roundToInt (x), plot.getY(), plot.getBottom());

        const auto label = frequency >= 1000.0
                         ? juce::String (frequency / 1000.0, frequency >= 10000.0 ? 0 : 1) + " kHz"
                         : juce::String (static_cast<int> (frequency)) + " Hz";
        graphics.setColour (secondaryTextColour);
        graphics.drawText (label,
                           juce::Rectangle<int> (juce::roundToInt (x - 24.0f),
                                                 juce::roundToInt (plot.getBottom() + 4.0f),
                                                 48, 13),
                           juce::Justification::centred);
    }

    juce::Path spectrumPath;
    auto pathStarted = false;

    for (int xOffset = 0; xOffset < juce::roundToInt (plot.getWidth()); ++xOffset)
    {
        const auto xProportion = static_cast<double> (xOffset)
                               / juce::jmax (1, juce::roundToInt (plot.getWidth()) - 1);
        const auto frequency = minimumFrequency
                             * std::pow (maximumFrequency / minimumFrequency, xProportion);
        const auto bin = juce::jlimit (1,
                                      RexMixAudioProcessor::spectrumBinCount - 1,
                                      juce::roundToInt (frequency * RexMixAudioProcessor::fftSize
                                                        / sampleRate));
        const auto decibels = juce::jlimit (-100.0f, 0.0f, processor.getSpectrumDb (bin));
        const auto y = plot.getBottom() - plot.getHeight()
                     * ((decibels + 100.0f) / 100.0f);
        const auto x = plot.getX() + static_cast<float> (xOffset);

        if (! pathStarted)
        {
            spectrumPath.startNewSubPath (x, y);
            pathStarted = true;
        }
        else
        {
            spectrumPath.lineTo (x, y);
        }
    }

    graphics.setColour (accentColour);
    graphics.strokePath (spectrumPath, juce::PathStrokeType (2.0f));
}

juce::String RexMixAudioProcessorEditor::formatLevel (float decibels,
                                                        bool channelAvailable) const
{
    return channelAvailable ? juce::String (decibels, 1) + " dB" : "--";
}

float RexMixAudioProcessorEditor::applyBallistics (float displayedDb,
                                                     float measuredDb) noexcept
{
    constexpr float attack = 0.55f;
    constexpr float release = 0.18f;
    const auto coefficient = measuredDb > displayedDb ? attack : release;
    return displayedDb + coefficient * (measuredDb - displayedDb);
}
