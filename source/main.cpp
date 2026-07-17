#include <3ds.h>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <string>
#include <libopenmpt/libopenmpt.hpp>
#include <citro2d.h>
#include <cstring>
#include <dirent.h>

// audio config
#define SAMPLE_RATE 48000
#define BUFFER_SAMPLES 2048
#define STREAM_CHANNELS 2
#define BUFFER_SIZE (BUFFER_SAMPLES * STREAM_CHANNELS)

struct FileEntry {
	bool isDir;
	std::string name;
};

C3D_RenderTarget* topTarget;
C3D_RenderTarget* bottomTarget;
int16_t* audioBufferPool;

std::string path;
std::unique_ptr<openmpt::module> mod = nullptr;

C2D_TextBuf pathTextBuffer, statusTextBuffer, staticTextBuffer, positionBuffer;
C2D_Text pathText, statusText, positionText;

std::vector<FileEntry> fileList;
int selectedFileIndex = 0;
int scrollOffset = 0;

C2D_TextBuf fileBrowserBox;
std::vector<C2D_Text> fileTexts;
std::filesystem::path currentPath = "/";

bool isPaused = false;

void populateFileList() {
	fileList.clear();
	fileTexts.clear();
	C2D_TextBufClear(fileBrowserBox);

	if (currentPath.string() != "/") {
		fileList.push_back({true, ".."});
	}

	for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(currentPath.string())) {
		if (std::filesystem::is_regular_file(entry.status())) {
			if (entry.path().extension() == ".mod" || entry.path().extension() == ".xm" || entry.path().extension() == ".mptm" || entry.path().extension() == ".s3m" || entry.path().extension() == ".it") {
				fileList.push_back({false, entry.path().filename().string()});
			}
		} else if (std::filesystem::is_directory(entry.status())) {
			fileList.push_back({true, entry.path().filename().string()});
		}
	}


	fileTexts.resize(fileList.size());
	for (size_t i = 0; i < fileList.size(); i++) {
		C2D_TextParse(&fileTexts[i], fileBrowserBox, fileList[i].name.size() > 40 ?
					  (fileList[i].name.substr(0, 37) + "...").c_str() : fileList[i].name.c_str());
	}

	selectedFileIndex = 0;
	scrollOffset = 0;
}

