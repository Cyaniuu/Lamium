#pragma once
#include <optional>
class IClientInstance;
// Client-side counts for the Debug View (BACKLOG L-57). Read-only, at most once
// a second; a count that cannot be read is left out.
namespace lamium::information {
struct ClientCounters {
    std::optional<int> entities;  // client actors in the player's dimension
    std::optional<int> chunks;    // chunks the client's chunk source holds for that dimension
    std::optional<int> particles; // legacy particles plus particle-system particles
};
ClientCounters clientCounters(IClientInstance&);
}
