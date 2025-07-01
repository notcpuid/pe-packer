#include <Windows.h>
#include <random>

inline int random_value(int from, int to) {
    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_int_distribution<std::mt19937::result_type> dist6(from, to);

    return dist6(rng);
}