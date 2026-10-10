//
// Created by void on 23/07/2026.
//

#ifndef MODPLAYER3DS_WAVEFORMNEW_H
#define MODPLAYER3DS_WAVEFORMNEW_H

#include "Waveform.h"

class WaveformNew : public Waveform {
public:
    void Draw(int16_t *audioBufferPool, int currentBufferIndex) override;
    std::string GetName() const override { return "Wave 1"; }
};

#endif //MODPLAYER3DS_WAVEFORMNEW_H
