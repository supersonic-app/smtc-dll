#pragma once

extern "C" {
	const int SMTCBUTTON_PLAY = 0;
	const int SMTCBUTTON_PAUSE = 1;
	const int SMTCBUTTON_STOP = 2;
	const int SMTCBUTTON_PREVIOUS = 4;
	const int SMTCBUTTON_NEXT = 5;

	const int PLAYBACKSTATE_STOPPED = 2;
	const int PLAYBACKSTATE_PLAYING = 3;
	const int PLAYBACKSTATE_PAUSED = 4;

	__declspec(dllexport)
		int InitializeForWindow(HWND hwnd, void (*btnCallback)(int smtcBtn), void (*seekCallback)(int millis));

	__declspec(dllexport)
		int SetEnabled(int enable);

	__declspec(dllexport)
		int SetPlaybackState(int playback_state);

	__declspec(dllexport)
		int SetMetadata(wchar_t* title, wchar_t* artist);

	__declspec(dllexport)
		int SetPosition(int posMillis, int durationMillis);

	__declspec(dllexport)
		int SetThumbnailPath(wchar_t* filepath);

	__declspec(dllexport)
		int ClearAll();

	__declspec(dllexport)
		int Destroy();
};
