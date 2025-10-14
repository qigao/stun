/**
 * \file ffmpeg.h
 * \brief FFmpeg error handling utilities
 */

#pragma once
#include <array>
#include <stdexcept>
extern "C" {
	#include "libavutil/avutil.h"
}

/**
 * \namespace ffmpeg
 * \brief Namespace for FFmpeg utility functions and error handling
 */
namespace ffmpeg {

/**
 * \class Error
 * \brief Exception class for FFmpeg errors
 * 
 * This class extends std::runtime_error to provide FFmpeg-specific
 * error information.
 */
class Error : public std::runtime_error {
public:
	/**
	 * \brief Constructs an error with a custom message
	 * \param message Error message string
	 */
	Error(const std::string &message);
	
	/**
	 * \brief Constructs an error from an FFmpeg error code
	 * \param status FFmpeg error code (negative value)
	 */
	Error(int status);
};

/**
 * \brief Converts an FFmpeg error code to a human-readable string
 * \param error_code FFmpeg error code
 * \return Error description string
 */
std::string error_string(const int error_code);

/**
 * \brief Checks an FFmpeg return status and throws on error
 * \param status FFmpeg function return value
 * \return The status value if non-negative
 * \throws ffmpeg::Error if status is negative
 */
inline int check(const int status) {
	if (status < 0) {
		throw ffmpeg::Error{status};
	}
	return status;
}

} // namespace ffmpeg
