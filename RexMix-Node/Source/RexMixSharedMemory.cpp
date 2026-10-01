#include "RexMixSharedMemory.h"

#include <Windows.h>
#include <objbase.h>

#include <cstring>

namespace
{
constexpr char rexMixSignature[8] { 'R', 'E', 'X', 'M', 'I', 'X', 'S', 'M' };
constexpr std::uint32_t reservedSlotState = 2;
constexpr std::uint64_t fileTimeToUnixEpochOffset = 116444736000000000ull;

std::uint64_t makeIdentifier() noexcept
{
    GUID guid {};
    if (SUCCEEDED (CoCreateGuid (&guid)))
    {
        std::uint64_t value = 0;
        std::memcpy (&value, &guid, sizeof (value));
        return value != 0 ? value : 1;
    }

    const auto ticks = GetTickCount64();
    const auto process = static_cast<std::uint64_t> (GetCurrentProcessId());
    const auto thread = static_cast<std::uint64_t> (GetCurrentThreadId());
    const auto value = ticks ^ (process << 32) ^ thread;
    return value != 0 ? value : 1;
}

volatile LONG* interlockedLong (std::uint32_t& value) noexcept
{
    return reinterpret_cast<volatile LONG*> (&value);
}

volatile LONG64* interlockedLong64 (std::uint64_t& value) noexcept
{
    return reinterpret_cast<volatile LONG64*> (&value);
}

std::uint64_t getCurrentTimestampNs() noexcept
{
    FILETIME fileTime {};
    GetSystemTimeAsFileTime (&fileTime);

    ULARGE_INTEGER ticks {};
    ticks.LowPart = fileTime.dwLowDateTime;
    ticks.HighPart = fileTime.dwHighDateTime;

    if (ticks.QuadPart < fileTimeToUnixEpochOffset)
        return 0;

    return (ticks.QuadPart - fileTimeToUnixEpochOffset) * 100u;
}

bool acquireSlotWrite (rexmix::NodeSlot& slot, std::uint64_t& evenSequence) noexcept
{
    const auto sequence = static_cast<std::uint64_t> (
        InterlockedCompareExchange64 (interlockedLong64 (slot.sequence), 0, 0));

    if ((sequence & 1u) != 0)
        return false;

    if (InterlockedCompareExchange64 (interlockedLong64 (slot.sequence),
                                      static_cast<LONG64> (sequence + 1),
                                      static_cast<LONG64> (sequence))
        != static_cast<LONG64> (sequence))
        return false;

    evenSequence = sequence + 2;
    return true;
}

void finishSlotWrite (rexmix::NodeSlot& slot, std::uint64_t evenSequence) noexcept
{
    MemoryBarrier();
    InterlockedExchange64 (interlockedLong64 (slot.sequence),
                           static_cast<LONG64> (evenSequence));
}
}

