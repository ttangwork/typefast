#include "app.h"

#include <shlobj.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace {

constexpr UINT_PTR kTimerId = 1;
constexpr float kLineHeight = 46.0f;
constexpr int kVisibleLines = 3;

double qpcNow() {
    static const double freq = [] {
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        return static_cast<double>(f.QuadPart);
    }();
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return c.QuadPart / freq;
}

std::wstring scoresPath() {
    PWSTR roaming = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &roaming)))
        return L"";
    std::wstring dir = roaming;
    CoTaskMemFree(roaming);
    dir += L"\\typefast";
    CreateDirectoryW(dir.c_str(), nullptr);
    return dir + L"\\scores.txt";
}

}  // namespace

bool App::init(HINSTANCE inst, int showCmd) {
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, d2d_.GetAddressOf())))
        return false;
    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                   reinterpret_cast<IUnknown**>(dwrite_.GetAddressOf()))))
        return false;

    auto makeFormat = [&](const wchar_t* family, float size, DWRITE_FONT_WEIGHT weight,
                          DWRITE_TEXT_ALIGNMENT align, ComPtr<IDWriteTextFormat>& out) {
        if (FAILED(dwrite_->CreateTextFormat(family, nullptr, weight, DWRITE_FONT_STYLE_NORMAL,
                                             DWRITE_FONT_STRETCH_NORMAL, size, L"en-us",
                                             out.GetAddressOf())))
            return false;
        out->SetTextAlignment(align);
        return true;
    };
    // Calligraphy for the title only; legible monospace for everything typed.
    if (!makeFormat(L"Segoe Script", 64, DWRITE_FONT_WEIGHT_BOLD, DWRITE_TEXT_ALIGNMENT_CENTER,
                    fmtTitle_) ||
        !makeFormat(L"Consolas", 28, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_LEADING,
                    fmtBody_) ||
        !makeFormat(L"Consolas", 22, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER,
                    fmtMenu_) ||
        !makeFormat(L"Consolas", 14, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER,
                    fmtSmall_) ||
        !makeFormat(L"Consolas", 40, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER,
                    fmtBig_) ||
        !makeFormat(L"Consolas", 110, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_TEXT_ALIGNMENT_CENTER,
                    fmtHuge_))
        return false;
    fmtBody_->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM, kLineHeight, kLineHeight * 0.8f);

    WNDCLASSW wc = {};
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = wndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = L"typefast";
    if (!RegisterClassW(&wc))
        return false;

    const UINT dpi = GetDpiForSystem();
    hwnd_ = CreateWindowExW(0, L"typefast", L"typefast", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                            CW_USEDEFAULT, MulDiv(960, dpi, 96), MulDiv(640, dpi, 96), nullptr,
                            nullptr, inst, this);
    if (!hwnd_)
        return false;

    loadScores();
    ShowWindow(hwnd_, showCmd);
    return true;
}

int App::run() {
    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

LRESULT CALLBACK App::wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCCREATE) {
        auto* app = static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        app->hwnd_ = hwnd;
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    auto* app = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    return app ? app->handle(msg, wp, lp) : DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT App::handle(UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd_, &ps);
        render();
        EndPaint(hwnd_, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_CHAR:
        onChar(static_cast<wchar_t>(wp));
        return 0;
    case WM_TIMER:
        if (wp == kTimerId && screen_ == Screen::Typing) {
            if (clockRunning_ && qpcNow() - startTime_ >= kDurations[durSel_])
                finishGame();
            invalidate();
        }
        return 0;
    case WM_SIZE:
        if (rt_)
            rt_->Resize(D2D1::SizeU(LOWORD(lp), HIWORD(lp)));
        return 0;
    case WM_DPICHANGED: {
        if (rt_) {
            const float dpi = static_cast<float>(HIWORD(wp));
            rt_->SetDpi(dpi, dpi);
        }
        const RECT* r = reinterpret_cast<RECT*>(lp);
        SetWindowPos(hwnd_, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd_, msg, wp, lp);
}

void App::createDeviceResources() {
    if (rt_)
        return;
    RECT rc;
    GetClientRect(hwnd_, &rc);
    if (FAILED(d2d_->CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(),
            D2D1::HwndRenderTargetProperties(
                hwnd_, D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top)),
            rt_.GetAddressOf())))
        return;
    const float dpi = static_cast<float>(GetDpiForWindow(hwnd_));
    rt_->SetDpi(dpi, dpi);

    const struct {
        ComPtr<ID2D1SolidColorBrush>* brush;
        UINT32 rgb;
    } brushes[] = {
        {&brInk_, 0xE6E4DE},   {&brDim_, 0x5E5E68},    {&brWrong_, 0xE05252},
        {&brExtra_, 0x8A3A3A}, {&brAccent_, 0xD8A24A},
    };
    for (const auto& b : brushes)
        rt_->CreateSolidColorBrush(D2D1::ColorF(b.rgb), b.brush->GetAddressOf());
}

