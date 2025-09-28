#pragma once
#include <bit>          // std::endian, std::byteswap, std::bit_cast
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

#define LINE_BYTES 32

namespace bp {

struct ParseError : std::runtime_error {
    std::size_t offset{};
    std::size_t need{};
    std::size_t have{};
    explicit ParseError(std::string_view msg,
                        std::size_t off=0, std::size_t need_=0, std::size_t have_=0)
        : std::runtime_error(std::string(msg)), offset(off), need(need_), have(have_) {}
};

class ByteCursor {
public:
    static constexpr std::size_t kLineSize = LINE_BYTES; // 40 bytes

    explicit ByteCursor(std::span<const std::byte> buf) noexcept : buf_(buf) {}

    [[nodiscard]] std::size_t size()      const noexcept { return buf_.size(); }
    [[nodiscard]] std::size_t offset()    const noexcept { return off_; }
    [[nodiscard]] std::size_t remaining() const noexcept { return buf_.size() - off_; }
    [[nodiscard]] bool can_take(std::size_t n) const noexcept { return n <= remaining(); }

    std::span<const std::byte> take(std::size_t n) {
        if (!can_take(n)) throw ParseError{"out of range", off_, n, remaining()};
        auto s = buf_.subspan(off_, n);
        off_ += n;
        return s;
    }

    [[nodiscard]] std::expected<std::span<const std::byte>, ParseError>
    try_take(std::size_t n) noexcept {
        if (!can_take(n))
            return std::unexpected(ParseError{"out of range", off_, n, remaining()});
        auto s = buf_.subspan(off_, n);
        off_ += n;
        return s;
    }

    [[nodiscard]] std::span<const std::byte> peek(std::size_t n) const {
        if (n > remaining()) throw ParseError{"peek out of range", off_, n, remaining()};
        return buf_.subspan(off_, n);
    }

    [[nodiscard]] std::expected<std::span<const std::byte>, ParseError>
    try_peek(std::size_t n) const noexcept {
        if (n > remaining())
            return std::unexpected(ParseError{"peek out of range", off_, n, remaining()});
        return buf_.subspan(off_, n);
    }

    void skip(std::size_t n) { (void)take(n); }
    bool try_skip(std::size_t n) noexcept { if (!can_take(n)) return false; off_ += n; return true; }

    void align(std::size_t n) {
        if (n == 0) return;
        auto m = off_ % n;
        if (m) skip(n - m);
    }
    bool try_align(std::size_t n) noexcept {
        if (n == 0) return true;
        auto m = off_ % n;
        return m ? try_skip(n - m) : true;
    }

    void rewind(std::size_t n) {
        if (n > off_) throw ParseError{"rewind before begin", off_, n, off_};
        off_ -= n;
    }
    bool try_rewind(std::size_t n) noexcept {
        if (n > off_) return false;
        off_ -= n; return true;
    }

    template <class T>
    [[nodiscard]] T read_le() {
        static_assert(std::is_trivially_copyable_v<T>);
        static_assert(sizeof(T)==1 || sizeof(T)==2 || sizeof(T)==4 || sizeof(T)==8);
        auto s = take(sizeof(T));
        T v; std::memcpy(&v, s.data(), sizeof(T));
        if constexpr (std::endian::native == std::endian::big) v = byteswap(v);
        return v;
    }

    template <class T>
    [[nodiscard]] std::expected<T, ParseError> try_read_le() noexcept {
        static_assert(std::is_trivially_copyable_v<T>);
        static_assert(sizeof(T)==1 || sizeof(T)==2 || sizeof(T)==4 || sizeof(T)==8);
        if (!can_take(sizeof(T)))
            return std::unexpected(ParseError{"out of range", off_, sizeof(T), remaining()});
        T v; std::memcpy(&v, buf_.data()+off_, sizeof(T));
        off_ += sizeof(T);
        if constexpr (std::endian::native == std::endian::big) v = byteswap(v);
        return v;
    }

    template <class T>
    [[nodiscard]] T read_be() {
        static_assert(std::is_trivially_copyable_v<T>);
        static_assert(sizeof(T)==1 || sizeof(T)==2 || sizeof(T)==4 || sizeof(T)==8);
        auto s = take(sizeof(T));
        T v; std::memcpy(&v, s.data(), sizeof(T));
        if constexpr (std::endian::native == std::endian::little) v = byteswap(v);
        return v;
    }

    template <class T>
    [[nodiscard]] std::expected<T, ParseError> try_read_be() noexcept {
        static_assert(std::is_trivially_copyable_v<T>);
        static_assert(sizeof(T)==1 || sizeof(T)==2 || sizeof(T)==4 || sizeof(T)==8);
        if (!can_take(sizeof(T)))
            return std::unexpected(ParseError{"out of range", off_, sizeof(T), remaining()});
        T v; std::memcpy(&v, buf_.data()+off_, sizeof(T));
        off_ += sizeof(T);
        if constexpr (std::endian::native == std::endian::little) v = byteswap(v);
        return v;
    }

    uint8_t  u8()      { return read_le<uint8_t>(); }
    uint16_t u16_le()  { return read_le<uint16_t>(); }
    uint32_t u32_le()  { return read_le<uint32_t>(); }
    uint64_t u64_le()  { return read_le<uint64_t>(); }
    uint16_t u16_be()  { return read_be<uint16_t>(); }
    uint32_t u32_be()  { return read_be<uint32_t>(); }
    uint64_t u64_be()  { return read_be<uint64_t>(); }

    float  f32_le() { auto u = read_le<uint32_t>(); return std::bit_cast<float>(u); }
    double f64_le() { auto u = read_le<uint64_t>(); return std::bit_cast<double>(u); }
    float  f32_be() { auto u = read_be<uint32_t>();  return std::bit_cast<float>(u); }
    double f64_be() { auto u = read_be<uint64_t>();  return std::bit_cast<double>(u); }

private:
    template <class T>
    static constexpr T byteswap(T v) noexcept {
        if constexpr (sizeof(T)==1) {
            return v;
        } else if constexpr (sizeof(T)==2) {
            auto x = std::bit_cast<std::uint16_t>(v);
            return std::bit_cast<T>(std::byteswap(x));
        } else if constexpr (sizeof(T)==4) {
            auto x = std::bit_cast<std::uint32_t>(v);
            return std::bit_cast<T>(std::byteswap(x));
        } else { // 8
            auto x = std::bit_cast<std::uint64_t>(v);
            return std::bit_cast<T>(std::byteswap(x));
        }
    }

    std::span<const std::byte> buf_;
    std::size_t off_ = 0;
};

} // namespace bp