#include "features/schematic/EntityModels.h"
#include "features/schematic/RestPose.h"
#include "overlay/FaceMaterial.h"
#include "app/Runtime.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/gui/screens/ScreenContext.h"
#include "mc/client/model/geom/Cube.h"
#include "mc/client/model/geom/ModelPart.h"
#include "mc/client/model/models/DataDrivenGeometry.h"
#include "mc/client/model/models/Model.h"
#include "mc/client/renderer/RenderMaterialGroup.h"
#include "mc/client/renderer/Tessellator.h"
#include "mc/client/renderer/actor/ActorRenderDispatcher.h"
#include "mc/client/renderer/actor/DataDrivenRenderer.h"
#include "mc/common/client/renderer/helpers/MeshHelpers.h"
#include "mc/deps/core_graphics/enums/PrimitiveMode.h"
#include "mc/deps/minecraft_renderer/renderer/MaterialPtr.h"
#include "mc/deps/minecraft_renderer/renderer/MeshData.h"
#include "mc/deps/minecraft_renderer/renderer/TexturePtr.h"
#include "mc/deps/minecraft_renderer/resources/ClientTexture.h"
#include "mc/deps/minecraft_renderer/resources/OffscreenCaptureDescription.h"
#include "mc/deps/minecraft_renderer/resources/ServerTexture.h"
#include "mc/util/molang/ExpressionNode.h"
#include "mc/world/actor/animation/ActorAnimationGroup.h"
#include "mc/world/actor/animation/ActorAnimationInfo.h"
#include "mc/world/actor/animation/ActorSkeletalAnimation.h"
#include "mc/world/actor/animation/BoneAnimation.h"
#include "mc/world/actor/animation/BoneAnimationChannel.h"
#include "mc/world/actor/animation/BoneOrientation.h"
#include "mc/world/actor/animation/BoneTransformType.h"
#include "mc/world/actor/animation/ChannelTransform.h"
#include "mc/world/actor/animation/ChannelTransform_Float.h"
#include "mc/world/actor/animation/KeyFrameTransform.h"
#include "mc/world/actor/animation/KeyFrameTransformData.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <variant>

