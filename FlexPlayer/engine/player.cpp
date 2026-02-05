#if defined(_WIN32)
  #include <process.h>  // Required for _beginthreadex on Windows
#endif

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

double Player::get_duration() const {
	return demuxer_->duration();
}

double Player::get_position() const {
	return current_position_.load();
}

void Player::seek(double seconds) {
	// Clamp to valid range
	double duration = get_duration();
	if (seconds < 0.0) seconds = 0.0;
	if (duration > 0.0 && seconds > duration) seconds = duration;
	
	seek_target_ = seconds;
	seeking_ = true;
}

void Player::set_speed(double speed) {
	if (timer_) {
		timer_->set_speed(speed);
	}
}

double Player::get_speed() const {
	if (timer_) {
		return timer_->get_speed();
	}
	return 1.0;
}

bool Player::is_hardware_accelerated() const {
	return use_hw_decode_ && hw_decoder_ && hw_decoder_->is_hardware_accelerated();
}

std::string Player::get_hw_type() const {
	if (hw_decoder_) {
		return hw_decoder_->get_hw_type_name();
	}
	return "software";
}

void Player::set_filter(const std::string& filter_desc) {
	pending_filter_desc_ = filter_desc;
	filter_change_requested_ = true;
	std::cout << "Filter change requested: " << (filter_desc.empty() ? "None" : filter_desc) << "\n";
}

std::string Player::get_current_filter() const {
	return current_filter_desc_;
}

std::vector<SubtitleTrack> Player::get_subtitle_tracks() const {
	if (demuxer_) {
		return SubtitleRenderer::get_subtitle_tracks(demuxer_->format_context());
	}
	return {};
}

void Player::select_subtitle_track(int track_index) {
	current_subtitle_track_ = track_index;
	if (track_index >= 0) {
		std::cout << "Subtitle track selected: " << track_index << "\n";
		// TODO: Load subtitle track from demuxer
	} else {
		std::cout << "Subtitles disabled\n";
		if (subtitle_renderer_) {
			subtitle_renderer_->set_enabled(false);
		}
	}
}

bool Player::load_external_subtitle(const std::string& subtitle_file) {
	try {
		// Get video dimensions
		int width = video_decoder_ ? video_decoder_->width() : 
		            (hw_decoder_ ? hw_decoder_->width() : 1920);
		int height = video_decoder_ ? video_decoder_->height() : 
		             (hw_decoder_ ? hw_decoder_->height() : 1080);
		
		// Create subtitle renderer if not exists
		if (!subtitle_renderer_) {
			subtitle_renderer_ = std::make_unique<SubtitleRenderer>(width, height);
		}
		
		// Load subtitle file
		if (subtitle_renderer_->load_file(subtitle_file)) {
			subtitle_renderer_->set_enabled(true);
			subtitle_renderer_->set_font_scale(subtitle_font_scale_);
			current_subtitle_track_ = -2;  // -2 indicates external subtitle
			std::cout << "External subtitle loaded: " << subtitle_file << "\n";
			return true;
		}
	} catch (const std::exception& e) {
		std::cerr << "Failed to load subtitle: " << e.what() << "\n";
	}
	return false;
}

void Player::set_subtitle_font_size(double scale) {
	// Clamp to reasonable range
	if (scale < 0.5) scale = 0.5;
	if (scale > 3.0) scale = 3.0;
	
	subtitle_font_scale_ = scale;
	
	if (subtitle_renderer_) {
		subtitle_renderer_->set_font_scale(scale);
	}
	
	std::cout << "Subtitle font size: " << (scale * 100) << "%\n";
}

std::vector<AudioTrack> Player::get_audio_tracks() const {
	if (audio_track_manager_) {
		return audio_track_manager_->get_all_tracks();
	}
	if (demuxer_) {
		return AudioTrackManager::get_audio_tracks(demuxer_->format_context());
	}
	return {};
}

