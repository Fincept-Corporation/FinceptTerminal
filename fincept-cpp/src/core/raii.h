#pragma once
// RAII guards for C resource handles
// Prevents manual cleanup and ensures exception-safe resource release

#include <curl/curl.h>
#include <cstdio>
#include <utility>  // std::exchange

namespace fincept::core {

// RAII wrapper for CURL* handles
class CurlHandle {
public:
    CurlHandle() : handle_(curl_easy_init()) {}
    ~CurlHandle() { if (handle_) curl_easy_cleanup(handle_); }

    CurlHandle(const CurlHandle&) = delete;
    CurlHandle& operator=(const CurlHandle&) = delete;

    CurlHandle(CurlHandle&& other) noexcept
        : handle_(std::exchange(other.handle_, nullptr)) {}
    CurlHandle& operator=(CurlHandle&& other) noexcept {
        if (this != &other) {
            if (handle_) curl_easy_cleanup(handle_);
            handle_ = std::exchange(other.handle_, nullptr);
        }
        return *this;
    }

    explicit operator bool() const noexcept { return handle_ != nullptr; }
    CURL* get() const noexcept { return handle_; }

private:
    CURL* handle_;
};

// RAII wrapper for curl_slist* header lists
class CurlSlist {
public:
    CurlSlist() = default;
    ~CurlSlist() { if (list_) curl_slist_free_all(list_); }

    CurlSlist(const CurlSlist&) = delete;
    CurlSlist& operator=(const CurlSlist&) = delete;

    CurlSlist(CurlSlist&& other) noexcept
        : list_(std::exchange(other.list_, nullptr)) {}
    CurlSlist& operator=(CurlSlist&& other) noexcept {
        if (this != &other) {
            if (list_) curl_slist_free_all(list_);
            list_ = std::exchange(other.list_, nullptr);
        }
        return *this;
    }

    void append(const char* value) { list_ = curl_slist_append(list_, value); }
    curl_slist* get() const noexcept { return list_; }

private:
    curl_slist* list_ = nullptr;
};

// RAII wrapper for FILE* handles
class FileHandle {
public:
    explicit FileHandle(const char* path, const char* mode)
        : fp_(std::fopen(path, mode)) {}

#ifdef _WIN32
    explicit FileHandle(const wchar_t* path, const wchar_t* mode)
        : fp_(_wfopen(path, mode)) {}
#endif

    ~FileHandle() { if (fp_) std::fclose(fp_); }

    FileHandle(const FileHandle&) = delete;
    FileHandle& operator=(const FileHandle&) = delete;

    FileHandle(FileHandle&& other) noexcept
        : fp_(std::exchange(other.fp_, nullptr)) {}
    FileHandle& operator=(FileHandle&& other) noexcept {
        if (this != &other) {
            if (fp_) std::fclose(fp_);
            fp_ = std::exchange(other.fp_, nullptr);
        }
        return *this;
    }

    explicit operator bool() const noexcept { return fp_ != nullptr; }
    FILE* get() const noexcept { return fp_; }

private:
    FILE* fp_;
};

} // namespace fincept::core
