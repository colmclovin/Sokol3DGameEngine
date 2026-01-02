#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "../../External/dr_wav.h" // header from External/dr_wav.h (CMake include path)
#include "../../External/Sokol/sokol_audio.h" // uses the project's include path

class AudioEngine {
public:
    AudioEngine() noexcept;
    ~AudioEngine() noexcept;

    // Initialize sokol-audio with callback
    bool Init();

    // Load a WAV file into internal buffer. Returns true on success.
    bool LoadWav(const char *path);

    // Start/stop playback and control volume
    void Play();
    void Stop();
    bool IsPlaying() const;

    void SetVolume(float v); // 0.0 .. 1.0
    float GetVolume() const;

    // Shutdown, free resources
    void Shutdown();

private:
    // callback for sokol audio stream
    static void StreamCallback(float *buffer, int num_frames, int num_channels);

    drwav m_wav;
    float *m_audio_buffer;
    uint64_t m_total_samples;
    std::atomic<uint64_t> m_sample_index;
    std::atomic<bool> m_playing;
    std::atomic<float> m_volume;
    bool m_has_buffer;
};