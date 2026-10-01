#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

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

class NodePublisher final
{
public:
    explicit NodePublisher (const wchar_t* objectName = sharedMemoryName) noexcept;
    ~NodePublisher();

    NodePublisher (const NodePublisher&) = delete;
    NodePublisher& operator= (const NodePublisher&) = delete;

    bool tryConnect() noexcept;
    bool isConnected() const noexcept;
    std::uint32_t getSlotIndex() const noexcept;
    std::uint64_t getNodeId() const noexcept;
    std::uint64_t getSessionId() const noexcept;

    bool publish (const NodeSlot& frame) noexcept;
    void setNodeType (NodeType type) noexcept;
    void retryPendingNodeTypeUpdate() noexcept;
    NodeType getNodeType() const noexcept;
    void disconnect() noexcept;

private:
    bool validateHeader() const noexcept;
    bool claimSlot() noexcept;
    bool masterIsPresent (const SharedMemoryRegion& mappedRegion) const noexcept;
    void cleanupAttachment() noexcept;

    void* mappingHandle = nullptr;
    std::array<wchar_t, 128> objectName {};
    std::atomic<SharedMemoryRegion*> region { nullptr };
    std::atomic<std::uint32_t> slotIndex { maxNodes };
    std::atomic<std::uint32_t> activePublishers { 0 };
    std::atomic<bool> connected { false };
    std::atomic<NodeType> nodeType { NodeType::other };
    std::atomic<bool> nodeTypeUpdatePending { false };
    std::uint64_t nodeId = 0;
    std::uint64_t sessionId = 0;
    std::uint64_t frameSequence = 0;
};
}
