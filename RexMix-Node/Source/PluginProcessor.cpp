#include "PluginProcessor.h"

#include "PluginEditor.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr float silenceDb = -100.0f;
}

RexMixAudioProcessor::RexMixAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    nodePublisher.tryConnect();

    for (auto& value : rmsDb)
        value.store (silenceDb);

    for (auto& value : peakDb)
        value.store (silenceDb);

    for (auto& value : spectrumDb)
        value.store (silenceDb);

    startTimer (500);
}

RexMixAudioProcessor::~RexMixAudioProcessor()
{
    stopTimer();
    nodePublisher.disconnect();
}

void RexMixAudioProcessor::prepareToPlay (double sampleRate, int)
{
    hostSampleRate.store (sampleRate, std::memory_order_relaxed);
    for (int band = 0; band < static_cast<int> (rexmix::spectrumBinCount); ++band)
    {
        const auto proportion = (static_cast<double> (band) + 0.5)
                              / rexmix::spectrumBinCount;
        const auto frequency = 20.0 * std::pow (1000.0, proportion);
        spectrumSourceBins[static_cast<size_t> (band)] = juce::jlimit (
            1,
            spectrumBinCount - 1,
            juce::roundToInt (frequency * fftSize / juce::jmax (1.0, sampleRate)));
    }

    fftHistory.fill (0.0f);
    fftData.fill (0.0f);
    fftWritePosition = 0;
    fftSamplesSinceTransform = 0;
    fftSamplesCollected = 0;
}

void RexMixAudioProcessor::releaseResources()
{
}

bool RexMixAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();

    const auto supportedInput = input == juce::AudioChannelSet::mono()
                             || input == juce::AudioChannelSet::stereo();

    return supportedInput && output == input;
}

void RexMixAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                         juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    hostSampleRate.store (getSampleRate(), std::memory_order_relaxed);
    const auto numSamples = buffer.getNumSamples();
    if (auto* hostPlayHead = getPlayHead())
    {
        if (const auto position = hostPlayHead->getPosition())
        {
            const auto hostIsPlaying = position->getIsPlaying();
            transportState.store (hostIsPlaying ? TransportState::playing
                                                : TransportState::stopped,
                                  std::memory_order_relaxed);

            if (const auto time = position->getTimeInSeconds(); time.hasValue())
            {
                playbackTimeSeconds.store (*time, std::memory_order_relaxed);
                playbackTimeAvailable.store (true, std::memory_order_relaxed);
            }
            else
                playbackTimeAvailable.store (false, std::memory_order_relaxed);

            if (const auto sample = position->getTimeInSamples(); sample.hasValue())
            {
                samplePosition.store (*sample, std::memory_order_relaxed);
                samplePositionAvailable.store (true, std::memory_order_relaxed);
            }
            else
                samplePositionAvailable.store (false, std::memory_order_relaxed);

            if (! hostIsPlaying)
                return;
        }
        else
        {
            transportState.store (TransportState::unknown, std::memory_order_relaxed);
            playbackTimeAvailable.store (false, std::memory_order_relaxed);
            samplePositionAvailable.store (false, std::memory_order_relaxed);
        }
    }
    else
    {
        transportState.store (TransportState::unknown, std::memory_order_relaxed);
        playbackTimeAvailable.store (false, std::memory_order_relaxed);
        samplePositionAvailable.store (false, std::memory_order_relaxed);
    }

    const auto numChannels = juce::jmin (2, buffer.getNumChannels());
    measuredChannelCount.store (numChannels, std::memory_order_relaxed);

    std::array<double, 2> sumOfSquares {};
    std::array<float, 2> peaks {};
    double sumOfProducts = 0.0;
    double sumOfMidSquares = 0.0;
    double sumOfSideSquares = 0.0;

    const auto* left = numChannels > 0 ? buffer.getReadPointer (0) : nullptr;
    const auto* right = numChannels > 1 ? buffer.getReadPointer (1) : nullptr;

    for (int sampleIndex = 0; sampleIndex < numSamples; ++sampleIndex)
    {
        const auto leftSample = left != nullptr ? left[sampleIndex] : 0.0f;
        const auto rightSample = right != nullptr ? right[sampleIndex] : 0.0f;

        sumOfSquares[0] += static_cast<double> (leftSample) * leftSample;
        sumOfSquares[1] += static_cast<double> (rightSample) * rightSample;
        sumOfProducts += static_cast<double> (leftSample) * rightSample;
        peaks[0] = juce::jmax (peaks[0], std::abs (leftSample));
        peaks[1] = juce::jmax (peaks[1], std::abs (rightSample));

        const auto midSample = (leftSample + rightSample) * 0.5f;
        const auto sideSample = (leftSample - rightSample) * 0.5f;
        sumOfMidSquares += static_cast<double> (midSample) * midSample;
        sumOfSideSquares += static_cast<double> (sideSample) * sideSample;

        const auto monoSample = right != nullptr
                              ? (leftSample + rightSample) * 0.5f
                              : leftSample;
        pushSpectrumSample (monoSample);
    }

    if (numSamples > 0)
    {
        for (int channel = 0; channel < 2; ++channel)
        {
            const auto rms = static_cast<float> (
                std::sqrt (sumOfSquares[static_cast<size_t> (channel)] / numSamples));

            rmsDb[static_cast<size_t> (channel)].store (
                juce::Decibels::gainToDecibels (rms, silenceDb),
                std::memory_order_relaxed);
            peakDb[static_cast<size_t> (channel)].store (
                juce::Decibels::gainToDecibels (peaks[static_cast<size_t> (channel)], silenceDb),
                std::memory_order_relaxed);
        }

        publishAnalysisFrame (sumOfProducts,
                              sumOfMidSquares,
                              sumOfSideSquares,
                              sumOfSquares,
                              numSamples);
    }
}

