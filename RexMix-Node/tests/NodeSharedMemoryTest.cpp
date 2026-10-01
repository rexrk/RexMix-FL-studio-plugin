#include "RexMixSharedMemory.h"

#include <Windows.h>

#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

namespace
{
bool check (bool condition, const char* message)
{
    if (! condition)
        std::cerr << "FAIL: " << message << '\n';

    return condition;
}

class MasterMappingFixture final
{
public:
    explicit MasterMappingFixture (const std::wstring& name)
    {
        const auto regionSize = static_cast<std::uint64_t> (sizeof (rexmix::SharedMemoryRegion));
        handle = CreateFileMappingW (INVALID_HANDLE_VALUE,
                                     nullptr,
                                     PAGE_READWRITE,
                                     static_cast<DWORD> (regionSize >> 32),
                                     static_cast<DWORD> (regionSize & 0xffffffffu),
                                     name.c_str());
        if (handle == nullptr)
            return;

        region = static_cast<rexmix::SharedMemoryRegion*> (
            MapViewOfFile (handle, FILE_MAP_ALL_ACCESS, 0, 0, sizeof (rexmix::SharedMemoryRegion)));
        if (region == nullptr)
            return;

        std::memset (region, 0, sizeof (*region));
        constexpr char signature[8] { 'R', 'E', 'X', 'M', 'I', 'X', 'S', 'M' };
        std::memcpy (region->header.magic, signature, sizeof (signature));
        region->header.protocolVersion = rexmix::protocolVersion;
        region->header.structureVersion = rexmix::structureVersion;
        region->header.headerSize = sizeof (rexmix::SharedMemoryHeader);
        region->header.totalSize = sizeof (rexmix::SharedMemoryRegion);
        region->header.maxNodes = rexmix::maxNodes;
        region->header.validState = rexmix::validState;
        region->header.reserved[rexmix::masterPresenceReservedIndex] = 1;
        MemoryBarrier();
    }

    ~MasterMappingFixture()
    {
        if (region != nullptr)
            UnmapViewOfFile (region);

        if (handle != nullptr)
            CloseHandle (handle);
    }

    MasterMappingFixture (const MasterMappingFixture&) = delete;
    MasterMappingFixture& operator= (const MasterMappingFixture&) = delete;

    bool isReady() const noexcept { return handle != nullptr && region != nullptr; }
    rexmix::SharedMemoryRegion* get() const noexcept { return region; }

private:
    HANDLE handle = nullptr;
    rexmix::SharedMemoryRegion* region = nullptr;
};

std::wstring uniqueName()
{
    return L"Local\\RexMix_NodePublisherTest_"
         + std::to_wstring (GetCurrentProcessId())
         + L"_"
         + std::to_wstring (GetTickCount64());
}
}

