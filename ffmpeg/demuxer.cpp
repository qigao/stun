#include "demuxer.h"
#include "ffmpeg.h"

Demuxer::Demuxer(const std::string &file_name) {
	ffmpeg::check(avformat_open_input(
		&format_context_, file_name.c_str(), nullptr, nullptr));
	ffmpeg::check(avformat_find_stream_info(
		format_context_, nullptr));
	video_stream_index_ = ffmpeg::check(av_find_best_stream(
		format_context_, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0));
	
	// Try to find audio stream (optional)
	audio_stream_index_ = av_find_best_stream(
		format_context_, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
}

Demuxer::~Demuxer() {
	avformat_close_input(&format_context_);
}

AVCodecParameters* Demuxer::video_codec_parameters() {
	return format_context_->streams[video_stream_index_]->codecpar;
}

AVCodecParameters* Demuxer::audio_codec_parameters() {
	if (audio_stream_index_ < 0) {
		return nullptr;
	}
	return format_context_->streams[audio_stream_index_]->codecpar;
}

int Demuxer::video_stream_index() const {
	return video_stream_index_;
}

int Demuxer::audio_stream_index() const {
	return audio_stream_index_;
}

AVRational Demuxer::video_time_base() const {
	return format_context_->streams[video_stream_index_]->time_base;
}

AVRational Demuxer::audio_time_base() const {
	if (audio_stream_index_ < 0) {
		return {0, 1};
	}
	return format_context_->streams[audio_stream_index_]->time_base;
}

AVRational Demuxer::time_base() const {
	return video_time_base();
}

bool Demuxer::has_audio() const {
	return audio_stream_index_ >= 0;
}

bool Demuxer::operator()(AVPacket &packet) {
	return av_read_frame(format_context_, &packet) >= 0;
}
