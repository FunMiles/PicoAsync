//
// Created by Michel Lesoinne on 6/8/22.
//

#pragma once

#include <atomic>
inline
void test() {
	std::atomic<int> tt;
	auto y = tt.load(std::memory_order_acquire);
	auto w = 2*y;
	tt.store(w,std::memory_order_release);
}