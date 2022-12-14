// Copyright 2016 Navtech Radar Limited
// This file is part of iasdk which is released under The MIT License (MIT).
// See file LICENSE.txt in project root or go to https://opensource.org/licenses/MIT
// for full license details.
//

#include <chrono>
#include <iostream>
#include <thread>

#include "timer.h"

namespace Navtech {
    Timer::Timer() : Timer::Timer(std::chrono::milliseconds(1000)) { }

    Timer::Timer(std::chrono::milliseconds timeout) : timeout_in_milliseconds { timeout }, is_enabled { false } { }

    void Timer::set_callback(std::function<void()> fn)
    {
        std::lock_guard lock { callback_mutex };
        callback = std::move(fn);
    }

    std::chrono::milliseconds Timer::timeout() const { return timeout_in_milliseconds; }

    void Timer::timeout(const std::chrono::milliseconds& new_timeout) { timeout_in_milliseconds = new_timeout; }

    void Timer::do_work()
    {
        auto now = std::chrono::steady_clock::now().time_since_epoch();

        while (std::chrono::steady_clock::now().time_since_epoch() < now + timeout_in_milliseconds) {
            std::this_thread::sleep_for(std::chrono::microseconds(10000));

            if (std::chrono::steady_clock::now().time_since_epoch() - now > std::chrono::milliseconds(15000)) {
                now = std::chrono::steady_clock::now().time_since_epoch();
            }

            if (stop_requested) break;
        }

        std::lock_guard lock { callback_mutex };
        if (is_enabled && callback != nullptr) callback();
    }

    void Timer::enable(const bool enable)
    {
        if (is_enabled != enable) {
            is_enabled = enable;

            if (is_enabled)
                start();
            else
                stop();
        }
    }

} // namespace Navtech
