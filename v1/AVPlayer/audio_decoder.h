/**
 * \file audio_decoder.h
 * \brief Audio decoding functionality using FFmpeg's libavcodec
 */

#pragma once
extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libswresample/swresample.h>
}

/**
 * \class AudioDecoder
 * \brief Decodes audio packets into raw audio frames
 * 
 * This class wraps FFmpeg's audio decoding functionality, providing a simple
 * interface for decoding compressed audio packets into raw audio frames.
 */
class AudioDecoder {
public:
	/**
	 * \brief Constructs an audio decoder with the specified codec parameters
	 * \param codec_parameters The codec parameters from the demuxer
	 */
	AudioDecoder(AVCodecParameters* codec_parameters);
	
	/**
	 * \brief Destructor - cleans up codec context resources
	 */
	~AudioDecoder();
	
	/**
	 * \brief Sends a compressed audio packet to the decoder
	 * \param packet The audio packet to decode
	 * \return true if the packet was successfully sent, false otherwise
	 */
	bool send(AVPacket* packet);
	
	/**
	 * \brief Receives a decoded audio frame from the decoder
	 * \param frame The frame to receive decoded audio data into
	 * \return true if a frame was successfully received, false otherwise
	 */
	bool receive(AVFrame* frame);
	
	/**
	 * \brief Gets the sample rate of the decoded audio
	 * \return Sample rate in Hz
	 */
	int sample_rate() const;
	
	/**
	 * \brief Gets the number of audio channels
	 * \return Number of channels (e.g., 1 for mono, 2 for stereo)
	 */
	int channels() const;
	
	/**
	 * \brief Gets the sample format of the decoded audio
	 * \return The AVSampleFormat enum value
	 */
	AVSampleFormat sample_format() const;
	
	/**
	 * \brief Gets the time base for timestamp calculations
	 * \return The time base as an AVRational
	 */
	AVRational time_base() const;

private:
	AVCodecContext* codec_context_{}; ///< FFmpeg codec context for decoding
	AVRational time_base_{};          ///< Time base for timestamp calculations
};
