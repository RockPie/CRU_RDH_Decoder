#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>

#include <bit>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <vector>

#include "binparse/bytecursor.hpp"

namespace py = pybind11;

// ---------- helpers ----------
static inline uint8_t  le8_at (std::span<const std::byte> s, std::size_t off) {
    if (off + 1 > s.size()) return 0;
    uint8_t v; std::memcpy(&v, s.data()+off, 1); return v;
}
static inline uint16_t le16_at(std::span<const std::byte> s, std::size_t off) {
    if (off + 2 > s.size()) return 0;
    uint16_t v; std::memcpy(&v, s.data()+off, 2);
    if constexpr (std::endian::native == std::endian::big) v = std::byteswap(v);
    return v;
}
static inline uint32_t le32_at(std::span<const std::byte> s, std::size_t off) {
    if (off + 4 > s.size()) return 0;
    uint32_t v; std::memcpy(&v, s.data()+off, 4);
    if constexpr (std::endian::native == std::endian::big) v = std::byteswap(v);
    return v;
}
static inline uint64_t le64_at(std::span<const std::byte> s, std::size_t off) {
    if (off + 8 > s.size()) return 0;
    uint64_t v; std::memcpy(&v, s.data()+off, 8);
    if constexpr (std::endian::native == std::endian::big) v = std::byteswap(v);
    return v;
}

enum class LineClass : uint8_t { Data, TRG, RDH_L0, RDH_L1, Undefined };

static inline LineClass classify(std::span<const std::byte> line) {
    uint16_t t = le16_at(line, 0);
    uint8_t low = static_cast<uint8_t>(t & 0xff);
    if (low == 0xac) return LineClass::Data;
    if (low == 0xbb) return LineClass::TRG;
    if (low == 0x07) return LineClass::RDH_L0;
    if (low == 0x03) return LineClass::RDH_L1;
    return LineClass::Undefined;
}

// ---------- offsets (match your latest version) ----------
namespace off_L0 {
    constexpr std::size_t header_version     = 0;   // u8
    constexpr std::size_t header_size        = 1;   // u8
    constexpr std::size_t fee_id             = 2;   // u16
    constexpr std::size_t priority_bit       = 4;   // u8
    constexpr std::size_t system_id          = 5;   // u8
    constexpr std::size_t reserved0          = 6;   // u16
    constexpr std::size_t offset_new_packet  = 8;   // u16
    constexpr std::size_t memory_size        = 10;  // u16
    constexpr std::size_t link_id            = 12;  // u8
    constexpr std::size_t packet_counter     = 13;  // u8
    constexpr std::size_t cru_id             = 14;  // u16 (12 bits)
    constexpr std::size_t dw                 = 15;  // u8  (4 bits: high nibble per your code)
    constexpr std::size_t bc                 = 16;  // u16 (12 bits)
    constexpr std::size_t reserved1          = 17;  // u32 (20 bits) NOTE: spans 17..20
    constexpr std::size_t orbit              = 20;  // u32
    constexpr std::size_t data_format        = 24;  // u8
    constexpr std::size_t reserved2          = 25;  // u32 (24 bits)
    constexpr std::size_t reserved3          = 28;  // u32
}
namespace off_L1 {
    constexpr std::size_t trg_type           = 0;   // u32
    constexpr std::size_t hb_packet_counter  = 4;   // u16
    constexpr std::size_t stop_bit           = 6;   // u8
    constexpr std::size_t reserved0          = 7;   // u8
    constexpr std::size_t reserved1          = 8;   // u32
    constexpr std::size_t reserved2          = 12;  // u32
    constexpr std::size_t detector_field     = 16;  // u32
    constexpr std::size_t par_bit            = 20;  // u16
    constexpr std::size_t reserved3          = 22;  // u16
    constexpr std::size_t reserved4          = 24;  // u32
    constexpr std::size_t reserved5          = 28;  // u32
}
namespace off_data {
    constexpr std::size_t header_type        = 0;   // u8
    constexpr std::size_t header_vldb_id     = 1;   // u8
    constexpr std::size_t bx_cnt             = 2;   // u16
    constexpr std::size_t ob_cnt             = 4;   // u16
    constexpr std::size_t reserved0          = 6;   // u16
    constexpr std::size_t data_word0         = 8;   // u32
    constexpr std::size_t data_word1         = 12;  // u32
    constexpr std::size_t data_word2         = 16;  // u32
    constexpr std::size_t data_word3         = 20;  // u32
    constexpr std::size_t data_word4         = 24;  // u32
    constexpr std::size_t data_word5         = 28;  // u32
}
namespace off_trg {
    constexpr std::size_t header_type        = 0;   // u32
    constexpr std::size_t bx_cnt             = 4;   // u64
    constexpr std::size_t ob_cnt             = 12;  // u64
    constexpr std::size_t reserved0          = 20;  // u32
    constexpr std::size_t reserved1          = 24;  // u64
}

