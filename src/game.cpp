#include "game.h"

#include "words.h"

void Game::reset(unsigned seed) {
    rng_.seed(seed);
    words_.clear();
    current_ = 0;
    correctChars_ = 0;
    typedChars_ = 0;
    correctKeystrokes_ = 0;
    totalKeystrokes_ = 0;
    ensureWords(100);
}

void Game::ensureWords(size_t count) {
    std::uniform_int_distribution<size_t> pick(0, kWordCount - 1);
    while (words_.size() < count) {
        const wchar_t* w = kWords[pick(rng_)];
        if (!words_.empty() && words_.back().target == w)
            continue;  // no immediate repeats
        words_.push_back({w, {}});
    }
}

void Game::typeChar(wchar_t c) {
    Word& w = words_[current_];
    if (w.typed.size() >= w.target.size() + 8)
        return;  // cap runaway extra characters on one word
    ++totalKeystrokes_;
    if (w.typed.size() < w.target.size() && c == w.target[w.typed.size()])
        ++correctKeystrokes_;
    w.typed.push_back(c);
}

void Game::commitWord() {
    Word& w = words_[current_];
    typedChars_ += static_cast<int>(w.typed.size()) + 1;
    if (w.typed == w.target)
        correctChars_ += static_cast<int>(w.target.size()) + 1;
    ++current_;
    ensureWords(current_ + 90);
}

void Game::backspace(bool wholeWord) {
    Word& w = words_[current_];
    if (wholeWord)
        w.typed.clear();
    else if (!w.typed.empty())
        w.typed.pop_back();
}

void Game::finishPending() {
    const Word& w = words_[current_];
    typedChars_ += static_cast<int>(w.typed.size());
    if (!w.typed.empty() && w.typed == w.target)
        correctChars_ += static_cast<int>(w.target.size());
}
