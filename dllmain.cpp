#include "pch.h"

#include <chrono>
#include <format>

#include "smtc.h"
#include <SystemMediaTransportControlsInterop.h>
#include <winrt/windows.foundation.h>
#include <winrt/windows.media.h>
#include <winrt/windows.storage.h>
#include <winrt/windows.storage.streams.h>

#pragma comment(lib, "RuntimeObject.lib")

using namespace std::chrono_literals;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Media;
using winrt::Windows::Storage::StorageFile;
using winrt::Windows::Storage::Streams::RandomAccessStreamReference;

SystemMediaTransportControls smtc{ nullptr };

void (*extButtonCallback)(int);
void (*extSeekCallback)(int);

__declspec(dllexport)
int InitializeForWindow(HWND hwnd, void (*btnCallback)(int smtcBtn), void (*seekCallback)(int millis)) {
    extButtonCallback = btnCallback;
    extSeekCallback = seekCallback;

    auto interop = winrt::get_activation_factory<SystemMediaTransportControls, ISystemMediaTransportControlsInterop>();
    HRESULT hr = interop->GetForWindow(hwnd, winrt::guid_of<SystemMediaTransportControls>(), winrt::put_abi(smtc));
    if (FAILED(hr)) {
        return hr;
    }

    smtc.IsEnabled(true);
    smtc.PlaybackStatus(MediaPlaybackStatus::Stopped);
    smtc.IsPlayEnabled(true);
    smtc.IsPauseEnabled(true);
    smtc.IsPreviousEnabled(true);
    smtc.IsNextEnabled(true);
    smtc.IsStopEnabled(true);

    auto updater = smtc.DisplayUpdater();
    updater.Type(MediaPlaybackType::Music);
    updater.Update();

    smtc.ButtonPressed([&](const SystemMediaTransportControls&, const SystemMediaTransportControlsButtonPressedEventArgs& args) {
        if (!extButtonCallback) {
            return -1;
        }

        switch (args.Button()) {
        case SystemMediaTransportControlsButton::Play:
            extButtonCallback(SMTCBUTTON_PLAY);
            return 0;
        case SystemMediaTransportControlsButton::Pause:
            extButtonCallback(SMTCBUTTON_PAUSE);
            return 0;
        case SystemMediaTransportControlsButton::Stop:
            extButtonCallback(SMTCBUTTON_STOP);
            return 0;
        case SystemMediaTransportControlsButton::Previous:
            extButtonCallback(SMTCBUTTON_PREVIOUS);
            return 0;
        case SystemMediaTransportControlsButton::Next:
            extButtonCallback(SMTCBUTTON_NEXT);
            return 0;
        }

        return -1;
    });

    smtc.PlaybackPositionChangeRequested([&](const SystemMediaTransportControls&, const PlaybackPositionChangeRequestedEventArgs &args) {
        TimeSpan pos = args.RequestedPlaybackPosition();
        auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(pos);

        if (extSeekCallback) {
            extSeekCallback(millis.count());
        }
    });

    return 0;
}

__declspec(dllexport)
int SetEnabled(int enable) {
    smtc.IsEnabled(enable > 0);
    return 0;
}

__declspec(dllexport)
int Destroy() {
    smtc.IsEnabled(false);
    return 0;
}

__declspec(dllexport)
int SetPlaybackState(int playback_state) {
    switch (playback_state) {
    case PLAYBACKSTATE_STOPPED:
        smtc.PlaybackStatus(MediaPlaybackStatus::Stopped);
        break;
    case PLAYBACKSTATE_PLAYING:
        smtc.PlaybackStatus(MediaPlaybackStatus::Playing);
        break;
    case PLAYBACKSTATE_PAUSED:
        smtc.PlaybackStatus(MediaPlaybackStatus::Paused);
        break;
    }

    return 0;
}

__declspec(dllexport)
int SetMetadata(wchar_t* title, wchar_t* artist) {
    auto updater = smtc.DisplayUpdater();
    updater.Type(MediaPlaybackType::Music);

    const auto& props = updater.MusicProperties();
    props.Title(winrt::to_hstring(title));
    props.Artist(winrt::to_hstring(artist));

    updater.Update();
    return 0;
}

__declspec(dllexport)
int ClearAll() {
    auto updater = smtc.DisplayUpdater();
    updater.ClearAll();
    updater.Update();
}

__declspec(dllexport)
int SetPosition(int posMillis, int durationMillis) {
    SystemMediaTransportControlsTimelineProperties timeProps;
    timeProps.StartTime(0s);
    timeProps.MinSeekTime(0s);
    timeProps.Position(std::chrono::milliseconds(posMillis));
    timeProps.MaxSeekTime(std::chrono::milliseconds(durationMillis));
    timeProps.EndTime(std::chrono::milliseconds(durationMillis));
    smtc.UpdateTimelineProperties(timeProps);

    return 0;
}

__declspec(dllexport)
int SetThumbnailPath(wchar_t* filepath) {
    try {
        StorageFile storageFile = StorageFile::GetFileFromPathAsync(winrt::to_hstring(filepath)).get();
        RandomAccessStreamReference ref = RandomAccessStreamReference::CreateFromFile(storageFile);
        auto updater = smtc.DisplayUpdater();
        updater.Thumbnail(ref);
        updater.Update();
    } catch (winrt::hresult_error e){
        return e.code();
    }
    return 0;
}
