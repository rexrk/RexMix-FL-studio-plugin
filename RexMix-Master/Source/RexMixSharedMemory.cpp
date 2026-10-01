#include "RexMixSharedMemory.h"

#include <Windows.h>

#include <algorithm>
#include <cstring>

namespace
{
constexpr char rexMixSignature[8] { 'R', 'E', 'X', 'M', 'I', 'X', 'S', 'M' };

class ScopedHandle final
{
public:
    explicit ScopedHandle (HANDLE value = nullptr) noexcept : handle (value) {}
    ~ScopedHandle()
    {
        if (handle != nullptr)
            CloseHandle (handle);
    }

    ScopedHandle (const ScopedHandle&) = delete;
    ScopedHandle& operator= (const ScopedHandle&) = delete;

    HANDLE get() const noexcept { return handle; }

private:
    HANDLE handle;
};

class ScopedMutexLock final
{
public:
    explicit ScopedMutexLock (HANDLE mutex) noexcept
        : handle (mutex), result (WaitForSingleObject (mutex, INFINITE))
    {
    }

    ~ScopedMutexLock()
    {
        if (ownsMutex())
            ReleaseMutex (handle);
    }

    ScopedMutexLock (const ScopedMutexLock&) = delete;
    ScopedMutexLock& operator= (const ScopedMutexLock&) = delete;

    bool ownsMutex() const noexcept
    {
        return result == WAIT_OBJECT_0 || result == WAIT_ABANDONED;
    }

    DWORD getResult() const noexcept { return result; }

private:
    HANDLE handle;
    DWORD result;
};

class ScopedView final
{
public:
    explicit ScopedView (void* value = nullptr) noexcept : view (value) {}
    ~ScopedView()
    {
        if (view != nullptr)
            UnmapViewOfFile (view);
    }

    ScopedView (const ScopedView&) = delete;
    ScopedView& operator= (const ScopedView&) = delete;

    void* get() const noexcept { return view; }
    void* release() noexcept
    {
        auto* result = view;
        view = nullptr;
        return result;
    }

private:
    void* view;
};

bool isAllZero (const rexmix::SharedMemoryRegion& value) noexcept
{
    const auto* bytes = reinterpret_cast<const unsigned char*> (&value);
    return std::all_of (bytes, bytes + sizeof (value), [] (unsigned char byte)
    {
        return byte == 0;
    });
}

void initializeRegion (rexmix::SharedMemoryRegion& value) noexcept
{
    std::memset (&value, 0, sizeof (value));
    std::memcpy (value.header.magic, rexMixSignature, sizeof (rexMixSignature));
    value.header.protocolVersion = rexmix::protocolVersion;
    value.header.structureVersion = rexmix::structureVersion;
    value.header.headerSize = sizeof (rexmix::SharedMemoryHeader);
    value.header.totalSize = sizeof (rexmix::SharedMemoryRegion);
    value.header.maxNodes = rexmix::maxNodes;
    MemoryBarrier();
    value.header.validState = rexmix::validState;
}

bool hasExpectedLayout (const rexmix::SharedMemoryHeader& header) noexcept
{
    return std::memcmp (header.magic, rexMixSignature, sizeof (rexMixSignature)) == 0
        && header.protocolVersion == rexmix::protocolVersion
        && header.structureVersion == rexmix::structureVersion
        && header.headerSize == sizeof (rexmix::SharedMemoryHeader)
        && header.totalSize == sizeof (rexmix::SharedMemoryRegion)
        && header.maxNodes == rexmix::maxNodes
        && header.validState == rexmix::validState;
}

std::uint64_t readSequence (const std::uint64_t& sequence) noexcept
{
    const auto* interlockedSequence = reinterpret_cast<const volatile LONG64*> (&sequence);
    return static_cast<std::uint64_t> (
        InterlockedCompareExchange64 (const_cast<volatile LONG64*> (interlockedSequence), 0, 0));
}
}

namespace rexmix
{
SharedMemory::SharedMemory (const wchar_t* requestedName)
{
    if (requestedName == nullptr || requestedName[0] == L'\0')
    {
        errorCode = ERROR_INVALID_PARAMETER;
        return;
    }

    objectName = requestedName;
    openOrInitialize();
}

SharedMemory::~SharedMemory()
{
    if (region != nullptr)
        UnmapViewOfFile (region);

    if (mappingHandle != nullptr)
        CloseHandle (static_cast<HANDLE> (mappingHandle));
}

bool SharedMemory::isReady() const noexcept
{
    return region != nullptr && errorCode == ERROR_SUCCESS;
}

std::uint32_t SharedMemory::getErrorCode() const noexcept
{
    return errorCode;
}

const std::wstring& SharedMemory::getObjectName() const noexcept
{
    return objectName;
}

bool SharedMemory::getHeaderSnapshot (SharedMemoryHeader& destination) const noexcept
{
    if (! isReady())
        return false;

    std::memcpy (&destination, &region->header, sizeof (destination));
    return hasExpectedLayout (destination);
}

bool SharedMemory::tryReadNodeSlot (std::uint32_t index, NodeSlot& destination) const noexcept
{
    if (! isReady() || index >= maxNodes)
        return false;

    const auto& source = region->nodes[index];
    const auto before = readSequence (source.sequence);

    if ((before & 1u) != 0)
        return false;

    std::memcpy (&destination, &source, sizeof (destination));
    MemoryBarrier();
    const auto after = readSequence (source.sequence);

    return before == after && (after & 1u) == 0;
}

void SharedMemory::openOrInitialize()
{
    const auto regionSize = static_cast<std::uint64_t> (sizeof (SharedMemoryRegion));
    const auto mapping = CreateFileMappingW (INVALID_HANDLE_VALUE,
                                             nullptr,
                                             PAGE_READWRITE,
                                             static_cast<DWORD> (regionSize >> 32),
                                             static_cast<DWORD> (regionSize & 0xffffffffu),
                                             objectName.c_str());

    if (mapping == nullptr)
    {
        errorCode = GetLastError();
        return;
    }

    mappingHandle = mapping;
    const auto createdNewMapping = GetLastError() != ERROR_ALREADY_EXISTS;

    auto mutexName = objectName + L"_Init";
    ScopedHandle initializationMutex (CreateMutexW (nullptr, FALSE, mutexName.c_str()));
    if (initializationMutex.get() == nullptr)
    {
        errorCode = GetLastError();
        return;
    }

    ScopedMutexLock initializationLock (initializationMutex.get());
    if (! initializationLock.ownsMutex())
    {
        errorCode = initializationLock.getResult() == WAIT_FAILED
                  ? GetLastError()
                  : ERROR_GEN_FAILURE;
        return;
    }

    ScopedView mappedView (MapViewOfFile (mapping,
                                          FILE_MAP_ALL_ACCESS,
                                          0,
                                          0,
                                          sizeof (SharedMemoryRegion)));

    if (mappedView.get() == nullptr)
    {
        errorCode = GetLastError();
        return;
    }

    auto* mappedRegion = static_cast<SharedMemoryRegion*> (mappedView.get());

    if (createdNewMapping || isAllZero (*mappedRegion))
    {
        initializeRegion (*mappedRegion);
    }
    else if (! hasExpectedLayout (mappedRegion->header))
    {
        errorCode = ERROR_REVISION_MISMATCH;
        return;
    }

    region = static_cast<SharedMemoryRegion*> (mappedView.release());
}
}
