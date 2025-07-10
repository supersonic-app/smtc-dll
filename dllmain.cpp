#include "pch.h"

#include <chrono>
#include <format>
#include <thread>
#include <mutex>
#include <queue>
#include <atomic>
#include <condition_variable>
#include <functional>

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

static std::thread smtcThread;
static std::mutex queueMutex;
static std::condition_variable queueCv;
static std::queue<std::function<void()>> commandQueue;
static std::atomic<bool> smtcThreadRunning = false;

static void smtcThreadProc() {
    winrt::init_apartment(winrt::apartment_type::multi_threaded);

    smtcThreadRunning = true;
    while (smtcThreadRunning) {
        std::function<void()> cmd;
        {
            std::unique_lock lock(queueMutex);
            queueCv.wait(lock, [] { return !commandQueue.empty() || !smtcThreadRunning; });
            if (!smtcThreadRunning) break;
            cmd = std::move(commandQueue.front());
            commandQueue.pop();
        }
        try {
            cmd();
        }
        catch (...) {
            // Swallow exceptions silently
        }
    }

    // Clean up
    smtc = nullptr;
    winrt::uninit_apartment();
}

__declspec(dllexport)
int InitializeForWindow(HWND hwnd, void (*btnCallback)(int smtcBtn), void (*seekCallback)(int millis)) {
    smtcThread = std::thread(smtcThreadProc);

    extButtonCallback = btnCallback;
    extSeekCallback = seekCallback;

    // Post a command to initialize smtc on the dedicated thread
    {
        std::lock_guard lock(queueMutex);
        commandQueue.push([hwnd]() {
            auto interop = winrt::get_activation_factory<SystemMediaTransportControls, ISystemMediaTransportControlsInterop>();
            HRESULT hr = interop->GetForWindow(hwnd, winrt::guid_of<SystemMediaTransportControls>(), winrt::put_abi(smtc));
            if (FAILED(hr)) {
                return;
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

            smtc.PlaybackPositionChangeRequested([&](const SystemMediaTransportControls&, const PlaybackPositionChangeRequestedEventArgs& args) {
                TimeSpan pos = args.RequestedPlaybackPosition();
                auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(pos);

                if (extSeekCallback) {
                    extSeekCallback(millis.count());
                }
            });
        });
    }
    queueCv.notify_one();
    return 0;
}

__declspec(dllexport)
int Destroy() {
    smtcThreadRunning = false;
    queueCv.notify_one();
    if (smtcThread.joinable()) {
        smtcThread.join();
    }
    return 0;
}

__declspec(dllexport)
int SetEnabled(int enable) {
    {
        std::lock_guard lock(queueMutex);
        commandQueue.push([enable]() {
            smtc.IsEnabled(enable);
        });
    }
    queueCv.notify_one();
    return 0;
}

__declspec(dllexport)
int SetPlaybackState(int playback_state) {
    {
        std::lock_guard lock(queueMutex);
        commandQueue.push([playback_state]() {
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
        });
    }
    queueCv.notify_one();
    return 0;
}

__declspec(dllexport)
int SetMetadata(wchar_t* title, wchar_t* artist) {
    winrt::hstring safeTitle = title;
    winrt::hstring safeArtist = artist;

    {
        std::lock_guard lock(queueMutex);
        commandQueue.push([safeTitle, safeArtist]() {
            auto updater = smtc.DisplayUpdater();
            updater.Type(MediaPlaybackType::Music);

            const auto & props = updater.MusicProperties();
            props.Title(safeTitle);
            props.Artist(safeArtist);

            updater.Update();
        });
    }
    queueCv.notify_one();
    return 0;
}

__declspec(dllexport)
int ClearAll() {
    {
        std::lock_guard lock(queueMutex);
        commandQueue.push([]() {
            auto updater = smtc.DisplayUpdater();
            updater.ClearAll();
            updater.Update();
        });
    }
    queueCv.notify_one();
    return 0;
}

__declspec(dllexport)
int SetPosition(int posMillis, int durationMillis) {
    {
        std::lock_guard lock(queueMutex);
        commandQueue.push([posMillis, durationMillis]() {
            SystemMediaTransportControlsTimelineProperties timeProps;
            timeProps.StartTime(0s);
            timeProps.MinSeekTime(0s);
            timeProps.Position(std::chrono::milliseconds(posMillis));
            timeProps.MaxSeekTime(std::chrono::milliseconds(durationMillis));
            timeProps.EndTime(std::chrono::milliseconds(durationMillis));
            smtc.UpdateTimelineProperties(timeProps);
        });
    }
    queueCv.notify_one();
    return 0;
}

__declspec(dllexport)
int SetThumbnailPath(wchar_t* filepath) {
    if (!filepath || wcslen(filepath) == 0) {
        return E_INVALIDARG;
    }
    winrt::hstring safePath = filepath;

    {
        std::lock_guard lock(queueMutex);
        commandQueue.push([safePath]() -> winrt::fire_and_forget {
            try {
                StorageFile file = co_await StorageFile::GetFileFromPathAsync(safePath);
                auto ref = RandomAccessStreamReference::CreateFromFile(file);

                auto updater = smtc.DisplayUpdater();
                updater.Thumbnail(ref);
                updater.Update();
            }
            catch (...) {
                // Swallow any errors
            }
            co_return;
        });
    }
    queueCv.notify_one();
    return 0;
}
