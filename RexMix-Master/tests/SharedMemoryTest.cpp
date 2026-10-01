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

    std::cout << "Shared-memory initialization, attach, layout, empty registry, and release passed. "
              << "Region size: " << sizeof (rexmix::SharedMemoryRegion) << " bytes.\n";
    return 0;
}
