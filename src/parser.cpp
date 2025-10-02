#include "binparse/parser.hpp"
#include "binparse/bytecursor.hpp"
#include <cstring>
#include <iostream>
#include <iomanip>
#include <cstddef>

namespace bp {
namespace {
    // constexpr std::size_t kLine = ByteCursor::kLineSize;
    // constexpr std::size_t kLinesPerPacket = LINE_BYTES;
    // constexpr std::size_t kLinesPerHeartbeat = 2;

    inline uint8_t le8_at(std::span<const std::byte> s, std::size_t off) {
        if (off + 1 > s.size()) return 0;
        uint8_t v; std::memcpy(&v, s.data()+off, 1);
        return v;
    }
    inline uint16_t le16_at(std::span<const std::byte> s, std::size_t off) {
        if (off + 2 > s.size()) return 0;
        uint16_t v; std::memcpy(&v, s.data()+off, 2);
        if constexpr (std::endian::native == std::endian::big) v = std::byteswap(v);
        return v;
    }
    inline uint32_t le32_at(std::span<const std::byte> s, std::size_t off) {
        if (off + 4 > s.size()) return 0;
        uint32_t v; std::memcpy(&v, s.data()+off, 4);
        if constexpr (std::endian::native == std::endian::big) v = std::byteswap(v);
        return v;
    }
    inline uint64_t le64_at(std::span<const std::byte> s, std::size_t off) {
        if (off + 8 > s.size()) return 0;
        uint64_t v; std::memcpy(&v, s.data()+off, 8);
        if constexpr (std::endian::native == std::endian::big) v = std::byteswap(v);
        return v;
    }

    inline LineType classify(std::span<const std::byte> line) {
        uint16_t t = le16_at(line, 0);           // 头2字节的小端
        uint8_t low = static_cast<uint8_t>(t & 0xff);
        if (low == 0xac) return LineType::Data;
        if (low == 0xbb) return LineType::TRG;
        if (low == 0x07) return LineType::RDH_L0;
        if (low == 0x03) return LineType::RDH_L1;
        return LineType::Undefined;
    }

    namespace off_L0 {
        constexpr std::size_t header_version   = 0; // uint8_t
        constexpr std::size_t header_size      = 1; // uint8_t
        constexpr std::size_t fee_id           = 2; // uint16_t
        constexpr std::size_t priority_bit     = 4; // uint8_t
        constexpr std::size_t system_id        = 5; // uint8_t
        constexpr std::size_t reserved0        = 6; // uint16_t
        constexpr std::size_t offset_new_packet= 8; // uint16_t
        constexpr std::size_t memory_size      = 10; // uint16_t
        constexpr std::size_t link_id          = 12; // uint8_t
        constexpr std::size_t packet_counter   = 13; // uint8_t
        constexpr std::size_t cru_id           = 14; // uint16_t (12 bits)
        constexpr std::size_t dw               = 15; // uint8_t  (4 bits)
        constexpr std::size_t bc               = 16; // uint16_t (12 bits
        constexpr std::size_t reserved1        = 17; // uint32_t (20 bits)
        constexpr std::size_t orbit            = 20; // uint32_t
        constexpr std::size_t data_format      = 24; // uint8_t
        constexpr std::size_t reserved2        = 25; // uint32_t (24 bits)
        constexpr std::size_t reserved3        = 28; // uint32_t
    }
    namespace off_L1 {
        constexpr std::size_t trg_type          = 0; // uint32_t
        constexpr std::size_t hb_packet_counter = 4; // uint16_t
        constexpr std::size_t stop_bit          = 6; // uint8_t
        constexpr std::size_t reserved0         = 7; // uint8_t
        constexpr std::size_t reserved1         = 8; // uint32_t
        constexpr std::size_t reserved2         = 12; // uint32_t
        constexpr std::size_t detector_field    = 16; // uint32_t
        constexpr std::size_t par_bit           = 20; // uint16_t
        constexpr std::size_t reserved3         = 22; // uint16_t
        constexpr std::size_t reserved4         = 24; // uint32_t
        constexpr std::size_t reserved5         = 28; // uint32_t
    }
    namespace off_data {
        constexpr std::size_t header_type = 0; // uint8_t
        constexpr std::size_t header_vldb_id = 1; // uint8_t
        constexpr std::size_t bx_cnt = 2; // uint16_t
        constexpr std::size_t ob_cnt = 4; // uint32_t
        constexpr std::size_t data_word0 = 8; // uint32_t
        constexpr std::size_t data_word1 = 12; // uint32_t
        constexpr std::size_t data_word2 = 16; // uint32_t
        constexpr std::size_t data_word3 = 20; // uint32_t
        constexpr std::size_t data_word4 = 24; // uint32_t
        constexpr std::size_t data_word5 = 28; // uint32_t
    }
    namespace off_trg {
        constexpr std::size_t header_type = 0; // uint32_t
        constexpr std::size_t bx_cnt = 4;   // uint64_t
        constexpr std::size_t ob_cnt = 12;  // uint64_t
        constexpr std::size_t reserved0 = 20; // uint32_t
        constexpr std::size_t reserved1 = 24; // uint64_t
    }