void RexMixAudioProcessor::pushSpectrumSample (float sample) noexcept
{
    fftHistory[static_cast<size_t> (fftWritePosition)] = sample;
    fftWritePosition = (fftWritePosition + 1) % fftSize;
    fftSamplesCollected = juce::jmin (fftSamplesCollected + 1, fftSize);

    ++fftSamplesSinceTransform;
    if (fftSamplesSinceTransform >= fftSize / 2)
    {
        fftSamplesSinceTransform = 0;
        if (fftSamplesCollected == fftSize)
            calculateSpectrum();
    }
}

void RexMixAudioProcessor::calculateSpectrum() noexcept
{
    for (int index = 0; index < fftSize; ++index)
    {
        const auto historyIndex = (fftWritePosition + index) % fftSize;
        fftData[static_cast<size_t> (index)] = fftHistory[static_cast<size_t> (historyIndex)];
    }

    window.multiplyWithWindowingTable (fftData.data(), static_cast<size_t> (fftSize));
    std::fill (fftData.begin() + fftSize, fftData.end(), 0.0f);
    fft.performFrequencyOnlyForwardTransform (fftData.data(), true);

    for (int bin = 0; bin < spectrumBinCount; ++bin)
    {
        const auto amplitude = fftData[static_cast<size_t> (bin)] * (2.0f / fftSize);
        spectrumDb[static_cast<size_t> (bin)].store (
            juce::Decibels::gainToDecibels (amplitude, silenceDb),
            std::memory_order_relaxed);
    }
}

void RexMixAudioProcessor::publishAnalysisFrame (double sumOfProducts,
                                                  double sumOfMidSquares,
                                                  double sumOfSideSquares,
                                                  const std::array<double, 2>& sumOfSquares,
                                                  int numSamples) noexcept
{
    if (! nodePublisher.isConnected())
        return;

    rexmix::NodeSlot frame {};
    frame.nodeType = nodePublisher.getNodeType();
    frame.samplePosition = hasSamplePosition() ? getSamplePosition() : -1;
    frame.sampleRate = static_cast<float> (getHostSampleRate());

    for (int channel = 0; channel < 2; ++channel)
    {
        frame.rms[channel] = juce::Decibels::decibelsToGain (
            getRmsDb (channel), silenceDb);
        frame.peak[channel] = juce::Decibels::decibelsToGain (
            getPeakDb (channel), silenceDb);
    }

    if (numSamples > 0 && getMeasuredChannelCount() > 1)
    {
        const auto energyProduct = sumOfSquares[0] * sumOfSquares[1];
        if (energyProduct > 0.0)
        {
            frame.correlation = static_cast<float> (
                juce::jlimit (-1.0, 1.0, sumOfProducts / std::sqrt (energyProduct)));
        }

        const auto midSideEnergy = sumOfMidSquares + sumOfSideSquares;
        if (midSideEnergy > 0.0)
        {
            frame.stereoWidth = static_cast<float> (
                juce::jlimit (0.0, 1.0, sumOfSideSquares / midSideEnergy));
        }
    }

    for (int band = 0; band < static_cast<int> (rexmix::spectrumBinCount); ++band)
    {
        frame.spectrum[band] = juce::Decibels::decibelsToGain (
            getSpectrumDb (spectrumSourceBins[static_cast<size_t> (band)]), silenceDb);
    }

    nodePublisher.publish (frame);
}

