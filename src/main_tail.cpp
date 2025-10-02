#include "binparse/tail.hpp"
#include "binparse/parser.hpp"
#include "binparse/bytecursor.hpp"
#include <iostream>
#include <vector>
#include <chrono>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: bpx_tail <path>\n";
        return 1;
    }

    const std::string path = argv[1];

    std::vector<std::byte> stash;
    std::size_t total_bytes = 0;
    std::size_t total_lines = 0;
    std::size_t n_packets = 0;
    std::size_t n_heartbeats = 0;
    std::size_t n_syncs = 0;

    auto t_start = std::chrono::steady_clock::now();

    bp::StreamParser parser(
        /* on_packet */
        [&](const bp::Packet& pkt) {
            ++n_packets;
            total_lines += pkt.block.size() / bp::ByteCursor::kLineSize;
        },
        /* on_heartbeat */
        [&](const bp::Heartbeat&) {
            ++n_heartbeats;
            total_lines += 2;
        },
        /* on_sync */
        [&](std::span<const std::byte>) {
            ++n_syncs;
            total_lines += 1;
        }
    );

    std::cout << "Reading and parsing file: " << path << std::endl;

    bp::TailOptions opts;
    opts.poll_ms = 50;                 // check for new data every 50 ms
    opts.read_chunk = 1u << 20;        // 1 MB read chunk
    opts.inactivity_timeout_ms = 5000; // exit if no new data for 5 seconds
    bp::tail_growing_file(path, opts, [&](std::span<const std::byte> chunk) {
        total_bytes += chunk.size();

        // merge with stash to ensure full 40-byte lines
        if (!stash.empty()) {
            const std::size_t need = bp::ByteCursor::kLineSize - stash.size();
            if (chunk.size() >= need) {
                std::vector<std::byte> one(stash.begin(), stash.end());
                one.insert(one.end(), chunk.begin(), chunk.begin() + need);
                parser.feed(one);
                chunk = chunk.subspan(need);
                stash.clear();
            } else {
                stash.insert(stash.end(), chunk.begin(), chunk.end());
                return;
            }
        }

        const std::size_t remainder = chunk.size() % bp::ByteCursor::kLineSize;
        const auto main_part = chunk.first(chunk.size() - remainder);
        if (!main_part.empty()) parser.feed(main_part);
        if (remainder) {
            stash.assign(chunk.end() - remainder, chunk.end());
        }

        // show simple progress every ~1 MB
        if (total_bytes % (1 << 20) < bp::ByteCursor::kLineSize) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - t_start);
            std::cout << "[Progress] "
                      << total_bytes / 1e6 << " MB read, "
                      << total_lines << " lines parsed, "
                      << "time elapsed: " << elapsed.count() << " ms\r"
                      << std::flush;
        }
    });

    auto t_end = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(t_end - t_start);

    std::cout << "\n\n=== Parsing summary ===\n"
              << "Total bytes read   : " << total_bytes << " bytes\n"
              << "Total lines parsed : " << total_lines << "\n"
              << "Packets detected   : " << n_packets << "\n"
              << "Heartbeats detected: " << n_heartbeats << "\n"
              << "Sync lines detected: " << n_syncs << "\n"
              << "Elapsed time       : " << elapsed.count() << " ms\n"
              << "=======================\n";

    return 0;
}