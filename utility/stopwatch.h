#pragma once
#include <chrono>

class Stopwatch {
private:
    using TimePoint = std::chrono::steady_clock::time_point;

    TimePoint m_lasttime;

public:
    Stopwatch() {
        reset();
    }

    // 計測をリセットする
    void reset() {
        m_lasttime = std::chrono::steady_clock::now();
    }

    // ラップタイムを返す
    long long lap() {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = now - m_lasttime;

        m_lasttime = now;

        return std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    }
};