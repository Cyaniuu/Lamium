#include "features/information/ClientCounters.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/client/renderer/game/LevelRenderer.h"
#include "mc/client/particle/ParticleEngine.h"
#include "mc/client/particlesystem/particle/ParticleSystemEngine.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/dimension/Dimension.h"
#include "mc/world/level/chunk/ChunkSource.h"
#include <chrono>
#include <limits>

namespace lamium::information {
namespace {
using Clock = std::chrono::steady_clock;
ClientCounters cached;
Clock::time_point lastRead{};
int clamped(std::uint64_t value) {
    return static_cast<int>(std::min<std::uint64_t>(value, std::numeric_limits<int>::max()));
}
ClientCounters read(IClientInstance& client) {
    ClientCounters out;
    auto* player = client.getLocalPlayer();
    if (!player) return out;
    try {
        auto const& dimension = player->getDimension();
        int count = 0;
        for (auto* actor : player->getLevel().getRuntimeActorList())
            if (actor && &actor->getDimension() == &dimension) ++count;
        out.entities = count;
    } catch (...) {}
    try {
        // The size only: the map is filled by loading threads, so it is never walked here.
        out.chunks = clamped(player->getDimension().getChunkSource().getStorage().size());
    } catch (...) {}
    try {
        if (auto* renderer = client.getLevelRenderer()) {
            std::uint64_t total = 0;
            auto& engine = renderer->getParticleEngine();
            for (auto n : static_cast<uint const(&)[105]>(engine.particleCount)) total += n;
            if (auto systems = static_cast<Bedrock::NonOwnerPointer<ParticleSystemEngine> const&>(renderer->mParticleSystemEngine))
                total += static_cast<std::uint64_t const&>(systems->mTotalParticleCount);
            out.particles = clamped(total);
        }
    } catch (...) {}
    return out;
}
}
ClientCounters clientCounters(IClientInstance& client) {
    auto now = Clock::now();
    if (now - lastRead >= std::chrono::seconds(1)) {
        cached = read(client);
        lastRead = now;
    }
    return cached;
}
}
