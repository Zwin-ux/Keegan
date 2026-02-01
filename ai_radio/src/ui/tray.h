#pragma once

#include <string>
#include <functional>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <shellapi.h>
#endif

namespace ui {

// Mood identifiers for callback
enum class MoodId {
    FocusRoom,
    RainCave,
    ArcadeNight,
    SleepShip
};

// Tray icon color/state
enum class TrayColor {
    Amber,      // Focus Room
    Blue,       // Rain Cave
    Magenta,    // Arcade Night
    Indigo      // Sleep Ship
};

// Callback types
using MoodCallback = std::function<void(MoodId)>;
using ActionCallback = std::function<void()>;

// Windows system tray controller for Keegan.
// Provides play/pause, mood selection, and visual feedback.
class TrayController {
public:
    TrayController();
    ~TrayController();

    // Initialize the tray icon. Must be called from the main thread.
    bool init(void* hInstance);

    // Show/hide the tray icon.
    void show();
    void hide();

    // Update the icon state.
    void setColor(TrayColor color);
    void setPlaying(bool playing);
    void setEnergy(float level);  // 0.0-1.0, affects pulse rate
    void setTooltip(const std::string& text);

    // Set callbacks for user actions.
    void setOnMoodSelect(MoodCallback callback) { onMoodSelect_ = callback; }
    void setOnPlayPause(ActionCallback callback) { onPlayPause_ = callback; }
    void setOnQuit(ActionCallback callback) { onQuit_ = callback; }

    // Process Windows messages. Call from main message loop.
    // Returns true if message was handled.
    bool processMessage(void* hwnd, unsigned int msg, void* wParam, void* lParam);

    // Run the message loop until quit. Blocking call.
    void runMessageLoop();

    // Request exit from message loop.
    void requestQuit();

    // Get current state
    bool isPlaying() const { return isPlaying_; }
    TrayColor currentColor() const { return currentColor_; }

private:
#ifdef _WIN32
    HWND hwnd_ = nullptr;
    NOTIFYICONDATAW nid_ = {};
    HMENU hMenu_ = nullptr;
    HINSTANCE hInstance_ = nullptr;
    
    // Icon handles for different states
    HICON iconDefault_ = nullptr;
    HICON iconPlaying_ = nullptr;
    HICON iconPaused_ = nullptr;

    // Timer for pulsing animation
    UINT_PTR pulseTimerId_ = 0;
    float energyLevel_ = 0.5f;
    bool pulseState_ = false;

    void createWindow();
    void createMenu();
    void createIcons();
    void updateIcon();
    void showContextMenu();
    
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

    MoodCallback onMoodSelect_;
    ActionCallback onPlayPause_;
    ActionCallback onQuit_;

    bool isPlaying_ = false;
    bool isVisible_ = false;
    TrayColor currentColor_ = TrayColor::Amber;
    bool shouldQuit_ = false;
};

// Convert MoodId to string identifier used in engine.
inline std::string moodIdToString(MoodId id) {
    switch (id) {
        case MoodId::FocusRoom: return "focus_room";
        case MoodId::RainCave: return "rain_cave";
        case MoodId::ArcadeNight: return "arcade_night";
        case MoodId::SleepShip: return "sleep_ship";
    }
    return "focus_room";
}

// Convert string to MoodId.
inline MoodId stringToMoodId(const std::string& str) {
    if (str == "rain_cave") return MoodId::RainCave;
    if (str == "arcade_night") return MoodId::ArcadeNight;
    if (str == "sleep_ship") return MoodId::SleepShip;
    return MoodId::FocusRoom;
}

} // namespace ui
