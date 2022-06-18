//
// Created by Michel Lesoinne on 5/22/22.
//

#pragma once

#include <coroutine>
#include <memory>
#include <optional>

namespace coroutines = std;

template <typename T = void, bool lazy = true>
struct [[nodiscard]] task {
	using value_type = T;
	struct task_promise;
	using promise_type = task_promise;
	explicit task(coroutines::coroutine_handle<promise_type> handle) : my_handle(handle) {}

	task(task &&t) noexcept : my_handle(t.my_handle) { t.my_handle = nullptr; }
	/// Disable copy construction/assignment.
	task(const task &) = delete;
	task &operator=(const task &) = delete;

	~task()
	{
		// Void tasks do not need to return a value from the promise, so they can
		// self-cleanup, see the promise.
		if constexpr (!std::is_same_v<T, void>) {
			if (my_handle)
				my_handle.destroy();
		}
	}

	auto get_handle() const { return my_handle; }
	void detach() {
		my_handle = nullptr;
	}

	task &operator=(task &&other) noexcept
	{
		if (std::addressof(other) != this) {
			if (my_handle) {
				my_handle.destroy();
			}

			my_handle = other.my_handle;
			other.my_handle = nullptr;
		}

		return *this;
	}
	auto operator co_await() noexcept
	{
		struct awaiter {
			bool await_ready() const noexcept { return !coro_handle || coro_handle.done(); }

			coroutines::coroutine_handle<>
			await_suspend(coroutines::coroutine_handle<> awaiting_coroutine) noexcept
			{
				coro_handle.promise().set_continuation(awaiting_coroutine);
				return coro_handle;
			}
			T await_resume() noexcept
			{
				if constexpr (std::is_same_v<T, void>)
					return;
				else
					return std::move(coro_handle.promise().data.value());
			}

			coroutines::coroutine_handle<promise_type> coro_handle;
		};
		return awaiter{my_handle};
	}

private:
	coroutines::coroutine_handle<promise_type> my_handle;
};

/** \brief Component of the promise handling the result.
 *
 * @tparam T Result type
 *
 * \details The C++20 coroutines' promises need to expose either a return_void or
 * return_value function, whether they return an actual value or a void.
 * They cannot expose both. Many implementations specialize the full promise type.
 * Here we only specialize the return value handling.
 */
template <typename T>
struct promise_setter {
	template <typename U>
	void return_value(U &&u)
	{
		data.template emplace(std::forward<U>(u));
	}
	std::optional<T> data;
};

/** \brief Specialization of promise_setter for void returning tasks. */
template <>
struct promise_setter<void> {
	void return_void() {}
};
/** \brief Promise type associated with a task
 *
 * @tparam T the return type of the task.
 */
template <typename T, bool lazy>
struct task<T, lazy>::task_promise : public promise_setter<T> {

	/// \brief Invoked when we first enter a coroutine, build the task object with the coroutine
	/// handle.
	task get_return_object() noexcept
	{
		return task{coroutines::coroutine_handle<task_promise>::from_promise(*this)};
	};

	auto initial_suspend() const noexcept
	{
		if constexpr (lazy)
			return coroutines::suspend_always{};
		else
			return coroutines::suspend_never{};
	}

	auto final_suspend() const noexcept
	{
		struct awaiter {
			// Return false here to return control to the thread's event loop. Remember that we're
			// running on some async thread at this point.
			/// \brief The current coroutine is done, so it is not ready to restart.
			/// \details await_suspend will be called next allowing to return the handle of the
			/// continuation.
			bool await_ready() const noexcept { return false; }

			void await_resume() const noexcept {}
			/** \brief Suspension method for the ending co-routine.
			 * \details The returned handle will be resumed.  Either this co-routine is at a root of
			 * a complete task tree or it is a branch. In the first case, a noop handle is returned,
			 * otherwise the continuation handle is returned. It is possible to return noop and
			 * schedule the continuation back on the task system instead in order to implement a
			 * fair-scheduling system.
			 *
			 * @param h Handle of the current and ending co-routine.
			 * @return The handle of the coroutine that continues the work if any.
			 */
			coroutines::coroutine_handle<>
			await_suspend(coroutines::coroutine_handle<task::task_promise> h) noexcept
			{
				return h.promise().continuation;
			}
		};
		if constexpr (std::is_same_v<T,void>)
			return coroutines::suspend_never{};
		else
			return awaiter{};
	}

	void unhandled_exception() noexcept
	{
		//    std::cerr << "Unhandled exception caught...\n";
		std::terminate();
	}

	void set_continuation(coroutines::coroutine_handle<> handle) { continuation = handle; }

private:
	coroutines::coroutine_handle<> continuation = coroutines::noop_coroutine();
};