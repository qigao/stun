#include "audio_decoder.h"
#include "ffmpeg.h"
#include <stdexcept>

AudioDecoder::AudioDecoder(AVCodecParameters* codec_parameters) {
	const AVCodec* codec = avcodec_find_decoder(codec_parameters->codec_id);
	if (!codec) {
		throw std::runtime_error("Unsupported audio codec");
	}

	codec_context_ = avcodec_alloc_context3(codec);
	if (!codec_context_) {
		throw std::runtime_error("Could not allocate audio codec context");
	}

	ffmpeg::check(avcodec_parameters_to_context(codec_context_, codec_parameters));
	ffmpeg::check(avcodec_open2(codec_context_, codec, nullptr));
	
	time_base_ = codec_context_->time_base;
}

AudioDecoder::~AudioDecoder() {
	avcodec_free_context(&codec_context_);
}

bool AudioDecoder::send(AVPacket* packet) {
	int ret = avcodec_send_packet(codec_context_, packet);
	if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
		return false;
	}
	ffmpeg::check(ret);
	return true;
}

bool AudioDecoder::receive(AVFrame* frame) {
	int ret = avcodec_receive_frame(codec_context_, frame);
	if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
		return false;
	}
	ffmpeg::check(ret);
	return true;
}

int AudioDecoder::sample_rate() const {
	return codec_context_->sample_rate;
}

int AudioDecoder::channels() const {
	return codec_context_->ch_layout.nb_channels;
}

AVSampleFormat AudioDecoder::sample_format() const {
	return codec_context_->sample_fmt;
}

AVRational AudioDecoder::time_base() const {
	return time_base_;
}
