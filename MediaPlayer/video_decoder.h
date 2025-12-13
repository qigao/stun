/**
 * \file video_decoder.h
 * \brief Video decoding functionality using FFmpeg's libavcodec
 */

#pragma once
extern "C" {
	#include "libavcodec/avcodec.h"
}

/**
 * \class VideoDecoder
 * \brief Decodes video packets into raw video frames
 * 
 * This class wraps FFmpeg's video decoding functionality, providing a simple
 * interface for decoding compressed video packets into raw video frames.
 */
class VideoDecoder {
public:
	/**
	 * \brief Constructs a video decoder with the specified codec parameters
	 * \param codec_parameters The codec parameters from the demuxer
	 */
	VideoDecoder(AVCodecParameters* codec_parameters);
	
	/**
	 * \brief Destructor - cleans up codec context resources
	 */
	~VideoDecoder();
	
	/**
	 * \brief Sends a compressed video packet to the decoder
	 * \param packet The video packet to decode
	 * \return true if the packet was successfully sent, false otherwise
	 */
	bool send(AVPacket* packet);
	
	/**
	 * \brief Receives a decoded video frame from the decoder
	 * \param frame The frame to receive decoded video data into
	 * \return true if a frame was successfully received, false otherwise
	 */
	bool receive(AVFrame* frame);
	
	/**
	 * \brief Gets the width of the decoded video frames
	 * \return Frame width in pixels
	 */
	unsigned width() const;
	
	/**
	 * \brief Gets the height of the decoded video frames
	 * \return Frame height in pixels
	 */
	unsigned height() const;
	
	/**
	 * \brief Gets the pixel format of the decoded video frames
	 * \return The AVPixelFormat enum value
	 */
	AVPixelFormat pixel_format() const;
	
	/**
	 * \brief Gets the time base for timestamp calculations
	 * \return The time base as an AVRational
	 */
	AVRational time_base() const;
	
private:
	AVCodecContext* codec_context_{}; ///< FFmpeg codec context for decoding
};
