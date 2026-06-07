#pragma once

#include "csv_parser.hpp"

#include "../../../core-cpp/include/matching_engine/matching_engine.hpp"

#include <chrono>
#include <thread>
#include <iostream>

namespace hft {

class ReplayEngine {

private:

    MatchingEngine& engine;

public:

    explicit ReplayEngine(
        MatchingEngine& e)
        : engine(e) {}

    void replay(
        const std::vector<ReplayEvent>& events,
        double speed = 1.0)
    {
        if(events.empty())
            return;

        uint64_t previous =
            events.front().timestamp_ns;

        for(const auto& event : events)
        {
            uint64_t gap =
                event.timestamp_ns
                - previous;

            if(speed > 0.0)
            {
                uint64_t sleep_ns =
                    static_cast<uint64_t>(
                        gap / speed);

                std::this_thread::sleep_for(
                    std::chrono::nanoseconds(
                        sleep_ns));
            }

            process(event);

            previous =
                event.timestamp_ns;
        }
    }

private:

    void process(
        const ReplayEvent& event)
    {
        switch(event.type)
        {
            case ReplayEventType::NEW_ORDER:
            {
                auto result =
                    engine.match_order(
                        event.order);

                for(const auto& trade :
                    result.trades)
                {
                    std::cout
                        << "TRADE "
                        << trade.trade_id
                        << " "
                        << trade.quantity
                        << "@"
                        << trade.price
                        << "\n";
                }

                break;
            }

            case ReplayEventType::CANCEL_ORDER:
                break;
        }
    }
};

} // namespace hft