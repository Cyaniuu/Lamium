#include "features/camera/CameraMovementInput.h"
#include "mc/entity/components/MoveInputComponent.h"
#include "mc/entity/components/RawMoveInputComponent.h"
#include <iostream>
#include <stdexcept>
#include <bitset>

void check(bool value, char const* message) {
    if (!value) throw std::runtime_error(message);
}
int main() try {
    extern void nativeEdgeGuardTraceTests();
    nativeEdgeGuardTraceTests();
    extern void nativeEdgeGuardReplayTests();
    nativeEdgeGuardReplayTests();
    using Flag = MoveInputState::Flag;
    enum class PacketFlag {
        Move = 10, Sneaking = 8, SneakDown = 9, Sprinting = 20,
        SneakToggle = 23, StartSneaking = 27, StartSprinting = 25, Jump = 31,
        StartFlying = 42, Use = 34
    };
    std::bitset<65> fullPacketFlags;
    for (auto flag : {PacketFlag::Move, PacketFlag::Sneaking, PacketFlag::SneakDown,
             PacketFlag::Sprinting, PacketFlag::SneakToggle, PacketFlag::StartSneaking,
             PacketFlag::StartSprinting, PacketFlag::Jump, PacketFlag::StartFlying, PacketFlag::Use})
        fullPacketFlags.set(static_cast<size_t>(flag));
    lamium::camera::clearMovementFlags(fullPacketFlags,
        PacketFlag::Move, PacketFlag::Sneaking, PacketFlag::SneakDown,
        PacketFlag::Sprinting, PacketFlag::SneakToggle, PacketFlag::StartSneaking,
        PacketFlag::StartSprinting, PacketFlag::Jump, PacketFlag::StartFlying);
    check(!fullPacketFlags.test(static_cast<size_t>(PacketFlag::Move))
        && !fullPacketFlags.test(static_cast<size_t>(PacketFlag::Sneaking))
        && !fullPacketFlags.test(static_cast<size_t>(PacketFlag::SneakDown))
        && !fullPacketFlags.test(static_cast<size_t>(PacketFlag::Sprinting))
        && !fullPacketFlags.test(static_cast<size_t>(PacketFlag::SneakToggle))
        && !fullPacketFlags.test(static_cast<size_t>(PacketFlag::StartSneaking))
        && !fullPacketFlags.test(static_cast<size_t>(PacketFlag::StartSprinting))
        && !fullPacketFlags.test(static_cast<size_t>(PacketFlag::Jump))
        && !fullPacketFlags.test(static_cast<size_t>(PacketFlag::StartFlying))
        && fullPacketFlags.test(static_cast<size_t>(PacketFlag::Use)),
        "player auth input suppression clears locomotion transitions while preserving unrelated actions");
    RawMoveInputComponent raw{};
    auto& flags = *raw.mRawInput->mFlagValues;
    for (size_t i = 0; i < static_cast<size_t>(Flag::Count); ++i) flags.set(i);
    *raw.mRawMove = Vec2{.25f, -.75f};
    *raw.mRawInput->mAnalogMoveVector = Vec2{.5f, -.5f};
    raw.mRawInput->mLookSlightDirField = 1;
    raw.mRawInput->mLookNormalDirField = 2;
    raw.mRawInput->mLookSmoothDirField = 3;
    auto original = raw;
    auto axes = lamium::camera::consumeMovement(raw);
    check(axes == lamium::DetachedCameraMotion::Vector{.25,0,-.75}, "native horizontal axes / opposing vertical inputs");
    check(raw.mRawMove->x == 0 && raw.mRawMove->z == 0
        && raw.mRawInput->mAnalogMoveVector->x == 0 && raw.mRawInput->mAnalogMoveVector->z == 0,
        "both extracted movement vectors must be suppressed");
    for (size_t i = 0; i < static_cast<size_t>(Flag::Count); ++i) {
        bool preserved = i == static_cast<size_t>(Flag::BlockSelectDown)
            || i == static_cast<size_t>(Flag::LookCenter)
            || i == static_cast<size_t>(Flag::SneakInputCurrentlyDown)
            || i == static_cast<size_t>(Flag::JumpInputCurrentlyDown);
        check(flags.test(i) == preserved, "movement flags cleared while unrelated flags remain");
        check(original.mRawInput->mFlagValues->test(i), "copy must not share physical input flag storage");
    }
        flags.set(static_cast<size_t>(Flag::JumpInputCurrentlyDown), false);
        flags.set(static_cast<size_t>(Flag::SneakInputCurrentlyDown), false);
        check(raw.mRawInput->mLookSlightDirField == 1 && raw.mRawInput->mLookNormalDirField == 2
        && raw.mRawInput->mLookSmoothDirField == 3, "keyboard look fields preserved");
    check(original.mRawMove->x == .25f && original.mRawInput->mAnalogMoveVector->x == .5f,
          "consumption must not mutate other input snapshots");
    flags.set(static_cast<size_t>(Flag::JumpDown));
    check(lamium::camera::consumeMovement(raw)[1] == 1, "jump ascends the camera");
    flags.set(static_cast<size_t>(Flag::JumpInputCurrentlyDown));
    check(lamium::camera::consumeMovement(raw)[1] == 1
        && flags.test(static_cast<size_t>(Flag::JumpInputCurrentlyDown)),
        "held jump state remains available for continuous camera ascent after clearing the jump edge");
    flags.set(static_cast<size_t>(Flag::JumpInputCurrentlyDown), false);
    flags.set(static_cast<size_t>(Flag::SneakToggleDown));
    check(lamium::camera::consumeMovement(raw)[1] == 0, "persistent sneak must not move the camera");
    flags.set(static_cast<size_t>(Flag::SneakDown));
    check(lamium::camera::consumeMovement(raw)[1] == -1, "held sneak descends the camera");
    flags.set(static_cast<size_t>(Flag::SneakInputCurrentlyDown));
    check(lamium::camera::consumeMovement(raw)[1] == -1
        && flags.test(static_cast<size_t>(Flag::SneakInputCurrentlyDown)),
        "held sneak state remains available for continuous camera descent after clearing the sneak edge");
    RawMoveInputComponent keys{};
    check(!lamium::camera::freecameraSprintHeld(keys), "sprint is off without a held native sprint input");
    keys.mRawInput->mFlagValues->set(static_cast<size_t>(Flag::SprintDown));
    check(lamium::camera::freecameraSprintHeld(keys), "native remapped sprint input accelerates the camera");
    lamium::camera::consumeMovement(keys);
    check(!lamium::camera::freecameraSprintHeld(keys), "sprint is captured before movement consumption");
    auto& keyFlags = *keys.mRawInput->mFlagValues;
    keyFlags.set(static_cast<size_t>(Flag::Up));
    keyFlags.set(static_cast<size_t>(Flag::Right));
    check(lamium::camera::freecameraInputAxes(keys) == lamium::DetachedCameraMotion::Vector{1, 0, 1},
          "keyboard direction flags drive WASD axes");
    keyFlags.set(static_cast<size_t>(Flag::Down));
    check(lamium::camera::freecameraInputAxes(keys) == lamium::DetachedCameraMotion::Vector{1, 0, 0},
          "opposing direction flags cancel");
    keyFlags.set(static_cast<size_t>(Flag::JumpDown));
    check(lamium::camera::freecameraInputAxes(keys)[1] == 1, "jump flag ascends the stash");
    keyFlags.set(static_cast<size_t>(Flag::Left));
    *keys.mRawMove = Vec2{.5f, .5f};
    check(lamium::camera::freecameraInputAxes(keys) == lamium::DetachedCameraMotion::Vector{0,1,0},
          "opposing keyboard directions do not fall back to stale analog motion");
    RawMoveInputComponent pad{};
    *pad.mRawMove = Vec2{.5f, -.25f};
    check(lamium::camera::freecameraInputAxes(pad) == lamium::DetachedCameraMotion::Vector{.5, 0, -.25},
          "analog vector is the fallback without flags");
    MoveInputComponent move{};
    move.mRawInputState->mFlagValues->set(static_cast<size_t>(Flag::Up));
    move.mRawInputState->mFlagValues->set(static_cast<size_t>(Flag::Right));
    move.mRawInputState->mFlagValues->set(static_cast<size_t>(Flag::JumpDown));
    move.mRawInputState->mFlagValues->set(static_cast<size_t>(Flag::JumpInputCurrentlyDown));
    move.mRawInputState->mFlagValues->set(static_cast<size_t>(Flag::SprintDown));
    move.mInputState->mFlagValues->set(static_cast<size_t>(Flag::Up));
    move.mFlagValues->set(static_cast<size_t>(MoveInputComponent::Flag::Sprinting));
    move.mFlagValues->set(static_cast<size_t>(MoveInputComponent::Flag::PersistSneak));
    *move.mMove = Vec2{1, 1};
    check(lamium::camera::freecameraInputAxes(move) == lamium::DetachedCameraMotion::Vector{1, 1, 1},
          "26.20 component flags provide keyboard and vertical movement");
    check(lamium::camera::consumeMovement(move) == lamium::DetachedCameraMotion::Vector{1, 1, 1}
        && move.mMove->x == 0 && move.mMove->z == 0
        && !move.mFlagValues->test(static_cast<size_t>(MoveInputComponent::Flag::Sneaking))
        && !move.mFlagValues->test(static_cast<size_t>(MoveInputComponent::Flag::Sprinting))
        && !move.mFlagValues->test(static_cast<size_t>(MoveInputComponent::Flag::WantUp))
        && !move.mFlagValues->test(static_cast<size_t>(MoveInputComponent::Flag::Jumping))
        && !move.mFlagValues->test(static_cast<size_t>(MoveInputComponent::Flag::PersistSneak))
        && move.mRawInputState->mFlagValues->test(static_cast<size_t>(Flag::SprintDown))
        && move.mRawInputState->mFlagValues->test(static_cast<size_t>(Flag::Up))
        && move.mRawInputState->mFlagValues->test(static_cast<size_t>(Flag::Right))
        && !move.mRawInputState->mFlagValues->test(static_cast<size_t>(Flag::JumpDown))
        && move.mRawInputState->mFlagValues->test(static_cast<size_t>(Flag::JumpInputCurrentlyDown))
        && !move.mInputState->mFlagValues->test(static_cast<size_t>(Flag::Up)),
        "26.20 movement is captured, player vertical actions are suppressed, and horizontal raw input is preserved");
    *move.mMove = Vec2{1, 1};
    move.mInputState->mFlagValues->set(static_cast<size_t>(Flag::Up));
    check(lamium::camera::freecameraInputAxes(move) == lamium::DetachedCameraMotion::Vector{1, 1, 1}
        && lamium::camera::consumeMovement(move) == lamium::DetachedCameraMotion::Vector{1, 1, 1}
        && move.mRawInputState->mFlagValues->test(static_cast<size_t>(Flag::Up))
        && !move.mRawInputState->mFlagValues->test(static_cast<size_t>(Flag::JumpDown))
        && move.mRawInputState->mFlagValues->test(static_cast<size_t>(Flag::JumpInputCurrentlyDown))
        && !move.mInputState->mFlagValues->test(static_cast<size_t>(Flag::Up)),
        "held vertical input drives continuous camera movement while player action edges stay cleared");
    move.mRawInputState->mFlagValues->set(static_cast<size_t>(Flag::JumpInputCurrentlyDown), false);
    move.mRawInputState->mFlagValues->set(static_cast<size_t>(Flag::SneakInputCurrentlyDown));
    move.mRawInputState->mFlagValues->set(static_cast<size_t>(Flag::SneakDown));
    move.mRawInputState->mFlagValues->set(static_cast<size_t>(Flag::SneakToggleDown));
    check(lamium::camera::freecameraInputAxes(move) == lamium::DetachedCameraMotion::Vector{1, -1, 1}
        && lamium::camera::consumeMovement(move) == lamium::DetachedCameraMotion::Vector{1, -1, 1}
        && move.mRawInputState->mFlagValues->test(static_cast<size_t>(Flag::SneakInputCurrentlyDown))
        && !move.mRawInputState->mFlagValues->test(static_cast<size_t>(Flag::SneakDown))
        && !move.mRawInputState->mFlagValues->test(static_cast<size_t>(Flag::SneakToggleDown)),
        "held sneak state drives continuous camera descent while player sneak edges stay cleared");
    MoveInputComponent touch{};
    *touch.mMove = Vec2{.5f, -.25f};
    check(lamium::camera::freecameraInputAxes(touch) == lamium::DetachedCameraMotion::Vector{.5, 0, -.25},
          "touch movement uses the processed move vector when direction flags are absent");
    MoveInputComponent analog{};
    *analog.mRawInputState->mAnalogMoveVector = Vec2{-.5f, .25f};
    check(lamium::camera::freecameraInputAxes(analog) == lamium::DetachedCameraMotion::Vector{-.5, 0, .25},
          "raw analog movement is used when processed movement is empty");
    std::cout << "Native camera movement extraction checks passed\n";
    return 0;
} catch (std::exception const& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
