/**
 * \file queue.h
 * \brief Thread-safe queue implementation for media packets and frames
 */

#pragma once
#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>

struct AVPacket;
struct AVFrame;

/**
 * \class Queue
 * \brief Thread-safe bounded queue for producer-consumer pattern
 * 
 * This template class implements a thread-safe queue with a maximum size,
 * suitable for passing data between producer and consumer threads in a
 * media pipeline.
 * 
 * \tparam T The type of data stored in the queue
 */
template <class T>
class Queue {
protected:
	std::queue<T> queue_;                          ///< Underlying queue data structure
	typename std::queue<T>::size_type size_max_;   ///< Maximum queue size

	std::mutex mutex_;                             ///< Mutex for thread synchronization
	std::condition_variable full_;                 ///< Condition variable for full queue
	std::condition_variable empty_;                ///< Condition variable for empty queue

	std::atomic_bool quit_{false};                 ///< Flag to signal immediate shutdown
	std::atomic_bool finished_{false};             ///< Flag to signal no more input

public:
	/**
	 * \brief Constructs a queue with the specified maximum size
	 * \param size_max Maximum number of elements the queue can hold
	 */
	Queue(const size_t size_max);

	/**
	 * \brief Pushes data onto the queue (blocks if full)
	 * \param data Data to push (moved into the queue)
	 * \return true if data was pushed, false if queue is quit or finished
	 */
	bool push(T &&data);

	/**
	 * \brief Pops data from the queue (blocks if empty)
	 * \param data Reference to receive the popped data
	 * \return true if data was popped, false if queue is quit or finished and empty
	 */
	bool pop(T &data);

	/**
	 * \brief Signals that no more data will be pushed to the queue
	 * 
	 * After calling this, consumers will continue to pop remaining items
	 * until the queue is empty.
	 */
	void finished();
	
	/**
	 * \brief Immediately stops all queue operations
	 * 
	 * All waiting push() and pop() calls will return false.
	 */
	void quit();

};

using PacketQueue =
	Queue<std::unique_ptr<AVPacket, std::function<void(AVPacket*)>>>;
using FrameQueue =
	Queue<std::unique_ptr<AVFrame, std::function<void(AVFrame*)>>>;

template <class T>
Queue<T>::Queue(size_t size_max) :
		size_max_{size_max} {
}

template <class T>
bool Queue<T>::push(T &&data) {
	std::unique_lock<std::mutex> lock(mutex_);

	while (!quit_ && !finished_) {

		if (queue_.size() < size_max_) {
			queue_.push(std::move(data));

			empty_.notify_all();
			return true;
		} else {
			full_.wait(lock);
		}
	}

	return false;
}

template <class T>
bool Queue<T>::pop(T &data) {
	std::unique_lock<std::mutex> lock(mutex_);

	while (!quit_) {

		if (!queue_.empty()) {
			data = std::move(queue_.front());
			queue_.pop();

			full_.notify_all();
			return true;
		} else if (queue_.empty() && finished_) {
			return false;
		} else {
			empty_.wait(lock);
		}
	}

	return false;
}

template <class T>
void Queue<T>::finished() {
	finished_ = true;
	empty_.notify_all();
}

template <class T>
void Queue<T>::quit() {
	quit_ = true;
	empty_.notify_all();
	full_.notify_all();
}
