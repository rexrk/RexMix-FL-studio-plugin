#include "RexMixSharedMemory.h"

#include <Windows.h>

#include <array>
#include <iostream>
#include <string>

namespace
{
bool check (bool condition, const char* message)
{
    if (! condition)
        std::cerr << "FAIL: " << message << '\n';

    return condition;
}

class TestNodeWriter final
{
public:
    explicit TestNodeWriter (const std::wstring& name)
    {
        handle = OpenFileMappingW (FILE_MAP_ALL_ACCESS, FALSE, name.c_str());
        if (handle != nullptr)
            region = static_cast<rexmix::SharedMemoryRegion*> (
                MapViewOfFile (handle, FILE_MAP_ALL_ACCESS, 0, 0,
                               sizeof (rexmix::SharedMemoryRegion)));
    }

    ~TestNodeWriter()
    {
        if (region != nullptr)
            UnmapViewOfFile (region);
        if (handle != nullptr)
            CloseHandle (handle);
    }

    TestNodeWriter (const TestNodeWriter&) = delete;
    TestNodeWriter& operator= (const TestNodeWriter&) = delete;

    bool isReady() const noexcept { return region != nullptr; }

    void publish (std::uint32_t index,
                  rexmix::NodeType type,
                  std::uint64_t nodeId,
                  std::int64_t samplePosition,
                  std::uint64_t frameSequence) noexcept
    {
        auto& slot = region->nodes[index];
        auto* sequence = reinterpret_cast<volatile LONG64*> (&slot.sequence);
        InterlockedExchange64 (sequence, static_cast<LONG64> (frameSequence * 2 - 1));
        slot.active = 1;
        slot.nodeType = type;
        slot.nodeId = nodeId;
        slot.samplePosition = samplePosition;
        slot.frameSequence = frameSequence;
        MemoryBarrier();
        InterlockedExchange64 (sequence, static_cast<LONG64> (frameSequence * 2));
    }

private:
    HANDLE handle = nullptr;
    rexmix::SharedMemoryRegion* region = nullptr;
};
}

int main()
{
    const auto testName = std::wstring (L"Local\\RexMix_SharedMemoryTest_")
                        + std::to_wstring (GetCurrentProcessId())
                        + L"_"
                        + std::to_wstring (GetTickCount64());

    bool passed = true;

    {
        rexmix::SharedMemory first (testName.c_str());
        rexmix::SharedMemory second (testName.c_str());
        rexmix::SharedMemoryHeader header {};

        passed &= check (first.isReady(), "first mapping initializes");
        passed &= check (second.isReady(), "second mapping attaches");
        passed &= check (first.getHeaderSnapshot (header), "header validates");
        passed &= check (header.protocolVersion == rexmix::protocolVersion,
                         "protocol version is initialized");
        passed &= check (header.structureVersion == rexmix::structureVersion,
                         "structure version is initialized");
        passed &= check (header.maxNodes == rexmix::maxNodes,
                         "maximum node count is fixed at 64");
        passed &= check (header.totalSize == sizeof (rexmix::SharedMemoryRegion),
                         "header reports the expected region size");
        passed &= check (header.reserved[rexmix::masterPresenceReservedIndex] == 1,
                         "Master marks its shared-memory presence");

        std::array<rexmix::NodeSlot, rexmix::maxNodes> nodes {};
        for (std::uint32_t index = 0; index < rexmix::maxNodes; ++index)
        {
            passed &= check (first.tryReadNodeSlot (index, nodes[index]),
                             "initialized node slot is readable");
            passed &= check (nodes[index].active == 0,
                             "node slots start inactive");
        }

        passed &= check (second.getHeaderSnapshot (header),
                         "second instance observes the same initialized region");

        TestNodeWriter nodeWriter (testName);
        passed &= check (nodeWriter.isReady(), "test Nodes can publish into the Master mapping");
        if (nodeWriter.isReady())
        {
            nodeWriter.publish (0, rexmix::NodeType::kick, 0x101, 44100, 3);
            nodeWriter.publish (1, rexmix::NodeType::bass808, 0x202, 88200, 7);

            rexmix::NodeSlot firstNode {};
            rexmix::NodeSlot secondNode {};
            passed &= check (first.tryReadNodeSlot (0, firstNode)
                                 && firstNode.active == 1
                                 && firstNode.nodeType == rexmix::NodeType::kick
                                 && firstNode.samplePosition == 44100,
                             "Master reads the first Node's Audio Type and sample position");
            passed &= check (first.tryReadNodeSlot (1, secondNode)
                                 && secondNode.active == 1
                                 && secondNode.nodeType == rexmix::NodeType::bass808
                                 && secondNode.samplePosition == 88200,
                             "Master reads a second Node independently");
            passed &= check (firstNode.nodeId == 0x101 && firstNode.frameSequence == 3
                                 && secondNode.nodeId == 0x202 && secondNode.frameSequence == 7,
                             "Node IDs and frame sequences remain available internally");

            nodeWriter.publish (0, rexmix::NodeType::snare, 0x101, 132300, 4);
            passed &= check (first.tryReadNodeSlot (0, firstNode)
                                 && firstNode.nodeType == rexmix::NodeType::snare
                                 && firstNode.samplePosition == 132300,
                             "Master observes a live Audio Type and sample-position update");
        }
    }

    {
        const auto presenceName = testName + L"_presence";
        HANDLE keepAlive = nullptr;
        rexmix::SharedMemoryRegion* mappedRegion = nullptr;
        {
            rexmix::SharedMemory owner (presenceName.c_str());
            passed &= check (owner.isReady(), "presence-test Master owns its mapping");
            keepAlive = OpenFileMappingW (FILE_MAP_ALL_ACCESS, FALSE, presenceName.c_str());
            if (keepAlive != nullptr)
                mappedRegion = static_cast<rexmix::SharedMemoryRegion*> (
                    MapViewOfFile (keepAlive, FILE_MAP_ALL_ACCESS, 0, 0,
                                   sizeof (rexmix::SharedMemoryRegion)));
            passed &= check (mappedRegion != nullptr,
                             "presence-test observer keeps the mapping available");
            if (mappedRegion != nullptr)
                passed &= check (mappedRegion->header.reserved[
                                     rexmix::masterPresenceReservedIndex] == 1,
                                 "Master presence is set while its mapping is owned");
        }

        if (mappedRegion != nullptr)
            passed &= check (mappedRegion->header.reserved[
                                 rexmix::masterPresenceReservedIndex] == 0,
                             "Master clears presence on normal destruction");

        if (mappedRegion != nullptr)
            UnmapViewOfFile (mappedRegion);
        if (keepAlive != nullptr)
            CloseHandle (keepAlive);
    }

    {
        rexmix::SharedMemory recreated (testName.c_str());
        rexmix::SharedMemoryHeader header {};
        rexmix::NodeSlot firstSlot {};
        passed &= check (recreated.isReady(), "mapping recreates after all handles close");
        passed &= check (recreated.getHeaderSnapshot (header),
                         "recreated header validates");
        passed &= check (recreated.tryReadNodeSlot (0, firstSlot)
                             && firstSlot.active == 0,
                         "recreated mapping initializes an empty registry");
    }

    if (! passed)
        return 1;

    std::cout << "Shared-memory initialization, attach, layout, Node type/sample-position reads, "
                 "live updates, empty registry, and release passed. "
              << "Region size: " << sizeof (rexmix::SharedMemoryRegion) << " bytes.\n";
    return 0;
}
