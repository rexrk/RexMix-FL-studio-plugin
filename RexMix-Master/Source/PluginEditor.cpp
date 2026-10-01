#include "PluginEditor.h"

namespace
{
const juce::Colour backgroundColour { 0xff101318 };
const juce::Colour panelColour { 0xff191e26 };
const juce::Colour accentColour { 0xff4de0b5 };
const juce::Colour primaryTextColour { 0xffedf2f7 };
const juce::Colour secondaryTextColour { 0xff8d98a8 };
const juce::Colour gridColour { 0xff303844 };

juce::String nodeTypeName (rexmix::NodeType type)
{
    switch (type)
    {
        case rexmix::NodeType::audio:      return "Audio";
        case rexmix::NodeType::instrument: return "Instrument";
        case rexmix::NodeType::midi:       return "MIDI";
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
    graphics.drawText ("SLOT", 12, 0, 54, 30, juce::Justification::centredLeft);
    graphics.drawText ("NODE ID", 77, 0, 165, 30, juce::Justification::centredLeft);
    graphics.drawText ("TYPE", 255, 0, 100, 30, juce::Justification::centredLeft);
    graphics.drawText ("SAMPLE", 365, 0, 130, 30, juce::Justification::centredLeft);
    graphics.drawText ("FRAME", 510, 0, 100, 30, juce::Justification::centredLeft);

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

        graphics.setColour (accentColour);
        graphics.setFont (juce::Font (juce::FontOptions { 12.0f }));
        graphics.drawText (juce::String (static_cast<int> (index + 1)),
                           12, rowY, 54, 25, juce::Justification::centredLeft);
        graphics.drawText (juce::String::toHexString (
                               static_cast<juce::int64> (node.nodeId)).paddedLeft ('0', 16),
                           77, rowY, 165, 25, juce::Justification::centredLeft);

        graphics.setColour (primaryTextColour);
        graphics.drawText (nodeTypeName (node.nodeType),
                           255, rowY, 100, 25, juce::Justification::centredLeft);
        graphics.drawText (node.samplePosition >= 0
                               ? juce::String (static_cast<juce::int64> (node.samplePosition))
                               : "--",
                           365, rowY, 130, 25, juce::Justification::centredLeft);
        graphics.drawText (juce::String (static_cast<juce::uint64> (node.frameSequence)),
                           510, rowY, 100, 25, juce::Justification::centredLeft);

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
    setSize (760, 500);
    registryViewport.setViewedComponent (&registryView, false);
    registryViewport.setScrollBarsShown (true, false);
    addAndMakeVisible (registryViewport);
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
}

void RexMixMasterAudioProcessorEditor::resized()
{
    registryViewport.setBounds (22, 210, getWidth() - 44, getHeight() - 232);
}

void RexMixMasterAudioProcessorEditor::timerCallback()
{
    refreshRegistry();
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
