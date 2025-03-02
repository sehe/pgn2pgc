#include "stpwatch.h"
#include <iostream>

namespace pgn2pgc::support {
    std::map<std::string_view, StopWatch> gTimers;

    namespace {
        static struct AtProgramExit {
            ~AtProgramExit() {
                using namespace std::chrono_literals;
                std::cout << std::fixed << std::setprecision(4);
                for (auto& [name, timer] : gTimers)
                    // std::cout << std::setw(8) << timer.time() / 1.s << "s " << name << "\n";
                    std::cout << std::setw(6) << timer.time() / 1ms << " ms " << name << "\n";
            }
        } gAtProgramExit{};
    } // namespace
} // namespace pgn2pgc::support