void RexMixAudioProcessor::timerCallback()
{
    nodePublisher.tryConnect();
    nodePublisher.retryPendingNodeTypeUpdate();

    const auto timeText = hasPlaybackTime()
                        ? juce::String (getPlaybackTimeSeconds(), 3) + "s"
                        : "unavailable";
    const auto sampleText = hasSamplePosition()
                          ? juce::String (static_cast<juce::int64> (getSamplePosition()))
                          : "unavailable";

    const auto currentTransportState = getTransportState();
    const auto stateText = currentTransportState == TransportState::stopped ? "stopped"
                         : currentTransportState == TransportState::playing ? "playing/analyzing"
                         : "transport unknown/analyzing";

    const auto message = juce::String ("RexMix Node | State: ") + stateText
                       + " | Time: " + timeText
                       + " | Sample: " + sampleText
                       + " | RMS L: " + juce::String (getRmsDb (0), 1) + " dB"
                       + " | RMS R: " + juce::String (getRmsDb (1), 1) + " dB"
                       + " | Peak L: " + juce::String (getPeakDb (0), 1) + " dB"
                       + " | Peak R: " + juce::String (getPeakDb (1), 1) + " dB";

    juce::Logger::writeToLog (message);
}

juce::AudioProcessorEditor* RexMixAudioProcessor::createEditor()
{
    return new RexMixAudioProcessorEditor (*this);
}

bool RexMixAudioProcessor::hasEditor() const
{
    return true;
}

const juce::String RexMixAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool RexMixAudioProcessor::acceptsMidi() const
{
    return false;
}

bool RexMixAudioProcessor::producesMidi() const
{
    return false;
}

bool RexMixAudioProcessor::isMidiEffect() const
{
    return false;
}

double RexMixAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int RexMixAudioProcessor::getNumPrograms()
{
    return 1;
}

int RexMixAudioProcessor::getCurrentProgram()
{
    return 0;
}

void RexMixAudioProcessor::setCurrentProgram (int)
{
}

const juce::String RexMixAudioProcessor::getProgramName (int)
{
    return {};
}

void RexMixAudioProcessor::changeProgramName (int, const juce::String&)
{
}

void RexMixAudioProcessor::getStateInformation (juce::MemoryBlock&)
{
}

void RexMixAudioProcessor::setStateInformation (const void*, int)
{
}

double RexMixAudioProcessor::getHostSampleRate() const noexcept
{
    return hostSampleRate.load (std::memory_order_relaxed);
}

RexMixAudioProcessor::TransportState RexMixAudioProcessor::getTransportState() const noexcept
{
    return transportState.load (std::memory_order_relaxed);
}

double RexMixAudioProcessor::getPlaybackTimeSeconds() const noexcept
{
    return playbackTimeSeconds.load (std::memory_order_relaxed);
}

bool RexMixAudioProcessor::hasPlaybackTime() const noexcept
{
    return playbackTimeAvailable.load (std::memory_order_relaxed);
}

std::int64_t RexMixAudioProcessor::getSamplePosition() const noexcept
{
    return samplePosition.load (std::memory_order_relaxed);
}

bool RexMixAudioProcessor::hasSamplePosition() const noexcept
{
    return samplePositionAvailable.load (std::memory_order_relaxed);
}

int RexMixAudioProcessor::getMeasuredChannelCount() const noexcept
{
    return measuredChannelCount.load (std::memory_order_relaxed);
}

float RexMixAudioProcessor::getRmsDb (int channel) const noexcept
{
    jassert (channel >= 0 && channel < 2);
    return rmsDb[static_cast<size_t> (channel)].load (std::memory_order_relaxed);
}

float RexMixAudioProcessor::getPeakDb (int channel) const noexcept
{
    jassert (channel >= 0 && channel < 2);
    return peakDb[static_cast<size_t> (channel)].load (std::memory_order_relaxed);
}

float RexMixAudioProcessor::getSpectrumDb (int bin) const noexcept
{
    jassert (bin >= 0 && bin < spectrumBinCount);
    return spectrumDb[static_cast<size_t> (bin)].load (std::memory_order_relaxed);
}

bool RexMixAudioProcessor::isMasterConnected() const noexcept
{
    return nodePublisher.isConnected();
}

rexmix::NodeType RexMixAudioProcessor::getAudioType() const noexcept
{
    return nodePublisher.getNodeType();
}

void RexMixAudioProcessor::setAudioType (rexmix::NodeType type) noexcept
{
    nodePublisher.setNodeType (type);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RexMixAudioProcessor();
}
