//
// Created by cc on 2021/11/3.
//

#include "utils.hpp"
#include <string>
#include <cstdlib>
#include <memory>
#ifdef __GNUG__
#include <cxxabi.h>
#endif

#ifdef _WIN32
#include <Windows.h>
#endif

namespace gddi {
namespace utils {


void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
}

void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

void trim(std::string &s) {
    gddi::utils::ltrim(s);
    gddi::utils::rtrim(s);
}

std::vector<std::string> ssplit(std::string &s, std::string &delimiter) {
    size_t pos = 0;
    std::vector<std::string> res_{};
    while ((pos = s.find(delimiter)) != std::string::npos) {
        std::string token = s.substr(0, pos);
        gddi::utils::trim(token);
        res_.push_back(token);
        s.erase(0, pos + delimiter.length());
    }
    gddi::utils::trim(s);
    res_.push_back(s);
    return res_;
}

#ifdef __GNUG__
std::string demangle(const char *name) {

    int status = -4; // some arbitrary value to eliminate the compiler warning

    // enable c++11 by passing the flag -std=c++11 to g++
    std::unique_ptr<char, void (*)(void *)> res{
        abi::__cxa_demangle(name, NULL, NULL, &status),
        std::free
    };

    return (status == 0) ? res.get() : name;
}
#else
// does nothing if not g++
std::string demangle(const char* name) {
    return name;
}
#endif


void windows_utf8_cout() {
#ifdef _WIN32
    // // Set console code page to UTF-8 so console known how to interpret string data
    // SetConsoleOutputCP(CP_UTF8);
    // // Enable buffering to prevent VS from chopping up UTF-8 byte sequences
    // setvbuf(stdout, nullptr, _IOFBF, 1000);

    // std::cout << "这是中文\n";
#endif
}

}
}