#pragma once

#include "event.hpp"

#include <fstream>
#include <sstream>
#include <vector>
#include <string>

namespace hft {

class CSVParser {
public:

    static std::vector<ReplayEvent>
    parse(const std::string& file)
    {
        std::vector<ReplayEvent> events;

        std::ifstream in(file);

        if (!in.is_open()) {
            throw std::runtime_error(
                "unable to open replay file");
        }

        std::string line;

        getline(in, line);

        while (getline(in, line)) {

            std::stringstream ss(line);

            std::string ts;
            std::string side;
            std::string price;
            std::string qty;
            std::string symbol;

            getline(ss, ts, ',');
            getline(ss, side, ',');
            getline(ss, price, ',');
            getline(ss, qty, ',');
            getline(ss, symbol, ',');

            Order order(
                std::stoull(ts),
                std::stoul(price),
                std::stoul(qty),
                side == "BUY"
                    ? OrderSide::BUY
                    : OrderSide::SELL,
                std::stoul(symbol)
            );

            ReplayEvent event;

            event.timestamp_ns =
                std::stoull(ts);

            event.type =
                ReplayEventType::NEW_ORDER;

            event.order = order;

            events.push_back(event);
        }

        return events;
    }
};

} //namespace hft