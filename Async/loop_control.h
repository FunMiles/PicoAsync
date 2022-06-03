//
// Created by Michel Lesoinne on 5/23/22.
//

#pragma once
#include <algorithm>
#include <array>
#include <coroutine>
#include <cstdint>
#include <utility>

#include <hardware/sync.h>
#include <hardware/timer.h>

template <typename T>
concept Task = requires(T task) {
	               {
		               task.get_handle().resume()
	               };
               };

class loop_control {
	constexpr static const auto cmp = [](const auto &a, const auto &b) {
		return a.first > b.first;
	};

public:
	loop_control(uint num_tasks = 16) { scheduled.reserve(num_tasks); }

	/** \brief Schedule a coroutine for future restart.
	 * \details This routine should only be called on the same core and from
	 * regular execution code. Do not call this from an interrupt.
	 * @param handle
	 * @param when
	 */
	void schedule(std::coroutine_handle<> handle, uint64_t when)
	{
		scheduled.push_back({when, handle});
		std::push_heap(scheduled.begin(), scheduled.end(), cmp);
	}

	/** \brief Main loop of execution.
	 * \details Runs tasks when their scheduled execution time has been reached.
	 *
	 * @tparam T
	 * @param tasks
	 */
	template <Task... T>
	void loop(T... tasks)
	{
		start(tasks...);
		while (run) {
			auto min_time = scheduled.front();
			if (min_time.first < time_us_64()) {
				std::pop_heap(scheduled.begin(), scheduled.end(), cmp);
				scheduled.pop_back();
				min_time.second.resume();
			}
			for (auto p : constant_processors)
				if (p.hasArgument)
					(*p.with_arg)(p.arg);
				else
					(*p.without_arg)();
		}
	}

	void add_constant_processor(void (*processor)()) {
		constant_processors.push_back({false,processor,nullptr});
	}
	void add_constant_processor(void (*processor)(void *), void *arg) {
		Processor p;
		p.hasArgument = true;
		p.with_arg = processor;
		p.arg = arg;
		constant_processors.push_back(p);
	}

	void remove_constant_processor(void (*processor)()) {
		for (auto &p : constant_processors)
			if (p.without_arg == processor) {
				std::swap(p, constant_processors.back());
				constant_processors.pop_back();
			}
	}
	void remove_constant_processor(void (*processor)(void *), void *arg) {
		for (auto &p : constant_processors)
			if (p.with_arg == processor && p.arg == arg) {
				std::swap(p, constant_processors.back());
				constant_processors.pop_back();
			}
	}


private:
	bool run{true};
	/// \brief Heap of scheduled coroutines. Top of the heap is earliest time
	std::vector<std::pair<uint64_t, std::coroutine_handle<>>> scheduled;

	struct Processor {
		bool hasArgument;
		union {
		    void (*without_arg)();
			void (*with_arg)(void *);
		};
		void *arg;
	};
	/// \brief List of processors to run at each loop iteration.
	/// \details Typically IRQ generated input processors.
	std::vector<Processor> constant_processors;

	template <typename T0, typename... R>
	void start(T0 &t0, R &...r)
	{
		/// Start the first task. As soon as it co_await anything, it will re-queue itself.
		t0.get_handle().resume();
		if constexpr (sizeof...(r) > 0) {
			start(r...);
		}
	}
};

/// \brief Global loop objects, one for each core.
inline std::array<loop_control,2> core_loops;
/// \brief Core 0 loop object.
inline loop_control &loop_control = core_loops[0];
/// \brief Get the loop_control of the core executing this function.
inline auto &core_loop() {
	return core_loops[ get_core_num() ];
}