#pragma once
#include <cstddef>
#include <functional>
#include <span>
#include <string>

namespace bp {

struct TailOptions {
    std::size_t read_chunk = 1u << 20;
    int         poll_ms    = 50;

    int         inactivity_timeout_ms = 0;
};

void tail_growing_file(const std::string& path,
                       TailOptions opt,
                       const std::function<void(std::span<const std::byte>)>& on_bytes);

} // namespace bp