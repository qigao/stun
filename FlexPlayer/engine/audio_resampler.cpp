#include "audio_resampler.h"
#include "ffmpeg.h"
#include <stdexcept>

AudioResampler::AudioResampler(
	int src_sample_rate, AVSampleFormat src_format, int src_channels,
	int dst_sample_rate, AVSampleFormat dst_format, int dst_channels)
	: dst_channels_(dst_channels) {
	
	AVChannelLayout src_ch_layout, dst_ch_layout;
	av_channel_layout_default(&src_ch_layout, src_channels);
	av_channel_layout_default(&dst_ch_layout, dst_channels);
	
	ffmpeg::check(swr_alloc_set_opts2(
		&swr_context_,
		&dst_ch_layout, dst_format, dst_sample_rate,
		&src_ch_layout, src_format, src_sample_rate,
		0, nullptr));
	
	ffmpeg::check(swr_init(swr_context_));
}

AudioResampler::~AudioResampler() {
	swr_free(&swr_context_);
}

int AudioResampler::resample(AVFrame* src, uint8_t** dst, int dst_samples) {
	return swr_convert(swr_context_, dst, dst_samples,
		const_cast<const uint8_t**>(src->data), src->nb_samples);
}
