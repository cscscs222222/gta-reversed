#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "IntelligenceCore.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

namespace {

static_assert(sizeof(void*) == 4, "The GTA San Andreas adapter requires an x86 build");
static_assert(sizeof(ulw::Point) == 12);

constexpr std::uintptr_t ImageBase = 0x400000;
constexpr std::uintptr_t ShotDispatchCall = 0x4C39AC;
constexpr std::uintptr_t NativeShotResponse = 0x4BC710;
constexpr std::uintptr_t PedPool = 0xB74490;
constexpr std::uintptr_t TaskPool = 0xB744A8;
constexpr std::uintptr_t ScriptSpace = 0xA49960;
constexpr std::uintptr_t MissionFlagOffset = 0xA476AC;
constexpr std::size_t MainScriptLimit = 200000;

using ShotResponse = void(__thiscall*)(void* handler, const void* shot, void* activeTask, void* simplestTask);
using PedReference = int(__cdecl*)(const void* ped);
using LineClear = bool(__cdecl*)(const ulw::Point&, const ulw::Point&, bool, bool, bool, bool, bool, bool, bool);
using AllocateTask = void*(__cdecl*)(std::size_t);
using TurnToPoint = void*(__thiscall*)(void*, const ulw::Point&, float, float);
using FleePoint = void*(__thiscall*)(void*, const ulw::Point&, bool, float, int);

ulw::IntelligenceCore s_Core{};
std::filesystem::path s_LogPath;
std::array<unsigned, 4> s_Reactions{};
unsigned s_AllocationFailures{};
std::uint32_t s_LastLogTime{};
std::uint32_t s_LastGameTime{};
bool s_Diagnostics{};
bool s_WasActive{};

template<typename T>
T Read(const void* object, std::size_t offset = 0) noexcept {
    T value{};
    std::memcpy(&value, static_cast<const std::byte*>(object) + offset, sizeof(value));
    return value;
}

template<typename T>
T GameValue(std::uintptr_t address) noexcept {
    return Read<T>(reinterpret_cast<const void*>(address));
}

template<typename T>
T GameFunction(std::uintptr_t address) noexcept {
    return reinterpret_cast<T>(address);
}

void Log(const char* message) noexcept {
    if (s_LogPath.empty()) {
        OutputDebugStringA(message);
        return;
    }
    std::FILE* log = nullptr;
    if (_wfopen_s(&log, s_LogPath.c_str(), L"a") == 0) {
        std::fprintf(log, "%s\n", message);
        std::fclose(log);
    }
}

bool Matches(std::uintptr_t address, const auto& bytes) noexcept {
    MEMORY_BASIC_INFORMATION page{};
    if (!VirtualQuery(reinterpret_cast<void*>(address), &page, sizeof(page)) ||
        page.State != MEM_COMMIT || page.AllocationBase != reinterpret_cast<void*>(ImageBase) ||
        (page.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0 ||
        address + bytes.size() > reinterpret_cast<std::uintptr_t>(page.BaseAddress) + page.RegionSize) {
        return false;
    }
    return std::memcmp(reinterpret_cast<const void*>(address), bytes.data(), bytes.size()) == 0;
}

bool SupportedImage() noexcept {
    const auto* const image = reinterpret_cast<const std::byte*>(GetModuleHandleW(nullptr));
    if (reinterpret_cast<std::uintptr_t>(image) != ImageBase) {
        return false;
    }
    const auto dos = Read<IMAGE_DOS_HEADER>(image);
    if (dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew < 0x40 || dos.e_lfanew > 0x400) {
        return false;
    }
    const auto header = Read<IMAGE_NT_HEADERS32>(image, dos.e_lfanew);
    return header.Signature == IMAGE_NT_SIGNATURE &&
        header.FileHeader.Machine == IMAGE_FILE_MACHINE_I386 &&
        header.FileHeader.TimeDateStamp == 0x427101CA &&
        header.OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC &&
        header.OptionalHeader.AddressOfEntryPoint == 0x424570 &&
        header.OptionalHeader.SizeOfImage == 0x1177000;
}

bool FreeRoam() noexcept {
    if (GameValue<std::uint8_t>(0xA444B1) || GameValue<std::uint8_t>(0xB5F852) ||
        GameValue<std::uint8_t>(0xB7CB48) || GameValue<std::uint8_t>(0xB7CB49)) {
        return false;
    }
    const auto offset = GameValue<std::uint32_t>(MissionFlagOffset);
    return offset != 0 && offset <= MainScriptLimit - sizeof(std::uint32_t) &&
        GameValue<std::uint32_t>(ScriptSpace + offset) == 0;
}

ulw::Point PedPosition(const void* ped) noexcept {
    const auto* const matrix = Read<const void*>(ped, 0x14);
    return matrix ? Read<ulw::Point>(matrix, 0x30) : Read<ulw::Point>(ped, 0x04);
}

void* ChooseReaction(const void* handler, const void* shot) noexcept {
    if (!FreeRoam()) {
        if (s_WasActive) {
            s_Core.Reset();
            s_WasActive = false;
        }
        return nullptr;
    }
    const auto now = GameValue<std::uint32_t>(0xB7CB84);
    if (now < s_LastGameTime) {
        s_Core.Reset();
        s_LastLogTime = now;
    }
    s_WasActive = true;
    s_LastGameTime = now;

    const auto* const ped = Read<const void*>(handler);
    const auto* const shooter = Read<const void*>(shot, 0x14);
    if (!ped || !shooter || shooter == ped || Read<std::uint8_t>(shot, 0x30)
        || Read<std::uint8_t>(ped, 0x484) != 1 || Read<std::uint8_t>(ped, 0x2F) != 0
        || Read<std::uint8_t>(shooter, 0x2F) != 0 || (Read<std::uint8_t>(ped, 0x46D) & 0x07)
        || !(Read<float>(ped, 0x540) > 0.0f)) {
        return nullptr;
    }
    const auto type = Read<std::uint32_t>(ped, 0x598);
    const auto selectedTask = Read<std::int16_t>(shot, 0x0E);
    // Preserve dispatch for armed responses, crime handling and other
    // special decisions. These are NONE, SMART_FLEE_ENTITY and TURN.
    if ((type != 4 && type != 5) || (selectedTask != 200 && selectedTask != 911 && selectedTask != 920)) {
        return nullptr;
    }
    const auto position = PedPosition(ped);
    const auto source = Read<ulw::Point>(shot, 0x18);
    const auto dx = source.x - position.x;
    const auto dy = source.y - position.y;
    const auto dz = source.z - position.z;
    const auto distance = dx * dx + dy * dy + dz * dz;
    if (!(distance <= 42.0f * 42.0f) || std::abs(dz) > 14.0f ||
        !s_Core.TakeBudget(GameValue<std::uint32_t>(0xB7CB4C))) {
        return nullptr;
    }
    if (!GameValue<void*>(PedPool) || !GameValue<void*>(TaskPool)) {
        return nullptr;
    }
    const auto reference = GameFunction<PedReference>(0x54FF60)(ped);
    if (reference < 0 || (reference & 0x80) != 0) {
        return nullptr;
    }

    const auto heading = Read<float>(ped, 0x558);
    const auto dot = -std::sin(heading) * dx + std::cos(heading) * dy;
    const bool facing = dot > 0.0f && dot * dot >= 0.35f * (dx * dx + dy * dy);
    const ulw::Point eye{position.x, position.y, position.z + 0.65f};
    const auto lineClear = GameFunction<LineClear>(0x56A490);
    const bool visible = facing && lineClear(eye, source, true, true, false, true, true, false, false);
    const auto decision = s_Core.ReactToShot({static_cast<std::uint32_t>(reference), now, source, distance, visible});
    if (decision.response == ulw::Response::Vanilla) {
        return nullptr;
    }
    const bool look = decision.response == ulw::Response::Look;
    auto* const task = GameFunction<AllocateTask>(0x61A5A0)(look ? 0x28 : 0x44);
    if (!task) {
        ++s_AllocationFailures;
        return nullptr;
    }
    ++s_Reactions[static_cast<unsigned>(decision.response)];
    if (look) {
        return GameFunction<TurnToPoint>(0x66B910)(task, decision.focus, 0.5f, 0.2f);
    }
    const bool panic = decision.response == ulw::Response::Panic;
    return GameFunction<FleePoint>(0x65BD20)(task, decision.focus, panic, panic ? 30.0f : 22.0f, panic ? 4800 : 2900);
}

void __fastcall OnShot(void* handler, void*, const void* shot, void* activeTask, void* simplestTask) noexcept {
    auto* const task = ChooseReaction(handler, shot);
    if (task) {
        // The native dispatcher takes ownership through this response slot.
        std::memcpy(static_cast<std::byte*>(handler) + 0x24, &task, sizeof(task));
    } else {
        GameFunction<ShotResponse>(NativeShotResponse)(handler, shot, activeTask, simplestTask);
    }
    if (s_Diagnostics && s_LastGameTime - s_LastLogTime >= 30000) {
        char message[128]{};
        std::snprintf(message, sizeof(message), "Look=%u; Flee=%u; Panic=%u; native task pool exhausted=%u",
            s_Reactions[1], s_Reactions[2], s_Reactions[3], s_AllocationFailures);
        Log(message);
        s_LastLogTime = s_LastGameTime;
    }
}

bool Install() noexcept {
    if (!SupportedImage()) {
        Log("Skipped: this executable does not match the supported Hoodlum 1.0 US layout.");
        return false;
    }
    // Check the entire call context and each native entry used by the adapter.
    using Entry = std::array<std::uint8_t, 8>;
    constexpr std::array<std::uint8_t, 15> dispatch{
        0x55, 0x53, 0x57, 0x8B, 0xCE, 0xE8, 0x5F, 0x8D, 0xFF, 0xFF, 0xE9, 0xFE, 0x02, 0x00, 0x00,
    };
    if (!Matches(0x4C39A7, dispatch)
        || !Matches(NativeShotResponse, Entry{0x64, 0xA1, 0, 0, 0, 0, 0x6A, 0xFF})
        || !Matches(0x4BCB3A, std::array<std::uint8_t, 3>{0x89, 0x47, 0x24})
        || !Matches(0x54FF60, Entry{0x8B, 0x4C, 0x24, 0x04, 0x56, 0x8B, 0x35, 0x90})
        || !Matches(0x56A490, Entry{0x83, 0xEC, 0x5C, 0x66, 0x81, 0x3D, 0x78, 0xCD})
        || !Matches(0x61A5A0, Entry{0x8B, 0x0D, 0xA8, 0x44, 0xB7, 0x00, 0xE9, 0x55})
        || !Matches(0x65BD20, Entry{0x56, 0x8B, 0xF1, 0xE8, 0x88, 0xE6, 0xFB, 0xFF})
        || !Matches(0x66B910, Entry{0x56, 0x8B, 0xF1, 0xE8, 0x98, 0xEA, 0xFA, 0xFF})) {
        Log("Skipped: the gunshot dispatcher or a native function differs from the verified executable.");
        return false;
    }
    if (GameValue<void*>(PedPool)) {
        Log("Skipped: the ASI loader initialized after game pool creation.");
        return false;
    }

    // The loader calls this before gameplay. Pin the image so the
    // dispatcher can never retain an address into an unloaded DLL.
    HMODULE pinned{};
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCWSTR>(&OnShot), &pinned)) {
        Log("Skipped: could not pin the module.");
        return false;
    }
    auto* const operand = reinterpret_cast<void*>(ShotDispatchCall + 1);
    const auto displacement = reinterpret_cast<uintptr_t>(&OnShot) - (ShotDispatchCall + 5);
    DWORD oldProtection{};
    if (!VirtualProtect(operand, sizeof(displacement), PAGE_EXECUTE_READWRITE, &oldProtection)) {
        Log("Skipped: the game code page could not be made writable.");
        return false;
    }
    std::memcpy(operand, &displacement, sizeof(displacement));
    DWORD unused{};
    const bool restored = VirtualProtect(operand, sizeof(displacement), oldProtection, &unused) != 0;
    const bool flushed = FlushInstructionCache(GetCurrentProcess(), operand, sizeof(displacement)) != 0;
    if (!restored || !flushed) {
        Log("Warning: Windows did not confirm code protection or instruction cache refresh.");
    }
    return true;
}

}

extern "C" __declspec(dllexport) void __cdecl InitializeASI() noexcept {
    static bool initialized{};
    if (initialized) {
        return;
    }
    initialized = true;
    try {
        HMODULE module{};
        wchar_t path[MAX_PATH]{};
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&InitializeASI), &module)) {
            return;
        }
        const auto length = GetModuleFileNameW(module, path, MAX_PATH);
        if (!length || length >= MAX_PATH) {
            return;
        }
        const auto directory = std::filesystem::path(path).parent_path();
        const auto config = directory / L"UltimateLivingWorld.ini";
        s_LogPath = directory / L"UltimateLivingWorld.log";
        if (!GetPrivateProfileIntW(L"General", L"Enabled", 1, config.c_str())) {
            Log("Disabled in UltimateLivingWorld.ini.");
            return;
        }
        s_Diagnostics = GetPrivateProfileIntW(L"General", L"Diagnostics", 1, config.c_str()) != 0;
        if (Install()) {
            Log("Ultimate Living World 0.1.0: civilian gunshot event adapter installed.");
        }
    } catch (...) {
        Log("Initialization failed while preparing the configuration or log.");
    }
}
