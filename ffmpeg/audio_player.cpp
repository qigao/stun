#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include "audio_player.h"
#include <cstring>
#include <stdexcept>

AudioPlayer::AudioPlayer(int sample_rate, int channels) 
	: device_(new ma_device()), channels_(channels) {
	ma_device_config config = ma_device_config_init(ma_device_type_playback);
	config.playback.format = ma_format_f32;
	config.playback.channels = channels;
	config.sampleRate = sample_rate;
	config.dataCallback = data_callback;
	config.pUserData = this;

	if (ma_device_init(nullptr, &config, device_.get()) != MA_SUCCESS) {
		throw std::runtime_error("Failed to initialize audio device");
	}
}

AudioPlayer::~AudioPlayer() {
	stop();
	ma_device_uninit(device_.get());
}

void AudioPlayer::start() {
	if (ma_device_start(device_.get()) != MA_SUCCESS) {
		throw std::runtime_error("Failed to start audio device");
	}
	is_playing_ = true;
}

void AudioPlayer::stop() {
	is_playing_ = false;
	ma_device_stop(device_.get());
}

void AudioPlayer::push_samples(const float* samples, size_t frame_count) {
	std::lock_guard<std::mutex> lock(buffer_mutex_);
	size_t sample_count = frame_count * channels_;
	buffer_.insert(buffer_.end(), samples, samples + sample_count);
}

bool AudioPlayer::is_playing() const {
	return is_playing_;
}

size_t AudioPlayer::buffered_frames() const {
	std::lock_guard<std::mutex> lock(buffer_mutex_);
	return buffer_.size() / channels_;
}

void AudioPlayer::data_callback(ma_device* device, void* output, const void* /* input */, unsigned int frame_count) {
	AudioPlayer* player = static_cast<AudioPlayer*>(device->pUserData);
	float* output_buffer = static_cast<float*>(output);
	
	std::lock_guard<std::mutex> lock(player->buffer_mutex_);
	
	size_t requested_samples = static_cast<size_t>(frame_count * player->channels_);
	size_t available_samples = player->buffer_.size();
	size_t samples_to_copy = (requested_samples < available_samples) ? requested_samples : available_samples;
	
	if (samples_to_copy > 0) {
		std::memcpy(output_buffer, player->buffer_.data(), samples_to_copy * sizeof(float));
		player->buffer_.erase(player->buffer_.begin(), player->buffer_.begin() + samples_to_copy);
	}
	
	// Fill remaining with silence
	if (samples_to_copy < frame_count * player->channels_) {
		std::memset(output_buffer + samples_to_copy, 0, 
			(frame_count * player->channels_ - samples_to_copy) * sizeof(float));
	}
}
