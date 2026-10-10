//
// Created by void on 23/07/2026.
//

#include "WaveformOld.h"

#include <citro2d.h>
#include "Playback.h"

void WaveformOld::Draw(int16_t *audioBufferPool, int currentBufferIndex) {
    if (audioBufferPool == nullptr) return;

    int16_t* pcmData = &audioBufferPool[currentBufferIndex * BUFFER_SIZE];

    int totalBufferElements = BUFFER_SIZE;

    float prevX = 0.0f;
    float prevY = 120.0f;

    u32 lineColor = C2D_Color32(68, 0, 215, 255);
    u32 shadowColor = C2D_Color32(0, 0, 0, 255);

    float samplesPerPixel = (float)totalBufferElements / 400.0f;

    for (int x = 0; x < 400; x += 4) {
        // Calculate sample index for current X position
        int currIndex = (int)(x * samplesPerPixel);

        if (currIndex % 2 != 0) currIndex--;

        if (currIndex + 1 >= totalBufferElements) {
            break;
        }

        int16_t sample = (pcmData[currIndex] + pcmData[currIndex + 1]) >> 1;
        float currentY = 120.0f + (float)(sample >> 9);

        if (x > 0) {
            if (currentY >= 0.0f && currentY <= 240.0f && prevY >= 0.0f && prevY <= 240.0f) {
                C2D_DrawLine(
                    prevX, prevY + 3.0f, shadowColor,
                    x, currentY + 3.0f, shadowColor,
                    2.0f, 0.5f
                );
                C2D_DrawLine(
                    prevX, prevY, lineColor,
                    x, currentY, lineColor,
                    2.0f, 0.5f
                );
            }
        }
        prevX = (float)x;
        prevY = currentY;
    }
}