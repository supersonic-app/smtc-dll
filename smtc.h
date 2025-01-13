#pragma once

extern "C" {
	const int SMTCBUTTON_PLAY = 1;
	const int SMTCBUTTON_PAUSE = 2;
	const int SMTCBUTTON_STOP = 3;
	const int SMTCBUTTON_PREVIOUS = 4;
	const int SMTCBUTTON_NEXT = 5;

	const int PLAYBACKSTATE_STOPPED = 0;
	const int PLAYBACKSTATE_PLAYING = 1;
	const int PLAYBACKSTATE_PAUSED = 2;

	__declspec(dllexport)
		int InitializeForWindow(HWND hwnd, void (*btnCallback)(int smtcBtn), void (*seekCallback)(int millis));

	__declspec(dllexport)
		int UpdatePlaybackState(int playback_state);

	__declspec(dllexport)
		int UpdateMetadata(wchar_t* title, wchar_t* artist);

	__declspec(dllexport)
		int Destroy();
};
