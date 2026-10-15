#include "features/camera/CameraMovementInput.h"
#include "mc/entity/components/MoveInputComponent.h"
#include "mc/entity/components/RawMoveInputComponent.h"
#include "mc/input/MoveInputState.h"
#include <algorithm>
#include <array>

namespace lamium::camera {
bool freecameraSprintHeld(RawMoveInputComponent const& raw) {
    return raw.mRawInput->mFlagValues->test(static_cast<size_t>(MoveInputState::Flag::SprintDown));
}
DetachedCameraMotion::Vector freecameraInputAxes(RawMoveInputComponent const& raw) {
    using Flag = MoveInputState::Flag;
    auto& flags = *raw.mRawInput->mFlagValues;
    auto held = [&](Flag flag) { return flags.test(static_cast<size_t>(flag)); };
    // Flags first: summing both sources could cancel when their signs differ.
    double x = 0, z = 0;
    if (held(Flag::Right)) x += 1;
    if (held(Flag::Left)) x -= 1;
    if (held(Flag::Up)) z += 1;
    if (held(Flag::Down)) z -= 1;
    if (!held(Flag::Right) && !held(Flag::Left) && !held(Flag::Up) && !held(Flag::Down)) {
        x = raw.mRawMove->x;
        z = raw.mRawMove->z;
    }
    double y = static_cast<double>(held(Flag::JumpDown) || held(Flag::Ascend))
        - static_cast<double>(held(Flag::SneakDown) || held(Flag::Descend));
    return {std::clamp(x, -1.0, 1.0), y, std::clamp(z, -1.0, 1.0)};
}
DetachedCameraMotion::Vector consumeMovement(RawMoveInputComponent& raw) {
    using Flag = MoveInputState::Flag;
    auto& flags = *raw.mRawInput->mFlagValues;
    auto down = [&](Flag flag) { return flags.test(static_cast<size_t>(flag)); };
    // Horizontal axes have already passed through vanilla's device handling.
    // Vertical flight uses momentary jump/sneak, not the persisted sneak toggle.
    DetachedCameraMotion::Vector axes{
        raw.mRawMove->x,
        static_cast<double>(down(Flag::JumpDown) || down(Flag::Ascend))
            - static_cast<double>(down(Flag::SneakDown) || down(Flag::Descend)),
        raw.mRawMove->z,
    };
    *raw.mRawMove = Vec2{};
    *raw.mRawInput->mAnalogMoveVector = Vec2{};
    constexpr auto movementFlags = std::to_array<Flag>({
        Flag::SneakDown, Flag::SneakToggleDown, Flag::WantDownSlow, Flag::WantUpSlow,
        Flag::AscendBlock, Flag::DescendBlock,
        Flag::AscendBlock, Flag::DescendBlock, Flag::JumpDown, Flag::SprintDown,
        Flag::UpLeft, Flag::UpRight, Flag::DownLeft, Flag::DownRight,
        Flag::Up, Flag::Down, Flag::Left, Flag::Right,
        Flag::Ascend, Flag::Descend, Flag::ChangeHeight,
        Flag::SneakInputCurrentlyDown, Flag::SneakInputWasReleased, Flag::SneakInputWasPressed,
        Flag::JumpInputWasReleased, Flag::JumpInputWasPressed, Flag::JumpInputCurrentlyDown,
    });
    for (auto flag : movementFlags) flags.set(static_cast<size_t>(flag), false);
    return axes;
}
DetachedCameraMotion::Vector freecameraInputAxes(MoveInputComponent const& input) {
    using Flag = MoveInputState::Flag;
    auto& flags = *input.mRawInputState->mFlagValues;
    auto held = [&](Flag flag) { return flags.test(static_cast<size_t>(flag)); };
    double x = static_cast<double>(held(Flag::Right)) - static_cast<double>(held(Flag::Left));
    double z = static_cast<double>(held(Flag::Up)) - static_cast<double>(held(Flag::Down));
    if (x == 0 && z == 0) {
        x = input.mMove->x;
        z = input.mMove->z;
        if (x == 0 && z == 0) {
            x = input.mRawInputState->mAnalogMoveVector->x;
            z = input.mRawInputState->mAnalogMoveVector->z;
        }
    }
    double y = static_cast<double>(held(Flag::JumpDown) || held(Flag::Ascend))
        - static_cast<double>(held(Flag::SneakDown) || held(Flag::Descend));
    return {std::clamp(x, -1.0, 1.0), y, std::clamp(z, -1.0, 1.0)};
}
DetachedCameraMotion::Vector consumeMovement(MoveInputComponent& input) {
    auto axes = freecameraInputAxes(input);
    auto clearState = [](MoveInputState& state) {
        using InputFlag = MoveInputState::Flag;
        constexpr auto movementFlags = std::to_array<InputFlag>({
            InputFlag::SneakDown, InputFlag::SneakToggleDown, InputFlag::WantDownSlow, InputFlag::WantUpSlow,
            InputFlag::AscendBlock, InputFlag::DescendBlock, InputFlag::JumpDown, InputFlag::SprintDown,
            InputFlag::UpLeft, InputFlag::UpRight, InputFlag::DownLeft, InputFlag::DownRight,
            InputFlag::Up, InputFlag::Down, InputFlag::Left, InputFlag::Right,
            InputFlag::Ascend, InputFlag::Descend, InputFlag::ChangeHeight,
            InputFlag::SneakInputCurrentlyDown, InputFlag::SneakInputWasReleased, InputFlag::SneakInputWasPressed,
            InputFlag::JumpInputWasReleased, InputFlag::JumpInputWasPressed, InputFlag::JumpInputCurrentlyDown,
        });
        for (auto flag : movementFlags) state.mFlagValues->set(static_cast<size_t>(flag), false);
        *state.mAnalogMoveVector = Vec2{};
    };
    clearState(*input.mRawInputState);
    clearState(*input.mInputState);
    *input.mMove = Vec2{};
    for (auto flag : {MoveInputComponent::Flag::Sneaking, MoveInputComponent::Flag::Sprinting,
             MoveInputComponent::Flag::WantUp, MoveInputComponent::Flag::WantDown,
             MoveInputComponent::Flag::Jumping})
        input.mFlagValues->set(static_cast<size_t>(flag), false);
    return axes;
}
}