namespace rexmix
{
NodePublisher::NodePublisher (const wchar_t* requestedName) noexcept
    : nodeId (makeIdentifier()), sessionId (makeIdentifier())
{
    if (requestedName == nullptr)
        return;

    std::size_t index = 0;
    while (requestedName[index] != L'\0' && index < objectName.size() - 1)
    {
        objectName[index] = requestedName[index];
        ++index;
    }

    if (requestedName[index] != L'\0')
        objectName.fill (L'\0');
}

NodePublisher::~NodePublisher()
{
    disconnect();
}

bool NodePublisher::tryConnect() noexcept
{
    if (isConnected())
        return true;

    if (region.load (std::memory_order_acquire) == nullptr)
    {
        if (objectName[0] == L'\0')
            return false;

        const auto mapping = OpenFileMappingW (FILE_MAP_ALL_ACCESS, FALSE, objectName.data());
        if (mapping == nullptr)
            return false;

        const auto mappedView = static_cast<SharedMemoryRegion*> (
            MapViewOfFile (mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof (SharedMemoryRegion)));

        if (mappedView == nullptr)
        {
            CloseHandle (mapping);
            return false;
        }

        mappingHandle = mapping;
        region.store (mappedView, std::memory_order_release);

        if (! validateHeader())
        {
            disconnect();
            return false;
        }
    }

    return claimSlot();
}

bool NodePublisher::isConnected() const noexcept
{
    return region.load (std::memory_order_acquire) != nullptr
        && slotIndex.load (std::memory_order_acquire) < maxNodes;
}

std::uint32_t NodePublisher::getSlotIndex() const noexcept
{
    return slotIndex.load (std::memory_order_acquire);
}

std::uint64_t NodePublisher::getNodeId() const noexcept
{
    return nodeId;
}

std::uint64_t NodePublisher::getSessionId() const noexcept
{
    return sessionId;
}

bool NodePublisher::publish (const NodeSlot& frame) noexcept
{
    if (! isConnected())
        return false;

    auto* mappedRegion = region.load (std::memory_order_acquire);
    const auto currentSlot = slotIndex.load (std::memory_order_acquire);
    if (mappedRegion == nullptr || currentSlot >= maxNodes)
        return false;

    auto& destination = mappedRegion->nodes[currentSlot];
    std::uint64_t evenSequence = 0;
    if (! acquireSlotWrite (destination, evenSequence))
        return false;

    destination.nodeType = frame.nodeType;
    destination.nodeId = nodeId;
    destination.registrationSessionId = sessionId;
    destination.samplePosition = frame.samplePosition;
    destination.timestampNs = frame.timestampNs != 0 ? frame.timestampNs : getCurrentTimestampNs();
    destination.frameSequence = ++frameSequence;
    destination.sampleRate = frame.sampleRate;
    destination.rms[0] = frame.rms[0];
    destination.rms[1] = frame.rms[1];
    destination.peak[0] = frame.peak[0];
    destination.peak[1] = frame.peak[1];
    destination.stereoWidth = frame.stereoWidth;
    destination.correlation = frame.correlation;
    destination.pitchHz = frame.pitchHz;
    destination.transientStrength = frame.transientStrength;
    std::memcpy (destination.spectrum, frame.spectrum, sizeof (destination.spectrum));
    destination.reserved[0] = 0;
    destination.reserved[1] = 0;
    finishSlotWrite (destination, evenSequence);
    return true;
}

void NodePublisher::disconnect() noexcept
{
    const auto currentSlot = slotIndex.exchange (maxNodes, std::memory_order_acq_rel);
    auto* mappedRegion = region.exchange (nullptr, std::memory_order_acq_rel);

    if (mappedRegion != nullptr && currentSlot < maxNodes)
    {
        auto& slot = mappedRegion->nodes[currentSlot];
        const auto previousActive = static_cast<std::uint32_t> (
            InterlockedExchange (interlockedLong (slot.active), 0));

        if (previousActive == 1)
            InterlockedDecrement (interlockedLong (mappedRegion->header.activeNodeCount));
    }

    if (mappedRegion != nullptr)
    {
        UnmapViewOfFile (mappedRegion);
    }

    if (mappingHandle != nullptr)
    {
        CloseHandle (static_cast<HANDLE> (mappingHandle));
        mappingHandle = nullptr;
    }
}

bool NodePublisher::validateHeader() const noexcept
{
    auto* mappedRegion = region.load (std::memory_order_acquire);
    if (mappedRegion == nullptr)
        return false;

    SharedMemoryHeader header {};
    std::memcpy (&header, &mappedRegion->header, sizeof (header));

    return std::memcmp (header.magic, rexMixSignature, sizeof (rexMixSignature)) == 0
        && header.protocolVersion == protocolVersion
        && header.structureVersion == structureVersion
        && header.headerSize == sizeof (SharedMemoryHeader)
        && header.totalSize == sizeof (SharedMemoryRegion)
        && header.maxNodes == maxNodes
        && header.validState == validState;
}

bool NodePublisher::claimSlot() noexcept
{
    auto* mappedRegion = region.load (std::memory_order_acquire);
    if (mappedRegion == nullptr || ! validateHeader())
        return false;

    for (std::uint32_t index = 0; index < maxNodes; ++index)
    {
        auto& slot = mappedRegion->nodes[index];
        std::uint64_t evenSequence = 0;

        if (! acquireSlotWrite (slot, evenSequence))
            continue;

        if (slot.active != 0)
        {
            finishSlotWrite (slot, evenSequence);
            continue;
        }

        slot.active = reservedSlotState;
        slot.nodeType = NodeType::audio;
        slot.nodeId = nodeId;
        slot.registrationSessionId = sessionId;
        slot.samplePosition = -1;
        slot.timestampNs = getCurrentTimestampNs();
        slot.frameSequence = 0;
        slot.sampleRate = 0.0f;
        slot.rms[0] = 0.0f;
        slot.rms[1] = 0.0f;
        slot.peak[0] = 0.0f;
        slot.peak[1] = 0.0f;
        slot.stereoWidth = 0.0f;
        slot.correlation = 0.0f;
        slot.pitchHz = 0.0f;
        slot.transientStrength = 0.0f;
        std::memset (slot.spectrum, 0, sizeof (slot.spectrum));
        slot.reserved[0] = 0;
        slot.reserved[1] = 0;
        slot.active = 1;
        finishSlotWrite (slot, evenSequence);
        slotIndex.store (index, std::memory_order_release);
        InterlockedIncrement (interlockedLong (mappedRegion->header.activeNodeCount));
        return true;
    }

    return false;
}
}
