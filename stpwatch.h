#pragma once
///////////////////////////////////////////////////////////////////////////////
//	StpWatch.h
//
//	A StopWatch class and a function Delay (number of seconds)
//
//	Use class StopWatch exactly as you would a stopwatch.
//	start..stop..time..reset..start..time..time..reset..start..time..stop..
//	start..stop..start..stop..time..reset..time..start..start..stop..time..
//	You get the idea.
//
//	Useful as a profiler for programs, as long as you don't need too much
//	accuracy.  If you need more accuracy, see "Zen of Code Optimization"
//	for a non-portable timer in assembly that has a very high accuracy.
//
//	ABOUT THIS FILE: Please send any questions, comments, suggestions, bug
//		reports, bug fixes, and useful modifications to joallen@trentu.ca.
//		Released to the public domain.
//		For more information visit www.trentu.ca/~joallen.
//
///////////////////////////////////////////////////////////////////////////////
#include <chrono>
#include <map>
#include <string_view>
#include <utility> // std::exchange

namespace pgn2pgc::support {
    class StopWatch {
      public:
        using Clock    = std::chrono::high_resolution_clock;
        using Duration = Clock::duration;

        void start() {
            if (!std::exchange(active_, true))
                start_ = Clock::now();
        }

        void stop() {
            if (std::exchange(active_, false)) {
                auto elapsed = Clock::now() - start_;
                cumTime_    += elapsed;
            }
        }

        void reset() { cumTime_ = {}; }

        Duration time() { return cumTime_; }

        template <typename F,                             //
                  typename R   = std::invoke_result_t<F>, //
                  bool is_void = std::is_void_v<std::decay_t<R>>>
            requires(std::invocable<F> && not std::is_reference_v<R>)
        auto timed(F action) try {
            start();
            if constexpr (is_void) {
                action();
                stop();
            } else {
                auto r = action();
                stop();
                return std::move(r);
            }
        } catch (...) {
            stop();
            throw;
        }

      private:
        bool              active_  = false;                   // are we between start and stop?
        Clock::time_point start_   = {};                      // time when we hit the start button
        Duration          cumTime_ = std::chrono::seconds(0); // the cummulative time
    };

    extern std::map<std::string_view, StopWatch> gTimers;
} // namespace pgn2pgc::support

#define TIMED(action) ::pgn2pgc::support::gTimers[#action].timed([&] -> decltype(auto) { return action; })
