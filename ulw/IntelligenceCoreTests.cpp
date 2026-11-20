#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "IntelligenceCore.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

using ulw::IntelligenceCore;
using ulw::Response;

void Check(bool value, const char* reason) {
    if (!value) {
        throw std::runtime_error(reason);
    }
}

void TestDecisions() {
    IntelligenceCore core;
    for (unsigned i = 0; i < IntelligenceCore::DecisionsPerFrame; ++i) {
        Check(core.TakeBudget(42), "expected a budget slot");
    }
    Check(!core.TakeBudget(42), "frame budget exceeded");
    Check(core.TakeBudget(43), "budget did not reset next frame");

    const ulw::Point source{12.0f, 19.0f, 5.0f};
    Check(core.ReactToShot({0x101, 1000, source, 16.0f, false}).response == Response::Panic, "a nearby sound should cause panic");
    Check(core.ReactToShot({0x201, 1000, source, 43.0f * 43.0f, true}).response == Response::Vanilla, "far events should stay native");
    Check(core.ReactToShot({0x201, 1000, source, std::numeric_limits<float>::quiet_NaN(), true}).response == Response::Vanilla,
        "invalid coordinates should stay native");

    std::uint32_t cautious{};
    std::uint32_t nextLifetime{};
    for (std::uint32_t slot = 1; slot < 128 && !cautious; ++slot) {
        const auto before = (slot << 8) + 1;
        const auto after = (slot << 8) + 2;
        core.Reset();
        const auto first = core.ReactToShot({before, 1000, source, 900.0f, false});
        core.Reset();
        const auto second = core.ReactToShot({after, 1000, source, 900.0f, false});
        if (first.response == Response::Look && second.response == Response::Look) {
            cautious = before;
            nextLifetime = after;
        }
    }
    Check(cautious != 0, "expected differing civilian temperaments");
    core.Reset();
    const auto look = core.ReactToShot({cautious, 1000, source, 900.0f, false});
    Check(look.response == Response::Look && look.focus.x == source.x && look.focus.y == source.y, "the civilian should face the sound");
    Check(core.ReactToShot({cautious, 1200, source, 900.0f, false}).response == Response::Vanilla, "repeat cooldown failed");
    Check(core.ReactToShot({cautious, 1550, source, 900.0f, false}).response == Response::Flee, "repeated threat should escalate");
    Check(core.ReactToShot({nextLifetime, 1600, source, 900.0f, false}).response == Response::Look, "reused slot inherited old memory");
    Check(core.ReactToShot({nextLifetime, 4601, source, 900.0f, false}).response == Response::Look, "old event memory did not expire");
}

void TestLoader(const std::filesystem::path& asi) {
    const auto logPath = asi.parent_path() / L"UltimateLivingWorld.log";
    std::error_code error;
    std::filesystem::remove(logPath, error);
    Check(!error, "could not clear the test log");
    auto* const module = LoadLibraryW(asi.c_str());
    Check(module != nullptr, "could not load UltimateLivingWorld.asi in a test process");
    using Initialize = void(__cdecl*)();
    const auto address = GetProcAddress(module, "InitializeASI");
    Check(address != nullptr, "the ASI loader entry point is missing");
    static_assert(sizeof(address) == sizeof(Initialize));
    Initialize initialize{};
    std::memcpy(&initialize, &address, sizeof(initialize));
    initialize();
    FreeLibrary(module);
    std::ifstream log{logPath};
    const std::string text{std::istreambuf_iterator<char>{log}, std::istreambuf_iterator<char>{}};
    Check(text.find("does not match the supported Hoodlum") != std::string::npos,
        "a non-game process should reject the hook before touching any game address");
}

}

int wmain(int argc, wchar_t** argv) {
    try {
        TestDecisions();
        std::puts("Core: frame budget, perception, memory, slot reuse and reactions passed.");
        if (argc == 2) {
            TestLoader(argv[1]);
            std::puts("ASI: load and InitializeASI passed. The non-game process was rejected.");
        }
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
