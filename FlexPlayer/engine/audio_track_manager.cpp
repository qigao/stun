#include "audio_track_manager.h"
#include <iostream>
#include <cstring>

AudioTrackManager::AudioTrackManager() {}

AudioTrackManager::~AudioTrackManager() {}

std::vector<AudioTrack> AudioTrackManager::get_audio_tracks(void* format_ctx_ptr) {
	std::vector<AudioTrack> tracks;
	
	AVFormatContext* format_ctx = static_cast<AVFormatContext*>(format_ctx_ptr);
	if (!format_ctx) {
		return tracks;
	}
	
	for (unsigned i = 0; i < format_ctx->nb_streams; i++) {
		AVStream* stream = format_ctx->streams[i];
		if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
			AudioTrack track;
			track.index = i;
			track.is_external = false;
			
			// Get language
			AVDictionaryEntry* lang = av_dict_get(stream->metadata, "language", nullptr, 0);
			if (lang) {
				track.language = lang->value;
			}
			
			// Get title
			AVDictionaryEntry* title = av_dict_get(stream->metadata, "title", nullptr, 0);
			if (title) {
				track.title = title->value;
			}
			
			// Get codec name
			const AVCodecDescriptor* desc = avcodec_descriptor_get(stream->codecpar->codec_id);
			if (desc) {
				track.codec_name = desc->name;
			}
			
			// Get audio properties
			track.channels = stream->codecpar->ch_layout.nb_channels;
			track.sample_rate = stream->codecpar->sample_rate;
			track.bitrate = stream->codecpar->bit_rate;
			
			// Check if default
			track.is_default = (stream->disposition & AV_DISPOSITION_DEFAULT) != 0;
			
			tracks.push_back(track);
		}
	}
	
	return tracks;
}

int AudioTrackManager::add_external_audio(const std::string& audio_file) {
	// Open external audio file
	AVFormatContext* format_ctx = nullptr;
	if (avformat_open_input(&format_ctx, audio_file.c_str(), nullptr, nullptr) < 0) {
		std::cerr << "Failed to open external audio file: " << audio_file << "\n";
		return -1;
	}
	
	if (avformat_find_stream_info(format_ctx, nullptr) < 0) {
		avformat_close_input(&format_ctx);
		std::cerr << "Failed to find stream info in external audio file\n";
		return -1;
	}
	
	// Find audio stream
	int audio_stream_index = -1;
	for (unsigned i = 0; i < format_ctx->nb_streams; i++) {
		if (format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
			audio_stream_index = i;
			break;
		}
	}
	
	if (audio_stream_index < 0) {
		avformat_close_input(&format_ctx);
		std::cerr << "No audio stream found in external file\n";
		return -1;
	}
	
	// Create track info
	AVStream* stream = format_ctx->streams[audio_stream_index];
	AudioTrack track;
	track.index = static_cast<int>(internal_tracks_.size() + external_tracks_.size());
	track.is_external = true;
	track.source_file = audio_file;
	track.title = "External: " + audio_file.substr(audio_file.find_last_of("/\\") + 1);
	
	const AVCodecDescriptor* desc = avcodec_descriptor_get(stream->codecpar->codec_id);
	if (desc) {
		track.codec_name = desc->name;
	}
	
	track.channels = stream->codecpar->ch_layout.nb_channels;
	track.sample_rate = stream->codecpar->sample_rate;
	track.bitrate = stream->codecpar->bit_rate;
	track.is_default = false;
	
	external_tracks_.push_back(track);
	avformat_close_input(&format_ctx);
	
	std::cout << "Added external audio track: " << track.title << "\n";
	return track.index;
}

void AudioTrackManager::remove_external_audio(int track_index) {
	for (auto it = external_tracks_.begin(); it != external_tracks_.end(); ++it) {
		if (it->index == track_index) {
			std::cout << "Removed external audio track: " << it->title << "\n";
			external_tracks_.erase(it);
			return;
		}
	}
}

