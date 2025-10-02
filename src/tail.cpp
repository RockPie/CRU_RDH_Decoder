#include "binparse/tail.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <span>
#include <stdexcept>
#include <system_error>
#include <thread>
#include <vector>

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

    const auto poll = (opt.poll_ms > 0) ? milliseconds(opt.poll_ms) : milliseconds(50);
    const std::size_t chunk = (opt.read_chunk > 0) ? opt.read_chunk : (1u << 20);

    const bool use_timeout = (opt.inactivity_timeout_ms > 0);
    const auto timeout = milliseconds(opt.inactivity_timeout_ms);

    auto last_activity = steady_clock::now(); // 最近一次读到新数据的时间

#ifndef _WIN32
    // -------- POSIX 版本（使用 open/fstat/pread）--------
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
            if (use_timeout && (steady_clock::now() - last_activity > timeout)) return;
            continue;
        }

        // 轮转或截断
        if (st.st_ino != ino || st.st_size < pos) {
            ::close(fd);
            fd = ::open(path.c_str(), O_RDONLY);
            if (fd < 0) throw std::runtime_error("reopen failed: " + path);
            pos = 0;
            ino = st.st_ino;
            // 轮转/截断不算活动，只有真正读到字节时才刷新 last_activity
        }

        if (st.st_size > pos) {
            const off_t avail = st.st_size - pos;
            const std::size_t to_read = static_cast<std::size_t>(
                std::min<off_t>(static_cast<off_t>(chunk), avail));

            ssize_t n = ::pread(fd, buf.data(), to_read, pos);
            if (n > 0) {
                pos += n;
                last_activity = steady_clock::now(); // 读到新数据，刷新活动时间
                on_bytes(std::span<const std::byte>(buf.data(), static_cast<std::size_t>(n)));
            } else {
                // 没读到（例如被另一进程占用），休眠后再试
                std::this_thread::sleep_for(poll);
            }
        } else {
            // 没有新增
            std::this_thread::sleep_for(poll);
        }

        if (use_timeout && (steady_clock::now() - last_activity > timeout)) return;
    }

#else
    // -------- Windows / 可移植版本（ifstream + filesystem 轮询）--------
    std::vector<std::byte> buf;
    buf.resize(chunk);

    std::error_code ec;
    uintmax_t pos = 0;

    for (;;) {
        ec.clear();
        const auto size_now = std::filesystem::file_size(path, ec);
        if (ec) {
            std::this_thread::sleep_for(poll);
            if (use_timeout && (steady_clock::now() - last_activity > timeout)) return;
            continue;
        }

        // 截断或轮转
        if (size_now < pos) {
            pos = 0;
        }

        if (size_now > pos) {
            std::ifstream in(path, std::ios::binary);
            if (!in) {
                std::this_thread::sleep_for(poll);
                if (use_timeout && (steady_clock::now() - last_activity > timeout)) return;
                continue;
            }
            in.seekg(static_cast<std::streamoff>(pos), std::ios::beg);

            uintmax_t remaining = size_now - pos;
            while (remaining > 0) {
                const std::size_t to_read = static_cast<std::size_t>(
                    std::min<uintmax_t>(buf.size(), remaining));
                in.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(to_read));
                const auto got = static_cast<std::size_t>(in.gcount());
                if (got == 0) break;

                last_activity = steady_clock::now(); // 读到新数据
                on_bytes(std::span<const std::byte>(buf.data(), got));
                remaining -= got;
                pos       += got;
            }
        } else {
            std::this_thread::sleep_for(poll);
        }

        if (use_timeout && (steady_clock::now() - last_activity > timeout)) return;
    }
#endif
}

} // namespace bp