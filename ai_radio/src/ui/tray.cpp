#include "tray.h"
#include "../util/logger.h"
#include <string>

#ifdef _WIN32

namespace ui {

namespace {
constexpr UINT WM_TRAYICON = WM_USER + 1;
constexpr UINT IDM_FOCUS = 1001;
constexpr UINT IDM_RAIN = 1002;
constexpr UINT IDM_ARCADE = 1003;
constexpr UINT IDM_SLEEP = 1004;
constexpr UINT IDM_PLAYPAUSE = 1010;
constexpr UINT IDM_QUIT = 1099;
constexpr UINT IDT_PULSE = 2001;

const wchar_t* CLASS_NAME = L"KeeganTrayClass";
const wchar_t* WINDOW_NAME = L"Keegan";

TrayController* g_instance = nullptr;

// Create a simple colored icon programmatically
HICON createColoredIcon(COLORREF color, bool filled) {
    HDC hdcScreen = GetDC(nullptr);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    
    const int size = 16;
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = size;
    bmi.bmiHeader.biHeight = size;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP hbmColor = CreateDIBSection(hdcMem, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HBITMAP hbmMask = CreateCompatibleBitmap(hdcScreen, size, size);

    if (bits) {
        uint32_t* pixels = static_cast<uint32_t*>(bits);
        uint32_t col = (GetRValue(color) << 16) | (GetGValue(color) << 8) | GetBValue(color) | 0xFF000000;
        uint32_t bg = 0x00000000;

        int cx = size / 2;
        int cy = size / 2;
        int r = size / 2 - 1;
        int rInner = r - 3;

        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                int dx = x - cx;
                int dy = (size - 1 - y) - cy; // Flip Y for bottom-up bitmap
                int dist = dx * dx + dy * dy;
                
                if (filled) {
                    pixels[y * size + x] = (dist <= r * r) ? col : bg;
                } else {
                    // Ring shape
                    pixels[y * size + x] = (dist <= r * r && dist >= rInner * rInner) ? col : bg;
                }
            }
        }
    }

    // Create mask
    HDC hdcMask = CreateCompatibleDC(hdcScreen);
    HBITMAP oldMask = (HBITMAP)SelectObject(hdcMask, hbmMask);
    RECT rc = {0, 0, size, size};
    FillRect(hdcMask, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));
    SelectObject(hdcMask, oldMask);
    DeleteDC(hdcMask);

    ICONINFO ii = {};
    ii.fIcon = TRUE;
    ii.hbmColor = hbmColor;
    ii.hbmMask = hbmMask;
    HICON icon = CreateIconIndirect(&ii);

    DeleteObject(hbmColor);
    DeleteObject(hbmMask);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);

    return icon;
}

COLORREF getColorForMood(TrayColor color) {
    switch (color) {
        case TrayColor::Amber:   return RGB(255, 191, 0);   // Focus Room - warm amber
        case TrayColor::Blue:    return RGB(64, 164, 223);  // Rain Cave - deep teal
        case TrayColor::Magenta: return RGB(255, 0, 128);   // Arcade Night - hot pink
        case TrayColor::Indigo:  return RGB(75, 0, 130);    // Sleep Ship - deep indigo
    }
    return RGB(255, 191, 0);
}

} // namespace

TrayController::TrayController() {
    g_instance = this;
}

TrayController::~TrayController() {
    hide();
    if (hMenu_) DestroyMenu(hMenu_);
    if (iconDefault_) DestroyIcon(iconDefault_);
    if (iconPlaying_) DestroyIcon(iconPlaying_);
    if (iconPaused_) DestroyIcon(iconPaused_);
    if (hwnd_) {
        KillTimer(hwnd_, IDT_PULSE);
        DestroyWindow(hwnd_);
    }
    UnregisterClassW(CLASS_NAME, hInstance_);
    g_instance = nullptr;
}

