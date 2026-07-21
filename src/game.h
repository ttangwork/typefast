#pragma once
#include <cstddef>
#include <random>
#include <string>
#include <vector>

struct Word {
    std::wstring target;
    std::wstring typed;
};

// Pure game logic: word stream, per-word typed state, scoring.
// No Win32 or rendering dependencies.
class Game {
public:
    void reset(unsigned seed);
    void ensureWords(size_t count);

    void typeChar(wchar_t c);
    void commitWord();               // player pressed space
    void backspace(bool wholeWord);  // wholeWord = ctrl+backspace
    void finishPending();            // credit the in-progress word at time-up

    const std::vector<Word>& words() const { return words_; }
    size_t current() const { return current_; }
    const std::wstring& currentTyped() const { return words_[current_].typed; }

    // Net WPM counts only fully correct words (their chars + one space).
    double netWpm(double minutes) const { return minutes > 0 ? (correctChars_ / 5.0) / minutes : 0.0; }
    double rawWpm(double minutes) const { return minutes > 0 ? (typedChars_ / 5.0) / minutes : 0.0; }
    double accuracy() const {
        return totalKeystrokes_ ? 100.0 * correctKeystrokes_ / totalKeystrokes_ : 100.0;
    }

private:
    std::vector<Word> words_;
    size_t current_ = 0;
    int correctChars_ = 0;
    int typedChars_ = 0;
    int correctKeystrokes_ = 0;
    int totalKeystrokes_ = 0;
    std::mt19937 rng_;
};
