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

#include "Theme.h"
#include "Waveform.h"

double Playback::GetPosition() { return mod ? mod->get_position_seconds() : 0; }
double Playback::GetDuration() { return mod ? mod->get_duration_seconds() : 0; }
bool Playback::IsModLoaded() { return mod != nullptr; }
int Playback::GetInterpolation() { return mod->get_render_param(openmpt::module::render_param::RENDER_INTERPOLATIONFILTER_LENGTH); }

void Playback::ShowMessage(std::string msg) {
    message = msg;
    messageShown = osGetTime();
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
    songInfoTextBuffer = C2D_TextBufNew(256);

    C2D_TextBufClear(statusTextBuffer);
    C2D_TextBufClear(songInfoTextBuffer);
    C2D_TextParse(&songInfo, songInfoTextBuffer, "Idle");
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
            C2D_TextBufClear(songInfoTextBuffer);
            C2D_TextParse(&songInfo, songInfoTextBuffer, "Idle");
            ShowMessage("Invalid file");

        }
    } else {
        C2D_TextBufClear(songInfoTextBuffer);
        C2D_TextParse(&songInfo, songInfoTextBuffer, "Idle");
        ShowMessage("Invalid file");
    }
}

void Playback::Pause() {
    isPaused = !isPaused;
    ndspChnSetPaused(hardwareChannel, isPaused);
}

void Playback::Forward() {
    double duration = mod->get_duration_seconds();

    double relativePos = std::fmod(mod->get_position_seconds(), duration);

    double newPos = relativePos + 5.0;
    if (newPos >= duration) {
        newPos = std::fmod(newPos, duration);
    }

    mod->set_position_seconds(newPos);
}

void Playback::Backward() {
    double duration = mod->get_duration_seconds();

    double relativePos = std::fmod(mod->get_position_seconds(), duration);

    double newPos = relativePos - 5.0;
    if (newPos < 0.0) {
        newPos += duration;
    }

    mod->set_position_seconds(newPos);
}

void Playback::SetInterpolation(int interpolation) {
    mod->set_render_param(openmpt::module::render_param::RENDER_INTERPOLATIONFILTER_LENGTH, interpolation);
    ShowMessage("Interpolation: " + std::to_string(interpolation));
    setInterpolation = interpolation;
}

void Playback::SetWaveform(Waveform* waveform) {
    currentWaveform = waveform;
    ShowMessage("Visualization: " + waveform->GetName());
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

        C2D_TextParse(&statusText, statusTextBuffer, message.c_str());

        int totalProgressBarWidth = 400;
        float progress = currentSecs / durationSecs;
        if (progress > 1.0f) progress = 1.0f;
        if (progress < 0.0f) progress = 0.0f;

        playProgressWidth = totalProgressBarWidth * progress;

        C2D_TextBufClear(songInfoTextBuffer);
        std::string status = isPaused ? "|| " : "> ";
        status += songTitle;
        status += "\nTime: " + std::to_string(curMins) + ":" + (curSecs < 10 ? "0" : "") + std::to_string(curSecs);
        status += " / " + std::to_string(durMins) + ":" + (durSecs < 10 ? "0" : "") + std::to_string(durSecs);
        C2D_TextParse(&songInfo, songInfoTextBuffer, status.c_str());
    }

    if (currentTime - messageShown >= 3000 && !message.empty()) {
        message = "";
    }
}

void Playback::Draw() {
    if (mod && audioBufferPool != nullptr && currentWaveform != nullptr) {
        currentWaveform->Draw(audioBufferPool, currentBufferIndex);
    }

    C2D_DrawText(&statusText, C2D_WithColor, 7.0f, 7.0f, 0.6f, 0.45f, 0.45f, C2D_Color32(0, 0, 0, 255));
    C2D_DrawText(&statusText, C2D_WithColor, 5.0f, 5.0f, 0.6f, 0.45f, 0.45f, mainText);

    C2D_DrawRectSolid(0, 237, 0.6f, 400, 3, progressBarBackground);
    C2D_DrawRectSolid(0, 237, 0.6f, playProgressWidth, 3, progressBarMain);

    float textWidth, textHeight;
    C2D_TextGetDimensions(&songInfo, 0.45f, 0.45f, &textWidth, &textHeight);
    float textY = 237 - 5.0f - textHeight;
    C2D_DrawText(&songInfo, C2D_WithColor, 7, textY + 2, 0.5f, 0.45f, 0.45f, C2D_Color32(0, 0, 0, 255));
    C2D_DrawText(&songInfo, C2D_WithColor, 5, textY, 0.5f, 0.45f, 0.45f, mainText);
}