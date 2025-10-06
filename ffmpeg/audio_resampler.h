/**
 * \file audio_resampler.h
 * \brief Audio resampling and format conversion using FFmpeg's libswresample
 */

#pragma once
extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libswresample/swresample.h>
}

/**
 * \class AudioResampler
 * \brief Resamples and converts audio between different formats and sample rates
 * 
 * This class wraps FFmpeg's libswresample to convert audio between different
 * sample rates, formats, and channel configurations.
 */
class AudioResampler {
public:
	/**
	 * \brief Constructs an audio resampler with source and destination parameters
	 * \param src_sample_rate Source sample rate in Hz
	 * \param src_format Source sample format (e.g., AV_SAMPLE_FMT_S16)
	 * \param src_channels Number of source channels
	 * \param dst_sample_rate Destination sample rate in Hz
	 * \param dst_format Destination sample format
	 * \param dst_channels Number of destination channels
	 */
	AudioResampler(
		int src_sample_rate, AVSampleFormat src_format, int src_channels,
		int dst_sample_rate, AVSampleFormat dst_format, int dst_channels);
	
	/**
	 * \brief Destructor - cleans up resampler context
	 */
	~AudioResampler();
	
	/**
	 * \brief Resamples audio from source frame to destination buffer
	 * \param src Source audio frame
	 * \param dst Destination buffer to write resampled audio to
	 * \param dst_samples Maximum number of samples to write to destination
	 * \return Number of samples written to destination, or negative on error
	 */
	int resample(AVFrame* src, uint8_t** dst, int dst_samples);

private:
	SwrContext* swr_context_{};  ///< FFmpeg resampler context
	int dst_channels_{};         ///< Number of destination channels
};