namespace lamium::schematic::models {
namespace {
// At most this many models a frame; the rest keep their frames.
constexpr size_t maxModels = 64;

void log(std::string const& text) {
    try { Runtime::instance().self().getLogger().info("Schematic models: {}", text); } catch (...) {}
}
// Trace builds (xmake f --schematic_model_trace=y) log how each model is
// chosen and posed, once per identifier.
std::string traced; // the identifier being set up
#ifdef LAMIUM_SCHEMATIC_MODEL_TRACE
void trace(std::string const& text) { log(text); }
#else
void trace(std::string const&) {}
#endif

// The constant pose of an entity, by bone name: rotations in degrees and
// position offsets in model pixels, both added to the bone's rest values.
using Bones = std::map<std::string, Vec3>;
struct Pose {
    Bones rotations, positions;
};
struct Entry {
    std::shared_ptr<DataDrivenRenderer> renderer; // null: no model, the frame stays
    DataDrivenGeometry* geometry = nullptr;
    Pose pose;
    bool tinted = false; // drawn with light-blue faces instead of its skin
    bool traced = false;
};
std::map<std::string, Entry> entries;
// What compileCubes adds to each part's mirrored cubes, measured once.
std::map<ModelPart const*, std::optional<glm::vec3>> compiledOffsets;

// A model holds several geometries (adult, baby, charged, variants); the one
// a live entity uses is picked by its render controller, which needs the
// entity. Take "default", else the first that is not a baby or a charged layer.
DataDrivenGeometry* plainGeometry(Model& model) {
    DataDrivenGeometry* first = nullptr;
    DataDrivenGeometry* plain = nullptr;
    for (auto const& geometry : *model.mGeometries) {
        if (!geometry) continue;
        auto name = geometry->mGeoName->getString();
        if (name == "default") return geometry.get();
        if (!first) first = geometry.get();
        if (!plain && name.find("baby") == std::string::npos && name.find("charged") == std::string::npos) plain = geometry.get();
    }
    return plain ? plain : first;
}

// A bone's rest value: position (0) or rotation in degrees (1).
Vec3 restValue(DataDrivenGeometry const& geometry, std::string const& bone, int kind) {
    for (auto const& rest : *geometry.mDefaultBoneOrientations)
        if (rest.mName->getString() == bone) return reinterpret_cast<Vec3 const*>(&rest.mDefaultTransform->mData)[kind];
    return {};
}
// The first key frame of a channel when every axis is entity-free Molang.
std::optional<Vec3> constantChannel(BoneAnimationChannel const& channel, Vec3 self) {
    if (channel.mKeyFrames->empty()) return std::nullopt;
    auto const& prePost = *channel.mKeyFrames->front().mPrePost;
    if (prePost.empty()) return std::nullopt;
    Vec3 sum{};
    for (auto const& f : *prePost.front().mChannelTransforms_Floats) {
        auto const* v = reinterpret_cast<float const*>(&f.mXYZ);
        sum.x += v[0]; sum.y += v[1]; sum.z += v[2];
    }
    float const selves[3] = {self.x, self.y, self.z};
    float* axes[3] = {&sum.x, &sum.y, &sum.z};
    for (auto const& transform : *prePost.front().mChannelTransforms) {
        auto const* nodes = reinterpret_cast<ExpressionNode const*>(&transform.mXYZ);
        for (int axis = 0; axis < 3; ++axis) {
            std::string text;
            try { text = nodes[axis].getExpressionString(); } catch (...) {}
            auto value = constantMolang(text, selves[axis]);
            if (!value) return std::nullopt;
            *axes[axis] += *value;
        }
    }
    return sum;
}

// The entity's resting pose from the animation group. The entity's own
// animation list is not reachable without the entity, so its animations are
// found by name: "animation.wolf.*" setup/general ones (and an armor stand's
// default_pose) pose the model. Entities also share animations (a witch uses
// the villager's crossed arms): a bone the entity's own animations leave
// alone takes the constant rotation other entities' setup/general animations
// agree on, counting only animations whose every bone is in this model.
Pose restPose(IClientInstance& client, std::string const& id, DataDrivenGeometry const& geometry) {
    Pose pose;
    auto group = client.getActorAnimationGroup();
    if (!group) return pose;
    std::string name = id.substr(id.find(':') + 1);
    if (name.ends_with("_v2")) name.resize(name.size() - 3);
    std::string prefix = "animation." + name + ".";
    struct Shared {
        std::optional<Vec3> rotation;
        int count = 0;
    };
    std::map<std::string, Shared> shared;
    std::set<std::string> touched;
    std::lock_guard lock(static_cast<std::mutex&>(group->mActorAnimationMutex));
    auto fitsModel = [&](ActorSkeletalAnimation const& animation) {
        for (auto const& bone : *animation.mBoneAnimations) {
            bool found = false;
            for (auto const& part : *geometry.mModelParts) found = found || part.mName->getString() == bone.mBoneName->getString();
            if (!found) return false;
        }
        return true;
    };
    for (auto const& [key, info] : *group->mAnimations) {
        if (!info) continue;
        auto const& animationName = key.getString();
        auto const* animation = static_cast<std::unique_ptr<ActorSkeletalAnimation> const&>(info->mPtr).get();
        if (!animation) continue;
        bool own = animationName.starts_with(prefix);
        bool general = animationName.find("setup") != std::string::npos || animationName.find("general") != std::string::npos;
        // "animation.chicken.general.v1.0" is the legacy copy kept for old
        // packs; the current one has the same name without the suffix.
        if (auto version = animationName.rfind(".v"); version != std::string::npos && version > prefix.size()
            && group->mAnimations->contains(HashedString{animationName.substr(0, version)}))
            continue;
        if (!own && !general) continue;
        if (!own && !fitsModel(*animation)) continue;
        // Own animations count only when they fit this model too: a horse's
        // name also covers its legacy animations, whose bones are gone and
        // whose head offset dropped the head below the neck.
        bool used = own && (general || animationName.ends_with(".default_pose")) && fitsModel(*animation);
        std::string channels;
        for (auto const& bone : *animation->mBoneAnimations) {
            auto boneName = bone.mBoneName->getString();
            for (auto const& channel : *bone.mAnimationChannels) {
                bool moves = channel.mBoneTransformType == BoneTransformType::Position;
                if (!moves && channel.mBoneTransformType != BoneTransformType::Rotation) continue;
                // `this` is the bone's rest value, so the result is the
                // offset from it ("90 - this" turns the bone to 90).
                auto value = constantChannel(channel, restValue(geometry, boneName, moves ? 0 : 1));
                channels += value ? std::format(" {} {} {:.1f},{:.1f},{:.1f}", boneName, moves ? "pos" : "rot", value->x, value->y, value->z)
                                  : std::format(" {} {} ?", boneName, moves ? "pos" : "rot");
                if (own && !moves) touched.insert(boneName);
                if (used && value) {
                    auto& at = (moves ? pose.positions : pose.rotations)[boneName];
                    at = at + *value;
                }
                if (own || moves) continue;
                auto& entry = shared[boneName];
                if (!value) entry.count = -1000;
                else if (entry.count == 0) entry.rotation = value;
                else if (entry.rotation && glm::length(glm::vec3{entry.rotation->x - value->x, entry.rotation->y - value->y, entry.rotation->z - value->z}) > .01f)
                    entry.rotation.reset();
                ++entry.count;
            }
        }
        if (own) trace(std::format("{}: {}{}{}", id, animationName, used ? " (used)" : "", channels));
    }
    for (auto const& [bone, entry] : shared)
        if (entry.rotation && entry.count >= 2 && !touched.contains(bone) && (entry.rotation->x || entry.rotation->y || entry.rotation->z))
            pose.rotations[bone] = *entry.rotation;
    return pose;
}

// Parts are placed in the y-up space the cubes are stored in: each turns
// about its bone's pivot (absolute, y-up). Bedrock turns x and y the other
// way round from the right hand; in the stored (x-mirrored) space that
// leaves x and z reversed.
glm::mat4 upRotation(glm::mat4 m, Vec3 degrees) {
    m = glm::rotate(m, glm::radians(-degrees.z), glm::vec3{0, 0, 1});
    m = glm::rotate(m, glm::radians(degrees.y), glm::vec3{0, 1, 0});
    return glm::rotate(m, glm::radians(-degrees.x), glm::vec3{1, 0, 0});
}
// A cube's own rotation about its pivot (radians in the cube).
glm::mat4 cubeTurn(Cube const& cube) {
    auto turn = *cube.mRotation;
    if (!turn.x && !turn.y && !turn.z) return glm::mat4{1.f};
    auto pivot = *cube.mCubePivot;
    glm::vec3 p{pivot.x, pivot.y, pivot.z};
    return glm::translate(upRotation(glm::translate(glm::mat4{1.f}, p), Vec3{glm::degrees(turn.x), glm::degrees(turn.y), glm::degrees(turn.z)}), -p);
}
glm::vec4 corner(Cube const& cube, int k) {
    auto o = *cube.mOrigin, z = *cube.mSize;
    return {k & 1 ? o.x + z.x : o.x, k & 2 ? o.y + z.y : o.y, k & 4 ? o.z + z.z : o.z, 1.f};
}

// compileCubes emits each cube mirrored into a y-down space plus an offset
// of the part's own; measure it once from an untransformed compile so the
// faces follow the same y-up chain as the outlines (the part's own
// placement, translateTo, put the armor stand's arms off). Box centers, not
// corners: inflated cubes (a witch's robe, a drowned's outer layer) grow on
// every side, and a corner match shifted the whole part.
std::optional<glm::vec3> compiledOffset(ScreenContext& screen, ModelPart& part) {
    if (auto found = compiledOffsets.find(&part); found != compiledOffsets.end()) return found->second;
    auto& offset = compiledOffsets[&part];
    if (part.mCubes->empty()) return offset;
    Tessellator scratch(screen.tessellator.mBufferResourceService);
    scratch.begin({}, mce::PrimitiveMode::QuadList, static_cast<int>(part.mCubes->size()) * 24, false);
    static_cast<bool&>(scratch.mApplyTransform) = false;
    part.compileCubes(scratch);
    auto const& positions = *static_cast<mce::MeshData&>(scratch.mMeshData).mPositions;
    if (positions.empty()) return offset;
    glm::vec3 compiledLow{1e9f}, compiledHigh{-1e9f}, storedLow{1e9f}, storedHigh{-1e9f};
    for (auto const& v : positions) { compiledLow = glm::min(compiledLow, v); compiledHigh = glm::max(compiledHigh, v); }
    for (auto const& cube : *part.mCubes) {
        auto turn = cubeTurn(cube);
        for (int k = 0; k < 8; ++k) {
            auto v = glm::vec3(turn * corner(cube, k));
            glm::vec3 mirrored{-v.x, -v.y, v.z};
            storedLow = glm::min(storedLow, mirrored);
            storedHigh = glm::max(storedHigh, mirrored);
        }
    }
    offset = (compiledLow + compiledHigh - storedLow - storedHigh) / 2.f;
    trace(std::format("{} part {}: {} cubes, offset {:.2f},{:.2f},{:.2f}, compiled size {:.1f},{:.1f},{:.1f}, stored size {:.1f},{:.1f},{:.1f}",
        traced, part.mName->getString(), part.mCubes->size(), offset->x, offset->y, offset->z, compiledHigh.x - compiledLow.x, compiledHigh.y - compiledLow.y,
        compiledHigh.z - compiledLow.z, storedHigh.x - storedLow.x, storedHigh.y - storedLow.y, storedHigh.z - storedLow.z));
    return offset;
}

// The renderer's default skin is only one texture of the entity's set: for
// entities whose look is layered by its state (horse coat and armor, llama
// decor, villager profession) it is a layer, not the body. Without the
// entity the right set cannot be chosen, so those draw light-blue faces.
bool layerSkin(std::string const& path) {
    for (auto const* layer : {"/armor/", "/decor/", "markings", "/professions/", "_none", "baby", "saddle", "overlay"})
        if (path.find(layer) != std::string::npos) return true;
    return path.empty();
}

struct Build {
    ScreenContext& screen;
    DataDrivenGeometry const& geometry;
    Pose const& pose;
    Tessellator& faces;
    Tessellator& lines;
    bool tinted;
    std::string const& id;
    bool traceParts;
};
void addPart(Build& build, ModelPart& part, glm::mat4 const& parent, int depth) {
    // The part's mRot is not used: the game writes a live entity's pose into
    // the shared model (one posed armor stand moved every ghost).
    if (depth > 16 || part.mNeverRender) return;
    auto name = part.mName->getString();
    auto rest = restValue(build.geometry, name, 1);
    if (auto found = build.pose.rotations.find(name); found != build.pose.rotations.end()) rest = rest + found->second;
    glm::mat4 turn{1.f};
    if (auto found = build.pose.positions.find(name); found != build.pose.positions.end())
        turn = glm::translate(turn, glm::vec3{found->second.x, found->second.y, found->second.z});
    auto const& bones = *build.geometry.mDefaultBoneOrientations;
    int index = part.mBoneOrientationIndex;
    if (index >= 0 && index < static_cast<int>(bones.size())) {
        auto pivot = *bones[index].mPivot;
        glm::vec3 p{pivot.x, pivot.y, pivot.z};
        turn = glm::translate(upRotation(glm::translate(turn, p), rest), -p);
    }
    glm::mat4 world = parent * turn;
#ifdef LAMIUM_SCHEMATIC_MODEL_TRACE
    if (build.traceParts) {
        std::string cubes;
        for (auto const& cube : *part.mCubes) {
            auto o = *cube.mOrigin, z = *cube.mSize, t = *cube.mRotation;
            auto cp = *cube.mCubePivot;
            cubes += std::format(" [{:.1f},{:.1f},{:.1f} size {:.1f},{:.1f},{:.1f} turn {:.2f},{:.2f},{:.2f} about {:.1f},{:.1f},{:.1f}]", o.x, o.y, o.z, z.x, z.y, z.z,
                t.x, t.y, t.z, cp.x, cp.y, cp.z);
        }
        auto restPos = restValue(build.geometry, name, 0);
        auto pivot = index >= 0 && index < static_cast<int>(bones.size()) ? *bones[index].mPivot : Vec3{};
        trace(std::format("{} {}: pivot {:.1f},{:.1f},{:.1f} rest pos {:.1f},{:.1f},{:.1f} rot {:.1f},{:.1f},{:.1f}{}", build.id, name, pivot.x, pivot.y, pivot.z,
            restPos.x, restPos.y, restPos.z, rest.x, rest.y, rest.z, cubes));
    }
#endif
    // Faces: a compiled vertex is mirror(stored) + offset.
    if (build.tinted) {
        constexpr int sides[6][4] = {{0,2,6,4},{1,5,7,3},{0,4,5,1},{2,3,7,6},{0,1,3,2},{4,6,7,5}};
        for (auto const& cube : *part.mCubes) {
            auto at = world * cubeTurn(cube);
            glm::vec3 c[8];
            for (int k = 0; k < 8; ++k) c[k] = glm::vec3(at * corner(cube, k));
            for (auto const& side : sides) {
                for (int k = 0; k < 4; ++k) build.faces.vertex(c[side[k]].x, c[side[k]].y, c[side[k]].z);
                for (int k = 3; k >= 0; --k) build.faces.vertex(c[side[k]].x, c[side[k]].y, c[side[k]].z);
            }
        }
    } else if (auto offset = compiledOffset(build.screen, part)) {
        static_cast<bool&>(build.faces.mApplyTransform) = true;
        static_cast<glm::mat4x4&>(build.faces.mTransformMatrix) = glm::translate(glm::scale(world, glm::vec3{-1, -1, 1}), -*offset);
        part.compileCubes(build.faces);
    }
    constexpr int edges[12][2] = {{0,1},{2,3},{4,5},{6,7},{0,2},{1,3},{4,6},{5,7},{0,4},{1,5},{2,6},{3,7}};
    for (auto const& cube : *part.mCubes) {
        auto at = world * cubeTurn(cube);
        glm::vec3 c[8];
        for (int k = 0; k < 8; ++k) c[k] = glm::vec3(at * corner(cube, k));
        for (auto [a, b] : edges) {
            build.lines.vertex(c[a].x, c[a].y, c[a].z);
            build.lines.vertex(c[b].x, c[b].y, c[b].z);
        }
    }
    for (auto* child : *part.mChildren) if (child) addPart(build, *child, world, depth + 1);
}

Entry& entry(IClientInstance& client, std::string const& id) {
    if (auto found = entries.find(id); found != entries.end()) return found->second;
    Entry& made = entries[id];
    traced = id;
    try {
        if (auto dispatcher = client.getEntityRenderDispatcher()) made.renderer = dispatcher->getDataDrivenRenderer(HashedString{id});
        auto* model = made.renderer ? static_cast<std::shared_ptr<Model>&>(made.renderer->mModel).get() : nullptr;
        made.geometry = model ? plainGeometry(*model) : nullptr;
        if (made.geometry) made.pose = restPose(client, id, *made.geometry);
        if (made.geometry) {
            auto const& skin = static_cast<mce::TexturePtr const&>(made.renderer->mDefaultSkin);
            std::string geometries;
            for (auto const& geometry : *model->mGeometries) if (geometry) geometries += " " + geometry->mGeoName->getString();
            std::string pose;
            for (auto const& [bone, v] : made.pose.rotations) pose += std::format(" {} rot {:.1f},{:.1f},{:.1f}", bone, v.x, v.y, v.z);
            for (auto const& [bone, v] : made.pose.positions) pose += std::format(" {} pos {:.1f},{:.1f},{:.1f}", bone, v.x, v.y, v.z);
            std::string path = skin.mResourceLocationPtr.get() ? skin.mResourceLocationPtr.get()->mPath->value : std::string();
            made.tinted = layerSkin(path);
            trace(std::format("{}: geometries{}; drawing {}; texture {}{}; pose{}", id, geometries, made.geometry->mGeoName->getString(),
                path.empty() ? std::string("none") : path, made.tinted ? " (a layer: light-blue faces)" : "", pose));
        }
    } catch (...) {
        made.geometry = nullptr;
    }
    if (!made.geometry) {
        made.renderer.reset();
        log(std::format("no model for {}; it stays a frame", id));
    }
    return made;
}
} // namespace

std::vector<bool> draw(ScreenContext& screen, IClientInstance& client, Vec3 const& camera, std::vector<Spot> const& spots,
                       std::function<void(std::function<void()> const&)> const& inWorld) {
    std::vector<bool> drawn(spots.size(), false);
    mce::MaterialPtr lineMaterial(mce::RenderMaterialGroup::common(), HashedString{"debug"});
    // The faces of skinless models use the overlay's face material, which
    // changes with the graphics mode (the hologram one draws nothing in Simple).
    auto tint = overlay::faceMaterial(client);
    auto const& tintMaterial = tint.material;
    size_t count = 0;
    for (size_t i = 0; i < spots.size() && count < maxModels; ++i) {
        auto const& spot = spots[i];
        auto& model = entry(client, spot.identifier);
        if (!model.geometry) continue;
        auto const& material = static_cast<mce::MaterialPtr const&>(model.renderer->mEntityAlphatestMaterial);
        if (!(model.tinted ? tintMaterial : material).mRenderMaterialInfoPtr) continue;
        auto& parts = *model.geometry->mModelParts;
        int cubes = 0;
        for (auto const& part : parts) cubes += static_cast<int>(part.mCubes->size());
        if (!cubes) continue;
        // Camera-relative, turned to the saved facing, model pixels to blocks.
        glm::mat4 base = glm::translate(glm::mat4{1.f}, glm::vec3{static_cast<float>(spot.at.x - camera.x), static_cast<float>(spot.at.y - camera.y),
                                                                  static_cast<float>(spot.at.z - camera.z)});
        base = glm::scale(glm::rotate(base, glm::radians(180.f - spot.yaw), glm::vec3{0, 1, 0}), glm::vec3{1.f / 16});
        Tessellator faces(screen.tessellator.mBufferResourceService), lines(screen.tessellator.mBufferResourceService);
        faces.begin({}, mce::PrimitiveMode::QuadList, cubes * 48, false);
        lines.begin({}, mce::PrimitiveMode::LineList, cubes * 24, false);
        lines.color(.35f, .85f, 1.f, 1.f);
        if (model.tinted) faces.color(.35f, .85f, 1.f, tint.alpha);
        Build build{screen, *model.geometry, model.pose, faces, lines, model.tinted, spot.identifier, !model.traced};
        model.traced = true;
        for (auto root : *model.geometry->mRootModelParts)
            if (root < parts.size()) addPart(build, parts[root], base, 0);
        using Texture = std::variant<std::monostate, mce::TexturePtr, mce::ClientTexture, mce::ServerTexture>;
        Texture texture{static_cast<mce::TexturePtr const&>(model.renderer->mDefaultSkin)};
        inWorld([&] {
            if (model.tinted) MeshHelpers::renderMeshImmediately(screen, faces, tintMaterial, OffscreenCaptureDescription{});
            else MeshHelpers::renderMeshImmediately(screen, faces, material, texture, OffscreenCaptureDescription{});
            if (lineMaterial.mRenderMaterialInfoPtr) MeshHelpers::renderMeshImmediately(screen, lines, lineMaterial, OffscreenCaptureDescription{});
        });
        drawn[i] = true;
        ++count;
    }
    return drawn;
}

void reset() {
    entries.clear();
    compiledOffsets.clear();
}
} // namespace lamium::schematic::models
