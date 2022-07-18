//
// Created by Michel Lesoinne on 5/23/22.
//

#pragma once
#include <algorithm>
#include <array>
#include <coroutine>
#include <cstdint>
#include <utility>

#include <atomic>
#include <hardware/sync.h>
#include <hardware/timer.h>

#include "task.h"

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

	union ActionArg {
		void *p;
		std::coroutine_handle<> h;
		std::array<std::byte,4> a;
	};

	loop_control(uint num_tasks = 16) {
		scheduled.reserve(num_tasks);
		for (auto &fi : fromInterrupts)
			fi.reserve(64);
	}

	/** \brief Start a new task. The task can terminate by calling co_return; . */
	void start_task(task<> task, uint64_t when = 0)
	{
		schedule(task.get_handle(), when);
	}

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

	auto& get_schedule() const { return scheduled; }

	/** \brief Called by interrupts to insert actions to be taken.
	 *
	 * @param handle
	 */
	inline
	void scheduleInterruptAction(std::coroutine_handle<> handle)
	{
		auto &inactiveVector = fromInterrupts[1-activeIRQVector];
		inactiveVector.push_back({
		    .f = [](ActionArg arg) {
			    if (arg.h) arg.h.resume();
		    },
		    .arg = {.h = handle}
		});
		IRQVectorStatus[1-activeIRQVector] = IRQVectorStatus[1-activeIRQVector] + 1;
		__mem_fence_release();
	}

	/** \brief Called by interrupts to insert actions to be taken.
	 *
	 * @param handle
	 */
	inline
	void scheduleInterruptAction(void (*f)(ActionArg), ActionArg arg)
	{
		auto &inactiveVector = fromInterrupts[1-activeIRQVector];
		inactiveVector.push_back({
		    .f = f,
		    .arg = arg
		});
		IRQVectorStatus[1-activeIRQVector] = IRQVectorStatus[1-activeIRQVector] + 1;
		__mem_fence_release();
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
			if (!scheduled.empty())
			{
				auto min_time = scheduled.front();
				if (min_time.first < time_us_64()) {
					std::pop_heap(scheduled.begin(), scheduled.end(), cmp);
					scheduled.pop_back();
					if (min_time.second)
						min_time.second.resume();
					else
						std::cout << "Killed???" << std::endl;
				}
			}
			for (auto p : constant_processors)
				if (p.hasArgument)
					(*p.with_arg)(p.arg);
				else
					(*p.without_arg)();
			// Run the interrupt-added actions.
			if (IRQVectorStatus[1-activeIRQVector]) {
				activeIRQVector = 1-activeIRQVector;
				std::atomic_thread_fence(std::memory_order_release);
				auto &activeVector = fromInterrupts[activeIRQVector];
				for (auto &action : activeVector) {
					(*action.f)(action.arg);
				}
				IRQVectorStatus[activeIRQVector] = 0;
				activeVector.clear();
			}
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
				break;
			}
	}
	void remove_constant_processor(void (*processor)(void *), void *arg) {
		for (auto &p : constant_processors)
			if (p.with_arg == processor && p.arg == arg) {
				std::swap(p, constant_processors.back());
				constant_processors.pop_back();
				break;
			}
	}
    /// \brief Remove the handle from being scheduled.
	bool remove(std::coroutine_handle<> handle) {
		for (auto &p : scheduled)
			if (p.second == handle) {
				std::swap(p, scheduled.back());
				scheduled.pop_back();
				std::make_heap(scheduled.begin(), scheduled.end(), cmp);
				return true;
			}
		return false;
	}

private:
	bool run{true};
	/// \brief Heap of scheduled coroutines. Top of the heap is earliest time
	std::vector<std::pair<uint64_t, std::coroutine_handle<>>> scheduled;


	struct IRQAction {
		void (*f)(ActionArg);
		ActionArg arg;
	};

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

	int activeIRQVector = 0;
	volatile int IRQVectorStatus[2] = {0,0};
	std::vector<IRQAction> fromInterrupts[2];

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