//
// Created by Michel Lesoinne on 7/17/22.
//

#pragma once
#include <coroutine>
#include <loop_control.h>
#include <task.h>
#include <variant>

namespace async {

namespace details {

template <typename CAwaiter>
struct promise;

/** \brief Simple lazy, self-destructing on completion coroutine type
 * with listener.
 * \details The promise contains a pointer to a listener to allow
 * communication between the coroutine and a coroutine-composing code
 * such as either.
 * @tparam CAwaiter
 */
template <typename CAwaiter>
struct coroutine : std::coroutine_handle<promise<CAwaiter>> {
	using promise_type = struct promise<CAwaiter>;
};

template <typename CAwaiter>
struct promise {

	coroutine<CAwaiter> get_return_object()
	{
		return {coroutines::coroutine_handle<promise>::from_promise(*this)};
	}
	std::suspend_always initial_suspend() noexcept { return {}; }
	/// Causes destruction on ending.
	std::suspend_never final_suspend() noexcept { return {}; }
	void                return_void() {}
	void                unhandled_exception() {}

	void set_listener(CAwaiter *ca) { awaiter = ca; }

	CAwaiter *awaiter;
};

template <typename CAwaiter>
struct DestAwaitable {
	bool                    await_ready() { return false; }
	std::coroutine_handle<> await_suspend(std::coroutine_handle<promise<CAwaiter>> h)
	{
		auto &p = h.promise();
		compose_awaiter = p.awaiter;
		return h;
	}
	auto     &await_resume() { return *compose_awaiter; }
	CAwaiter *compose_awaiter;
};

} // namespace details
template <typename T>
concept Awaitable = requires(T t) { [](T &x) -> task<> { co_await x; }(t); };

template <typename T>
concept Awaiter = requires(T t) {
	                  {
		                  t.await_ready()
		                  } -> std::convertible_to<bool>;
	                  {
		                  t.await_suspend(std::coroutine_handle<>{})
	                  };
	                  {
		                  t.await_resume()
	                  };
                  };

template <typename CAwaiter, size_t i>
details::coroutine<CAwaiter>
one_coroutine(auto a)
{
	auto      result = co_await a;
	CAwaiter &dest = co_await details::DestAwaitable<CAwaiter>{};
	dest.result.template emplace<i>(std::move(result));
	dest.done(i);
}

template <typename CAwaiter, typename... A, std::size_t... Is>
auto
all_coroutines(std::index_sequence<Is...>, A... a)
{
	return std::array<details::coroutine<CAwaiter>, sizeof...(a)>{
	    one_coroutine<CAwaiter, Is>(std::move(a))...};
}

/** \brief Simultaneously co_await a set of awaiters and return the result of the first one
 * to complete.
 * @tparam A
 * @param a Set of awaiters to co_await.
 * @return A variant of all the return types from the awaiters with
 * the value from the first awaiter to complete.
 */
template <Awaiter... A>
auto
either(A... a)
{
	using namespace std;

	struct awaitable {
		awaitable(A... a)
		    : runners{all_coroutines<awaitable>(make_index_sequence<sizeof...(a)>{}, move(a)...)},
		      result{dummy{}}
		{
		}
		/* Return false to go through the process of await_suspend.
		 * TODO: Change to start each coroutine to its first suspension point (allows for shortcut).
		 */
		bool await_ready() { return false; }
		void await_suspend(coroutine_handle<> h)
		{
			waiting = h;
			// Schedule all the coroutines.
			for (int i = 0; auto cr : runners) {
				cr.promise().set_listener(this);
				core_loop().schedule(cr, 0);
			}
		}
		auto await_resume() { return move(result); }
		void done(int i)
		{
			core_loop().schedule(waiting, 0);
			// Destroy all the coroutines except the one calling done.
			// That one will self-destruct on completion.
			for (int j = 0; j < sizeof...(a); ++j)
				if (j != i) {
					core_loop().remove(runners[j]);
					runners[j].destroy();
				}
		}
		array<details::coroutine<awaitable>, sizeof...(a)> runners;
		struct dummy {};
		variant<decltype(a.await_resume())..., dummy> result;
		coroutine_handle<>                            waiting;
	};

	return awaitable(forward<A>(a)...);
}

} // namespace async