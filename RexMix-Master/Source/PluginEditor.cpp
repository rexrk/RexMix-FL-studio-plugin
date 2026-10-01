#include "PluginEditor.h"

namespace
{
const juce::Colour backgroundColour { 0xff101318 };
const juce::Colour panelColour { 0xff191e26 };
const juce::Colour accentColour { 0xff4de0b5 };
const juce::Colour primaryTextColour { 0xffedf2f7 };
const juce::Colour secondaryTextColour { 0xff8d98a8 };
const juce::Colour gridColour { 0xff303844 };

juce::String audioTypeName (rexmix::NodeType type)
{
    switch (type)
    {
        case rexmix::NodeType::audio:      return "Audio";
        case rexmix::NodeType::instrument: return "Instrument";
        case rexmix::NodeType::midi:       return "MIDI";
        case rexmix::NodeType::kick:      return "Kick";
        case rexmix::NodeType::snare:     return "Snare";
        case rexmix::NodeType::hiHat:     return "Hi-Hat";
        case rexmix::NodeType::clap:      return "Clap";
        case rexmix::NodeType::tom:       return "Tom";
        case rexmix::NodeType::percussion:return "Percussion";
        case rexmix::NodeType::bass808:   return "Bass / 808";
        case rexmix::NodeType::synthBass: return "Bass / Synth Bass";
        case rexmix::NodeType::bassGuitar:return "Bass / Bass Guitar";
        case rexmix::NodeType::subBass:   return "Bass / Sub Bass";
        case rexmix::NodeType::piano:     return "Piano";
        case rexmix::NodeType::guitar:    return "Guitar";
        case rexmix::NodeType::acousticGuitar: return "Acoustic Guitar";
        case rexmix::NodeType::electricGuitar: return "Electric Guitar";
        case rexmix::NodeType::synth:     return "Synth";
        case rexmix::NodeType::lead:      return "Lead";
        case rexmix::NodeType::pad:       return "Pad";
        case rexmix::NodeType::strings:   return "Strings";
        case rexmix::NodeType::keys:      return "Keys";
        case rexmix::NodeType::leadVocal: return "Vocal / Lead Vocal";
        case rexmix::NodeType::backingVocal: return "Vocal / Backing Vocal";
        case rexmix::NodeType::vocalChop: return "Vocal / Vocal Chop";
        case rexmix::NodeType::spoken:    return "Vocal / Spoken";
        case rexmix::NodeType::impact:    return "Impact";
        case rexmix::NodeType::riser:     return "Risers";
        case rexmix::NodeType::sweep:     return "Sweep";
        case rexmix::NodeType::texture:   return "Texture";
        case rexmix::NodeType::fxOther:   return "FX Other";
        case rexmix::NodeType::ambience:  return "Ambience";
        case rexmix::NodeType::other:     return "Other";
        case rexmix::NodeType::unknown:    break;
    }

    return "Unknown";
}
}

RexMixNodeRegistryView::RexMixNodeRegistryView()
{
    setSize (720, static_cast<int> (rexmix::maxNodes) * 27 + 34);
}

void RexMixNodeRegistryView::setNodes (
    const std::array<rexmix::NodeSlot, rexmix::maxNodes>& nodes)
{
    nodeSlots = nodes;
    repaint();
}

