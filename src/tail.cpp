#include "binparse/tail.hpp"
#include <chrono>
#include <thread>
#include <vector>
#include <stdexcept>

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

namespace bp {

void tail_growing_file(const std::string& path,
                       TailOptions opt,
                       const std::function<void(std::span<const std::byte>)>& on_bytes)
{
    int fd = ::open(path.c_str(), O_RDONLY);
    if (fd < 0) throw std::runtime_error("open failed: " + path);

    std::vector<std::byte> buf;
    off_t pos = 0;
    ino_t ino = 0;
    {
        struct stat st{};
        if (fstat(fd, &st) == 0) ino = st.st_ino;
    }

    for (;;) {
        struct stat st{};
        if (fstat(fd, &st) != 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(opt.poll_ms));
            continue;
        }

        if (st.st_ino != ino || st.st_size < pos) {
            ::close(fd);
            fd = ::open(path.c_str(), O_RDONLY);
            if (fd < 0) throw std::runtime_error("reopen failed: " + path);
            pos = 0;
            ino = st.st_ino;
            continue;
        }

        if (st.st_size > pos) {
            std::size_t to_read = static_cast<std::size_t>(std::min<off_t>(opt.read_chunk, st.st_size - pos));
            buf.resize(to_read);
            ssize_t n = ::pread(fd, buf.data(), to_read, pos);
            if (n > 0) {
                pos += n;
                on_bytes(std::span<const std::byte>(buf.data(), static_cast<std::size_t>(n)));
                continue;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(opt.poll_ms));
    }
}

} // namespace bp