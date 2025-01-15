#include "pch.h"

#include "smtc.h";
#include <SystemMediaTransportControlsInterop.h>
#include <windows.media.h>
#include <windows.media.control.h>
#include <wrl/event.h>
#include <roapi.h>

#pragma comment(lib, "RuntimeObject.lib")

using namespace Microsoft::WRL;
using namespace ABI::Windows::Media;
using ABI::Windows::Foundation::ITypedEventHandler;
using ABI::Windows::Foundation::TimeSpan;
using Microsoft::WRL::Wrappers::HStringReference;

typedef ITypedEventHandler<SystemMediaTransportControls*, SystemMediaTransportControlsButtonPressedEventArgs*> ButtonPressCallback;
typedef ITypedEventHandler<SystemMediaTransportControls*, PlaybackPositionChangeRequestedEventArgs*> SeekCallback;

int on_button_pressed(ISystemMediaTransportControls*, ISystemMediaTransportControlsButtonPressedEventArgs*);
int on_seek(ISystemMediaTransportControls*, IPlaybackPositionChangeRequestedEventArgs*);

ComPtr<ISystemMediaTransportControls> smtc;
ComPtr<ISystemMediaTransportControlsDisplayUpdater> displayUpdater;

EventRegistrationToken btnEvtTkn;
EventRegistrationToken seekEvtTkn;
bool btnEventsRegistered = false;
bool seekEventsRegistered = false;

void (*extButtonCallback)(int);
void (*extSeekCallback)(int);

#define IFFAILRET(hr) if (FAILED((hr))) return (hr)

__declspec(dllexport)
int InitializeForWindow(HWND hwnd, void (*btnCallback)(int smtcBtn), void (*seekCallback)(int millis)) {
    HRESULT hr;
    extButtonCallback = btnCallback;
    extSeekCallback = seekCallback;

    ComPtr<ISystemMediaTransportControlsInterop> interop;
    auto strRef = HStringReference(RuntimeClass_Windows_Media_SystemMediaTransportControls);
    hr = Windows::Foundation::GetActivationFactory(strRef.Get(), &interop);
    IFFAILRET(hr);
    hr = interop->GetForWindow(hwnd, IID_PPV_ARGS(&smtc));
    IFFAILRET(hr);

    smtc->put_IsEnabled(true);
    smtc->put_PlaybackStatus(MediaPlaybackStatus_Stopped);
    smtc->put_IsPlayEnabled(true);
    smtc->put_IsPauseEnabled(true);
    smtc->put_IsPreviousEnabled(true);
    smtc->put_IsNextEnabled(true);
    smtc->put_IsStopEnabled(true);

    hr = smtc->get_DisplayUpdater(&displayUpdater);
    IFFAILRET(hr);
    displayUpdater->put_Type(MediaPlaybackType_Music);
    displayUpdater->Update();

    ComPtr<ButtonPressCallback> cbBtnPressed = Callback<ButtonPressCallback>(on_button_pressed);
    hr = smtc->add_ButtonPressed(cbBtnPressed.Get(), &btnEvtTkn);
    IFFAILRET(hr);
    btnEventsRegistered = true;

    ComPtr<ISystemMediaTransportControls2> smtc2;
    hr = smtc.As(&smtc2);
    if (FAILED(hr)) {
        // seek won't be enabled, but buttons will work
        return 0;
    }

    ComPtr<SeekCallback> cbSeek = Callback<SeekCallback>(on_seek);
    smtc2->add_PlaybackPositionChangeRequested(cbSeek.Get(), &seekEvtTkn);
    if (SUCCEEDED(hr)) {
        seekEventsRegistered = true;
    }

    return 0;
}

__declspec(dllexport)
int Destroy() {
    if (btnEventsRegistered) {
        smtc->remove_ButtonPressed(btnEvtTkn);
        btnEventsRegistered = false;
    }
    if (seekEventsRegistered) {
        ComPtr<ISystemMediaTransportControls2> smtc2;
        HRESULT hr = smtc.As(&smtc2);
        if (SUCCEEDED(hr)) {
            smtc2->remove_PlaybackPositionChangeRequested(seekEvtTkn);
        }
    }
    displayUpdater->ClearAll();
    smtc->put_IsEnabled(false);
    return 0;
}

