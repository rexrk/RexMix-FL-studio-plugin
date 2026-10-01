#include "PluginProcessor.h"

#include "PluginEditor.h"

RexMixMasterAudioProcessor::RexMixMasterAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

void RexMixMasterAudioProcessor::prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock)
{
    juce::ignoreUnused (sampleRate, maximumExpectedSamplesPerBlock);
}

void RexMixMasterAudioProcessor::releaseResources()
{
}

bool RexMixMasterAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();
    const auto supportedInput = input == juce::AudioChannelSet::mono()
                             || input == juce::AudioChannelSet::stereo();

    return supportedInput && output == input;
}

void RexMixMasterAudioProcessor::processBlock (juce::AudioBuffer<float>&,
                                               juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
}

juce::AudioProcessorEditor* RexMixMasterAudioProcessor::createEditor()
{
    return new RexMixMasterAudioProcessorEditor (*this);
}

bool RexMixMasterAudioProcessor::hasEditor() const
{
    return true;
}

const juce::String RexMixMasterAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool RexMixMasterAudioProcessor::acceptsMidi() const
{
    return false;
}

bool RexMixMasterAudioProcessor::producesMidi() const
{
    return false;
}

bool RexMixMasterAudioProcessor::isMidiEffect() const
{
    return false;
}

double RexMixMasterAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int RexMixMasterAudioProcessor::getNumPrograms()
{
    return 1;
}

int RexMixMasterAudioProcessor::getCurrentProgram()
{
    return 0;
}

void RexMixMasterAudioProcessor::setCurrentProgram (int)
{
}

const juce::String RexMixMasterAudioProcessor::getProgramName (int)
{
    return {};
}

void RexMixMasterAudioProcessor::changeProgramName (int, const juce::String&)
{
}

void RexMixMasterAudioProcessor::getStateInformation (juce::MemoryBlock&)
{
}

void RexMixMasterAudioProcessor::setStateInformation (const void*, int)
{
}

const rexmix::SharedMemory& RexMixMasterAudioProcessor::getSharedMemory() const noexcept
{
    return sharedMemory;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RexMixMasterAudioProcessor();
}
