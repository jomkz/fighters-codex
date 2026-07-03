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
class AudioPlayer {
public:
    AudioPlayer();
    ~AudioPlayer();
    AudioPlayer(const AudioPlayer&)            = delete;
    AudioPlayer& operator=(const AudioPlayer&) = delete;

    bool IsPlaying() const;
    bool IsPaused() const { return m_state == PlayState::Paused; }

    void Update();          // once per frame: finish natural end-of-clip
    int  Position() const;  // current sample for the playhead display

    bool Play(const uint8_t* pcm, int samples, int rate, int fromSample);
    void Pause();           // pause at current position
    void PauseAt(int sample); // right-click seek: stop audio, park the head
    void Stop();

    int resumeFrom = 0;     // where Play resumes from; the UI seeks by writing it

private:
    PlayState m_state = PlayState::Stopped;
};

} // namespace platform
