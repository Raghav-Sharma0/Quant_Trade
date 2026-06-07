#pragma once

namespace hft {

class ReplayController {

private:

    bool paused_;
    double speed_;

public:

    ReplayController()
        : paused_(false),
          speed_(1.0)
    {}

    void pause()
    {
        paused_ = true;
    }

    void resume()
    {
        paused_ = false;
    }

    bool paused() const
    {
        return paused_;
    }

    void set_speed(
        double speed)
    {
        speed_ = speed;
    }

    double speed() const
    {
        return speed_;
    }
};

}