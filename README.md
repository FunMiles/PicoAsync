# Coroutine Based Asynchronous Embedded Programming

The purpose of this library is to make it easier to write concurrent
processing tasks for embedded systems.

Using the coroutine system, it becomes easy to write individual tasks
that are concurrent, sharing the processor. As long as each task 
awaits some event on a regular basis, other tasks can execute during
that yielded time.

Example of a pin toggling task:
```cpp
task<> f(int pin) {
    while (true) {
		co_await sleep(250ms);
		co_await setGPIO(pin, 1);
		co_await sleep(250ms);
		co_await setGPIO(pin, 0);	
	}
}
```