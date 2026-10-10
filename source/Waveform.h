//
// Created by void on 23/07/2026.
//

#ifndef MODPLAYER3DS_WAVEFORM_H
#define MODPLAYER3DS_WAVEFORM_H

#include <string>

class Waveform {
public:
    virtual void Draw(int16_t* audioBufferPool, int currentBufferIndex) = 0;
    virtual std::string GetName() const { return "Unknown"; }
};

#endif //MODPLAYER3DS_WAVEFORM_H
