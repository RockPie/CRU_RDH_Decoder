#pragma once
#include <cstddef>
#include <functional>
#include <span>
#include <string>

namespace bp {

struct TailOptions {
    std::size_t read_chunk = 1u<<20; // 1MB
    int         poll_ms    = 50;
};

void tail_growing_file(const std::string& path,
                       TailOptions opt,
                       const std::function<void(std::span<const std::byte>)>& on_bytes);

} // namespace bp