bool TrayController::init(void* hInstance) {
    hInstance_ = static_cast<HINSTANCE>(hInstance);
    
    // Register window class
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance_;
    wc.lpszClassName = CLASS_NAME;
    
    if (!RegisterClassExW(&wc)) {
        util::logError("TrayController: Failed to register window class");
        return false;
    }

    createWindow();
    createMenu();
    createIcons();

    // Setup notification icon data
    ZeroMemory(&nid_, sizeof(nid_));
    nid_.cbSize = sizeof(NOTIFYICONDATAW);
    nid_.hWnd = hwnd_;
    nid_.uID = 1;
    nid_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid_.uCallbackMessage = WM_TRAYICON;
    nid_.hIcon = iconDefault_;
    wcscpy_s(nid_.szTip, L"Keegan - Focus Room");

    util::logInfo("TrayController: Initialized");
    return true;
}

void TrayController::createWindow() {
    hwnd_ = CreateWindowExW(
        0, CLASS_NAME, WINDOW_NAME,
        WS_OVERLAPPED,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        nullptr, nullptr, hInstance_, nullptr
    );
}

void TrayController::createMenu() {
    hMenu_ = CreatePopupMenu();
    
    AppendMenuW(hMenu_, MF_STRING, IDM_PLAYPAUSE, L"▶ Play");
    AppendMenuW(hMenu_, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu_, MF_STRING, IDM_FOCUS, L"🎯 Focus Room");
    AppendMenuW(hMenu_, MF_STRING, IDM_RAIN, L"🌧 Rain Cave");
    AppendMenuW(hMenu_, MF_STRING, IDM_ARCADE, L"🕹 Arcade Night");
    AppendMenuW(hMenu_, MF_STRING, IDM_SLEEP, L"🚀 Sleep Ship");
    AppendMenuW(hMenu_, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu_, MF_STRING, IDM_QUIT, L"Quit");

    // Default selection
    CheckMenuRadioItem(hMenu_, IDM_FOCUS, IDM_SLEEP, IDM_FOCUS, MF_BYCOMMAND);
}

void TrayController::createIcons() {
    iconDefault_ = createColoredIcon(getColorForMood(currentColor_), true);
    iconPlaying_ = iconDefault_;
    iconPaused_ = createColoredIcon(getColorForMood(currentColor_), false);
}

void TrayController::show() {
    if (isVisible_) return;
    Shell_NotifyIconW(NIM_ADD, &nid_);
    isVisible_ = true;

    // Start pulse timer
    SetTimer(hwnd_, IDT_PULSE, 500, nullptr);
    util::logInfo("TrayController: Tray icon shown");
}

void TrayController::hide() {
    if (!isVisible_) return;
    Shell_NotifyIconW(NIM_DELETE, &nid_);
    isVisible_ = false;
    KillTimer(hwnd_, IDT_PULSE);
}

void TrayController::setColor(TrayColor color) {
    if (color == currentColor_) return;
    currentColor_ = color;

    // Recreate icons with new color
    if (iconDefault_) DestroyIcon(iconDefault_);
    if (iconPaused_) DestroyIcon(iconPaused_);
    
    iconDefault_ = createColoredIcon(getColorForMood(color), true);
    iconPlaying_ = iconDefault_;
    iconPaused_ = createColoredIcon(getColorForMood(color), false);

    updateIcon();

    // Update menu checkmark
    UINT menuId = IDM_FOCUS;
    switch (color) {
        case TrayColor::Amber: menuId = IDM_FOCUS; break;
        case TrayColor::Blue: menuId = IDM_RAIN; break;
        case TrayColor::Magenta: menuId = IDM_ARCADE; break;
        case TrayColor::Indigo: menuId = IDM_SLEEP; break;
    }
    CheckMenuRadioItem(hMenu_, IDM_FOCUS, IDM_SLEEP, menuId, MF_BYCOMMAND);
}