// ---------- to-dict parsers ----------
static py::dict parse_rdh_l0_dict(std::span<const std::byte> line) {
    py::dict d;
    d["header_version"]     = le8_at (line, off_L0::header_version);
    d["header_size"]        = le8_at (line, off_L0::header_size);
    d["fee_id"]             = le16_at(line, off_L0::fee_id);
    d["priority_bit"]       = le8_at (line, off_L0::priority_bit);
    d["system_id"]          = le8_at (line, off_L0::system_id);
    d["reserved0"]          = le16_at(line, off_L0::reserved0);
    d["offset_new_packet"]  = le16_at(line, off_L0::offset_new_packet);
    d["memory_size"]        = le16_at(line, off_L0::memory_size);
    d["link_id"]            = le8_at (line, off_L0::link_id);
    d["packet_counter"]     = le8_at (line, off_L0::packet_counter);
    d["cru_id"]             = static_cast<int>(le16_at(line, off_L0::cru_id) & 0x0FFF);
    // dw：你代码里是取高 4 位（>>4）
    d["dw"]                 = static_cast<int>((le8_at(line, off_L0::dw) >> 4) & 0x0F);
    d["bc"]                 = static_cast<int>(le16_at(line, off_L0::bc) & 0x0FFF);
    // reserved1：给出 20bit（此处与你的C++位排布一致：右移4再取20位）
    d["reserved1"]          = static_cast<int>((le32_at(line, off_L0::reserved1) & 0x00FFFFF0u) >> 4);
    d["orbit"]              = le32_at(line, off_L0::orbit);
    d["data_format"]        = le8_at (line, off_L0::data_format);
    d["reserved2"]          = static_cast<int>( le32_at(line, off_L0::reserved2) & 0x00FFFFFFu );
    d["reserved3"]          = le32_at(line, off_L0::reserved3);
    return d;
}

static py::dict parse_rdh_l1_dict(std::span<const std::byte> line) {
    py::dict d;
    d["trg_type"]           = le32_at(line, off_L1::trg_type);
    d["hb_packet_counter"]  = le16_at(line, off_L1::hb_packet_counter);
    d["stop_bit"]           = le8_at (line, off_L1::stop_bit);
    d["reserved0"]          = le8_at (line, off_L1::reserved0);
    d["reserved1"]          = le32_at(line, off_L1::reserved1);
    d["reserved2"]          = le32_at(line, off_L1::reserved2);
    d["detector_field"]     = le32_at(line, off_L1::detector_field);
    d["par_bit"]            = le16_at(line, off_L1::par_bit);
    d["reserved3"]          = le16_at(line, off_L1::reserved3);
    d["reserved4"]          = le32_at(line, off_L1::reserved4);
    d["reserved5"]          = le32_at(line, off_L1::reserved5);
    return d;
}

static py::dict parse_data_dict(std::span<const std::byte> line) {
    py::dict d;
    d["header_type"]    = le8_at (line, off_data::header_type);
    d["header_vldb_id"] = le8_at (line, off_data::header_vldb_id);
    d["bx_cnt"]         = le16_at(line, off_data::bx_cnt);
    d["ob_cnt"]         = le16_at(line, off_data::ob_cnt);
    d["reserved0"]      = le16_at(line, off_data::reserved0);
    d["data_word0"]     = le32_at(line, off_data::data_word0);
    d["data_word1"]     = le32_at(line, off_data::data_word1);
    d["data_word2"]     = le32_at(line, off_data::data_word2);
    d["data_word3"]     = le32_at(line, off_data::data_word3);
    d["data_word4"]     = le32_at(line, off_data::data_word4);
    d["data_word5"]     = le32_at(line, off_data::data_word5);
    return d;
}

