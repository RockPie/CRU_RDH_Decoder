#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <span>
#include <vector>
#include <iostream>
#include <iomanip>
#include <cstddef>

namespace bp {

enum class LineType : uint16_t {
    Data      = 0,
    TRG       = 0xBBBB,
    RDH_L0    = 0x0007,
    RDH_L1    = 0x0003,
    Sync      = 0xAAAA,
    Heartbeat = 0xEEEE,
    Undefined = 0xFFFF
};

struct Packet {
    std::span<const std::byte> block;
};

struct Heartbeat {
    std::array<std::span<const std::byte>, 2> lines;
};

struct DataLine {
    uint8_t     header_type; // 0xAC
    uint8_t     header_vldb_id;
    uint16_t    bx_cnt; // 12 bits
    uint32_t    ob_cnt;
    uint32_t    data_word0;
    uint32_t    data_word1;
    uint32_t    data_word2;
    uint32_t    data_word3;
    uint32_t    data_word4;
    uint32_t    data_word5;
    void display() const {
        std::ios old(nullptr);
        old.copyfmt(std::cout);
        std::cout << std::hex << std::setfill('0')
                  << "\n[DATA] hdr=" << std::setw(2) << (int)header_type
                  << " vldb_id=" << std::setw(2) << (int)header_vldb_id
                  << " bx=" << std::setw(4) << bx_cnt
                  << " ob=" << std::setw(8) << ob_cnt
                  << " dw0=" << std::setw(8) << data_word0
                  << " dw1=" << std::setw(8) << data_word1
                  << " dw2=" << std::setw(8) << data_word2
                  << " dw3=" << std::setw(8) << data_word3
                  << " dw4=" << std::setw(8) << data_word4
                  << " dw5=" << std::setw(8) << data_word5;
        std::cout.copyfmt(old);
    }
};

struct TrgLine {
    uint32_t    header_type; // 0xBBBB
    uint64_t    bx_cnt;
    uint64_t    ob_cnt;
    uint32_t    reserved0;
    uint64_t    reserved1;
    void display() const {
        std::ios old(nullptr);
        old.copyfmt(std::cout);
        std::cout << std::hex << std::setfill('0')
                  << "\n[TRG ] hdr=" << std::setw(4) << header_type
                  << " bx=" << std::setw(16) << bx_cnt
                  << " ob=" << std::setw(16) << ob_cnt;
        std::cout.copyfmt(old);
    }
};

struct RDH_L0{
    uint8_t     header_version;
    uint8_t     header_size;
    uint16_t    fee_id;
    uint8_t     priority_bit;
    uint8_t     system_id;
    uint16_t    reserved0;
    uint16_t    offset_new_packet;
    uint16_t    memory_size;
    uint8_t     link_id;
    uint8_t     packet_counter;
    uint16_t    cru_id;     // 12 bits
    uint8_t     dw;         // 4 bits
    uint16_t    bc;         // 12 bits
    uint32_t    reserved1;  // 20 bits
    uint32_t    orbit;
    uint8_t     data_format;
    uint32_t    reserved2;  // 24 bits
    uint32_t    reserved3;
    void display() const {
        std::ios old(nullptr);
        old.copyfmt(std::cout);
        std::cout << std::dec << std::setfill(' ')
                  << "\n[RDH_L0]"
                  << " version=" << (int)header_version
                  << " size=" << (int)header_size
                  << " fee_id=" << fee_id
                  << " priority=" << (int)priority_bit
                  << " system_id=" << (int)system_id
                  << " offset_new_packet=" << offset_new_packet
                  << " mem_size=" << memory_size
                  << " link=" << (int)link_id
                  << " pkt_cnt=" << (int)packet_counter
                  << " cru_id=" << cru_id
                  << " dw=" << (int)dw
                  << " bc=" << bc
                  << " orbit=" << orbit
                  << " fmt=" << (int)data_format;
        std::cout.copyfmt(old);
    }
};
struct RDH_L1{
    uint32_t    trg_type;
    uint16_t    hb_packet_counter;
    uint8_t     stop_bit;
    uint8_t     reserved0;
    uint32_t    reserved1;
    uint32_t    reserved2;
    uint32_t    detector_field;
    uint16_t    par_bit;
    uint16_t    reserved3;
    uint32_t    reserved4;
    uint32_t    reserved5;
    void display() const {
        std::ios old(nullptr);
        old.copyfmt(std::cout);
        std::cout << std::dec
                  << "\n[RDH_L1]"
                  << " trg_type=" << trg_type
                  << " hb_cnt=" << hb_packet_counter
                  << " stop=" << (int)stop_bit
                  << " detector_field=" << detector_field
                  << " par_bit=" << par_bit;
        std::cout.copyfmt(old);
    }
};

class StreamParser {
public:
    using PacketCb    = std::function<void(const Packet&)>;
    using HeartbeatCb = std::function<void(const Heartbeat&)>;
    using SyncCb      = std::function<void(std::span<const std::byte>)>;
    using RDH_L0_Cb   = std::function<void(const RDH_L0&, std::span<const std::byte>)>;
    using RDH_L1_Cb   = std::function<void(const RDH_L1&, std::span<const std::byte>)>;

    explicit StreamParser(PacketCb p, HeartbeatCb h, SyncCb s,
                          RDH_L0_Cb l0_cb = {}, RDH_L1_Cb l1_cb = {})
        : on_packet_(std::move(p))
        , on_heartbeat_(std::move(h))
        , on_sync_(std::move(s))
        , on_rdh_l0_(std::move(l0_cb))
        , on_rdh_l1_(std::move(l1_cb)) {}

    void feed(std::span<const std::byte> chunk);

private:
    enum class State { Idle, CollectPacket };
    // State state_ = State::Idle;

    std::vector<std::span<const std::byte>> cur_lines_;
    std::vector<std::span<const std::byte>> hb_lines_;

    PacketCb    on_packet_;
    HeartbeatCb on_heartbeat_;
    SyncCb      on_sync_;
    RDH_L0_Cb   on_rdh_l0_;
    RDH_L1_Cb   on_rdh_l1_;
};

} // namespace bp