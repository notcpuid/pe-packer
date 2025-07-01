#include "core/core.hpp"
c_core* packer = nullptr;

void enable_virtual_terminal_processing() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;

    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) return;

    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
}

int main(int argc, char* argv[])
{
    if (argc < 4) {
        print_error("invalid arguments");
        return EXIT_FAILURE;
    }

    arguments::init(argc, argv);

    enable_virtual_terminal_processing();

    time_t ctime = 0;
    time(&ctime);
    srand(static_cast<unsigned>(ctime));

    uint32_t mut_count = static_cast<uint32_t>(atoi(argv[3])) * 10;

    try
    {
        auto packer = std::make_unique<c_core>(argv[1], argv[2], mut_count);

        print_info("mutations count: %i\n", mut_count);
        packer->process();
    }
    catch(const std::exception& ex)
    {
		print_error("Exception: %s\n", ex.what());

        return EXIT_FAILURE;
    }

    return 0;
}
