#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace rexmix
{
inline constexpr wchar_t sharedMemoryName[] = L"Local\\RexMix_SharedMemory_v1";
inline constexpr std::uint32_t maxNodes = 64;
inline constexpr std::uint32_t spectrumBinCount = 64;
inline constexpr std::uint16_t protocolVersion = 1;
inline constexpr std::uint16_t structureVersion = 1;
inline constexpr std::uint32_t validState = 0x524D584D;
inline constexpr std::size_t masterPresenceReservedIndex = 0;

enum class NodeType : std::uint32_t
{
    unknown = 0,
    audio = 1,
    instrument = 2,
    midi = 3,
    kick = 100,
    snare = 101,
    hiHat = 102,
    clap = 103,
    tom = 104,
    percussion = 105,
    bass808 = 106,
    synthBass = 107,
    bassGuitar = 108,
    subBass = 109,
    piano = 110,
    guitar = 111,
    acousticGuitar = 112,
    electricGuitar = 113,
    synth = 114,
    lead = 115,
    pad = 116,
    strings = 117,
    keys = 118,
    leadVocal = 119,
    backingVocal = 120,
    vocalChop = 121,
    spoken = 122,
    impact = 123,
    riser = 124,
    sweep = 125,
    texture = 126,
    fxOther = 127,
    ambience = 128,
    other = 129
};

struct alignas(8) SharedMemoryHeader
{
    char magic[8];
    std::uint16_t protocolVersion;
    std::uint16_t structureVersion;
    std::uint32_t headerSize;
    std::uint32_t totalSize;
    std::uint32_t maxNodes;
    std::uint32_t activeNodeCount;
    std::uint32_t validState;
    std::uint32_t reserved[8];
};

struct alignas(8) NodeSlot
{
    std::uint64_t sequence;
    std::uint32_t active;
    NodeType nodeType;
    std::uint64_t nodeId;
    std::uint64_t registrationSessionId;
    std::int64_t samplePosition;
    std::uint64_t timestampNs;
    std::uint64_t frameSequence;
    float sampleRate;
    float rms[2];
    float peak[2];
    float stereoWidth;
    float correlation;
    float pitchHz;
    float transientStrength;
    float spectrum[spectrumBinCount];
    std::uint32_t reserved[2];
};

struct alignas(8) SharedMemoryRegion
{
    SharedMemoryHeader header;
    NodeSlot nodes[maxNodes];
};

static_assert (sizeof (SharedMemoryHeader) == 64);
static_assert (sizeof (NodeSlot) == 360);
static_assert (sizeof (SharedMemoryRegion) == 23104);
static_assert (offsetof (SharedMemoryRegion, nodes) == sizeof (SharedMemoryHeader));

class SharedMemory final
{
public:
    explicit SharedMemory (const wchar_t* objectName = sharedMemoryName);
    ~SharedMemory();

    SharedMemory (const SharedMemory&) = delete;
    SharedMemory& operator= (const SharedMemory&) = delete;

    bool isReady() const noexcept;
    std::uint32_t getErrorCode() const noexcept;
    const std::wstring& getObjectName() const noexcept;

    bool getHeaderSnapshot (SharedMemoryHeader& destination) const noexcept;
    bool tryReadNodeSlot (std::uint32_t index, NodeSlot& destination) const noexcept;

private:
    void openOrInitialize();

    std::wstring objectName;
    void* mappingHandle = nullptr;
    SharedMemoryRegion* region = nullptr;
    std::uint32_t errorCode = 0;
};
}
