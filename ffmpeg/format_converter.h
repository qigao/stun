/**
 * \file format_converter.h
 * \brief Video format conversion using FFmpeg's libswscale
 */

#pragma once
extern "C" {
	#include "libavformat/avformat.h"
	#include "libswscale/swscale.h"
}

/**
 * \class FormatConverter
 * \brief Converts video frames between different pixel formats
 * 
 * This class wraps FFmpeg's libswscale to convert video frames from one
 * pixel format to another (e.g., YUV420P to RGB24).
 */
class FormatConverter {
public:
	/**
	 * \brief Constructs a format converter with the specified parameters
	 * \param width Frame width in pixels
	 * \param height Frame height in pixels
	 * \param input_pixel_format Source pixel format (e.g., AV_PIX_FMT_YUV420P)
	 * \param output_pixel_format Destination pixel format (e.g., AV_PIX_FMT_RGB24)
	 */
	FormatConverter(
		size_t width, size_t height,
		AVPixelFormat input_pixel_format, AVPixelFormat output_pixel_format);
	
	/**
	 * \brief Converts a frame from source to destination format
	 * \param src Source frame to convert
	 * \param dst Destination frame to write converted data to
	 */
	void operator()(AVFrame* src, AVFrame* dst);
	
private:
	size_t width_;                       ///< Frame width
	size_t height_;                      ///< Frame height
	SwsContext* conversion_context_{};   ///< FFmpeg scaling/conversion context
};
