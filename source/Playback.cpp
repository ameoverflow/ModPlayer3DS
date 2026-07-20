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

int interpolation = 2;

double Playback::GetPosition() { return mod ? mod->get_position_seconds() : 0; }
double Playback::GetDuration() { return mod ? mod->get_duration_seconds() : 0; }
bool Playback::IsModLoaded() { return mod != nullptr; }

void Playback::DrawWaveform(float alphaVal) {
    if (audioBufferPool == nullptr) return;
    if (alphaVal <= 0.0f) return;

    int16_t* pcmData = audioBufferPool;

    int maxSafeElements = BUFFER_SIZE * 2;

    float prevX = 0.0f;
    float prevY = 120.0f;
    int step = 4;

    u32 lineColor = C2D_Color32(192, 255, 0, (u8)(178.0f * alphaVal));
    u32 shadowColor = C2D_Color32(0, 0, 0, (u8)(255.0f * alphaVal));

    for (int x = 0; x <= 400; x += 8) {
        int targetIndex = x * step;

        if (targetIndex >= maxSafeElements || targetIndex < 0) {
            break;
        }

        int16_t sample = pcmData[targetIndex] + pcmData[targetIndex + 1] + pcmData[targetIndex + 2] + pcmData[targetIndex + 3];
        sample /= 4;
        float normalized = (float)sample / 32768.0f;

        float currentY = 120.0f + (normalized * 80.0f);
        float currentX = (float)x;

        if (x > 0) {
            if (currentY >= 0.0f && currentY <= 240.0f && prevY >= 0.0f && prevY <= 240.0f) {
                C2D_DrawLine(
                    prevX, prevY, shadowColor,
                    currentX, currentY + 3.0f, shadowColor,
                    2.0f, 0.5f
                );
                C2D_DrawLine(
                    prevX, prevY, lineColor,
                    currentX, currentY, lineColor,
                    2.0f, 0.5f
                );
            }
        }
        prevX = currentX;
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

void Playback::PlayFile(std::string path) {
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

            bool isNew3DS = false;
            if (R_SUCCEEDED(APT_CheckNew3DS(&isNew3DS)) && isNew3DS) {
                SetInterpolation(4);
            } else {
                SetInterpolation(2);
            }

        } catch (...) {
            C2D_TextBufClear(statusTextBuffer);
            C2D_TextParse(&statusText, statusTextBuffer, "Invalid file");
        }
    } else {
        C2D_TextBufClear(statusTextBuffer);
        C2D_TextParse(&statusText, statusTextBuffer, "Couldn't open file");
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

void ShowMessage(std::string msg) {
    message = msg;
    messageShown = osGetTime();
}

void Playback::SetInterpolation(int interpolation) {
    mod->set_render_param(openmpt::module::render_param::RENDER_INTERPOLATIONFILTER_LENGTH, interpolation);
    ShowMessage("Interpolation: " + std::to_string(interpolation));
}

int Playback::GetInterpolation() {
    return mod->get_render_param(openmpt::module::render_param::RENDER_INTERPOLATIONFILTER_LENGTH);
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

    if (osGetTime() - lastTime >= 500) {
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

    if (osGetTime() - messageShown >= 3000 && !message.empty()) {
        message = "";
    }
}

void Playback::Draw() {
    u32 textColor = C2D_Color32(255, 255, 255, 255);
    if (mod && audioBufferPool != nullptr) {
        DrawWaveform(1.0f);
    }

    C2D_DrawText(&statusText, C2D_WithColor, 10.0f, 10.0f, 0.6f, 0.5f, 0.5f, textColor);
}
