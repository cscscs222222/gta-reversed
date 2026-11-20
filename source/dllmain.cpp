#include "StdInc.h"
#include <cstdlib>
#include <app_debug.h>
#include "extensions/CommandLine.h"
#include "extensions/debug.hpp"
#include "extensions/Configuration.hpp"
#include "InjectHooksMain.h"
#include "TaskComplexKillPedOnFootArmed.h"
#include "TaskSimpleGunControl.h"

static constexpr auto DEFAULT_INI_FILENAME = "gta-reversed.ini";

#include "extensions/Configs/FastLoader.hpp"
#include "extensions/Configs/Miscellaneous.hpp"
#include "dllmain.h"

HMODULE s_HandleOfDLL{};
static bool s_EnemyAIOnly{};

static const IMAGE_NT_HEADERS32& GetExecutableHeader(const std::byte* executable) {
    const auto& dosHeader = *reinterpret_cast<const IMAGE_DOS_HEADER*>(executable);
    return *reinterpret_cast<const IMAGE_NT_HEADERS32*>(executable + dosHeader.e_lfanew);
}

static bool HasHoodlumSection(const IMAGE_NT_HEADERS32& header) {
    const auto* const sections = IMAGE_FIRST_SECTION(&header);
    return std::ranges::any_of(std::span{ sections, header.FileHeader.NumberOfSections }, [](const auto& section) {
        return std::memcmp(section.Name, ".HOODLUM", IMAGE_SIZEOF_SHORT_NAME) == 0;
    });
}

static bool HasSupportedHoodlumLayout(const void* executable, const IMAGE_NT_HEADERS32& header) {
    return reinterpret_cast<uintptr_t>(executable) == 0x400000 && header.FileHeader.Machine == IMAGE_FILE_MACHINE_I386
        && header.FileHeader.TimeDateStamp == 0x427101CA && header.OptionalHeader.AddressOfEntryPoint == 0x424570
        && header.OptionalHeader.SizeOfImage == 0x1177000;
}

bool notsa::IsHoodlumExecutable() {
    static const bool isHoodlum = HasHoodlumSection(GetExecutableHeader(reinterpret_cast<const std::byte*>(GetModuleHandleW(nullptr))));
    return isHoodlum;
}

bool notsa::IsGameHeapReady() {
    if (!IsHoodlumExecutable()) {
        return true;
    }
    static const bool supported = [] {
        const auto* const executable = reinterpret_cast<const std::byte*>(GetModuleHandleW(nullptr));
        return HasSupportedHoodlumLayout(executable, GetExecutableHeader(executable));
    }();
    // The Hoodlum executable's C runtime initializes this handle before using its heap.
    return supported && *reinterpret_cast<void* const volatile*>(0xC9C2F4) != nullptr;
}

static bool CanInstallHoodlumEnemyAI(const void* executable, const IMAGE_NT_HEADERS32& header) {
    if (!HasSupportedHoodlumLayout(executable, header)) {
        return false;
    }

    // Check the Hoodlum 1.0 US entries before writing any hooks. Other ASIs may have patched them already.
    struct HookSite {
        uintptr_t Address;
        std::array<uint8, 8> Bytes;
    };
    static constexpr HookSite s_Sites[]{
        { 0x621500, { 0x83, 0xEC, 0x18, 0x53, 0x55, 0x56, 0x57, 0x8B } }, // LineOfSightClearForAttack
        { 0x62BF00, { 0x6A, 0xFF, 0x64, 0xA1, 0x00, 0x00, 0x00, 0x00 } }, // CreateFirstSubTask
        { 0x62CCE0, { 0x83, 0xEC, 0x30, 0x53, 0x55, 0x56, 0x57, 0x8B } }, // ControlSubTask
        { 0x625270, { 0x6A, 0xFF, 0x64, 0xA1, 0x00, 0x00, 0x00, 0x00 } }, // ProcessPed
    };
    const auto* const armedVtable = reinterpret_cast<const uintptr_t*>(0x86D918);
    const auto* const gunVtable = reinterpret_cast<const uintptr_t*>(0x86D76C);
    return armedVtable[9] == 0x62BF00 && armedVtable[10] == 0x62CCE0 && gunVtable[7] == 0x625270
        && std::ranges::all_of(s_Sites, [](const auto& site) {
            return std::memcmp(reinterpret_cast<const void*>(site.Address), site.Bytes.data(), site.Bytes.size()) == 0;
        });
}

