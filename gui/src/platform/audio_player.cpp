// miniaudio backend for the audio preview player (#87).
//
// Threading contract: the control plane (all public methods) runs on the
// GUI thread; the data plane is miniaudio's realtime callback. The two
// cross only through atomics — the callback never allocates, locks, or
// tears down the device, and Update() (GUI thread, once per frame)
// performs the teardown after natural end-of-clip, mirroring the old
// waveOut WHDR_DONE poll.
//
// FX_AUDIO_NULL=1 in the environment forces miniaudio's null backend —
// no audio hardware, realtime consumption simulated — which is how the
// gui_tests exercise the state machine in CI.
#define NOMINMAX // miniaudio includes windows.h (WASAPI); keep std::min/max
#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MA_NO_ENGINE
#include "miniaudio.h"

#include "audio_player.h"
#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace platform {

struct AudioPlayer::Impl {
    ma_context ctx{};
    bool       ctxInit = false;
    ma_device  dev{};
    bool       devInit = false;

    std::vector<uint8_t> buf;             // slice [fromSample, end)
    std::atomic<uint32_t> cursor{0};      // bytes consumed (u8 mono: bytes == samples)
    std::atomic<bool>     finished{false};
    int startSample = 0;

    ~Impl() {
        CloseDevice();
        if (ctxInit) ma_context_uninit(&ctx);
    }

    bool EnsureContext() {
        if (ctxInit) return true;
        ma_result r;
        if (std::getenv("FX_AUDIO_NULL")) {
            ma_backend nullBackend = ma_backend_null;
            r = ma_context_init(&nullBackend, 1, nullptr, &ctx);
        } else {
            r = ma_context_init(nullptr, 0, nullptr, &ctx);
        }
        ctxInit = (r == MA_SUCCESS);
        return ctxInit;
    }

    // Realtime thread. Touches only buf/cursor/finished.
    static void DataCallback(ma_device* dev, void* out, const void*,
                             ma_uint32 frames) {
        Impl* impl   = static_cast<Impl*>(dev->pUserData);
        uint8_t* dst = static_cast<uint8_t*>(out);

        uint32_t size  = (uint32_t)impl->buf.size();
        uint32_t cur   = impl->cursor.load(std::memory_order_relaxed);
        uint32_t avail = cur < size ? size - cur : 0;
        uint32_t n     = frames < avail ? frames : avail;

        if (n) std::memcpy(dst, impl->buf.data() + cur, n);
        // u8 silence is 0x80 — zero-filling would pop at clip end.
        if (n < frames) std::memset(dst + n, 0x80, frames - n);

        impl->cursor.store(cur + n, std::memory_order_relaxed);
        if (cur + n >= size)
            impl->finished.store(true, std::memory_order_release);
    }

    // GUI thread only — never called from the data callback.
    void CloseDevice() {
        if (devInit) {
            ma_device_uninit(&dev);
            devInit = false;
        }
        buf.clear();
        cursor.store(0, std::memory_order_relaxed);
        finished.store(false, std::memory_order_relaxed);
    }
};

AudioPlayer::AudioPlayer() : m_impl(new Impl) {}
AudioPlayer::~AudioPlayer() { delete m_impl; }

bool AudioPlayer::IsPlaying() const {
    return m_state == PlayState::Playing && m_impl->devInit &&
           !m_impl->finished.load(std::memory_order_acquire);
}

void AudioPlayer::Update() {
    if (m_state == PlayState::Playing && m_impl->devInit &&
        m_impl->finished.load(std::memory_order_acquire)) {
        m_impl->CloseDevice();
        m_state    = PlayState::Stopped;
        resumeFrom = 0;
    }
}

int AudioPlayer::Position() const {
    if (m_state == PlayState::Playing && m_impl->devInit) {
        // Submission cursor, not a device query: leads the audible output
        // by up to one device period — invisible on the waveform playhead.
        return m_impl->startSample +
               (int)m_impl->cursor.load(std::memory_order_relaxed);
    }
    return resumeFrom;
}

bool AudioPlayer::Play(const uint8_t* pcm, int samples, int rate,
                       int fromSample) {
    m_impl->CloseDevice();
    fromSample = std::clamp(fromSample, 0, std::max(0, samples - 1));
    int n = samples - fromSample;
    if (!pcm || n <= 0 || !m_impl->EnsureContext()) {
        m_state = PlayState::Stopped;
        return false;
    }

    m_impl->buf.assign(pcm + fromSample, pcm + samples);
    m_impl->startSample = fromSample;

    ma_device_config cfg  = ma_device_config_init(ma_device_type_playback);
    cfg.playback.format   = ma_format_u8;
    cfg.playback.channels = 1;
    cfg.sampleRate        = (ma_uint32)rate;
    cfg.dataCallback      = &Impl::DataCallback;
    cfg.pUserData         = m_impl;

    if (ma_device_init(&m_impl->ctx, &cfg, &m_impl->dev) != MA_SUCCESS) {
        m_impl->buf.clear();
        m_state = PlayState::Stopped;
        return false;
    }
    m_impl->devInit = true;

    if (ma_device_start(&m_impl->dev) != MA_SUCCESS) {
        m_impl->CloseDevice();
        m_state = PlayState::Stopped;
        return false;
    }

    resumeFrom = fromSample;
    m_state    = PlayState::Playing;
    return true;
}

void AudioPlayer::Pause() {
    if (m_state != PlayState::Playing) return;
    resumeFrom = Position();
    m_impl->CloseDevice();
    m_state = PlayState::Paused;
}

void AudioPlayer::PauseAt(int sample) {
    m_impl->CloseDevice();
    resumeFrom = sample;
    m_state    = PlayState::Paused;
}

void AudioPlayer::Stop() {
    m_impl->CloseDevice();
    resumeFrom = 0;
    m_state    = PlayState::Stopped;
}

} // namespace platform