__declspec(dllexport)
int UpdatePlaybackState(int playback_state) {
    if (!smtc) {
        return -1;
    }

    MediaPlaybackStatus status = MediaPlaybackStatus_Stopped;
    switch (playback_state) {
    case PLAYBACKSTATE_STOPPED:
        status = MediaPlaybackStatus_Stopped;
    case PLAYBACKSTATE_PAUSED:
        status = MediaPlaybackStatus_Paused;
    case PLAYBACKSTATE_PLAYING:
        status = MediaPlaybackStatus_Playing;
    }
    return smtc->put_PlaybackStatus(status);
}

__declspec(dllexport)
int UpdateMetadata(wchar_t* title, wchar_t* artist) {
    HRESULT hr;
    ComPtr<IMusicDisplayProperties> props;
    
    hr = displayUpdater->get_MusicProperties(&props);
    IFFAILRET(hr);
 
    HSTRING hString;
    hr = WindowsCreateString(title, lstrlenW(title), &hString);
    IFFAILRET(hr);
    props->put_Title(hString);
    WindowsDeleteString(hString);

    hr = WindowsCreateString(artist, lstrlenW(artist), &hString);
    IFFAILRET(hr);
    props->put_Artist(hString);
    WindowsDeleteString(hString);

    return displayUpdater->Update();
}

__declspec(dllexport)
int UpdatePosition(int posMillis, int durationMillis) {
    HRESULT hr;
    ComPtr<ISystemMediaTransportControls2> smtc2;
    hr = smtc.As(&smtc2);
    IFFAILRET(hr);

    ComPtr<ISystemMediaTransportControlsTimelineProperties> timeProps;
    auto strRef = HStringReference(RuntimeClass_Windows_Media_SystemMediaTransportControlsTimelineProperties);
    hr = RoActivateInstance(strRef.Get(), &timeProps);
    IFFAILRET(hr);

    TimeSpan timeZero = { 0 };
    timeProps->put_MinSeekTime(timeZero);
    timeProps->put_StartTime(timeZero);

    TimeSpan curPos;
    curPos.Duration = long(posMillis) * 1000000;
    TimeSpan dur;
    dur.Duration = long(durationMillis) * 1000000;
    timeProps->put_MaxSeekTime(dur);
    timeProps->put_EndTime(dur);
    timeProps->put_Position(curPos);

    return 0;
}

int on_button_pressed(ISystemMediaTransportControls*, ISystemMediaTransportControlsButtonPressedEventArgs* args) {
    if (!extButtonCallback) {
        return -1;
    }

    SystemMediaTransportControlsButton btn;
    HRESULT hr = args->get_Button(&btn);
    IFFAILRET(hr);

    switch (btn) {
    case SystemMediaTransportControlsButton_Play:
        extButtonCallback(SMTCBUTTON_PLAY);
        return 0;
    case SystemMediaTransportControlsButton_Pause:
        extButtonCallback(SMTCBUTTON_PAUSE);
        return 0;
    case SystemMediaTransportControlsButton_Stop:
        extButtonCallback(SMTCBUTTON_STOP);
        return 0;
    case SystemMediaTransportControlsButton_Next:
        extButtonCallback(SMTCBUTTON_NEXT);
        return 0;
    case SystemMediaTransportControlsButton_Previous:
        extButtonCallback(SMTCBUTTON_PREVIOUS);
        return 0;
    }

    return -1;
}

int on_seek(ISystemMediaTransportControls*, IPlaybackPositionChangeRequestedEventArgs* args) {
    if (!extSeekCallback) {
        return -1;
    }
    ABI::Windows::Foundation::TimeSpan time;
    HRESULT hr = args->get_RequestedPlaybackPosition(&time);
    IFFAILRET(hr);
    int millis = int(time.Duration / 1000000);

    extSeekCallback(millis);
    return 0;
}
