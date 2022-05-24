//
// Created by Michel Lesoinne on 5/23/22.
//

#pragma once
#include <algorithm>
#include <array>
#include <coroutine>
#include <cstdint>
#include <hardware/timer.h>
#include <utility>

template <typename T>
concept Task = requires(T task) {
                 { task.get_handle() };
               };

class loop_control {
  constexpr static const auto cmp = [](const auto &a, const auto &b) {
    return a.first > b.first;
  };
public:
  loop_control() { scheduled.reserve(16); }

  void schedule(std::coroutine_handle<> handle, uint64_t when) {
    scheduled.push_back({when, handle});
    std::push_heap(scheduled.begin(), scheduled.end(), cmp);
  }

  template <Task ... T>
  void loop(T ... tasks) {
    start(tasks...);
    while(!scheduled.empty()) {
      auto min_time = scheduled.front();
      if (min_time.first < time_us_64()) {
        std::pop_heap(scheduled.begin(), scheduled.end(), cmp);
        scheduled.pop_back();
        min_time.second.resume();
      }
    }
  }
private:
  bool run = true;
  std::vector<std::pair<uint64_t,std::coroutine_handle<>>> scheduled;

  template <typename T0, typename ...R>
  void start(T0 &t0, R& ...r) {
    t0.get_handle().resume();
    if constexpr (sizeof...(r) > 0) {
      start(r...);
    }
  }
};

inline loop_control loop_control;