void drawCaptureWaveform(float alphaVal) {
    if (audioBufferPool == nullptr) return;
    if (alphaVal <= 0.0f) return;

    int16_t* pcmData = audioBufferPool;

    int maxSafeElements = BUFFER_SIZE * 2;

    float prevX = 0.0f;
    float prevY = 120.0f;
    int step = 1;

    u32 lineColor = C2D_Color32(192, 255, 0, (u8)(178.0f * alphaVal));
    u32 shadowColor = C2D_Color32(0, 0, 0, (u8)(255.0f * alphaVal));

    for (int x = 0; x < 400; x += 2) {
        int targetIndex = x * step;

        if (targetIndex >= maxSafeElements || targetIndex < 0) {
            break;
        }

        int16_t sample = pcmData[targetIndex];
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

int main(int argc, char* argv[]) {
	gfxInitDefault();
	gfxSet3D(false);
	C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
	C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
	C2D_Prepare();

	romfsInit();

	pathTextBuffer = C2D_TextBufNew(256);
	positionBuffer = C2D_TextBufNew(256);
	staticTextBuffer = C2D_TextBufNew(4096);
	fileBrowserBox = C2D_TextBufNew(16384);
	statusTextBuffer = C2D_TextBufNew(256);

	topTarget = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
	bottomTarget = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

	// init audio
	ndspInit();
	ndspSetOutputMode(NDSP_OUTPUT_STEREO);

	// use hw channel 0
	int hardwareChannel = 0;
	ndspChnReset(hardwareChannel);
	ndspChnSetFormat(hardwareChannel, NDSP_FORMAT_STEREO_PCM16);
	ndspChnSetRate(hardwareChannel, SAMPLE_RATE);

	// allocate buffer memory
	audioBufferPool = (int16_t*)linearAlloc(BUFFER_SIZE * 2 * sizeof(int16_t));

	ndspWaveBuf waveBuf[2];
	std::memset(waveBuf, 0, sizeof(waveBuf));

	waveBuf[0].data_vaddr = &audioBufferPool[0];
	waveBuf[0].nsamples   = BUFFER_SAMPLES;
	waveBuf[1].data_vaddr = &audioBufferPool[BUFFER_SIZE];
	waveBuf[1].nsamples   = BUFFER_SAMPLES;

	C2D_TextBufClear(staticTextBuffer);
	C2D_TextBufClear(pathTextBuffer);
	C2D_TextBufClear(statusTextBuffer);

	C2D_TextParse(&pathText, pathTextBuffer, currentPath.string().c_str());

	C2D_TextParse(&statusText, statusTextBuffer, "Idle");

	populateFileList();

	int currentBufferIndex = 0;

	while (aptMainLoop()) {
		hidScanInput();

		u32 kDown = hidKeysDown();

		if (!fileList.empty()) {
			if (kDown & KEY_DOWN) {
				selectedFileIndex++;
				if (selectedFileIndex >= (int)fileList.size()) selectedFileIndex = 0;
			}
			if (kDown & KEY_UP) {
				selectedFileIndex--;
				if (selectedFileIndex < 0) selectedFileIndex = (int)fileList.size() - 1;
			}

			if (selectedFileIndex < scrollOffset) {
				scrollOffset = selectedFileIndex;
			} else if (selectedFileIndex >= scrollOffset + 14) {
				scrollOffset = selectedFileIndex - 13;
			}

			if (kDown & KEY_A) {
				if (fileList[selectedFileIndex].isDir) {
					if (fileList[selectedFileIndex].name == "..") {
						currentPath = currentPath.parent_path();
						populateFileList();
						selectedFileIndex = 0;
					} else {
						currentPath /= fileList[selectedFileIndex].name;
						populateFileList();
						selectedFileIndex = 0;
					}

					C2D_TextBufClear(pathTextBuffer);
					C2D_TextParse(&pathText, pathTextBuffer, currentPath.string().c_str());
				} else {
					std::filesystem::path targetTrack = currentPath / fileList[selectedFileIndex].name;
					std::ifstream file(targetTrack, std::ios::binary);
					if (file.is_open()) {
						ndspChnReset(hardwareChannel);
						ndspChnSetFormat(hardwareChannel, NDSP_FORMAT_STEREO_PCM16);
						ndspChnSetRate(hardwareChannel, SAMPLE_RATE);

						try {
							mod = std::make_unique<openmpt::module>(file);
							std::string titleStr = mod->get_metadata("title");
							if (titleStr.empty()) titleStr = "Untitled Track";

							std::string status = "> ";

							status += titleStr;

							C2D_TextBufClear(statusTextBuffer);
							C2D_TextParse(&statusText, statusTextBuffer, status.c_str());

							isPaused = false;
						} catch (...) {
							C2D_TextBufClear(statusTextBuffer);
							C2D_TextParse(&statusText, statusTextBuffer, "Invalid file");
						}
					} else {
						C2D_TextBufClear(statusTextBuffer);
						C2D_TextParse(&statusText, statusTextBuffer, "Couldn't open file");
					}
				}
			}
		}

		if (mod) {
			if (kDown & KEY_B && mod->get_position_seconds() < mod->get_duration_seconds()) {
				isPaused = !isPaused;
				ndspChnSetPaused(hardwareChannel, isPaused);

				std::string status = isPaused ? "|| " : "> ";

				std::string titleStr = mod->get_metadata("title");
				if (titleStr.empty()) titleStr = "Untitled Track";

				status += titleStr;

				C2D_TextBufClear(statusTextBuffer);
				C2D_TextParse(&statusText, statusTextBuffer, status.c_str());
			}

			if (kDown & KEY_DLEFT) {
				double currentPos = mod->get_position_seconds();
				mod->set_position_seconds(std::max(0.0, currentPos - 5.0));
			}

			if (kDown & KEY_DRIGHT) {
				double currentPos = mod->get_position_seconds();
				mod->set_position_seconds(currentPos + 5.0);
			}

			if ((waveBuf[currentBufferIndex].status == NDSP_WBUF_FREE || waveBuf[currentBufferIndex].status == NDSP_WBUF_DONE) && !isPaused) {
				int16_t* interleavedDest = waveBuf[currentBufferIndex].data_pcm16;
				size_t framesRead = mod->read_interleaved_stereo(SAMPLE_RATE, BUFFER_SAMPLES, interleavedDest);

				if (framesRead == 0) {
					ndspChnReset(hardwareChannel);
					ndspChnSetFormat(hardwareChannel, NDSP_FORMAT_STEREO_PCM16);
					ndspChnSetRate(hardwareChannel, SAMPLE_RATE);
				}
				if (mod->get_position_seconds() >= mod->get_duration_seconds()) {
					ndspChnReset(hardwareChannel);
					ndspChnSetFormat(hardwareChannel, NDSP_FORMAT_STEREO_PCM16);
					ndspChnSetRate(hardwareChannel, SAMPLE_RATE);
					isPaused = true;
				}

				DSP_FlushDataCache(waveBuf[currentBufferIndex].data_vaddr, BUFFER_SIZE * sizeof(int16_t));
				ndspChnWaveBufAdd(hardwareChannel, &waveBuf[currentBufferIndex]);
				currentBufferIndex = !currentBufferIndex;
			}
		}

		C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

		C2D_TargetClear(topTarget, C2D_Color32(64, 64, 64, 255));
		C2D_SceneBegin(topTarget);

		u32 textColor = C2D_Color32(255, 255, 255, 255);

		if (mod && audioBufferPool != nullptr) {
			drawCaptureWaveform(1.0f);

			C2D_TextBufClear(positionBuffer);

			double currentSecs = mod->get_position_seconds();
			double durationSecs = mod->get_duration_seconds();
			int curMins = (int)currentSecs / 60;
			int curSecs = (int)currentSecs % 60;
			int durMins = (int)durationSecs / 60;
			int durSecs = (int)durationSecs % 60;
			std::string posStr = "Time: " + std::to_string(curMins) + ":" + (curSecs < 10 ? "0" : "") + std::to_string(curSecs);
			posStr += " / " + std::to_string(durMins) + ":" + (durSecs < 10 ? "0" : "") + std::to_string(durSecs);
			C2D_TextParse(&positionText, positionBuffer, posStr.c_str());
			C2D_DrawText(&positionText, C2D_WithColor, 275.0f, 10.0f, 0.6f, 0.5f, 0.5f, textColor);
		}

		C2D_DrawText(&statusText, C2D_WithColor, 10.0f, 10.0f, 0.6f, 0.5f, 0.5f, textColor);

		C2D_TargetClear(bottomTarget, C2D_Color32(64, 64, 64, 255));
		C2D_SceneBegin(bottomTarget);

		float startX = 12.0f;
		float startY = 12.0f;
		float spacing = 14.0f;
		int maxVisibleItems = 14;

		for (int i = 0; i < maxVisibleItems; i++) {
			int itemIndex = scrollOffset + i;
			if (itemIndex >= (int)fileList.size()) break;

			float currentY = startY + (i * spacing);

			u32 color;
			if (itemIndex == selectedFileIndex) {
				color = C2D_Color32(192, 255, 0, 255);
				C2D_DrawRectSolid(startX - 8.0f, currentY + 4.0f, 0.5f, 4.0f, 4.0f, color);
			} else {
				color = C2D_Color32(255, 255, 255, 255);
			}

			C2D_DrawText(&fileTexts[itemIndex], C2D_WithColor, startX, currentY, 0.5f, 0.45f, 0.45f, color);
		}

		C2D_DrawText(&pathText, C2D_WithColor, 10.0f, 215.0f, 0.6f, 0.5f, 0.5f, textColor);

		C3D_FrameEnd(0);
	}

	linearFree(audioBufferPool);
	ndspExit();
	C2D_Fini();
	C3D_Fini();
	gfxExit();
	return 0;
}