void RexMixNodeRegistryView::paint (juce::Graphics& graphics)
{
    graphics.fillAll (backgroundColour);

    auto headerBounds = juce::Rectangle<int> (0, 0, getWidth(), 30);
    graphics.setColour (panelColour);
    graphics.fillRoundedRectangle (headerBounds.toFloat(), 5.0f);
    graphics.setColour (secondaryTextColour);
    graphics.setFont (juce::Font (juce::FontOptions { 11.0f, juce::Font::bold }));
    graphics.drawText ("AUDIO TYPE", 12, 0, 330, 30, juce::Justification::centredLeft);
    graphics.drawText ("SAMPLE POSITION", 370, 0, 250, 30,
                       juce::Justification::centredLeft);

    auto rowY = 36;
    for (std::uint32_t index = 0; index < rexmix::maxNodes; ++index)
    {
        const auto& node = nodeSlots[index];
        if (node.active == 0)
            continue;

        const auto row = juce::Rectangle<float> (0.0f,
                                                  static_cast<float> (rowY),
                                                  static_cast<float> (getWidth()),
                                                  25.0f);
        graphics.setColour (rowY % 2 == 0 ? panelColour : backgroundColour);
        graphics.fillRoundedRectangle (row, 4.0f);

        graphics.setColour (primaryTextColour);
        graphics.setFont (juce::Font (juce::FontOptions { 12.0f }));
        graphics.drawText (audioTypeName (node.nodeType),
                           12, rowY, 330, 25, juce::Justification::centredLeft);
        graphics.drawText (node.samplePosition >= 0
                               ? juce::String (static_cast<juce::int64> (node.samplePosition))
                               : "--",
                           370, rowY, 250, 25, juce::Justification::centredLeft);

        rowY += 27;
    }

    if (rowY == 36)
    {
        graphics.setColour (secondaryTextColour);
        graphics.setFont (juce::Font (juce::FontOptions { 13.0f }));
        graphics.drawText ("No Nodes have registered yet.",
                           12, rowY, getWidth() - 24, 32,
                           juce::Justification::centredLeft);
    }
}

RexMixMasterAudioProcessorEditor::RexMixMasterAudioProcessorEditor (
    RexMixMasterAudioProcessor& audioProcessor)
    : AudioProcessorEditor (audioProcessor), processor (audioProcessor)
{
    setSize (760, 660);
    registryViewport.setViewedComponent (&registryView, false);
    registryViewport.setScrollBarsShown (true, false);
    addAndMakeVisible (registryViewport);
    analyzeMixButton.setEnabled (false);
    analyzeMixButton.onClick = [this] { processor.analyzeMix(); };
    addAndMakeVisible (analyzeMixButton);
    refreshRegistry();
    startTimerHz (4);
}

RexMixMasterAudioProcessorEditor::~RexMixMasterAudioProcessorEditor()
{
    stopTimer();
}

