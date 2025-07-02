#pragma once
#include <iostream>

#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_CYAN    "\033[36m"

//#define print_warning(fmt, ...)      printf("[ " COLOR_YELLOW "warning" COLOR_RESET " ] " fmt, ##__VA_ARGS__)
//#define print_info(fmt, ...)         printf("[ " COLOR_CYAN   "info"    COLOR_RESET " ] " fmt, ##__VA_ARGS__)
//#define print_custom(fmt, mdl, ...)  printf("[ " COLOR_GREEN  "%s"      COLOR_RESET " ] " mdl, fmt, ##__VA_ARGS__)

inline void print_custom(const char* mdl, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("[ " COLOR_GREEN "%s" COLOR_RESET " ] ", mdl);
    vprintf(fmt, args);
    va_end(args);
}

inline void print_warning(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("[ " COLOR_YELLOW "info" COLOR_RESET " ] ");
    vprintf(fmt, args);
    va_end(args);
}

inline void print_info(const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	printf("[ " COLOR_CYAN "info" COLOR_RESET " ] ");
	vprintf(fmt, args);
	va_end(args);
}

[[noreturn]] inline void print_error(const std::string& msg) {
    std::stringstream ss;
    ss << msg;
    throw std::runtime_error(ss.str());
}

#define error_handling(condition, from, text) \
    try { \
        if (condition) { \
            throw std::exception(text); \
        } \
    } catch (std::exception& ex) { \
        MessageBoxA(NULL, ex.what(), from, 0x00000010L); \
        exit(0); \
    } 