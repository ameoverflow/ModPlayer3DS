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
#include <3ds.h>
#include <citro2d.h>
#include <memory>
#include "Waveform.h"
#include "libopenmpt/libopenmpt.hpp"

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
    void SetWaveform(Waveform* waveform);
    double GetPosition();
    double GetDuration();
    bool IsModLoaded();
    void ShowMessage(std::string msg);

    inline std::string songTitle;
    inline bool isPaused;

    inline int16_t* audioBufferPool;
    inline ndspWaveBuf waveBuf[2];
    inline int hardwareChannel = 0;
    inline u64 lastTime = 0;

    inline C2D_TextBuf statusTextBuffer, songInfoTextBuffer;
    inline C2D_Text statusText, songInfo;
    inline std::unique_ptr<openmpt::module> mod = nullptr;
    inline int currentBufferIndex = 0;

    inline std::string message = "";
    inline u64 messageShown;

    inline int setInterpolation = 4;
    inline Waveform* currentWaveform;

    inline int playProgressWidth;
}

#endif //MODPLAYER3DS_PLAYBACK_H
