#pragma once
#include <cstdint>

enum class PlayState { Stopped, Playing, Paused };

namespace platform {

// One-shot preview player for FA's 8-bit mono PCM clips (5000/8000/11025/
// 22050 Hz). Control-plane API — all methods are called from the GUI thread;
// the backend's realtime data path is an implementation detail behind Impl,
// crossing threads with atomics only.
//
// Semantics mirror the original waveOut player: Update() runs once per frame
// and catches natural end-of-clip (never torn down from the audio thread);
// Position() reports the playhead sample while playing and resumeFrom
// otherwise.
//
// Timing model: the playhead and natural end-of-clip are driven by an
// elapsed-time clock advanced in Update(), not by the realtime audio
// callback — so the state machine is deterministic and independent of
// backend scheduling. The no-arg Update() reads a real steady_clock delta
// (production); the Update(double) overload injects an explicit delta so
// tests can advance past clip end without sleeping on wall-clock (#401).
// The miniaudio device still renders the audible output; its callback only
// streams samples and never mutates control-plane state.
class AudioPlayer {
public:
    AudioPlayer();
    ~AudioPlayer();
    AudioPlayer(const AudioPlayer&)            = delete;
    AudioPlayer& operator=(const AudioPlayer&) = delete;

    bool IsPlaying() const;
    bool IsPaused() const { return m_state == PlayState::Paused; }

    void Update();               // once per frame: advance by real elapsed time
    void Update(double dtSeconds); // advance by an explicit delta (deterministic)
    int  Position() const;  // current sample for the playhead display

    bool Play(const uint8_t* pcm, int samples, int rate, int fromSample);
    void Pause();           // pause at current position
    void PauseAt(int sample); // right-click seek: stop audio, park the head
    void Stop();

    int resumeFrom = 0;     // where Play resumes from; the UI seeks by writing it

private:
    struct Impl;
    Impl*     m_impl  = nullptr;
    PlayState m_state = PlayState::Stopped;
};

} // namespace platform
