//
// Created by void on 23/07/2026.
//

#include "WaveformNew.h"
#include <citro2d.h>
#include "Playback.h"

void WaveformNew::Draw(int16_t *audioBufferPool, int currentBufferIndex) {
    if (audioBufferPool == nullptr) return;

    int16_t* pcmData = &audioBufferPool[currentBufferIndex * BUFFER_SIZE];
    int totalElements = BUFFER_SAMPLES * 2;

    int screenWidth = 400;
    float centerY = 120.0f;

    u32 lineColor = C2D_Color32(68, 0, 215, 255);
    u32 shadowColor = C2D_Color32(0, 0, 0, 255);

    int elementsPerPixel = totalElements / screenWidth;
    if (elementsPerPixel < 2) elementsPerPixel = 2;

    for (int x = 0; x < screenWidth; x += 2) {
        int startIdx = x * elementsPerPixel;
        int endIdx = startIdx + (elementsPerPixel * 2);

        if (startIdx >= totalElements) break;
        if (endIdx > totalElements) endIdx = totalElements;

        int16_t minVal = 32767;
        int16_t maxVal = -32768;

        for (int i = startIdx; i < endIdx - 1; i += 2) {
            int16_t monoSample = (pcmData[i] + pcmData[i + 1]) >> 1;

            if (monoSample > maxVal) maxVal = monoSample;
            if (monoSample < minVal) minVal = monoSample;
        }

        float topY = centerY + (float)(minVal >> 9);
        float bottomY = centerY + (float)(maxVal >> 9);

        if (topY < 0.0f)   topY = 0.0f;
        if (bottomY > 240.0f) bottomY = 240.0f;

        C2D_DrawLine(
            x, topY, shadowColor,
            x, bottomY + 2.0f, shadowColor,
            2.0f, 0.5f
        );

        C2D_DrawLine(
            x, topY, lineColor,
            x, bottomY, lineColor,
            2.0f, 0.5f
        );
    }
}