void Player::select_audio_track(int track_index) {
	pending_audio_track_ = track_index;
	audio_track_change_requested_ = true;
	std::cout << "Audio track change requested: " << track_index << "\n";
}

void Player::set_audio_channel_mode(AudioChannelMode mode) {
	if (audio_track_manager_) {
		audio_track_manager_->set_channel_mode(mode);
		if (display_) {
			display_->show_osd("Audio: " + AudioTrackManager::get_channel_mode_name(mode));
		}
	}
}

AudioChannelMode Player::get_audio_channel_mode() const {
	if (audio_track_manager_) {
		return audio_track_manager_->get_channel_mode();
	}
	return AudioChannelMode::ORIGINAL;
}

bool Player::add_external_audio(const std::string& audio_file) {
	if (!audio_track_manager_) {
		audio_track_manager_ = std::make_unique<AudioTrackManager>();
	}
	
	int track_index = audio_track_manager_->add_external_audio(audio_file);
	if (track_index >= 0) {
		if (display_) {
			display_->show_osd("External audio added");
		}
		return true;
	}
	return false;
}

bool Player::capture_screenshot(CaptureFormat format, const std::string& filename) {
	std::lock_guard<std::mutex> lock(frame_mutex_);
	
	if (!current_frame_ || !current_frame_.get()) {
		std::cerr << "No frame available for capture\n";
		return false;
	}
	
	std::string output_file = filename;
	if (output_file.empty()) {
		output_file = FrameCapture::generate_filename("screenshot", format);
	}
	
	bool success = FrameCapture::capture_and_save(current_frame_.get(), output_file);
	
	if (success && display_) {
		display_->show_osd("Screenshot saved: " + output_file);
	}
	
	return success;
}

