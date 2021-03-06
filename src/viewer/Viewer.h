//******************************************************************************
//* File:   Viewer.h
//* Author: Jon Newman <jpnewman snail mit dot edu>
//*
//* Copyright (c) Jon Newman (jpnewman snail mit dot edu)
//* All right reserved.
//* This file is part of the Oat project.
//* This is free software: you can redistribute it and/or modify
//* it under the terms of the GNU General Public License as published by
//* the Free Software Foundation, either version 3 of the License, or
//* (at your option) any later version.
//* This software is distributed in the hope that it will be useful,
//* but WITHOUT ANY WARRANTY; without even the implied warranty of
//* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//* GNU General Public License for more details.
//* You should have received a copy of the GNU General Public License
//* along with this source code.  If not, see <http://www.gnu.org/licenses/>.
//****************************************************************************

#ifndef OAT_VIEWER_H
#define OAT_VIEWER_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <boost/program_options.hpp>

#include "../../lib/base/Component.h"
#include "../../lib/base/Configurable.h"
#include "../../lib/shmemdf/Source.h"

namespace po = boost::program_options;

namespace oat {

template <typename T>
class Viewer : public Component, public Configurable<false> {

    using Clock = std::chrono::high_resolution_clock;

public:
    /**
     * @brief Abstract viewer.
     * All concrete viewer types implement this ABC.
     * @param source_address SOURCE node address
     */
    explicit Viewer(const std::string &source_name);
    virtual ~Viewer();

    // Implement Component interface
    oat::ComponentType type(void) const override { return oat::viewer; };
    std::string name(void) const override { return name_; }
    bool connectToNode(void) override;
    int process(void) override;

protected:
    // Viewer name
    const std::string name_;

    // Source address
    const std::string source_address_;

    // Mimumum display update period
    using Milliseconds = std::chrono::milliseconds;
    Milliseconds min_update_period_ms {33};

    /**
     * @brief Perform sample display. Override to implement display operation
     * in derived classes.
     * @param Sample to by displayed.
     */
    virtual void display(const T &sample) = 0;

private:
    // Sample SOURCE
    T sample_;
    oat::Source<T> source_;

    // Minimum viewer refresh period
    Clock::time_point tick_, tock_;

    // Display update thread
    std::atomic<bool> running_ {true};
    std::atomic<bool> display_complete_ {true};
    std::mutex display_mutex_;
    std::condition_variable display_cv_;
    std::thread display_thread_;

    /**
     * @brief Asynchronous execution of display(). This function is handled by
     * an asynchronous thread that throttles the GUI update period to
     * min_update_period_ms.
     */
    void processAsync(void);
};

}      /* namespace oat */
#endif /* OAT_VIEWER_H */
