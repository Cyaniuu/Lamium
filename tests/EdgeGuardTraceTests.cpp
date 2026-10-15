#include "features/interaction/EdgeGuardTracePlan.h"
#include <array>

void check(bool, char const*);

void edgeGuardTraceTests() {
    using lamium::interaction::replayInputSlot;
    using lamium::interaction::replayInputThunkJump;
    constexpr std::array<uint8_t, 10> pre{0x48,0x8b,0x01,0x48,0x8b,0x40,0x08,0x48,0xff,0xe0};
    constexpr std::array<uint8_t, 10> post{0x48,0x8b,0x01,0x48,0x8b,0x40,0x10,0x48,0xff,0xe0};
    constexpr std::array<uint8_t, 13> capture{0x48,0x8b,0x01,0x48,0x8b,0x80,0xb0,0,0,0,0x48,0xff,0xe0};
    check(replayInputSlot(pre) == 1 && replayInputSlot(post) == 2 && replayInputSlot(capture) == 22,
          "measured SDK virtual-call thunks resolve the actual capture and application slots");
    for (size_t length = 0; length < capture.size(); ++length)
        check(!replayInputSlot(std::span{capture}.first(length)), "incomplete virtual-call thunks fail open");
    auto unsupported = capture;
    unsupported[10] = 0x90;
    check(!replayInputSlot(unsupported), "an unsupported virtual jump is not guessed");
    unsupported = capture; unsupported[6] = 0xb1;
    check(!replayInputSlot(unsupported), "unaligned virtual entry displacement is rejected");
    unsupported = capture; unsupported[7] = 0xff;
    check(!replayInputSlot(unsupported), "out-of-range virtual entry displacement is rejected");
    unsupported = capture; unsupported[6] = 0;
    check(!replayInputSlot(unsupported), "the destructor slot is never used as an input hook");
    constexpr std::array<uint8_t, 5> jump{0xe9, 0x10, 0, 0, 0};
    check(replayInputThunkJump(jump) == 16, "compiler thunk relative jumps are decoded as signed offsets");
    constexpr std::array<uint8_t, 22> wrapped{
        0x50,0x48,0x89,0x0c,0x24,0x48,0x8b,0x0c,0x24,
        0x48,0x8b,0x01,0x48,0x8b,0x80,0xb0,0,0,0,0x48,0xff,0xe0};
    check(replayInputSlot(wrapped) == 22, "the compiler's stack-preserving virtual thunk resolves its slot");
    constexpr std::array<uint8_t, 21> popR10{
        0x50,0x48,0x89,0x0c,0x24,0x48,0x8b,0x0c,0x24,
        0x48,0x8b,0x01,0x48,0x8b,0x40,0x08,0x41,0x5a,0x48,0xff,0xe0};
    check(replayInputSlot(popR10) == 1, "the compiler's register-restoring virtual thunk resolves its slot");
}
