// Stub backend — playback lands with the miniaudio implementation (#87).
// The API and state machine match the header contract so the audio editor
// compiles and runs on both platforms; Play reports failure and the
// transport stays stopped.
#include "audio_player.h"

namespace platform {

AudioPlayer::AudioPlayer() = default;
AudioPlayer::~AudioPlayer() = default;

bool AudioPlayer::IsPlaying() const { return false; }

void AudioPlayer::Update() {}

int AudioPlayer::Position() const { return resumeFrom; }

bool AudioPlayer::Play(const uint8_t*, int, int, int fromSample) {
    resumeFrom = fromSample;
    m_state    = PlayState::Stopped;
    return false;
}

void AudioPlayer::Pause() {}

void AudioPlayer::PauseAt(int sample) {
    resumeFrom = sample;
    m_state    = PlayState::Paused;
}

void AudioPlayer::Stop() {
    resumeFrom = 0;
    m_state    = PlayState::Stopped;
}

} // namespace platform
