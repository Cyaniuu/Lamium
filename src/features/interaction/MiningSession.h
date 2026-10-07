#pragma once
namespace lamium::interaction::mining {
// L-73 B: one owner for vanilla's mining session. Breaking Restriction,
// Tool Protection and Tool Switch each answer for the client player's mining
// call, in that order; the first answer other than Proceed is applied and the
// later features are not asked (their answers may move items).
enum class Gate {
    Proceed,
    Pause,   // Keep the held session alive but make no progress.
    End,     // Refuse: a start fails, a continue ends the session.
    Restart, // Begin again on this block (after a tool was fetched).
};
enum class Step {
    Vanilla,      // Run vanilla's continue.
    Keep,         // Return true without progress.
    StopAndKeep,  // Abort the cracked block through vanilla's stop, then keep.
    End,          // Return false.
    Start,        // Re-enter vanilla's start on this block.
};
// A pause aborts progress already made, or the block keeps cracking under a
// crosshair that rests elsewhere; the next allowed call starts afresh so the
// server gets a start action again (L-36).
class Session {
public:
    void started() { restartPending = false; }
    Step next(Gate gate, bool progress) {
        switch (gate) {
        case Gate::End: return Step::End;
        case Gate::Pause:
            if (!progress) return Step::Keep;
            restartPending = true;
            return Step::StopAndKeep;
        case Gate::Restart: restartPending = false; return Step::Start;
        case Gate::Proceed: break;
        }
        if (!restartPending) return Step::Vanilla;
        restartPending = false;
        return Step::Start;
    }
    bool pending() const { return restartPending; }
    void reset() { restartPending = false; }
private:
    bool restartPending = false;
};
}
