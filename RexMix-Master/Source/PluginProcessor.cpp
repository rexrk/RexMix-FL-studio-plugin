#include "PluginProcessor.h"

#include "PluginEditor.h"

RexMixMasterAudioProcessor::RexMixMasterAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    startTimerHz (20);
}

RexMixMasterAudioProcessor::~RexMixMasterAudioProcessor()
{
    stopTimer();
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

    rexmix::HostTransportSnapshot snapshot;
    snapshot.sampleRate = getSampleRate();

    if (auto* hostPlayHead = getPlayHead())
    {
        if (const auto position = hostPlayHead->getPosition())
        {
            snapshot.playing = position->getIsPlaying();
            if (const auto ppq = position->getPpqPosition(); ppq.hasValue())
            {
                snapshot.ppqAvailable = true;
                snapshot.ppqPosition = *ppq;
            }

            if (const auto sample = position->getTimeInSamples(); sample.hasValue())
            {
                snapshot.samplePositionAvailable = true;
                snapshot.samplePosition = *sample;
            }

            if (const auto seconds = position->getTimeInSeconds(); seconds.hasValue())
            {
                snapshot.timeSecondsAvailable = true;
                snapshot.timeSeconds = *seconds;
            }

            if (const auto bpm = position->getBpm(); bpm.hasValue())
                snapshot.bpm = *bpm;

            if (const auto signature = position->getTimeSignature(); signature.hasValue())
            {
                snapshot.timeSignatureNumerator = signature->numerator;
                snapshot.timeSignatureDenominator = signature->denominator;
            }
        }
    }

    transportSnapshotSequence.fetch_add (1, std::memory_order_acq_rel);
    const auto wasPlaying = previousAudioThreadPlaying.exchange (
        snapshot.playing, std::memory_order_relaxed);
    if (snapshot.playing && ! wasPlaying)
    {
        const auto newGeneration = playbackGeneration.fetch_add (
            1, std::memory_order_relaxed) + 1;
        playbackStartGeneration.store (newGeneration, std::memory_order_relaxed);
        startPpqAvailable.store (snapshot.ppqAvailable, std::memory_order_relaxed);
        startPpqPosition.store (snapshot.ppqPosition, std::memory_order_relaxed);
        startSamplePositionAvailable.store (
            snapshot.samplePositionAvailable, std::memory_order_relaxed);
        startSamplePosition.store (snapshot.samplePosition, std::memory_order_relaxed);
        startTimeSecondsAvailable.store (snapshot.timeSecondsAvailable,
                                         std::memory_order_relaxed);
        startTimeSeconds.store (snapshot.timeSeconds, std::memory_order_relaxed);
        startSampleRate.store (snapshot.sampleRate, std::memory_order_relaxed);
        startBpm.store (snapshot.bpm, std::memory_order_relaxed);
        startTimeSignatureNumerator.store (snapshot.timeSignatureNumerator,
                                           std::memory_order_relaxed);
        startTimeSignatureDenominator.store (snapshot.timeSignatureDenominator,
                                             std::memory_order_relaxed);
    }
    transportPlaying.store (snapshot.playing, std::memory_order_relaxed);
    ppqAvailable.store (snapshot.ppqAvailable, std::memory_order_relaxed);
    ppqPosition.store (snapshot.ppqPosition, std::memory_order_relaxed);
    samplePositionAvailable.store (snapshot.samplePositionAvailable, std::memory_order_relaxed);
    samplePosition.store (snapshot.samplePosition, std::memory_order_relaxed);
    timeSecondsAvailable.store (snapshot.timeSecondsAvailable, std::memory_order_relaxed);
    timeSeconds.store (snapshot.timeSeconds, std::memory_order_relaxed);
    hostSampleRate.store (snapshot.sampleRate, std::memory_order_relaxed);
    hostBpm.store (snapshot.bpm, std::memory_order_relaxed);
    timeSignatureNumerator.store (snapshot.timeSignatureNumerator, std::memory_order_relaxed);
    timeSignatureDenominator.store (snapshot.timeSignatureDenominator, std::memory_order_relaxed);
    transportSnapshotSequence.fetch_add (1, std::memory_order_release);
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

const rexmix::CaptureSession& RexMixMasterAudioProcessor::getCaptureSession() const noexcept
{
    return captureSession;
}

void RexMixMasterAudioProcessor::analyzeMix() noexcept
{
    captureSession.analyze();
}

bool RexMixMasterAudioProcessor::readTransportSnapshot (
    rexmix::HostTransportSnapshot& snapshot) const noexcept
{
    for (int attempt = 0; attempt < 3; ++attempt)
    {
        const auto before = transportSnapshotSequence.load (std::memory_order_acquire);
        if ((before & 1u) != 0)
            continue;

        snapshot.playing = transportPlaying.load (std::memory_order_relaxed);
        snapshot.playbackGeneration = playbackGeneration.load (std::memory_order_relaxed);
        snapshot.ppqAvailable = ppqAvailable.load (std::memory_order_relaxed);
        snapshot.ppqPosition = ppqPosition.load (std::memory_order_relaxed);
        snapshot.samplePositionAvailable = samplePositionAvailable.load (std::memory_order_relaxed);
        snapshot.samplePosition = samplePosition.load (std::memory_order_relaxed);
        snapshot.timeSecondsAvailable = timeSecondsAvailable.load (std::memory_order_relaxed);
        snapshot.timeSeconds = timeSeconds.load (std::memory_order_relaxed);
        snapshot.sampleRate = hostSampleRate.load (std::memory_order_relaxed);
        snapshot.bpm = hostBpm.load (std::memory_order_relaxed);
        snapshot.timeSignatureNumerator = timeSignatureNumerator.load (
            std::memory_order_relaxed);
        snapshot.timeSignatureDenominator = timeSignatureDenominator.load (
            std::memory_order_relaxed);
        snapshot.playbackStartGeneration = playbackStartGeneration.load (
            std::memory_order_relaxed);
        snapshot.startPpqAvailable = startPpqAvailable.load (std::memory_order_relaxed);
        snapshot.startPpqPosition = startPpqPosition.load (std::memory_order_relaxed);
        snapshot.startSamplePositionAvailable = startSamplePositionAvailable.load (
            std::memory_order_relaxed);
        snapshot.startSamplePosition = startSamplePosition.load (std::memory_order_relaxed);
        snapshot.startTimeSecondsAvailable = startTimeSecondsAvailable.load (
            std::memory_order_relaxed);
        snapshot.startTimeSeconds = startTimeSeconds.load (std::memory_order_relaxed);
        snapshot.startSampleRate = startSampleRate.load (std::memory_order_relaxed);
        snapshot.startBpm = startBpm.load (std::memory_order_relaxed);
        snapshot.startTimeSignatureNumerator = startTimeSignatureNumerator.load (
            std::memory_order_relaxed);
        snapshot.startTimeSignatureDenominator = startTimeSignatureDenominator.load (
            std::memory_order_relaxed);

        if (transportSnapshotSequence.load (std::memory_order_acquire) == before)
            return true;
    }

    return false;
}

void RexMixMasterAudioProcessor::timerCallback()
{
    rexmix::HostTransportSnapshot transport;
    if (! readTransportSnapshot (transport))
        return;

    const auto wasCapturing = captureSession.getState() == rexmix::CaptureState::capturing;
    captureSession.updateTransport (transport);
    const auto captureState = captureSession.getState();
    const auto shouldPollFrames = captureState == rexmix::CaptureState::capturing
                               || (wasCapturing
                                   && captureState == rexmix::CaptureState::partial);
    if (! shouldPollFrames)
        return;

    for (std::uint32_t index = 0; index < rexmix::maxNodes; ++index)
    {
        rexmix::NodeSlot frame {};
        if (sharedMemory.tryReadNodeSlot (index, frame))
            captureSession.appendNodeFrame (index, frame);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RexMixMasterAudioProcessor();
}
