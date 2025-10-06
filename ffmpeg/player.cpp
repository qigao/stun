#include "player.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <string>
extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavutil/time.h>
	#include <libavutil/imgutils.h>
}

const size_t Player::queue_size_{5};

Player::Player(const std::string &file_name) :
	demuxer_{std::make_unique<Demuxer>(file_name)},
	video_decoder_{std::make_unique<VideoDecoder>(
		demuxer_->video_codec_parameters())},
	format_converter_{std::make_unique<FormatConverter>(
		video_decoder_->width(), video_decoder_->height(),
		video_decoder_->pixel_format(), AV_PIX_FMT_YUV420P)},
	display_{std::make_unique<ResizableDisplay>(
		video_decoder_->width(), video_decoder_->height())},
	timer_{std::make_unique<Timer>()},
	video_packet_queue_{std::make_unique<PacketQueue>(queue_size_)},
	frame_queue_{std::make_unique<FrameQueue>(queue_size_)},
	has_audio_{demuxer_->has_audio()} {
	
	if (has_audio_) {
		audio_decoder_ = std::make_unique<AudioDecoder>(
			demuxer_->audio_codec_parameters());
		audio_resampler_ = std::make_unique<AudioResampler>(
			audio_decoder_->sample_rate(), audio_decoder_->sample_format(), audio_decoder_->channels(),
			audio_decoder_->sample_rate(), AV_SAMPLE_FMT_FLT, audio_decoder_->channels());
		audio_player_ = std::make_unique<AudioPlayer>(
			audio_decoder_->sample_rate(), audio_decoder_->channels());
		audio_packet_queue_ = std::make_unique<PacketQueue>(queue_size_);
	}
}

void Player::operator()() {
	stages_.emplace_back(&Player::demultiplex, this);
	stages_.emplace_back(&Player::decode_video, this);
	
	if (has_audio_) {
		stages_.emplace_back(&Player::decode_audio, this);
		audio_player_->start();
	}
	
	video();

	if (has_audio_) {
		audio_player_->stop();
	}

	for (auto &stage : stages_) {
		stage.join();
	}

	if (exception_) {
		std::rethrow_exception(exception_);
	}
}

void Player::demultiplex() {
	try {
		for (;;) {
			// Create AVPacket
			std::unique_ptr<AVPacket, std::function<void(AVPacket*)>> packet{
				av_packet_alloc(),
				[](AVPacket* p){ av_packet_free(&p); }};
			packet->data = nullptr;

			// Read frame into AVPacket
			if (!(*demuxer_)(*packet)) {
				video_packet_queue_->finished();
				if (has_audio_) {
					audio_packet_queue_->finished();
				}
				break;
			}

			// Route packet to appropriate queue
			if (packet->stream_index == demuxer_->video_stream_index()) {
				if (!video_packet_queue_->push(std::move(packet))) {
					break;
				}
			} else if (has_audio_ && packet->stream_index == demuxer_->audio_stream_index()) {
				if (!audio_packet_queue_->push(std::move(packet))) {
					break;
				}
			}
		}
	} catch (...) {
		exception_ = std::current_exception();
		frame_queue_->quit();
		video_packet_queue_->quit();
		if (has_audio_) {
			audio_packet_queue_->quit();
		}
	}
}