int Player::batch_extract_frames(double interval_seconds, 
                                  CaptureFormat format,
                                  const std::string& output_prefix,
                                  double start_time,
                                  double end_time) {
	if (interval_seconds <= 0) {
		std::cerr << "Invalid interval: " << interval_seconds << "\n";
		return 0;
	}
	
	double duration = get_duration();
	if (duration <= 0) {
		std::cerr << "Invalid video duration\n";
		return 0;
	}
	
	// Validate and adjust time range
	if (start_time < 0) start_time = 0;
	if (end_time < 0 || end_time > duration) end_time = duration;
	if (start_time >= end_time) {
		std::cerr << "Invalid time range\n";
		return 0;
	}
	
	// Calculate number of frames to extract
	int frame_count = static_cast<int>((end_time - start_time) / interval_seconds) + 1;
	
	std::cout << "Batch extracting " << frame_count << " frames from " 
	          << start_time << "s to " << end_time << "s (interval: " 
	          << interval_seconds << "s)\n";
	
	// Store original position
	double original_position = get_position();
	
	// Ensure playback is running (needed for frame updates)
	bool was_paused = false;
	if (display_ && !display_->get_play()) {
		was_paused = true;
		// Note: Can't directly unpause from here, user must have video playing
	}
	
	int extracted = 0;
	std::string extension = FrameCapture::get_extension(format);
	
	for (int i = 0; i < frame_count; i++) {
		double target_time = start_time + (i * interval_seconds);
		if (target_time > end_time) break;
		
		// Seek to target time
		seek(target_time);
		
		// Wait longer for seek to complete and frame to be decoded
		// The video thread needs time to process the seek and decode the frame
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
		
		// Additional wait for position to stabilize
		int retries = 0;
		while (retries < 20) {
			double current_pos = get_position();
			if (std::abs(current_pos - target_time) < 1.0) {
				// Position is close enough, wait a bit more for frame to be ready
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				break;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			retries++;
		}
		
		// Try to capture frame
		{
			std::lock_guard<std::mutex> lock(frame_mutex_);
			AVFrame* frame_ptr = current_frame_.get();
			if (frame_ptr && frame_ptr->width > 0 && frame_ptr->height > 0) {
				// Generate filename with frame number
				char filename[256];
				snprintf(filename, sizeof(filename), "%s_%04d%s", 
				         output_prefix.c_str(), i + 1, extension.c_str());
				
				if (FrameCapture::capture_and_save(frame_ptr, filename)) {
					extracted++;
					std::cout << "Extracted frame " << (i + 1) << "/" << frame_count 
					          << " at " << get_position() << "s -> " << filename << "\n";
				} else {
					std::cerr << "Failed to save frame " << (i + 1) << "\n";
				}
			} else {
				std::cerr << "Frame not ready at " << target_time 
				          << "s (current pos: " << get_position() << "s)\n";
			}
		}
	}
	
	// Restore original position
	seek(original_position);
	
	std::cout << "Batch extraction complete: " << extracted << " frames saved\n";
	
	if (display_) {
		display_->show_osd("Extracted " + std::to_string(extracted) + " frames");
	}
	
	return extracted;
}

Player::Player(const std::string &file_name, bool use_hw_decode, 
               const std::string& filter_desc,
               std::function<std::unique_ptr<VideoDisplay>(int, int)> display_factory) :
	demuxer_{std::make_unique<Demuxer>(file_name)},
	use_hw_decode_{use_hw_decode},
	timer_{std::make_unique<Timer>()},
	video_packet_queue_{std::make_unique<PacketQueue>(queue_size_)},
	frame_queue_{std::make_unique<FrameQueue>(queue_size_)},
	has_audio_{demuxer_->has_audio()},
	audio_track_manager_{std::make_unique<AudioTrackManager>()} {
	
	// Get initial audio track
	if (has_audio_) {
		current_audio_track_ = demuxer_->audio_stream_index();
	}
	
	// Initialize decoder (hardware or software)
	int width, height;
	AVPixelFormat pix_fmt;
	AVRational time_base;
	
	if (use_hw_decode_) {
		try {
			hw_decoder_ = std::make_unique<HardwareDecoder>(
				demuxer_->video_codec_parameters());
			width = hw_decoder_->width();
			height = hw_decoder_->height();
			pix_fmt = hw_decoder_->pixel_format();
			time_base = hw_decoder_->time_base();
			std::cout << "Using hardware decoder: " << hw_decoder_->get_hw_type_name() << "\n";
		} catch (const std::exception& e) {
			std::cerr << "Hardware decoder failed: " << e.what() << "\n";
			std::cerr << "Falling back to software decoder\n";
			use_hw_decode_ = false;
		}
	}
	
	if (!use_hw_decode_) {
		video_decoder_ = std::make_unique<VideoDecoder>(
			demuxer_->video_codec_parameters());
		width = video_decoder_->width();
		height = video_decoder_->height();
		pix_fmt = video_decoder_->pixel_format();
		time_base = video_decoder_->time_base();
		std::cout << "Using software decoder\n";
	}
	
	// Initialize video filter if specified
	if (!filter_desc.empty()) {
		try {
			video_filter_ = std::make_unique<VideoFilter>(
				filter_desc, width, height, pix_fmt, time_base);
			current_filter_desc_ = filter_desc;
			// Update dimensions if filter changes them
			width = video_filter_->get_output_width();
			height = video_filter_->get_output_height();
		} catch (const std::exception& e) {
			std::cerr << "Failed to initialize video filter: " << e.what() << "\n";
			video_filter_.reset();
			current_filter_desc_ = "";
		}
	}
	
	// Note: format_converter_ will be created dynamically in decode_video()
	// because hardware decode may produce different pixel formats than reported
	
	// Initialize display
    if (display_factory) {
        display_ = display_factory(width, height);
    } else {
        throw std::runtime_error("No display factory provided");
    }
	
	if (has_audio_) {
		audio_decoder_ = std::make_unique<AudioDecoder>(
			demuxer_->audio_codec_parameters());
		audio_resampler_ = std::make_unique<AudioResampler>(
			audio_decoder_->sample_rate(), audio_decoder_->sample_format(), audio_decoder_->channels(),
			audio_decoder_->sample_rate(), AV_SAMPLE_FMT_FLT, audio_decoder_->channels());
		audio_player_ = std::make_unique<AudioPlayer>(
			audio_decoder_->sample_rate(), audio_decoder_->channels());
		audio_packet_queue_ = std::make_unique<PacketQueue>(queue_size_);
		
		// Connect audio player to display for volume control
		display_->set_audio_player(audio_player_.get());
	}
	
	// Connect player to display for seeking
	display_->set_player(this);
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
			// Check for seek request
			if (seeking_.load()) {
				double target = seek_target_.load();
				
				// Perform the seek
				if (demuxer_->seek(target)) {
					// Clear packet queues
					video_packet_queue_->flush();
					if (has_audio_) {
						audio_packet_queue_->flush();
					}
					
					// Clear frame queue
					frame_queue_->flush();
				}
				
				seeking_ = false;
			}
			
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

		// Track if we need to recreate filter with actual frame format
		static bool filter_needs_init = false;
		static std::string pending_filter_for_init = "";
		
		for (;;) {
			// Check for filter change request
			if (filter_change_requested_.load()) {
				pending_filter_for_init = pending_filter_desc_;
				filter_needs_init = true;
				filter_change_requested_ = false;
				
				if (pending_filter_for_init.empty()) {
					// Disable filter immediately
					video_filter_.reset();
					current_filter_desc_ = "";
					filter_needs_init = false;
					std::cout << "Filter disabled\n";
				} else {
					std::cout << "Filter change requested: " << pending_filter_for_init << "\n";
				}
			}
			
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

			// Decode using hardware or software decoder
			bool sent = false;
			while (!sent) {
				if (use_hw_decode_) {
					sent = hw_decoder_->send(packet.get());
				} else {
					sent = video_decoder_->send(packet.get());
				}

				// If a whole frame has been decoded, process it
				bool received = false;
				if (use_hw_decode_) {
					received = hw_decoder_->receive(frame_decoded.get());
				} else {
					received = video_decoder_->receive(frame_decoded.get());
				}
				
				while (received) {
					frame_decoded->pts = av_rescale_q(
						frame_decoded->pkt_dts,
						demuxer_->video_time_base(),
						microseconds);

					// Create filter with actual frame format if needed
					if (filter_needs_init && !pending_filter_for_init.empty()) {
						try {
							int width = frame_decoded->width;
							int height = frame_decoded->height;
							AVPixelFormat pix_fmt = static_cast<AVPixelFormat>(frame_decoded->format);
							// Use video stream time_base from demuxer (more reliable than codec time_base)
							AVRational time_base = demuxer_->video_time_base();
							
							video_filter_ = std::make_unique<VideoFilter>(
								pending_filter_for_init, width, height, pix_fmt, time_base);
							current_filter_desc_ = pending_filter_for_init;
							filter_needs_init = false;
							
							// Check if filter changes dimensions
							int out_width = video_filter_->get_output_width();
							int out_height = video_filter_->get_output_height();
							if (out_width != width || out_height != height) {
								std::cout << "Warning: Filter changes dimensions from " 
								          << width << "x" << height << " to " 
								          << out_width << "x" << out_height << "\n";
							}
							
							// Force format converter recreation on next frame
							// (filter may change dimensions or pixel format)
							format_converter_.reset();
							
							std::cout << "Filter applied: " << pending_filter_for_init 
							          << " (format: " << pix_fmt << ", time_base: " 
							          << time_base.num << "/" << time_base.den << ")\n";
						} catch (const std::exception& e) {
							std::cerr << "Failed to apply filter: " << e.what() << "\n";
							video_filter_.reset();
							current_filter_desc_ = "";
							filter_needs_init = false;
						}
					}

					// Apply video filter if active
					std::unique_ptr<AVFrame, std::function<void(AVFrame*)>>
						frame_to_convert{nullptr, [](AVFrame* f){ av_frame_free(&f); }};
					
					if (video_filter_ && video_filter_->is_active()) {
						AVFrame* filtered_frame = av_frame_alloc();
						try {
							if (video_filter_->filter(frame_decoded.get(), filtered_frame)) {
								frame_to_convert.reset(filtered_frame);
								// Debug: Uncomment to verify filtering is happening
								// static int filter_count = 0;
								// if (++filter_count % 100 == 0) {
								//     std::cout << "Filtered " << filter_count << " frames\n";
								// }
							} else {
								av_frame_free(&filtered_frame);
								frame_to_convert.reset(frame_decoded.release());
								// Filter returned false - might be buffering, not an error
							}
						} catch (const std::exception& e) {
							std::cerr << "Filter exception: " << e.what() << "\n";
							av_frame_free(&filtered_frame);
							frame_to_convert.reset(frame_decoded.release());
							// Disable problematic filter
							video_filter_.reset();
							current_filter_desc_ = "";
						}
					} else {
						frame_to_convert.reset(frame_decoded.release());
					}

					// Use actual frame dimensions and format
					int width = frame_to_convert->width;
					int height = frame_to_convert->height;
					AVPixelFormat input_fmt = static_cast<AVPixelFormat>(frame_to_convert->format);
					
					// Create format converter on first frame or if format changes
					static AVPixelFormat last_fmt = AV_PIX_FMT_NONE;
					static int last_width = 0;
					static int last_height = 0;
					
					if (!format_converter_ || 
					    input_fmt != last_fmt || 
					    width != last_width || 
					    height != last_height) {
						format_converter_ = std::make_unique<FormatConverter>(
							width, height, input_fmt, AV_PIX_FMT_YUV420P);
						last_fmt = input_fmt;
						last_width = width;
						last_height = height;
					}

					// Convert to YUV420P for display
					std::unique_ptr<AVFrame, std::function<void(AVFrame*)>>
						frame_converted{
							av_frame_alloc(),
							[](AVFrame* f){ av_frame_free(&f); }};
					if (!frame_converted) {
						throw std::runtime_error("Allocating frame");
					}
					if (av_frame_copy_props(frame_converted.get(),
						frame_to_convert.get()) < 0) {
						throw std::runtime_error("Copying frame properties");
					}

					frame_converted->format = AV_PIX_FMT_YUV420P;
					frame_converted->width = width;
					frame_converted->height = height;

					if (av_frame_get_buffer(frame_converted.get(), 32) < 0) {
						throw std::runtime_error("Allocating frame buffer");
					}
					if (av_frame_make_writable(frame_converted.get()) < 0) {
						throw std::runtime_error("Making frame writable");
					}

					(*format_converter_)(
						frame_to_convert.get(), frame_converted.get());

					if (!frame_queue_->push(std::move(frame_converted))) {
						break;
					}
					
					// Prepare for next frame
					frame_decoded.reset(av_frame_alloc());
					if (use_hw_decode_) {
						received = hw_decoder_->receive(frame_decoded.get());
					} else {
						received = video_decoder_->receive(frame_decoded.get());
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
				
				// Update current position for progress bar
				current_position_ = frame->pts / 1000000.0;  // Convert microseconds to seconds
				display_->set_progress(current_position_, get_duration());
				
				// Store current frame for screenshot capture (make a copy)
				{
					std::lock_guard<std::mutex> lock(frame_mutex_);
					// Clone the frame (unique_ptr will automatically free old frame)
					AVFrame* frame_copy = av_frame_clone(frame.get());
					current_frame_.reset(frame_copy);
				}

				display_->refresh(
					{frame->data[0], frame->data[1], frame->data[2]},
					{static_cast<size_t>(frame->linesize[0]),
					 static_cast<size_t>(frame->linesize[1]),
					 static_cast<size_t>(frame->linesize[2])});

			} else {
				// When paused, keep UI responsive by redrawing
				display_->redraw();
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
