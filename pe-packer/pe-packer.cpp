#include "core/core.hpp"
#include "utils/utils.hpp"

c_core* packer = nullptr;

int main(int argc, char* argv[])
{
    if (argc < 4) {
        print_error("Invalid arguments");
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

    return 0;
}
