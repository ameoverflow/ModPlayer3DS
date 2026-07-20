//
// Created by void on 20/07/2026.
//

#ifndef MODPLAYER3DS_PLAYBACK_H
#define MODPLAYER3DS_PLAYBACK_H

// audio config
#define SAMPLE_RATE 48000
#define BUFFER_SAMPLES 2048
#define STREAM_CHANNELS 2
#define BUFFER_SIZE (BUFFER_SAMPLES * STREAM_CHANNELS)

#include <string>

namespace Playback {
    void Init();
    void PlayFile(std::string path);
    void Update();
    void Pause();
    void Forward();
    void Backward();
    void Draw();
    void DrawWaveform(float alphaVal);
    void SetInterpolation(int interpolation);
    int GetInterpolation();
    double GetPosition();
    double GetDuration();
    bool IsModLoaded();
}

#endif //MODPLAYER3DS_PLAYBACK_H
