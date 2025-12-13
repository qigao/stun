#include "timer.h"
#include <algorithm>
#include <thread>

Timer::Timer() :
	target_time_{std::chrono::high_resolution_clock::now()} {
}

void Timer::wait(const int64_t period) {
	// Apply speed multiplier to period
	int64_t adjusted_period = static_cast<int64_t>(period / speed_);
	target_time_ += std::chrono::microseconds{adjusted_period};

	const auto lag =
		std::chrono::duration_cast<std::chrono::microseconds>(
			target_time_ - std::chrono::high_resolution_clock::now()) +
			std::chrono::microseconds{adjust()};

	std::this_thread::sleep_for(lag);

	const int64_t error =
		std::chrono::duration_cast<std::chrono::microseconds>(
			std::chrono::high_resolution_clock::now() - target_time_).count();
	derivative_ = error - proportional_;
	integral_ += error;
	proportional_ = error;

}

void Timer::update() {
	target_time_ = std::chrono::high_resolution_clock::now();
}

int64_t Timer::adjust() const {
	return P_ * proportional_ + I_ * integral_ + D_ * derivative_;
}

void Timer::set_speed(double speed) {
	// Clamp to reasonable range
	if (speed < 0.25) speed = 0.25;
	if (speed > 4.0) speed = 4.0;
	speed_ = speed;
}