int main()
{
    bool passed = true;
    const auto missingName = uniqueName() + L"_missing";
    {
        rexmix::NodePublisher withoutMaster (missingName.c_str());
        passed &= check (! withoutMaster.tryConnect(),
                         "Node does not connect to a missing Master mapping");
        passed &= check (! withoutMaster.isConnected(),
                         "a missing Master is reported as not connected");

        const auto mapping = OpenFileMappingW (FILE_MAP_READ, FALSE, missingName.c_str());
        passed &= check (mapping == nullptr,
                         "Node does not create a missing shared-memory mapping");
        if (mapping != nullptr)
            CloseHandle (mapping);
    }

    const auto mappingName = uniqueName();
    {
        MasterMappingFixture master (mappingName);
        passed &= check (master.isReady(), "test Master creates a compatible mapping");
        if (! master.isReady())
            return 1;
        passed &= check (master.get()->header.reserved[
                             rexmix::masterPresenceReservedIndex] == 1,
                         "test Master announces its presence");

        rexmix::NodePublisher first (mappingName.c_str());
        rexmix::NodePublisher second (mappingName.c_str());
        passed &= check (first.tryConnect(), "first Node attaches and claims a slot");
        passed &= check (second.tryConnect(), "second Node attaches and claims a slot");
        passed &= check (first.isConnected() && second.isConnected(),
                         "available Master is reported as connected");
        passed &= check (first.getSlotIndex() != second.getSlotIndex(),
                         "multiple Nodes claim different slots");
        passed &= check (master.get()->header.activeNodeCount == 2,
                         "active-node count reflects both instances");

        const auto firstSlotIndex = first.getSlotIndex();
        const auto secondSlotIndex = second.getSlotIndex();
        passed &= check (master.get()->nodes[firstSlotIndex].nodeType == rexmix::NodeType::other
                             && master.get()->nodes[secondSlotIndex].nodeType == rexmix::NodeType::other,
                         "new Node instances register with Other as the default type");

        master.get()->nodes[firstSlotIndex].sequence = 1;
        first.setNodeType (rexmix::NodeType::kick);
        passed &= check (master.get()->nodes[firstSlotIndex].nodeType == rexmix::NodeType::other,
                         "type update does not wait for a slot already being written");
        master.get()->nodes[firstSlotIndex].sequence = 2;
        first.retryPendingNodeTypeUpdate();
        passed &= check (master.get()->nodes[firstSlotIndex].nodeType == rexmix::NodeType::kick,
                         "pending type update succeeds on a later non-blocking attempt");

        const std::array<rexmix::NodeType, 4> selectedTypes
        {
            rexmix::NodeType::kick,
            rexmix::NodeType::bass808,
            rexmix::NodeType::electricGuitar,
            rexmix::NodeType::leadVocal
        };

        for (const auto selectedType : selectedTypes)
        {
            const auto sequenceBeforeTypeChange = master.get()->nodes[firstSlotIndex].frameSequence;
            first.setNodeType (selectedType);
            passed &= check (master.get()->nodes[firstSlotIndex].nodeType == selectedType,
                             "audio type changes publish immediately to the existing slot");
            passed &= check (master.get()->nodes[firstSlotIndex].frameSequence
                                 == sequenceBeforeTypeChange,
                             "type updates do not alter the analysis frame sequence");
        }

        second.setNodeType (rexmix::NodeType::subBass);
        passed &= check (master.get()->nodes[secondSlotIndex].nodeType == rexmix::NodeType::subBass
                             && master.get()->nodes[firstSlotIndex].nodeType
                                 == rexmix::NodeType::leadVocal,
                         "Node instances keep independent audio type selections");

        rexmix::NodeSlot frame {};
        frame.nodeType = rexmix::NodeType::audio;
        frame.samplePosition = 123456;
        frame.timestampNs = 987654321000ull;
        frame.sampleRate = 48000.0f;
        frame.rms[0] = 0.25f;
        frame.rms[1] = 0.20f;
        frame.peak[0] = 0.8f;
        frame.peak[1] = 0.7f;
        frame.stereoWidth = 0.35f;
        frame.correlation = 0.92f;
        frame.pitchHz = 440.0f;
        frame.transientStrength = 0.12f;
        for (std::uint32_t bin = 0; bin < rexmix::spectrumBinCount; ++bin)
            frame.spectrum[bin] = static_cast<float> (bin) / rexmix::spectrumBinCount;

        passed &= check (first.publish (frame), "first Node publishes an analysis frame");
        passed &= check (first.publish (frame), "first Node publishes a subsequent frame");
        passed &= check (second.publish (frame), "second Node publishes independently");

        const auto& firstSlot = master.get()->nodes[firstSlotIndex];
        const auto& secondSlot = master.get()->nodes[secondSlotIndex];
        passed &= check (firstSlot.active == 1 && secondSlot.active == 1,
                         "published slots are active");
        passed &= check (firstSlot.nodeId == first.getNodeId()
                             && secondSlot.nodeId == second.getNodeId(),
                         "slot Node IDs match registering instances");
        passed &= check (firstSlot.nodeType == rexmix::NodeType::leadVocal
                             && secondSlot.nodeType == rexmix::NodeType::subBass,
                         "published frames preserve each Node's selected audio type");
        passed &= check (firstSlot.registrationSessionId == first.getSessionId(),
                         "registration session ID is published");
        passed &= check (firstSlot.frameSequence == 2 && secondSlot.frameSequence == 1,
                         "per-instance frame sequence increases monotonically");
        passed &= check (firstSlot.samplePosition == frame.samplePosition
                             && firstSlot.timestampNs == frame.timestampNs,
                         "sample position and timestamp are published");
        passed &= check (firstSlot.rms[0] == frame.rms[0]
                             && firstSlot.peak[1] == frame.peak[1]
                             && firstSlot.spectrum[63] == frame.spectrum[63],
                         "analysis data matches the shared slot layout");

        first.disconnect();
        passed &= check (master.get()->header.activeNodeCount == 1,
                         "disconnect decrements active count once");
        passed &= check (master.get()->nodes[firstSlotIndex].active == 0,
                         "disconnect releases the first Node slot");
        passed &= check (master.get()->nodes[secondSlotIndex].active == 1,
                         "disconnect leaves the other Node registered");

        InterlockedExchange (reinterpret_cast<volatile LONG*> (
                                 &master.get()->header.reserved[
                                     rexmix::masterPresenceReservedIndex]), 0);
        passed &= check (! first.tryConnect() && ! first.isConnected(),
                         "Master presence clearing disconnects the first Node");
        passed &= check (! second.tryConnect() && ! second.isConnected(),
                         "Master presence clearing disconnects the second Node");
        passed &= check (! first.publish (frame) && ! second.publish (frame),
                         "disconnected Nodes stop shared-memory publication");
        passed &= check (master.get()->header.activeNodeCount == 0,
                         "each disconnected Node releases only its own slot");

        InterlockedExchange (reinterpret_cast<volatile LONG*> (
                                 &master.get()->header.reserved[
                                     rexmix::masterPresenceReservedIndex]), 1);
        passed &= check (first.tryConnect() && first.isConnected(),
                         "Node reconnects when Master presence returns");
        passed &= check (second.tryConnect() && second.isConnected(),
                         "second Node reconnects independently");
        passed &= check (first.getSlotIndex() != second.getSlotIndex()
                             && master.get()->header.activeNodeCount == 2,
                         "reconnected Nodes claim separate active slots");
    }

    {
        MasterMappingFixture master (mappingName);
        passed &= check (master.isReady(), "second test Master creates a fresh mapping");
        if (! master.isReady())
            return 1;

        constexpr std::size_t concurrentNodeCount = 8;
        std::array<std::unique_ptr<rexmix::NodePublisher>, concurrentNodeCount> nodes;
        std::array<bool, concurrentNodeCount> connected {};
        std::array<std::thread, concurrentNodeCount> threads;

        for (std::size_t index = 0; index < concurrentNodeCount; ++index)
            nodes[index] = std::make_unique<rexmix::NodePublisher> (mappingName.c_str());

        for (std::size_t index = 0; index < concurrentNodeCount; ++index)
        {
            threads[index] = std::thread ([&, index]
            {
                connected[index] = nodes[index]->tryConnect();
            });
        }

        for (auto& thread : threads)
            thread.join();

        std::array<bool, rexmix::maxNodes> occupiedSlots {};
        for (std::size_t index = 0; index < concurrentNodeCount; ++index)
        {
            passed &= check (connected[index], "concurrent Node claims a slot");
            if (connected[index])
            {
                const auto slot = nodes[index]->getSlotIndex();
                passed &= check (! occupiedSlots[slot], "concurrent claims use unique slots");
                occupiedSlots[slot] = true;
            }
        }

        passed &= check (master.get()->header.activeNodeCount == concurrentNodeCount,
                         "concurrent registration maintains the active count");
    }

    if (! passed)
        return 1;

    std::cout << "Node open-only mapping, multi-slot registration, frame publication, "
                 "monotonic sequence, and slot release passed. Protocol region: "
              << sizeof (rexmix::SharedMemoryRegion) << " bytes.\n";
    return 0;
}
