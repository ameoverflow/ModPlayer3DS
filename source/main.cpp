#include <3ds.h>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <string>
#include <libopenmpt/libopenmpt.hpp>
#include <citro2d.h>
#include <dirent.h>

#include "Playback.h"

struct FileEntry {
	bool isDir;
	std::string name;
};

C3D_RenderTarget* topTarget;
C3D_RenderTarget* bottomTarget;

std::string path;

C2D_TextBuf pathTextBuffer;
C2D_Text pathText;

std::vector<FileEntry> fileList;
int selectedFileIndex = 0;
int scrollOffset = 0;

C2D_TextBuf fileBrowserBox;
std::vector<C2D_Text> fileTexts;
std::filesystem::path currentPath = "/";
std::filesystem::path currentFile;

void ScanCurrentDir() {
	fileList.clear();
	fileTexts.clear();
	C2D_TextBufClear(fileBrowserBox);

	if (currentPath.string() != "/") {
		fileList.push_back({true, ".."});
	}

	// run the list twice, first for dirs, 2nd for files, and sort it
	for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(currentPath.string())) {
		if (std::filesystem::is_directory(entry.status())) {
			fileList.push_back({true, entry.path().filename().string()});
		}
	}

	std::sort(fileList.begin(), fileList.end(), [](const FileEntry& a, const FileEntry& b) {
		return a.name < b.name;
	});

	// temp file list so it can be sorted independently
	std::vector<FileEntry> onlyFileList;
	for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(currentPath.string())) {
		if (std::filesystem::is_regular_file(entry.status())) {
			std::string ext = entry.path().extension().string();

			if (!ext.empty() && ext[0] == '.') {
				ext = ext.substr(1);
			}

			std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
				return std::tolower(c);
			});

			if (openmpt::is_extension_supported2(ext)) {
				onlyFileList.push_back({false, entry.path().filename().string()});
			}
		}
	}

	std::sort(onlyFileList.begin(), onlyFileList.end(), [](const FileEntry& a, const FileEntry& b) {
		return a.name < b.name;
	});
	fileList.insert(fileList.end(), onlyFileList.begin(), onlyFileList.end());

	fileTexts.resize(fileList.size());
	for (size_t i = 0; i < fileList.size(); i++) {
		C2D_TextParse(&fileTexts[i], fileBrowserBox, fileList[i].name.size() > 64 ?
					  (fileList[i].name.substr(0, 37) + "...").c_str() : fileList[i].name.c_str());
	}

	selectedFileIndex = 0;
	scrollOffset = 0;
}

int main(int argc, char* argv[]) {
	osSetSpeedupEnable(true);
	aptSetSleepAllowed(false);

	gfxInitDefault();
	gfxSet3D(false);
	C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
	C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
	C2D_Prepare();

	romfsInit();

	pathTextBuffer = C2D_TextBufNew(256); // path at the bottom
	fileBrowserBox = C2D_TextBufNew(16384); // file browser

	topTarget = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
	bottomTarget = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

	// init audio
	ndspInit();
	ndspSetOutputMode(NDSP_OUTPUT_STEREO);

	Playback::Init();

	C2D_TextBufClear(pathTextBuffer);
	C2D_TextParse(&pathText, pathTextBuffer, currentPath.string().c_str());

	ScanCurrentDir();

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
						ScanCurrentDir();
						selectedFileIndex = 0;
					} else {
						currentPath /= fileList[selectedFileIndex].name;
						ScanCurrentDir();
						selectedFileIndex = 0;
					}

					C2D_TextBufClear(pathTextBuffer);
					C2D_TextParse(&pathText, pathTextBuffer, currentPath.string().c_str());
				} else {
					std::filesystem::path targetTrack = currentPath / fileList[selectedFileIndex].name;
					currentFile = targetTrack;
					Playback::PlayFile(targetTrack.string());
				}
			}
		}

		if (Playback::IsModLoaded()) {
			if (kDown & KEY_B && Playback::GetPosition() < Playback::GetDuration()) {
				Playback::Pause();
			}

			if (kDown & KEY_DLEFT) {
				Playback::Backward();
			}

			if (kDown & KEY_DRIGHT) {
				Playback::Forward();
			}

			if (kDown & KEY_L) {
				int interpolation = Playback::GetInterpolation();
				switch (interpolation) {
					case 8:
						Playback::SetInterpolation(1);
						break;
					case 1:
						Playback::SetInterpolation(2);
						break;
					case 2:
						Playback::SetInterpolation(4);
						break;
					case 4:
						Playback::SetInterpolation(8);
						break;
				}
			}

			if (kDown & KEY_R) {
				int waveform = Playback::GetWaveform();
				if (waveform >= 1) {
					Playback::SetWaveform(0);
				} else {
					Playback::SetWaveform(waveform + 1);
				}
			}

			Playback::Update();
		}

		u32 textColor = C2D_Color32(255, 255, 255, 255);

		C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

		C2D_TargetClear(topTarget, C2D_Color32(32, 32, 32, 255));
		C2D_SceneBegin(topTarget);

		Playback::Draw();

		C2D_TargetClear(bottomTarget, C2D_Color32(32, 32, 32, 255));
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
				C2D_DrawRectSolid(startX - 8.0f, currentY + 6.0f, 0.5f, 4.0f, 4.0f, color);
			} else {
				if (currentPath / fileList[itemIndex].name == currentFile) {
					color = C2D_Color32(61, 88, 244, 255);
				} else {
					color = fileList[itemIndex].isDir ? C2D_Color32(128, 128, 128, 255) : C2D_Color32(255, 255, 255, 255);
				}
			}

			C2D_DrawText(&fileTexts[itemIndex], C2D_WithColor, startX, currentY, 0.5f, 0.45f, 0.45f, color);
		}

		C2D_DrawText(&pathText, C2D_WithColor, 10.0f, 215.0f, 0.5f, 0.45f, 0.45f, textColor);

		C3D_FrameEnd(0);
	}
	aptSetSleepAllowed(true);
	ndspExit();
	C2D_Fini();
	C3D_Fini();
	gfxExit();
	return 0;
}