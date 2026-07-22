//
// Created by void on 20/07/2026.
//

#include "Playback.h"
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <citro2d.h>
#include "libopenmpt/libopenmpt.hpp"
#include <3ds.h>

std::string songTitle;
bool isPaused;

int16_t* audioBufferPool;
ndspWaveBuf waveBuf[2];
int hardwareChannel = 0;
u64 lastTime = 0;

C2D_TextBuf statusTextBuffer;
C2D_Text statusText;
std::unique_ptr<openmpt::module> mod = nullptr;
int currentBufferIndex = 0;

std::string message = "";
u64 messageShown;

int setInterpolation = 4;
int waveformSelected = 0;

double Playback::GetPosition() { return mod ? mod->get_position_seconds() : 0; }
double Playback::GetDuration() { return mod ? mod->get_duration_seconds() : 0; }
bool Playback::IsModLoaded() { return mod != nullptr; }
int Playback::GetInterpolation() { return mod->get_render_param(openmpt::module::render_param::RENDER_INTERPOLATIONFILTER_LENGTH); }
int Playback::GetWaveform() { return waveformSelected; }

void ShowMessage(std::string msg) {
    message = msg;
    messageShown = osGetTime();
}

void DrawWaveform() {
    if (audioBufferPool == nullptr) return;

    int16_t* pcmData = &audioBufferPool[currentBufferIndex * BUFFER_SIZE];
    int totalElements = BUFFER_SAMPLES * 2;

    int screenWidth = 400;
    float centerY = 120.0f;

    u32 lineColor = C2D_Color32(192, 255, 0, 178);
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

void DrawWaveformOld() {
    if (audioBufferPool == nullptr) return;

    int16_t* pcmData = audioBufferPool;

    int maxSafeElements = BUFFER_SIZE * 2;

    float prevX = 0.0f;
    float prevY = 120.0f;

    int step = maxSafeElements / 100;
    if (step < 1) step = 1;

    u32 lineColor = C2D_Color32(192, 255, 0, 178.0f);
    u32 shadowColor = C2D_Color32(0, 0, 0, 255.0f);

    int currIndex = 0;

    for (int x = 0; x < 400; x += 4) {
        currIndex = x / 4 * step;

        if (currIndex >= maxSafeElements || currIndex + 1 >= maxSafeElements) {
            break;
        }

        int16_t sample = (pcmData[currIndex] + pcmData[currIndex + 1]) >> 1;
        float currentY = 120.0f + (float)(sample >> 9);

        if (x > 0) {
            if (currentY >= 0.0f && currentY <= 240.0f && prevY >= 0.0f && prevY <= 240.0f) {
                C2D_DrawLine(
                    prevX, prevY, shadowColor,
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
        prevX = x;
        prevY = currentY;
    }
}

void Playback::Init() {
    ndspChnReset(hardwareChannel);
    ndspChnSetFormat(hardwareChannel, NDSP_FORMAT_STEREO_PCM16);
    ndspChnSetRate(hardwareChannel, SAMPLE_RATE);

    // allocate buffer memory
    audioBufferPool = (int16_t*)linearAlloc(BUFFER_SIZE * 2 * sizeof(int16_t));

    std::memset(waveBuf, 0, sizeof(waveBuf));

    waveBuf[0].data_vaddr = &audioBufferPool[0];
    waveBuf[0].nsamples   = BUFFER_SAMPLES;
    waveBuf[1].data_vaddr = &audioBufferPool[BUFFER_SIZE];
    waveBuf[1].nsamples   = BUFFER_SAMPLES;

    statusTextBuffer = C2D_TextBufNew(256);

    C2D_TextBufClear(statusTextBuffer);
    C2D_TextParse(&statusText, statusTextBuffer, "Idle");
}

void Playback::PlayFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (file.is_open()) {
        ndspChnReset(hardwareChannel);
        ndspChnSetFormat(hardwareChannel, NDSP_FORMAT_STEREO_PCM16);
        ndspChnSetRate(hardwareChannel, SAMPLE_RATE);

        try {
            mod = std::make_unique<openmpt::module>(file);
            std::string titleStr = mod->get_metadata("title");
            if (titleStr.empty()) titleStr = "Untitled Track";
            songTitle = titleStr;

            mod->set_repeat_count(-1);
            mod->set_render_param(openmpt::module::render_param::RENDER_INTERPOLATIONFILTER_LENGTH, setInterpolation);
            isPaused = false;

        } catch (...) {
            C2D_TextBufClear(statusTextBuffer);
            C2D_TextParse(&statusText, statusTextBuffer, "Idle");
            ShowMessage("Invalid file");

        }
    } else {
        C2D_TextBufClear(statusTextBuffer);
        C2D_TextParse(&statusText, statusTextBuffer, "Idle");
        ShowMessage("Invalid file");
    }
}

void Playback::Pause() {
    isPaused = !isPaused;
    ndspChnSetPaused(hardwareChannel, isPaused);
}

void Playback::Forward() {
    double currentPos = mod->get_position_seconds();
    mod->set_position_seconds(std::max(0.0, currentPos + 5.0));
}

void Playback::Backward() {
    double currentPos = mod->get_position_seconds();
    mod->set_position_seconds(std::max(0.0, currentPos - 5.0));
}

void Playback::SetInterpolation(int interpolation) {
    mod->set_render_param(openmpt::module::render_param::RENDER_INTERPOLATIONFILTER_LENGTH, interpolation);
    ShowMessage("Interpolation: " + std::to_string(interpolation));
    setInterpolation = interpolation;
}

void Playback::SetWaveform(int waveform) {
    waveformSelected = waveform;
    switch (waveform) {
        case 0:
            ShowMessage("Visualization: Wave 1");
            break;
        case 1:
            ShowMessage("Visualization: Wave 2");
            break;
        case 2:
            ShowMessage("Visualization: Wave 3");
            break;
    }
}

void Playback::Update() {
    if ((waveBuf[currentBufferIndex].status == NDSP_WBUF_FREE || waveBuf[currentBufferIndex].status == NDSP_WBUF_DONE) && !isPaused) {
        int16_t* interleavedDest = waveBuf[currentBufferIndex].data_pcm16;
        size_t framesRead = mod->read_interleaved_stereo(SAMPLE_RATE, BUFFER_SAMPLES, interleavedDest);

        if (framesRead == 0 && mod->get_repeat_count() == 0) {
            ndspChnReset(hardwareChannel);
            ndspChnSetFormat(hardwareChannel, NDSP_FORMAT_STEREO_PCM16);
            ndspChnSetRate(hardwareChannel, SAMPLE_RATE);
            Pause();
        }

        DSP_FlushDataCache(waveBuf[currentBufferIndex].data_vaddr, BUFFER_SIZE * sizeof(int16_t));
        ndspChnWaveBufAdd(hardwareChannel, &waveBuf[currentBufferIndex]);
        currentBufferIndex = !currentBufferIndex;
    }

    u64 currentTime = osGetTime();
    if (currentTime - lastTime >= 100) {
        lastTime = currentTime;
        C2D_TextBufClear(statusTextBuffer);
        double currentSecs = std::fmod(mod->get_position_seconds(), mod->get_duration_seconds());
        double durationSecs = mod->get_duration_seconds();
        int curMins = (int)currentSecs / 60;
        int curSecs = (int)currentSecs % 60;
        int durMins = (int)durationSecs / 60;
        int durSecs = (int)durationSecs % 60;

        std::string status = isPaused ? "|| " : "> ";
        status += songTitle;
        status += "\nTime: " + std::to_string(curMins) + ":" + (curSecs < 10 ? "0" : "") + std::to_string(curSecs);
        status += " / " + std::to_string(durMins) + ":" + (durSecs < 10 ? "0" : "") + std::to_string(durSecs);
        status += "\nPattern / row: " + std::to_string(mod->get_current_pattern()) + " / " + std::to_string(mod->get_current_row());
        status += "\n" + message;
        C2D_TextParse(&statusText, statusTextBuffer, status.c_str());
    }

    if (currentTime - messageShown >= 3000 && !message.empty()) {
        message = "";
    }
}

void Playback::Draw() {
    u32 textColor = C2D_Color32(255, 255, 255, 255);
    if (mod && audioBufferPool != nullptr) {
        switch (waveformSelected) {
            case 0:
                DrawWaveform();
                break;
            case 1:
                DrawWaveformOld();
                break;
        }
    }

    C2D_DrawText(&statusText, C2D_WithColor, 10.0f, 10.0f, 0.6f, 0.5f, 0.5f, textColor);
}
