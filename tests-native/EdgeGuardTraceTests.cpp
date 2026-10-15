#include "features/interaction/EdgeGuardTracePlan.h"
#include "mc/entity/utilities/IReplayableActorInput.h"
#include "ll/api/memory/Memory.h"
#include <span>

void check(bool, char const*);

void nativeEdgeGuardTraceTests() {
    auto slot = [](auto member) {
        check(sizeof(member) == sizeof(void*), "replay input member pointers use the supported single-pointer ABI");
        auto* code = static_cast<uint8_t const*>(ll::memory::toFuncPtr(member));
        size_t length = 16;
        if (code[0] == 0xe9) {
            auto displacement = lamium::interaction::replayInputThunkJump(std::span{code, size_t{5}});
            if (!displacement) return std::optional<size_t>{};
            code = reinterpret_cast<uint8_t const*>(reinterpret_cast<std::intptr_t>(code) + 5 + *displacement);
            length = 24;
        } else if (code[0] == 0x50) {
            length = 24;
        }
        return lamium::interaction::replayInputSlot(std::span{code, length});
    };
    auto capture = slot(static_cast<void (IReplayableActorInput::*)(MoveInputComponent const&)>(&IReplayableActorInput::capture));
    auto pre = slot(&IReplayableActorInput::preApplyInput), post = slot(&IReplayableActorInput::postApplyInput);
    check(capture && pre && post && capture != pre && capture != post && pre != post,
          "the compiler's actual SDK thunks resolve three distinct replay input methods without hardcoded slots");
}
