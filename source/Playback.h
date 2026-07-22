//
// Created by void on 20/07/2026.
//

#ifndef MODPLAYER3DS_PLAYBACK_H
#define MODPLAYER3DS_PLAYBACK_H

// audio config
#define SAMPLE_RATE 48000
#define BUFFER_SAMPLES 3072
#define BUFFER_SIZE (BUFFER_SAMPLES * 2)

#include <string>

namespace Playback {
    void Init();
    void PlayFile(const std::string& path);
    void Update();
    void Pause();
    void Forward();
    void Backward();
    void Draw();
    void SetInterpolation(int interpolation);
    int GetInterpolation();
    void SetWaveform(int waveform);
    int GetWaveform();
    double GetPosition();
    double GetDuration();
    bool IsModLoaded();
}

#endif //MODPLAYER3DS_PLAYBACK_H