void Player::decode_video() {
	try {
		const AVRational microseconds = {1, 1000000};

		for (;;) {
			// Create AVFrame and AVQueue
			std::unique_ptr<AVFrame, std::function<void(AVFrame*)>>
				frame_decoded{
					av_frame_alloc(), [](AVFrame* f){ av_frame_free(&f); }};
			std::unique_ptr<AVPacket, std::function<void(AVPacket*)>> packet{
				nullptr, [](AVPacket* p){ av_packet_unref(p); delete p; }};

			// Read packet from queue
			if (!video_packet_queue_->pop(packet)) {
				frame_queue_->finished();
				break;
			}

			// If the packet didn't send, receive more frames and try again
			bool sent = false;
			while (!sent) {
				sent = video_decoder_->send(packet.get());

				// If a whole frame has been decoded,
				// adjust time stamps and add to queue
				while (video_decoder_->receive(frame_decoded.get())) {
					frame_decoded->pts = av_rescale_q(
						frame_decoded->pkt_dts,
						demuxer_->video_time_base(),
						microseconds);

					std::unique_ptr<AVFrame, std::function<void(AVFrame*)>>
						frame_converted{
							av_frame_alloc(),
							[](AVFrame* f){ av_free(f->data[0]); }};
					if (av_frame_copy_props(frame_converted.get(),
						frame_decoded.get()) < 0) {
						throw std::runtime_error("Copying frame properties");
					}
					if (av_image_alloc(
						frame_converted->data, frame_converted->linesize,
						video_decoder_->width(), video_decoder_->height(),
						video_decoder_->pixel_format(), 1) < 0) {
						throw std::runtime_error("Allocating picture");
					}
					(*format_converter_)(
						frame_decoded.get(), frame_converted.get());

					if (!frame_queue_->push(std::move(frame_converted))) {
						break;
					}
				}
			}
		}
	} catch (...) {
		exception_ = std::current_exception();
		frame_queue_->quit();
		video_packet_queue_->quit();
	}
}

void Player::decode_audio() {
	try {
		for (;;) {
			std::unique_ptr<AVFrame, std::function<void(AVFrame*)>>
				frame_decoded{
					av_frame_alloc(), [](AVFrame* f){ av_frame_free(&f); }};
			std::unique_ptr<AVPacket, std::function<void(AVPacket*)>> packet{
				nullptr, [](AVPacket* p){ av_packet_unref(p); delete p; }};

			if (!audio_packet_queue_->pop(packet)) {
				break;
			}

			bool sent = false;
			while (!sent) {
				sent = audio_decoder_->send(packet.get());

				while (audio_decoder_->receive(frame_decoded.get())) {
					// Allocate buffer for resampled audio
					int dst_samples = frame_decoded->nb_samples;
					uint8_t* dst_data = nullptr;
					int dst_linesize = 0;
					
					av_samples_alloc(&dst_data, &dst_linesize,
						audio_decoder_->channels(), dst_samples,
						AV_SAMPLE_FMT_FLT, 0);

					int resampled = audio_resampler_->resample(
						frame_decoded.get(), &dst_data, dst_samples);

					if (resampled > 0) {
						audio_player_->push_samples(
							reinterpret_cast<float*>(dst_data), resampled);
					}

					av_freep(&dst_data);
				}
			}
		}
	} catch (...) {
		exception_ = std::current_exception();
		audio_packet_queue_->quit();
	}
}

void Player::video() {
	try {
		int64_t last_pts = 0;

		for (uint64_t frame_number = 0;; ++frame_number) {

			display_->input();

			if (display_->get_quit()) {
				break;

			} else if (display_->get_play()) {
				std::unique_ptr<AVFrame, std::function<void(AVFrame*)>> frame{
					nullptr, [](AVFrame* f){ av_frame_free(&f); }};
				if (!frame_queue_->pop(frame)) {
					break;
				}

				if (frame_number) {
					const int64_t frame_delay = frame->pts - last_pts;
					last_pts = frame->pts;
					timer_->wait(frame_delay);

				} else {
					last_pts = frame->pts;
					timer_->update();
				}

				display_->refresh(
					{frame->data[0], frame->data[1], frame->data[2]},
					{static_cast<size_t>(frame->linesize[0]),
					 static_cast<size_t>(frame->linesize[1]),
					 static_cast<size_t>(frame->linesize[2])});

			} else {
				std::chrono::milliseconds sleep(10);
				std::this_thread::sleep_for(sleep);
				timer_->update();
			}
		}

	} catch (...) {
		exception_ = std::current_exception();
	}

	frame_queue_->quit();
	video_packet_queue_->quit();
	if (has_audio_) {
		audio_packet_queue_->quit();
	}
}
