//
// Created by cc on 2021/11/3.
//

#ifndef FFMPEG_WRAPPER_SRC_UTILS_HPP_
#define FFMPEG_WRAPPER_SRC_UTILS_HPP_
#include <algorithm>
#include <deque>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <list>
#include <openssl/sha.h>
#include <sstream>
#include <string>
#include <vector>

namespace gddi {
namespace utils {

template<class First, class Second>
inline std::ostream &operator<<(std::ostream &oss, const std::pair<First, Second> &val) {
    oss << "{" << val.first << ", " << val.second << "}";
    return oss;
}

template<class Ty>
inline std::string fmts(const Ty &val) {
    std::stringstream oss;
    oss << val;
    return oss.str();
}

inline std::string _fmt_n(double fnn, const char *_unit) {
    std::stringstream sss;
    sss << std::fixed << std::setprecision(1) << std::setw(5) << fnn << _unit;
    return sss.str();
}

inline std::string _fmt_n(long long fnn) {
    std::stringstream sss;
    sss << std::setw(5) << fnn;
    return sss.str();
}

inline std::string human_readable_bytes(unsigned long long n) {
#define _1KB (1024LL)                            // NOLINT(bugprone-reserved-identifier)
#define _1MB (1024LL * 1024)                     // NOLINT(bugprone-reserved-identifier)
#define _1GB (1024LL * 1024 * 1024)              // NOLINT(bugprone-reserved-identifier)
#define _1TB (1024LL * 1024 * 1024 * 1024)       // NOLINT(bugprone-reserved-identifier)
#define _1PB (1024LL * 1024 * 1024 * 1024 * 1024)// NOLINT(bugprone-reserved-identifier)
    if (n < _1KB) return _fmt_n((long long)n);
    if (n < _1MB) return _fmt_n((double)n / _1KB, "K");
    if (n < _1GB) return _fmt_n((double)n / _1MB, "M");
    if (n < _1TB) return _fmt_n((double)n / _1GB, "G");
    if (n < _1PB) return _fmt_n((double)n / _1TB, "T");
    return _fmt_n((double)n / _1PB, "P");
}

std::string demangle(const char *name);

template<class ClassType>
inline std::string get_class_name(ClassType *class_obj_ptr) {
    std::string name = typeid(*class_obj_ptr).name();
#ifdef __GNUG__
    return demangle(name.c_str());
#else
    auto colons = name.find(' ');
    return name.substr(colons + 1);
#endif
}

template<class T>
inline std::string uuname() {
    auto typed_name = demangle(typeid(T).name());
    auto space_ch_pos = typed_name.find_first_of(' ');
    if (space_ch_pos != std::string::npos) { return typed_name.substr(space_ch_pos + 1); }
    return typed_name;
}

class TextWriter {
public:
    explicit TextWriter(std::string &text_buffer) : text_buffer_(text_buffer) {}
    ~TextWriter() { text_buffer_ = text_stream_.str(); }

    template<class Ty>
    std::stringstream &operator<<(Ty val) {
        text_stream_ << val;
        return text_stream_;
    }

    std::stringstream text_stream_;
    std::string text_buffer_;
};

class object_release_manager {
public:
    object_release_manager() = default;
    ~object_release_manager() {
        for (const auto &pair : release_func_vec_) { pair.second(); }

        release_func_vec_.clear();
    }

    void bind(const std::string &obj_, const std::function<void()> &release_func) {
        release_func_vec_.emplace_front(obj_, release_func);
    }

    std::stringstream oss;

private:
    std::deque<std::pair<std::string, std::function<void()>>> release_func_vec_;
};

inline bool read_file(const std::string &path, std::string &result_data) {
    std::ifstream file_ss;
    file_ss.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    try {
        // open files
        file_ss.open(path);

        std::stringstream fss;
        // read file's buffer contents into streams
        fss << file_ss.rdbuf();
        // close file handlers
        file_ss.close();
        // convert stream into string
        result_data = fss.str();
        return true;
    } catch (std::ifstream::failure & /*e*/) { std::cout << "error: file <" << path << "> read fail!" << std::endl; }
    return false;
}

inline bool sha256sum(const std::string &path, std::vector<uint8_t> &result_data) {
    FILE *fp = fopen(path.c_str(), "rb");
    if (fp == nullptr) { return false; }

    fseek(fp, 0, SEEK_END);
    auto buffer = std::vector<uint8_t>(ftell(fp));
    fseek(fp, 0, SEEK_SET);
    int buffer_size = fread(buffer.data(), 1, buffer.size(), fp);
    fclose(fp);

    result_data.resize(SHA256_DIGEST_LENGTH);
    SHA256(buffer.data(), buffer_size, result_data.data());

    return true;
}

inline bool sha256sum(const char *path, uint8_t *result_data, size_t data_size) {
    FILE *fp = fopen(path, "rb");
    if (fp == nullptr || data_size < SHA256_DIGEST_LENGTH) { return false; }

    fseek(fp, 0, SEEK_END);
    auto buffer = std::vector<uint8_t>(ftell(fp));
    fseek(fp, 0, SEEK_SET);
    fread(buffer.data(), 1, buffer.size(), fp);
    fclose(fp);

    SHA256(buffer.data(), buffer.size(), result_data);

    return true;
}

void ltrim(std::string &s);
void rtrim(std::string &s);
void trim(std::string &s);
std::vector<std::string> ssplit(std::string &s, std::string &delimiter);

void windows_utf8_cout();

}// namespace utils
}// namespace gddi

#endif//FFMPEG_WRAPPER_SRC_UTILS_HPP_
