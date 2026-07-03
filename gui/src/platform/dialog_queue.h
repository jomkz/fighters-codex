#pragma once
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace platform {

// Completion plumbing for async file dialogs, kept free of any SDL types so
// the contract is unit-testable. One dialog may be pending at a time — this
// preserves the modal mental model the GUI was written against.
//
// Threading contract: Begin() and Pump() run on the main thread; Complete()
// may be called from any thread (SDL dialog callbacks carry no thread
// guarantee). Continuations always run on the main thread, inside Pump().
//
// Continuation discipline for callers (an IGui lesson — frames pass between
// request and completion): capture inputs BY VALUE at request time, and
// re-validate any app state a continuation mutates on arrival.
class DialogQueue {
public:
    using Continuation = std::function<void(std::vector<std::string>)>;

    // Register a continuation for the next completion. Returns false (and
    // registers nothing) if a dialog is already pending.
    bool Begin(Continuation done);

    // Deliver a result (empty = cancelled). Any thread. No-op if nothing is
    // pending.
    void Complete(std::vector<std::string> result);

    // Run the continuation on the main thread if a result has arrived.
    void Pump();

    bool Busy() const;

    // Drop any pending continuation without running it (app teardown).
    void Shutdown();

private:
    mutable std::mutex       m_mutex;
    Continuation             m_cont;
    std::vector<std::string> m_result;
    bool                     m_busy = false;
    bool                     m_done = false;
};

// Append `ext` (no dot) when `path` has no extension — replaces the Win32
// lpstrDefExt behaviour that SDL save dialogs don't provide. Empty inputs
// pass through unchanged.
std::string EnsureExtension(std::string path, const char* ext);

} // namespace platform