// Ultimate ASI Loader calls this after LoadLibrary returns, outside the Windows loader lock.
extern "C" __declspec(dllexport) void InitializeASI() {
    if (!s_EnemyAIOnly || ReversibleHooks::RHManager::HasInstance()) {
        return;
    }

    std::ofstream status{ "gta_reversed_hoodlum.log" };
    const auto* const executable = reinterpret_cast<const std::byte*>(GetModuleHandleW(nullptr));
    if (!CanInstallHoodlumEnemyAI(executable, GetExecutableHeader(executable))) {
        status << "Hoodlum enemy AI was skipped because the executable or hook entries do not match.\n";
        return;
    }

    try {
        ReversibleHooks::RHManager::CreateInstance();
        CTaskComplexKillPedOnFootArmed::InjectHooks(true);
        CTaskSimpleGunControl::InjectHooks(true);
        status << "Hoodlum enemy AI hooks installed.\n";
    } catch (const std::exception& error) {
        ReversibleHooks::RHManager::DestroyInstance();
        status << "Failed to install Hoodlum enemy AI hooks: " << error.what() << '\n';
    }
}

void LoadConfigurations() {
    // Firstly load the INI into the memory.
    g_ConfigurationMgr.Load(DEFAULT_INI_FILENAME);

    // Then load all specific configurations.
    g_FastLoaderConfig.Load();
    g_MiscConfig.Load();
    // ...
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH: {
        s_HandleOfDLL = hModule;
        s_EnemyAIOnly = notsa::IsHoodlumExecutable();
        if (s_EnemyAIOnly) {
            break;
        }

        // Fail if RenderWare has already been started
        if (*(RwCamera**)0xC1703C) {
            MessageBox(NULL, "gta_reversed failed to load (RenderWare has already been started)", "Error", MB_ICONERROR | MB_OK);
            return FALSE;
        }

        std::setlocale(LC_ALL, "en_US.UTF-8");

        notsa::debug::DisplayConsole();
        notsa::debug::LoadSymbols(); // Used by logging
        notsa::Logging::CreateInstance();

        CommandLine::Load(__argc, __argv);
        if (CommandLine::s_WaitForDebugger) {
            notsa::debug::WaitForDebugger();
        }
        LoadConfigurations();

        ReversibleHooks::RHManager::CreateInstance();

        InjectHooksMain();

        break;
    }
    case DLL_PROCESS_DETACH: {
        if (s_EnemyAIOnly) {
            if (!lpReserved) {
                ReversibleHooks::RHManager::DestroyInstance();
            }
            break;
        }
        if (!notsa::Logging::HasInstance()) {
            break;
        }
        if (lpReserved == nullptr) {
            NOTSA_LOG_INFO("DLL_PROCESS_DETACH: Shutting down normally...");

            ReversibleHooks::RHManager::DestroyInstance();
            notsa::Logging::DestroyInstance();
            notsa::debug::UnloadSymbols();
        } else { // This is pretty much the only thing that's ever reached
            NOTSA_LOG_INFO("DLL_PROCESS_DETACH: Process is terminating, shutting down only what's necessary");

            notsa::Logging::DestroyInstance();
        }
        break;
    }
    case DLL_THREAD_ATTACH:
        break;
    case DLL_THREAD_DETACH:
        break;
    }
    return TRUE;
}

HMODULE notsa::GetDLLHandle() {
    return s_HandleOfDLL;
}