std::vector<AudioTrack> AudioTrackManager::get_all_tracks() const {
	std::vector<AudioTrack> all_tracks = internal_tracks_;
	all_tracks.insert(all_tracks.end(), external_tracks_.begin(), external_tracks_.end());
	return all_tracks;
}

void AudioTrackManager::set_active_track(int track_index) {
	active_track_index_ = track_index;
	std::cout << "Active audio track set to: " << track_index << "\n";
}

void AudioTrackManager::set_channel_mode(AudioChannelMode mode) {
	channel_mode_ = mode;
	std::cout << "Audio channel mode set to: " << get_channel_mode_name(mode) << "\n";
}

std::string AudioTrackManager::get_channel_mode_name(AudioChannelMode mode) {
	switch (mode) {
		case AudioChannelMode::STEREO: return "Stereo";
		case AudioChannelMode::MONO: return "Mono";
		case AudioChannelMode::SURROUND_5_1: return "5.1 Surround";
		case AudioChannelMode::SURROUND_7_1: return "7.1 Surround";
		case AudioChannelMode::LEFT_ONLY: return "Left Channel Only";
		case AudioChannelMode::RIGHT_ONLY: return "Right Channel Only";
		case AudioChannelMode::SWAP_CHANNELS: return "Swap L/R";
		case AudioChannelMode::ORIGINAL: return "Original";
		default: return "Unknown";
	}
}

bool AudioTrackManager::is_external_track(int track_index) const {
	for (const auto& track : external_tracks_) {
		if (track.index == track_index) {
			return true;
		}
	}
	return false;
}

// AudioChannelProcessor implementation

void AudioChannelProcessor::process(const float* input, float* output, 
                                    int num_samples, int input_channels, 
                                    int output_channels, AudioChannelMode mode) {
	switch (mode) {
		case AudioChannelMode::MONO:
			downmix_to_mono(input, output, num_samples, input_channels);
			break;
			
		case AudioChannelMode::LEFT_ONLY:
			extract_left(input, output, num_samples, input_channels);
			break;
			
		case AudioChannelMode::RIGHT_ONLY:
			extract_right(input, output, num_samples, input_channels);
			break;
			
		case AudioChannelMode::SWAP_CHANNELS:
			if (input_channels >= 2) {
				swap_channels(input, output, num_samples);
			} else {
				std::memcpy(output, input, num_samples * input_channels * sizeof(float));
			}
			break;
			
		case AudioChannelMode::ORIGINAL:
		case AudioChannelMode::STEREO:
		case AudioChannelMode::SURROUND_5_1:
		case AudioChannelMode::SURROUND_7_1:
		default:
			// Copy as-is for now (surround processing would need more complex logic)
			std::memcpy(output, input, num_samples * input_channels * sizeof(float));
			break;
	}
}

void AudioChannelProcessor::downmix_to_mono(const float* input, float* output, 
                                            int num_samples, int input_channels) {
	for (int i = 0; i < num_samples; i++) {
		float sum = 0.0f;
		for (int ch = 0; ch < input_channels; ch++) {
			sum += input[i * input_channels + ch];
		}
		output[i] = sum / input_channels;
	}
}

void AudioChannelProcessor::extract_left(const float* input, float* output, 
                                        int num_samples, int input_channels) {
	for (int i = 0; i < num_samples; i++) {
		output[i] = input[i * input_channels];  // Left channel is first
	}
}

void AudioChannelProcessor::extract_right(const float* input, float* output, 
                                         int num_samples, int input_channels) {
	if (input_channels < 2) {
		// If mono, just copy
		std::memcpy(output, input, num_samples * sizeof(float));
		return;
	}
	
	for (int i = 0; i < num_samples; i++) {
		output[i] = input[i * input_channels + 1];  // Right channel is second
	}
}

void AudioChannelProcessor::swap_channels(const float* input, float* output, 
                                         int num_samples) {
	for (int i = 0; i < num_samples; i++) {
		output[i * 2 + 0] = input[i * 2 + 1];  // Right to left
		output[i * 2 + 1] = input[i * 2 + 0];  // Left to right
	}
}
