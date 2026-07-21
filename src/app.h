#pragma once
#include <windows.h>

#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>

#include <map>
#include <string>

#include "game.h"

enum class Screen { Menu, Typing, Results };

// Owns the window and all Direct2D/DirectWrite resources; App owns time,
// Game owns counts.
class App {
public:
    bool init(HINSTANCE inst, int showCmd);
    int run();

private:
    static LRESULT CALLBACK wndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT handle(UINT msg, WPARAM wp, LPARAM lp);

    void createDeviceResources();
    void render();
    void renderMenu();
    void renderTyping();
    void renderResults();
    void onChar(wchar_t c);
    void startGame();
    void finishGame();
    void invalidate();
    void text(const std::wstring& s, IDWriteTextFormat* fmt, D2D1_RECT_F rc, ID2D1Brush* br);

    void loadScores();
    void saveScores();

    HWND hwnd_ = nullptr;

    Microsoft::WRL::ComPtr<ID2D1Factory> d2d_;
    Microsoft::WRL::ComPtr<IDWriteFactory> dwrite_;
    Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> rt_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brInk_, brDim_, brWrong_, brExtra_, brAccent_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> fmtTitle_, fmtBody_, fmtMenu_, fmtSmall_, fmtBig_,
        fmtHuge_;

    Screen screen_ = Screen::Menu;
    static constexpr int kDurations[3] = {60, 120, 300};
    static constexpr const wchar_t* kDurationLabels[3] = {L"60 seconds", L"2 minutes",
                                                          L"5 minutes"};
    int durSel_ = 0;

    Game game_;
    bool clockRunning_ = false;  // clock starts on the first typed character
    double startTime_ = 0.0;
    size_t firstVisible_ = 0;  // first word of the rolling layout window

    double finalWpm_ = 0, finalRaw_ = 0, finalAcc_ = 0;
    bool newBest_ = false;
    std::map<int, double> best_;  // duration seconds -> best net wpm
};