void TrayController::setPlaying(bool playing) {
    if (playing == isPlaying_) return;
    isPlaying_ = playing;
    
    // Update menu text
    ModifyMenuW(hMenu_, IDM_PLAYPAUSE, MF_BYCOMMAND | MF_STRING, IDM_PLAYPAUSE,
                playing ? L"⏸ Pause" : L"▶ Play");
    
    updateIcon();
}

void TrayController::setEnergy(float level) {
    energyLevel_ = level;
    // Adjust pulse timer based on energy
    UINT interval = static_cast<UINT>(800 - level * 500); // 300-800ms
    SetTimer(hwnd_, IDT_PULSE, interval, nullptr);
}

void TrayController::setTooltip(const std::string& text) {
    std::wstring wtext(text.begin(), text.end());
    wcsncpy_s(nid_.szTip, wtext.c_str(), 127);
    if (isVisible_) {
        Shell_NotifyIconW(NIM_MODIFY, &nid_);
    }
}

void TrayController::updateIcon() {
    if (!isVisible_) return;
    
    nid_.hIcon = (isPlaying_ && !pulseState_) ? iconPaused_ : iconDefault_;
    Shell_NotifyIconW(NIM_MODIFY, &nid_);
}

void TrayController::showContextMenu() {
    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd_);
    TrackPopupMenu(hMenu_, TPM_RIGHTALIGN | TPM_BOTTOMALIGN,
                   pt.x, pt.y, 0, hwnd_, nullptr);
    PostMessage(hwnd_, WM_NULL, 0, 0);
}

LRESULT CALLBACK TrayController::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (!g_instance) return DefWindowProcW(hwnd, msg, wParam, lParam);

    switch (msg) {
        case WM_TRAYICON:
            if (lParam == WM_LBUTTONUP) {
                // Toggle play/pause
                if (g_instance->onPlayPause_) {
                    g_instance->onPlayPause_();
                }
            } else if (lParam == WM_RBUTTONUP) {
                g_instance->showContextMenu();
            }
            return 0;

        case WM_COMMAND: {
            UINT id = LOWORD(wParam);
            switch (id) {
                case IDM_PLAYPAUSE:
                    if (g_instance->onPlayPause_) g_instance->onPlayPause_();
                    break;
                case IDM_FOCUS:
                    if (g_instance->onMoodSelect_) g_instance->onMoodSelect_(MoodId::FocusRoom);
                    g_instance->setColor(TrayColor::Amber);
                    break;
                case IDM_RAIN:
                    if (g_instance->onMoodSelect_) g_instance->onMoodSelect_(MoodId::RainCave);
                    g_instance->setColor(TrayColor::Blue);
                    break;
                case IDM_ARCADE:
                    if (g_instance->onMoodSelect_) g_instance->onMoodSelect_(MoodId::ArcadeNight);
                    g_instance->setColor(TrayColor::Magenta);
                    break;
                case IDM_SLEEP:
                    if (g_instance->onMoodSelect_) g_instance->onMoodSelect_(MoodId::SleepShip);
                    g_instance->setColor(TrayColor::Indigo);
                    break;
                case IDM_QUIT:
                    if (g_instance->onQuit_) g_instance->onQuit_();
                    g_instance->requestQuit();
                    break;
            }
            return 0;
        }

        case WM_TIMER:
            if (wParam == IDT_PULSE && g_instance->isPlaying_) {
                g_instance->pulseState_ = !g_instance->pulseState_;
                g_instance->updateIcon();
            }
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void TrayController::runMessageLoop() {
    MSG msg;
    while (!shouldQuit_ && GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

void TrayController::requestQuit() {
    shouldQuit_ = true;
    PostMessageW(hwnd_, WM_QUIT, 0, 0);
}

bool TrayController::processMessage(void* hwnd, unsigned int msg, void* wParam, void* lParam) {
    // For integration with custom message loops
    return false;
}

} // namespace ui

#endif // _WIN32