void App::render() {
    createDeviceResources();
    if (!rt_)
        return;
    rt_->BeginDraw();
    rt_->Clear(D2D1::ColorF(0x121216));
    switch (screen_) {
    case Screen::Menu:
        renderMenu();
        break;
    case Screen::Typing:
        renderTyping();
        break;
    case Screen::Results:
        renderResults();
        break;
    }
    if (rt_->EndDraw() == D2DERR_RECREATE_TARGET) {
        rt_.Reset();
        brInk_.Reset();
        brDim_.Reset();
        brWrong_.Reset();
        brExtra_.Reset();
        brAccent_.Reset();
    }
}

void App::text(const std::wstring& s, IDWriteTextFormat* fmt, D2D1_RECT_F rc, ID2D1Brush* br) {
    rt_->DrawText(s.c_str(), static_cast<UINT32>(s.size()), fmt, rc, br);
}

void App::renderMenu() {
    const D2D1_SIZE_F sz = rt_->GetSize();
    text(L"typefast", fmtTitle_.Get(), {0, sz.height * 0.13f, sz.width, sz.height * 0.13f + 110},
         brInk_.Get());
    text(L"an offline typing game", fmtSmall_.Get(),
         {0, sz.height * 0.13f + 118, sz.width, sz.height * 0.13f + 140}, brDim_.Get());

    for (int i = 0; i < 3; ++i) {
        const float y = sz.height * 0.48f + i * 40.0f;
        const std::wstring line = L"[" + std::to_wstring(i + 1) + L"]  " + kDurationLabels[i];
        text(line, fmtMenu_.Get(), {0, y, sz.width, y + 34},
             i == durSel_ ? brAccent_.Get() : brDim_.Get());
    }

    const auto it = best_.find(kDurations[durSel_]);
    if (it != best_.end()) {
        wchar_t buf[64];
        swprintf_s(buf, L"personal best \x00b7 %.1f wpm", it->second);
        const float y = sz.height * 0.48f + 3 * 40.0f + 16;
        text(buf, fmtSmall_.Get(), {0, y, sz.width, y + 22}, brDim_.Get());
    }

    text(L"1/2/3 \x00b7 duration     enter \x00b7 start     esc \x00b7 quit", fmtSmall_.Get(),
         {0, sz.height - 56, sz.width, sz.height - 32}, brDim_.Get());
}