    inline RDH_L0 parse_rdh_l0(std::span<const std::byte> line) {
        RDH_L0 r{};
        r.header_version    = le8_at(line, off_L0::header_version);
        r.header_size       = le8_at(line, off_L0::header_size);
        r.fee_id            = le16_at(line, off_L0::fee_id);
        r.priority_bit      = le8_at(line, off_L0::priority_bit);
        r.system_id         = le8_at(line, off_L0::system_id);
        r.reserved0         = le16_at(line, off_L0::reserved0);
        r.offset_new_packet = le16_at(line, off_L0::offset_new_packet);
        r.memory_size       = le16_at(line, off_L0::memory_size);
        r.link_id           = le8_at(line, off_L0::link_id);
        r.packet_counter    = le8_at(line, off_L0::packet_counter);
        r.cru_id            = le16_at(line, off_L0::cru_id) & 0x0FFF; // 12 bits
        r.dw                = (le8_at(line, off_L0::dw) >> 4) & 0x0F; // 4 bits
        r.bc                = le16_at(line, off_L0::bc) & 0x0FFF; // 12 bits
        r.reserved1         = (le32_at(line, off_L0::reserved1) & 0x00FFFFF0) >> 4;
        r.orbit             = le32_at(line, off_L0::orbit);
        r.data_format       = le8_at(line, off_L0::data_format);
        r.reserved2         = (le32_at(line, off_L0::reserved2) & 0x00FFFFFF);
        r.reserved3         = le32_at(line, off_L0::reserved3);
        return r;
    }

    inline RDH_L1 parse_rdh_l1(std::span<const std::byte> line) {
        RDH_L1 r{};
        r.trg_type          = le32_at(line, off_L1::trg_type);
        r.hb_packet_counter = le16_at(line, off_L1::hb_packet_counter);
        r.stop_bit          = le8_at(line, off_L1::stop_bit);
        r.reserved0         = le8_at(line, off_L1::reserved0);
        r.reserved1         = le32_at(line, off_L1::reserved1);
        r.reserved2         = le32_at(line, off_L1::reserved2);
        r.detector_field    = le32_at(line, off_L1::detector_field);
        r.par_bit           = le16_at(line, off_L1::par_bit);
        r.reserved3         = le16_at(line, off_L1::reserved3);
        r.reserved4         = le32_at(line, off_L1::reserved4);
        r.reserved5         = le32_at(line, off_L1::reserved5);
        return r;
    }

    inline DataLine parse_data_line(std::span<const std::byte> line) {
        DataLine r{};
        r.header_type   = le8_at(line, off_data::header_type);
        r.header_vldb_id= le8_at(line, off_data::header_vldb_id);
        r.bx_cnt        = (le16_at(line, off_data::bx_cnt) & 0x0FFF); // 12 bits
        r.ob_cnt        = le32_at(line, off_data::ob_cnt);
        r.data_word0    = le32_at(line, off_data::data_word0);
        r.data_word1    = le32_at(line, off_data::data_word1);
        r.data_word2    = le32_at(line, off_data::data_word2);
        r.data_word3    = le32_at(line, off_data::data_word3);
        r.data_word4    = le32_at(line, off_data::data_word4);
        r.data_word5    = le32_at(line, off_data::data_word5);
        return r;
    }

    inline TrgLine parse_trg_line(std::span<const std::byte> line) {
        TrgLine r{};
        r.header_type = le32_at(line, off_trg::header_type);
        r.bx_cnt      = le64_at(line, off_trg::bx_cnt);
        r.ob_cnt      = le64_at(line, off_trg::ob_cnt);
        r.reserved0   = le32_at(line, off_trg::reserved0);
        r.reserved1   = le64_at(line, off_trg::reserved1);
        return r;
    }
    
}

void StreamParser::feed(std::span<const std::byte> chunk) {
    constexpr size_t kLine = ByteCursor::kLineSize;
    const size_t n = chunk.size() / kLine;
    if (n == 0) return;

    for (size_t i = 0; i < n; ++i) {
        auto line = chunk.subspan(i * kLine, kLine);
        auto type = classify(line);

        switch (type) {
        case LineType::RDH_L0: {
            RDH_L0 r = parse_rdh_l0(line);
            // r.display();
            if (on_rdh_l0_) on_rdh_l0_(r, line);
            break;
        }
        case LineType::RDH_L1: {
            RDH_L1 r = parse_rdh_l1(line);
            // r.display();
            if (on_rdh_l1_) on_rdh_l1_(r, line);
            break;
        }
        case LineType::Data: {
            DataLine d = parse_data_line(line);
            if (on_data_line_) on_data_line_(d, line);
            break;
        }
        case LineType::TRG: {
            // std::cout << "[TRG ]" << std::endl;
            // print in hex
            ::bp::TrgLine t = parse_trg_line(line);
            if (on_trg_line_) on_trg_line_(t, line);
            break;
        }
        default:
            if (on_packet_) on_packet_(Packet{line});
            break;
        }
    }
}


} // namespace bp