void RexMixMasterAudioProcessorEditor::paint (juce::Graphics& graphics)
{
    graphics.fillAll (backgroundColour);

    graphics.setColour (accentColour);
    graphics.fillRoundedRectangle (22.0f, 21.0f, 4.0f, 30.0f, 2.0f);

    graphics.setColour (primaryTextColour);
    graphics.setFont (juce::Font (juce::FontOptions { 22.0f, juce::Font::bold }));
    graphics.drawText ("RexMix Master", 38, 18, 280, 30,
                       juce::Justification::centredLeft);

    graphics.setColour (secondaryTextColour);
    graphics.setFont (juce::Font (juce::FontOptions { 12.0f }));
    graphics.drawText ("SHARED ANALYSIS REGISTRY", 39, 48, 300, 18,
                       juce::Justification::centredLeft);

    const auto statusColour = processor.getSharedMemory().isReady()
                            ? accentColour
                            : juce::Colour { 0xffff7272 };
    graphics.setColour (panelColour);
    graphics.fillRoundedRectangle (22.0f, 78.0f,
                                   static_cast<float> (getWidth()) - 44.0f, 79.0f, 7.0f);

    graphics.setColour (statusColour);
    graphics.fillEllipse (36.0f, 94.0f, 9.0f, 9.0f);
    graphics.setFont (juce::Font (juce::FontOptions { 14.0f, juce::Font::bold }));
    graphics.drawText (processor.getSharedMemory().isReady()
                           ? "SHARED MEMORY: READY"
                           : "SHARED MEMORY: UNAVAILABLE",
                       53, 87, getWidth() - 90, 24,
                       juce::Justification::centredLeft);

    graphics.setColour (secondaryTextColour);
    graphics.setFont (juce::Font (juce::FontOptions { 11.0f }));

    const auto mappingName = juce::String (processor.getSharedMemory().getObjectName().c_str());
    graphics.drawText ("Mapping: " + mappingName, 36, 113, getWidth() - 72, 18,
                       juce::Justification::centredLeft);

    const auto errorText = processor.getSharedMemory().isReady()
                         ? "Protocol v1  /  64 fixed slots  /  no raw audio"
                         : "Windows error " + juce::String (
                               static_cast<int> (processor.getSharedMemory().getErrorCode()));
    graphics.drawText (errorText, 36, 133, getWidth() - 72, 17,
                       juce::Justification::centredLeft);

    graphics.setColour (primaryTextColour);
    graphics.setFont (juce::Font (juce::FontOptions { 15.0f, juce::Font::bold }));
    graphics.drawText ("Nodes detected: " + juce::String (static_cast<int> (activeNodeCount)),
                       22, 169, 300, 27, juce::Justification::centredLeft);

    graphics.setColour (gridColour);
    graphics.drawHorizontalLine (201, 22.0f, static_cast<float> (getWidth()) - 22.0f);

    graphics.setColour (panelColour);
    graphics.fillRoundedRectangle (22.0f, 430.0f,
                                   static_cast<float> (getWidth()) - 44.0f, 210.0f, 7.0f);
    graphics.setColour (accentColour);
    graphics.setFont (juce::Font (juce::FontOptions { 13.0f, juce::Font::bold }));
    graphics.drawText ("CAPTURE", 38, 443, getWidth() - 76, 22,
                       juce::Justification::centredLeft);

    const auto& capture = processor.getCaptureSession();
    const auto captureState = capture.getState();
    const auto stateText = captureState == rexmix::CaptureState::capturing
                         ? "Capturing..."
                         : captureState == rexmix::CaptureState::complete
                             ? "Capture Complete"
                             : captureState == rexmix::CaptureState::partial
                                 ? "Playback Stopped"
                                 : captureState == rexmix::CaptureState::analyzed
                                     ? "Capture ready for analysis."
                                     : "Ready to Capture";
    const auto detailText = captureState == rexmix::CaptureState::ready
                          ? "Play your mix to capture up to 16 bars."
                          : captureState == rexmix::CaptureState::partial
                              ? "Partial capture: "
                                    + juce::String (capture.getCapturedBarCount()) + " / 16 bars"
                                    + "    Nodes Captured: "
                                    + juce::String (static_cast<int> (
                                          capture.getCapturedNodeCount()))
                              : captureState == rexmix::CaptureState::complete
                                  ? "16 / 16 bars captured"
                                        + juce::String ("    Nodes Captured: ")
                                        + juce::String (static_cast<int> (
                                              capture.getCapturedNodeCount()))
                                  : captureState == rexmix::CaptureState::analyzed
                                      ? "The current capture is available to analysis."
                                      : "Bar " + juce::String (capture.getCurrentBar()) + " / 16"
                                            + "    Nodes Captured: "
                                            + juce::String (static_cast<int> (
                                                  capture.getCapturedNodeCount()));

    graphics.setColour (primaryTextColour);
    graphics.setFont (juce::Font (juce::FontOptions { 18.0f, juce::Font::bold }));
    graphics.drawText (stateText, 38, 475, getWidth() - 76, 30,
                       juce::Justification::centredLeft);
    graphics.setColour (secondaryTextColour);
    graphics.setFont (juce::Font (juce::FontOptions { 13.0f }));
    graphics.drawText (detailText, 38, 511, getWidth() - 76, 24,
                       juce::Justification::centredLeft);
}

void RexMixMasterAudioProcessorEditor::resized()
{
    registryViewport.setBounds (22, 210, getWidth() - 44, 205);
    analyzeMixButton.setBounds (getWidth() - 190, 590, 145, 32);
}

void RexMixMasterAudioProcessorEditor::timerCallback()
{
    refreshRegistry();
    analyzeMixButton.setEnabled (processor.getCaptureSession().canAnalyze());
    repaint();
}

void RexMixMasterAudioProcessorEditor::refreshRegistry()
{
    nodeSlots.fill ({});
    activeNodeCount = 0;

    const auto& memory = processor.getSharedMemory();
    rexmix::SharedMemoryHeader headerSnapshot {};
    const auto headerAvailable = memory.getHeaderSnapshot (headerSnapshot);

    if (headerAvailable)
    {
        for (std::uint32_t index = 0; index < rexmix::maxNodes; ++index)
        {
            rexmix::NodeSlot snapshot {};
            if (! memory.tryReadNodeSlot (index, snapshot))
                continue;

            nodeSlots[index] = snapshot;
            if (snapshot.active != 0)
                ++activeNodeCount;
        }
    }

    registryView.setNodes (nodeSlots);
}