void App::renderTyping() {
    const D2D1_SIZE_F sz = rt_->GetSize();
    float contentW = std::min(sz.width - 96.0f, 900.0f);
    if (contentW < 200.0f)
        contentW = std::max(sz.width - 16.0f, 100.0f);
    const float left = (sz.width - contentW) / 2.0f;
    const float top = sz.height * 0.34f;

    const double elapsed = clockRunning_ ? qpcNow() - startTime_ : 0.0;
    const int remaining =
        std::max(static_cast<int>(std::ceil(kDurations[durSel_] - elapsed)), 0);
    wchar_t clock[16];
    swprintf_s(clock, L"%d:%02d", remaining / 60, remaining % 60);
    text(clock, fmtBig_.Get(), {0, top - 122, sz.width, top - 62},
         clockRunning_ ? brAccent_.Get() : brDim_.Get());
    if (!clockRunning_)
        text(L"the clock starts on your first keystroke", fmtSmall_.Get(),
             {0, top - 54, sz.width, top - 32}, brDim_.Get());
    text(L"esc \x00b7 menu     ctrl+backspace \x00b7 clear word", fmtSmall_.Get(),
         {0, sz.height - 48, sz.width, sz.height - 24}, brDim_.Get());

    game_.ensureWords(firstVisible_ + 90);
    const std::vector<Word>& words = game_.words();
    const size_t cur = game_.current();

    ComPtr<IDWriteTextLayout> layout;
    UINT32 caret = 0;

    // Rebuild the layout from the rolling window; when the caret reaches the
    // third line, drop the words of the first line and lay out again.
    for (int pass = 0; pass < 8; ++pass) {
        std::wstring txt;
        std::vector<UINT32> starts;
        struct Colored {
            DWRITE_TEXT_RANGE range;
            ID2D1Brush* brush;
        };
        std::vector<Colored> colored;
        auto color = [&](UINT32 pos, UINT32 len, ID2D1Brush* brush) {
            if (!colored.empty() && colored.back().brush == brush &&
                colored.back().range.startPosition + colored.back().range.length == pos)
                colored.back().range.length += len;
            else
                colored.push_back({{pos, len}, brush});
        };

        const size_t end = std::min(words.size(), firstVisible_ + 80);
        for (size_t i = firstVisible_; i < end; ++i) {
            starts.push_back(static_cast<UINT32>(txt.size()));
            const Word& w = words[i];
            const size_t typed = w.typed.size();
            for (size_t j = 0; j < w.target.size(); ++j) {
                const UINT32 pos = static_cast<UINT32>(txt.size());
                txt.push_back(w.target[j]);
                if (j < typed)
                    color(pos, 1, w.typed[j] == w.target[j] ? brInk_.Get() : brWrong_.Get());
                else if (i < cur)
                    color(pos, 1, brExtra_.Get());  // letters skipped in a committed word
            }
            if (typed > w.target.size()) {
                const UINT32 pos = static_cast<UINT32>(txt.size());
                txt.append(w.typed, w.target.size(), typed - w.target.size());
                color(pos, static_cast<UINT32>(typed - w.target.size()), brExtra_.Get());
            }
            if (i == cur)
                caret = starts.back() + static_cast<UINT32>(typed);
            txt.push_back(L' ');
        }

        layout.Reset();
        if (FAILED(dwrite_->CreateTextLayout(txt.c_str(), static_cast<UINT32>(txt.size()),
                                             fmtBody_.Get(), contentW, 10000.0f,
                                             layout.GetAddressOf())))
            return;
        for (const Colored& c : colored)
            layout->SetDrawingEffect(c.brush, c.range);

        if (firstVisible_ >= cur)
            break;
        UINT32 lineCount = 0;
        layout->GetLineMetrics(nullptr, 0, &lineCount);
        std::vector<DWRITE_LINE_METRICS> lines(lineCount);
        if (lineCount == 0 || FAILED(layout->GetLineMetrics(lines.data(), lineCount, &lineCount)))
            break;
        UINT32 caretLine = 0;
        UINT32 accum = 0;
        for (UINT32 li = 0; li < lineCount; ++li) {
            accum += lines[li].length;
            if (caret < accum) {
                caretLine = li;
                break;
            }
        }
        if (caretLine < 2)
            break;
        size_t advance = 0;
        for (const UINT32 s : starts)
            if (s < lines[0].length)
                ++advance;
        advance = std::max<size_t>(advance, 1);
        firstVisible_ = std::min(firstVisible_ + advance, cur);
    }
    if (!layout)
        return;

    rt_->PushAxisAlignedClip(
        {left - 4, top - 4, left + contentW + 4, top + kVisibleLines * kLineHeight + 4},
        D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    rt_->DrawTextLayout({left, top}, layout.Get(), brDim_.Get());

    FLOAT cx = 0, cy = 0;
    DWRITE_HIT_TEST_METRICS hm = {};
    layout->HitTestTextPosition(caret, FALSE, &cx, &cy, &hm);
    const bool caretOn = !clockRunning_ || (GetTickCount64() / 530) % 2 == 0;
    if (caretOn)
        rt_->DrawLine({left + cx, top + cy + 8}, {left + cx, top + cy + hm.height - 8},
                      brAccent_.Get(), 2.0f);
    rt_->PopAxisAlignedClip();
}

void App::renderResults() {
    const D2D1_SIZE_F sz = rt_->GetSize();
    wchar_t buf[128];

    text(L"words per minute", fmtSmall_.Get(),
         {0, sz.height * 0.16f, sz.width, sz.height * 0.16f + 22}, brDim_.Get());
    swprintf_s(buf, L"%.0f", finalWpm_);
    text(buf, fmtHuge_.Get(), {0, sz.height * 0.19f, sz.width, sz.height * 0.19f + 135},
         brInk_.Get());

    swprintf_s(buf, L"raw %.1f     accuracy %.1f%%     %s", finalRaw_, finalAcc_,
               kDurationLabels[durSel_]);
    text(buf, fmtMenu_.Get(), {0, sz.height * 0.55f, sz.width, sz.height * 0.55f + 30},
         brDim_.Get());

    const float bestY = sz.height * 0.64f;
    if (newBest_) {
        text(L"new personal best!", fmtMenu_.Get(), {0, bestY, sz.width, bestY + 30},
             brAccent_.Get());
    } else {
        const auto it = best_.find(kDurations[durSel_]);
        if (it != best_.end()) {
            swprintf_s(buf, L"personal best \x00b7 %.1f wpm", it->second);
            text(buf, fmtSmall_.Get(), {0, bestY, sz.width, bestY + 22}, brDim_.Get());
        }
    }

    text(L"enter \x00b7 go again     esc \x00b7 menu", fmtSmall_.Get(),
         {0, sz.height - 56, sz.width, sz.height - 32}, brDim_.Get());
}

void App::onChar(wchar_t c) {
    switch (screen_) {
    case Screen::Menu:
        if (c >= L'1' && c <= L'3') {
            durSel_ = c - L'1';
            invalidate();
        } else if (c == L'\r') {
            startGame();
        } else if (c == 0x1B) {
            PostQuitMessage(0);
        }
        break;
    case Screen::Typing:
        if (c == 0x1B) {
            KillTimer(hwnd_, kTimerId);
            screen_ = Screen::Menu;
            invalidate();
        } else if (c == 0x08) {
            game_.backspace(false);
            invalidate();
        } else if (c == 0x7F) {  // ctrl+backspace arrives as DEL
            game_.backspace(true);
            invalidate();
        } else if (c == L' ') {
            if (!game_.currentTyped().empty()) {
                game_.commitWord();
                invalidate();
            }
        } else if (c >= 32) {
            if (!clockRunning_) {
                clockRunning_ = true;
                startTime_ = qpcNow();
            }
            game_.typeChar(c);
            invalidate();
        }
        break;
    case Screen::Results:
        if (c == L'\r') {
            startGame();
        } else if (c == 0x1B) {
            screen_ = Screen::Menu;
            invalidate();
        }
        break;
    }
}

void App::startGame() {
    game_.reset(std::random_device{}());
    firstVisible_ = 0;
    clockRunning_ = false;
    newBest_ = false;
    screen_ = Screen::Typing;
    SetTimer(hwnd_, kTimerId, 50, nullptr);
    invalidate();
}

void App::finishGame() {
    KillTimer(hwnd_, kTimerId);
    game_.finishPending();
    const double minutes = kDurations[durSel_] / 60.0;
    finalWpm_ = game_.netWpm(minutes);
    finalRaw_ = game_.rawWpm(minutes);
    finalAcc_ = game_.accuracy();

    newBest_ = false;
    if (finalWpm_ > 0) {
        const auto it = best_.find(kDurations[durSel_]);
        if (it == best_.end() || finalWpm_ > it->second) {
            best_[kDurations[durSel_]] = finalWpm_;
            saveScores();
            newBest_ = true;
        }
    }
    screen_ = Screen::Results;
    invalidate();
}

void App::invalidate() {
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void App::loadScores() {
    const std::wstring path = scoresPath();
    if (path.empty())
        return;
    FILE* f = nullptr;
    if (_wfopen_s(&f, path.c_str(), L"r") != 0 || !f)
        return;
    int duration = 0;
    double wpm = 0;
    while (fwscanf_s(f, L"%d %lf", &duration, &wpm) == 2)
        best_[duration] = wpm;
    fclose(f);
}

void App::saveScores() {
    const std::wstring path = scoresPath();
    if (path.empty())
        return;
    FILE* f = nullptr;
    if (_wfopen_s(&f, path.c_str(), L"w") != 0 || !f)
        return;
    for (const auto& [duration, wpm] : best_)
        fwprintf_s(f, L"%d %.1f\n", duration, wpm);
    fclose(f);
}
