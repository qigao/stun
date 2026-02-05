#include "demuxer.h"
#include "ffmpeg.h"
#include "stream_handler.h"
#include <iostream>

Demuxer::Demuxer(const std::string &file_name) {
	// Get stream information
	StreamInfo stream_info = StreamHandler::get_stream_info(file_name);
	
	std::cout << "Opening stream: " << file_name << "\n";
	std::cout << "Protocol: " << stream_info.protocol << "\n";
	std::cout << "Type: " << (stream_info.is_live ? "Live" : "VOD") << "\n";
	
	// Set options for streaming
	AVDictionary* options = nullptr;
	
	if (stream_info.type != StreamType::LOCAL_FILE) {
		// Set timeout for network streams
		if (stream_info.timeout_ms > 0) {
			std::string timeout_str = std::to_string(stream_info.timeout_ms * 1000); // microseconds
			av_dict_set(&options, "timeout", timeout_str.c_str(), 0);
		}
		
		// Set buffer size for streaming
		av_dict_set(&options, "buffer_size", "1024000", 0);
		
		// For RTSP streams
		if (stream_info.type == StreamType::RTSP) {
			av_dict_set(&options, "rtsp_transport", "tcp", 0);  // Use TCP for reliability
			av_dict_set(&options, "stimeout", "5000000", 0);    // 5 second timeout
		}
		
		// For HTTP streams
		if (stream_info.type == StreamType::HTTP || 
		    stream_info.type == StreamType::HLS || 
		    stream_info.type == StreamType::DASH) {
			av_dict_set(&options, "reconnect", "1", 0);
			av_dict_set(&options, "reconnect_streamed", "1", 0);
			av_dict_set(&options, "reconnect_delay_max", "5", 0);
		}
	}
	
	// Open input
	int ret = avformat_open_input(&format_context_, file_name.c_str(), nullptr, &options);
	av_dict_free(&options);
	ffmpeg::check(ret);
	
	// Find stream info
	ffmpeg::check(avformat_find_stream_info(format_context_, nullptr));
	
	// Find best video stream
	video_stream_index_ = ffmpeg::check(av_find_best_stream(
		format_context_, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0));
	
	// Try to find audio stream (optional)
	audio_stream_index_ = av_find_best_stream(
		format_context_, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
	
	std::cout << "Stream opened successfully\n";
	std::cout << "Video stream: " << video_stream_index_ << "\n";
	if (audio_stream_index_ >= 0) {
		std::cout << "Audio stream: " << audio_stream_index_ << "\n";
	}
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

double Demuxer::duration() const {
	// Try stream duration first
	if (format_context_->streams[video_stream_index_]->duration != AV_NOPTS_VALUE) {
		return format_context_->streams[video_stream_index_]->duration * 
		       av_q2d(format_context_->streams[video_stream_index_]->time_base);
	}
	
	// Fall back to container duration
	if (format_context_->duration != AV_NOPTS_VALUE) {
		return format_context_->duration / static_cast<double>(AV_TIME_BASE);
	}
	
	return 0.0;
}

bool Demuxer::seek(double seconds) {
	// Convert seconds to timestamp in stream time base
	int64_t timestamp = static_cast<int64_t>(seconds / av_q2d(video_time_base()));
	
	// Seek to keyframe at or before the target timestamp
	// AVSEEK_FLAG_BACKWARD ensures we seek to a keyframe before the target
	int result = av_seek_frame(format_context_, video_stream_index_, 
	                           timestamp, AVSEEK_FLAG_BACKWARD);
	
	return result >= 0;
}
