/**
 * \file timer.h
 * \brief Precise timing control using PID controller for video synchronization
 */

#pragma once
#include <cstdint>
#include <chrono>

/**
 * \class Timer
 * \brief High-precision timer with PID control for video frame synchronization
 * 
 * This class implements a PID (Proportional-Integral-Derivative) controller
 * to maintain precise timing for video frame presentation, compensating for
 * timing drift and system jitter.
 */
class Timer {
private:
	std::chrono::time_point<std::chrono::high_resolution_clock> target_time_;  ///< Target time for next frame

	int64_t proportional_{};  ///< Proportional term of PID controller
	int64_t integral_{};      ///< Integral term of PID controller
	int64_t derivative_{};    ///< Derivative term of PID controller

	constexpr static double P_{0.0};   ///< Proportional gain coefficient
	constexpr static double I_{-1.0};  ///< Integral gain coefficient
	constexpr static double D_{0.0};   ///< Derivative gain coefficient
	
	double speed_{1.0};  ///< Playback speed multiplier

public:
	/**
	 * \brief Constructs a timer and initializes the target time
	 */
	Timer();
	
	/**
	 * \brief Waits until the target time, then advances by the specified period
	 * \param period Time period to wait in nanoseconds
	 */
	void wait(int64_t period);
	
	/**
	 * \brief Updates the timer to the current time (for resynchronization)
	 */
	void update();
	
	/**
	 * \brief Sets the playback speed multiplier
	 * \param speed Speed multiplier (0.5 = half speed, 1.0 = normal, 2.0 = double speed)
	 */
	void set_speed(double speed);
	
	/**
	 * \brief Gets the current playback speed
	 * \return Speed multiplier
	 */
	double get_speed() const { return speed_; }

private:
	/**
	 * \brief Calculates the PID adjustment value
	 * \return Adjustment value in nanoseconds
	 */
	int64_t adjust() const;
};
