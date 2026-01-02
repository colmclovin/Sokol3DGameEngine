#include "AudioEngine.h"
#include <cstdio>
#include <cstring>
#include "../../External/Sokol/sokol_audio.h"
#include "../../External/Sokol/sokol_log.h"

// Provide dr_wav implementation in exactly one TU
#define DR_WAV_IMPLEMENTATION
#include "../../External/dr_wav.h"

// file-local globals used by StreamCallback (kept for callback compatibility)
static float* g_audio_buffer_ptr = nullptr;
static uint64_t g_audio_total_samples = 0;
static std::atomic<uint64_t> g_audio_sample_index{0};
static std::atomic<bool> g_audio_playing{false};
static std::atomic<float> g_audio_volume{1.0f};

AudioEngine::AudioEngine() noexcept
    : m_audio_buffer(nullptr)
    , m_total_samples(0)
    , m_sample_index(0)
    , m_playing(false)
    , m_volume(1.0f)
    , m_has_buffer(false)
    , m_saudio_setup(false)
{
    memset(&m_wav, 0, sizeof(m_wav));
}

AudioEngine::~AudioEngine() noexcept {
    Shutdown();
}

bool AudioEngine::Init() {
    saudio_desc ad = { 0 };
    ad.stream_cb = StreamCallback;
    ad.sample_rate = 44100;
    ad.num_channels = 2;
    saudio_setup(&ad);
    // mark that saudio was set up (so Shutdown can be safe/ idempotent)
    m_saudio_setup = true;
    return true;
}

bool AudioEngine::LoadWav(const char* path) {
    if (!drwav_init_file(&m_wav, path, NULL)) {
        printf("AudioEngine: failed to open WAV: %s\n", path);
        return false;
    }
    m_total_samples = m_wav.totalPCMFrameCount * m_wav.channels;
    m_audio_buffer = (float*)malloc((size_t)m_total_samples * sizeof(float));
    if (!m_audio_buffer) {
        drwav_uninit(&m_wav);
        printf("AudioEngine: failed to allocate audio buffer\n");
        return false;
    }
    drwav_read_pcm_frames_f32(&m_wav, m_wav.totalPCMFrameCount, m_audio_buffer);
    drwav_uninit(&m_wav);
    m_sample_index = 0;
    m_has_buffer = true;
    m_playing = false;
    m_volume = 1.0f;

    // set file-local globals for callback access
    g_audio_buffer_ptr = m_audio_buffer;
    g_audio_total_samples = m_total_samples;
    g_audio_sample_index = 0;
    g_audio_playing = false;
    g_audio_volume = 1.0f;

    return true;
}

void AudioEngine::Play() {
    if (!m_has_buffer) return;
    m_sample_index = 0;
    m_playing = true;
    g_audio_sample_index = 0;
    g_audio_playing = true;
}

void AudioEngine::Stop() {
    m_playing = false;
    g_audio_playing = false;
}

bool AudioEngine::IsPlaying() const {
    return m_playing.load();
}

void AudioEngine::SetVolume(float v) {
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    m_volume = v;
    g_audio_volume = v;
}

float AudioEngine::GetVolume() const {
    return m_volume.load();
}

void AudioEngine::Shutdown() {
    // only shutdown sokol-audio if it was previously setup
    if (m_saudio_setup.load()) {
        saudio_shutdown();
        m_saudio_setup = false;
    }
    if (m_audio_buffer) {
        free(m_audio_buffer);
        m_audio_buffer = nullptr;
    }
    m_has_buffer = false;
    m_total_samples = 0;
    m_sample_index = 0;
    m_playing = false;
    m_volume = 1.0f;

    // clear globals
    g_audio_buffer_ptr = nullptr;
    g_audio_total_samples = 0;
    g_audio_sample_index = 0;
    g_audio_playing = false;
    g_audio_volume = 1.0f;
}

void AudioEngine::StreamCallback(float* buffer, int num_frames, int num_channels) {
    int samples = num_frames * num_channels;
    if (!g_audio_buffer_ptr || !g_audio_playing.load()) {
        // silence if no buffer or not playing
        for (int i = 0; i < samples; ++i) buffer[i] = 0.0f;
        return;
    }

    float vol = g_audio_volume.load();
    for (int i = 0; i < samples; ++i) {
        if (g_audio_sample_index < g_audio_total_samples) {
            buffer[i] = g_audio_buffer_ptr[g_audio_sample_index++] * vol;
        } else {
            buffer[i] = 0.0f;
            // loop
            g_audio_sample_index = 0;
        }
    }
}