static py::dict parse_trg_dict(std::span<const std::byte> line) {
    py::dict d;
    d["header_type"] = le32_at(line, off_trg::header_type);
    d["bx_cnt"]      = le64_at(line, off_trg::bx_cnt);
    d["ob_cnt"]      = le64_at(line, off_trg::ob_cnt);
    d["reserved0"]   = le32_at(line, off_trg::reserved0);
    d["reserved1"]   = le64_at(line, off_trg::reserved1);
    return d;
}

// ---------- module ----------
PYBIND11_MODULE(pybinparse, m) {
    m.doc() = "Python bindings matching the latest line classification & offsets";

    // v3: count all types
    m.def("count_types_v3", [](py::buffer b){
        py::buffer_info bi = b.request();
        auto* p = static_cast<std::byte*>(bi.ptr);
        std::span<const std::byte> sp(p, static_cast<std::size_t>(bi.size));

        const std::size_t line = bp::ByteCursor::kLineSize; // 40
        const std::size_t n = sp.size() / line;

        std::size_t c_l0=0, c_l1=0, c_trg=0, c_data=0, c_undef=0;
        for (std::size_t i=0; i<n; ++i) {
            auto ln = sp.subspan(i*line, line);
            switch (classify(ln)) {
                case LineClass::RDH_L0:    ++c_l0;   break;
                case LineClass::RDH_L1:    ++c_l1;   break;
                case LineClass::TRG:       ++c_trg;  break;
                case LineClass::Data:      ++c_data; break;
                default:                   ++c_undef;break;
            }
        }
        py::dict d;
        d["L0"] = py::int_(c_l0);
        d["L1"] = py::int_(c_l1);
        d["TRG"] = py::int_(c_trg);
        d["DATA"] = py::int_(c_data);
        d["UNDEFINED"] = py::int_(c_undef);
        d["LINES"] = py::int_(n);
        return d;
    });

    // parse one 40B line → (type_str, dict_fields)
    m.def("parse_line", [](py::buffer one_line){
        py::buffer_info bi = one_line.request();
        if (bi.size < static_cast<py::ssize_t>(bp::ByteCursor::kLineSize))
            throw std::runtime_error("parse_line expects at least 40 bytes");
        auto* p = static_cast<std::byte*>(bi.ptr);
        std::span<const std::byte> line(p, bp::ByteCursor::kLineSize);

        switch (classify(line)) {
            case LineClass::RDH_L0: return py::make_tuple("L0",  parse_rdh_l0_dict(line));
            case LineClass::RDH_L1: return py::make_tuple("L1",  parse_rdh_l1_dict(line));
            case LineClass::TRG:    return py::make_tuple("TRG", parse_trg_dict(line));
            case LineClass::Data:   return py::make_tuple("DATA",parse_data_dict(line));
            default:                return py::make_tuple("UNDEFINED", py::dict());
        }
    });

    // scan first n lines (default 10)
    m.def("scan_first_n", [](py::buffer b, std::size_t n = 10){
        py::buffer_info bi = b.request();
        auto* p = static_cast<std::byte*>(bi.ptr);
        std::span<const std::byte> sp(p, static_cast<std::size_t>(bi.size));
        const std::size_t line = bp::ByteCursor::kLineSize;
        const std::size_t total = sp.size() / line;
        n = std::min(n, total);

        py::list out;
        for (std::size_t i=0; i<n; ++i) {
            auto ln = sp.subspan(i*line, line);
            switch (classify(ln)) {
                case LineClass::RDH_L0: out.append(py::make_tuple("L0",  parse_rdh_l0_dict(ln))); break;
                case LineClass::RDH_L1: out.append(py::make_tuple("L1",  parse_rdh_l1_dict(ln))); break;
                case LineClass::TRG:    out.append(py::make_tuple("TRG", parse_trg_dict(ln)));    break;
                case LineClass::Data:   out.append(py::make_tuple("DATA",parse_data_dict(ln)));   break;
                default:                out.append(py::make_tuple("UNDEFINED", py::dict()));      break;
            }
        }
        return out;
    });
}