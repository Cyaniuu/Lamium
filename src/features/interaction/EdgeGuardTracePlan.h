#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace lamium::interaction {
// clang-cl's local virtual-call thunks load the entry before jumping to it.
// Decode only the measured x64 forms; never assume an interface slot number.
inline std::optional<int32_t> replayInputThunkJump(std::span<uint8_t const> code) {
    if (code.size() < 5 || code[0] != 0xe9) return std::nullopt;
    uint32_t displacement = 0;
    for (size_t i = 0; i < 4; ++i) displacement |= uint32_t{code[1 + i]} << (i * 8);
    return static_cast<int32_t>(displacement);
}

inline std::optional<size_t> replayInputSlot(std::span<uint8_t const> code) {
    size_t start = 0;
    constexpr uint8_t savedThis[] = {0x50, 0x48, 0x89, 0x0c, 0x24, 0x48, 0x8b, 0x0c, 0x24};
    bool wrapped = code.size() >= sizeof(savedThis);
    for (size_t i = 0; wrapped && i < sizeof(savedThis); ++i) wrapped = code[i] == savedThis[i];
    if (wrapped) start = sizeof(savedThis);
    code = code.subspan(start);
    if (code.size() < 10 || code[0] != 0x48 || code[1] != 0x8b || code[2] != 0x01
        || code[3] != 0x48 || code[4] != 0x8b) return std::nullopt;
    uint32_t displacement = 0;
    size_t jump = 0;
    if (code[5] == 0x40) {
        displacement = code[6];
        jump = 7;
    } else if (code[5] == 0x80 && code.size() >= 13) {
        for (size_t i = 0; i < 4; ++i) displacement |= uint32_t{code[6 + i]} << (i * 8);
        jump = 10;
    } else return std::nullopt;
    if (code.size() >= jump + 5 && code[jump] == 0x41 && code[jump + 1] == 0x5a) jump += 2;
    if (code.size() < jump + 3) return std::nullopt;
    if (code[jump] != 0x48 || code[jump + 1] != 0xff || code[jump + 2] != 0xe0
        || displacement % 8 || displacement == 0 || displacement > 32 * 8) return std::nullopt;
    return displacement / 8;
}
}
