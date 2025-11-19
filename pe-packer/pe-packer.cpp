#include <windows.h>
#include <shellapi.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "core/core.hpp"
#include "gui/gui_app.hpp"
#include "utils/utils.hpp"

c_core* packer = nullptr;

// using only on cli mode
void ensure_cli() {
    if (GetConsoleWindow() != nullptr) {
        return;
    }

    auto hook_streams = []() {
        FILE* fp = nullptr;
        freopen_s(&fp, "CONOUT$", "w", stdout);
        freopen_s(&fp, "CONOUT$", "w", stderr);
        freopen_s(&fp, "CONIN$", "r", stdin);
    };

    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        hook_streams();
        return;
    }

    if (AllocConsole()) {
        hook_streams();
    }
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    int argc = 0;
    LPWSTR* argvWide = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argvWide) {
        return run_gui();
    }

    std::vector<std::string> args;
    args.reserve(argc);
    for (int i = 0; i < argc; ++i) {
        int requiredSize = WideCharToMultiByte(CP_UTF8, 0, argvWide[i], -1, nullptr, 0, nullptr, nullptr);
        std::string utf8(requiredSize, '\0');
        WideCharToMultiByte(CP_UTF8, 0, argvWide[i], -1, utf8.data(), requiredSize, nullptr, nullptr);
        utf8.pop_back();
        args.push_back(std::move(utf8));
    }
    LocalFree(argvWide);

    bool runGui = args.size() < 4;
    if (!runGui) {
        for (size_t i = 1; i < args.size(); ++i) {
            if (args[i] == "--gui") {
                runGui = true;
                break;
            }
        }
    }

    if (runGui) {
        HWND console = GetConsoleWindow();
        if (console) {
            ShowWindow(console, SW_HIDE);
            FreeConsole();
        }
        return run_gui();
    }

    auto argv = std::make_unique<char*[]>(args.size());
    for (size_t i = 0; i < args.size(); ++i) {
        argv[i] = args[i].data();
    }

    arguments::init(static_cast<int>(args.size()), argv.get());

    ensure_cli();
    enable_virtual_terminal_processing();

    time_t ctime = 0;
    time(&ctime);
    srand(static_cast<unsigned>(ctime));

    uint32_t mut_count = static_cast<uint32_t>(atoi(argv[3])) * 10;

    try
    {
        auto packer = std::make_unique<c_core>(argv[1], argv[2], mut_count);

        print_info("Mutations count: %i\n", mut_count);
        packer->process();
    }
    catch(const std::exception& ex)
    {
        std::stringstream ss;
        ss << "[ " << COLOR_RED << "error" << COLOR_RESET << " ] " << ex.what();
        std::cerr << ss.str();

        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

