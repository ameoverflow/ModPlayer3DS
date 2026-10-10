//
// Created by void on 23/07/2026.
//

#ifndef MODPLAYER3DS_WAVEFORMOLD_H
#define MODPLAYER3DS_WAVEFORMOLD_H

#include "Waveform.h"

class WaveformOld : public Waveform {
public:
    void Draw(int16_t *audioBufferPool, int currentBufferIndex) override;
    std::string GetName() const override { return "Wave 2"; }
};


#endif //MODPLAYER3DS_WAVEFORMOLD_H
