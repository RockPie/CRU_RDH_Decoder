// src/tail.cpp
#include "binparse/tail.hpp"

#include <chrono>
#include <thread>
#include <vector>
#include <span>
#include <functional>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <algorithm>

#ifndef _WIN32
  #include <sys/stat.h>
  #include <fcntl.h>
  #include <unistd.h>
#endif

namespace bp {

void tail_growing_file(const std::string& path,
                       TailOptions opt,
                       const std::function<void(std::span<const std::byte>)>& on_bytes)
{
    using namespace std::chrono;
    const auto poll = (opt.poll_ms > 0) ? milliseconds(opt.poll_ms) : milliseconds(200);
    const size_t chunk = (opt.read_chunk > 0) ? static_cast<size_t>(opt.read_chunk) : size_t(64 * 1024);

#ifndef _WIN32
    // -------- POSIX version (your original logic) --------
    int fd = ::open(path.c_str(), O_RDONLY);
    if (fd < 0) throw std::runtime_error("open failed: " + path);

    std::vector<std::byte> buf;
    buf.resize(chunk);

    off_t pos = 0;
    ino_t ino = 0;
    {
        struct stat st{};
        if (fstat(fd, &st) == 0) ino = st.st_ino;
    }

    for (;;) {
        struct stat st{};
        if (fstat(fd, &st) != 0) {
            std::this_thread::sleep_for(poll);
            continue;
        }

        // rotation or truncation
        if (st.st_ino != ino || st.st_size < pos) {
            ::close(fd);
            fd = ::open(path.c_str(), O_RDONLY);
            if (fd < 0) throw std::runtime_error("reopen failed: " + path);
            pos = 0;
            ino = st.st_ino;
            continue;
        }

        if (st.st_size > pos) {
            const off_t avail = st.st_size - pos;
            const size_t to_read = static_cast<size_t>(std::min<off_t>(static_cast<off_t>(chunk), avail));

            ssize_t n = ::pread(fd, buf.data(), to_read, pos);
            if (n > 0) {
                pos += n;
                on_bytes(std::span<const std::byte>(buf.data(), static_cast<size_t>(n)));
                continue;
            }
        }

        std::this_thread::sleep_for(poll);
    }

#else
    // -------- Windows version (portable polling) --------
    std::vector<std::byte> buf;
    buf.resize(chunk);

    std::error_code ec;
    uintmax_t pos = 0;

    for (;;) {
        ec.clear();
        const auto size_now = std::filesystem::file_size(path, ec);
        if (ec) {
            // file may not exist yet or be temporarily unavailable
            std::this_thread::sleep_for(poll);
            continue;
        }

        // truncation or rotation detected
        if (size_now < pos) {
            pos = 0;
        }

        if (size_now > pos) {
            // read the appended region [pos, size_now)
            std::ifstream in(path, std::ios::binary);
            if (!in) {
                std::this_thread::sleep_for(poll);
                continue;
            }
            in.seekg(static_cast<std::streamoff>(pos), std::ios::beg);

            uintmax_t remaining = size_now - pos;
            while (remaining > 0) {
                const size_t to_read = static_cast<size_t>(std::min<uintmax_t>(buf.size(), remaining));
                in.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(to_read));
                const auto got = static_cast<size_t>(in.gcount());
                if (got == 0) break;

                on_bytes(std::span<const std::byte>(buf.data(), got));
                remaining -= got;
                pos       += got;
            }
        } else {
            std::this_thread::sleep_for(poll);
        }
    }
#endif
}

} // namespace bp