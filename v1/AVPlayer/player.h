/**
 * \file player.h
 * \brief Multi-threaded media player implementation
 */

#pragma once
#include "demuxer.h"
#include "display_resizable.h"
#include "format_converter.h"
#include "queue.h"
#include "timer.h"
#include "video_decoder.h"
#include "hw_decoder.h"
#include "video_filter.h"
#include "subtitle_renderer.h"
#include "audio_decoder.h"
#include "audio_player.h"
#include "audio_resampler.h"
#include "audio_track_manager.h"
#include "frame_capture.h"
#include <memory>
#include <stdexcept>

#if defined(_WIN32)
  #include <process.h>  // Required for _beginthreadex on Windows
#endif

#include <thread>
#include <vector>
#include <mutex>

/**
 * \class Player
 * \brief Multi-threaded media player with synchronized audio and video playback
 * 
 * This class implements a complete media player using a multi-threaded pipeline:
 * - Demultiplexing thread: Reads packets from the media file
 * - Video decode thread: Decodes video packets into frames
 * - Audio decode thread: Decodes audio packets into frames
 * - Video presentation thread: Displays video frames with precise timing
 * 
 * The threads communicate via thread-safe queues and synchronize audio/video
 * using timestamps.
 */
class Player {
public:
	/**
	 * \brief Constructs a player for the specified media file
	 * \param file_name Path to the media file to play
	 * \param use_hw_decode Enable hardware-accelerated decoding
	 * \param filter_desc Video filter description (empty for no filter)
	 */
	Player(const std::string &file_name, bool use_hw_decode = true, 
	       const std::string& filter_desc = "");
	
	/**
	 * \brief Starts playback and runs until completion or user quits
	 * 
	 * This method blocks until playback finishes or the user closes the window.
	 */
	void operator()();
	
	/**
	 * \brief Gets the total duration of the video
	 * \return Duration in seconds
	 */
	double get_duration() const;
	
	/**
	 * \brief Gets the current playback position
	 * \return Current position in seconds
	 */
	double get_position() const;
	
	/**
	 * \brief Requests a seek to the specified position
	 * \param seconds Target position in seconds
	 */
	void seek(double seconds);
	
	/**
	 * \brief Sets the playback speed
	 * \param speed Speed multiplier (0.25 to 4.0)
	 */
	void set_speed(double speed);
	
	/**
	 * \brief Gets the current playback speed
	 * \return Speed multiplier
	 */
	double get_speed() const;
	
	/**
	 * \brief Checks if hardware acceleration is active
	 * \return true if using hardware decode
	 */
	bool is_hardware_accelerated() const;
	
	/**
	 * \brief Gets the hardware acceleration type name
	 * \return Hardware type (e.g., "cuda", "vaapi") or "software"
	 */
	std::string get_hw_type() const;
	
	/**
	 * \brief Sets a video filter dynamically
	 * \param filter_desc Filter description string
	 */
	void set_filter(const std::string& filter_desc);
	
	/**
	 * \brief Gets the current filter description
	 * \return Current filter string
	 */
	std::string get_current_filter() const;
	
	/**
	 * \brief Gets available subtitle tracks
	 * \return Vector of subtitle track information
	 */
	std::vector<struct SubtitleTrack> get_subtitle_tracks() const;
	
	/**
	 * \brief Selects a subtitle track
	 * \param track_index Stream index of subtitle track (-1 to disable)
	 */
	void select_subtitle_track(int track_index);
	
	/**
	 * \brief Gets the currently selected subtitle track index
	 * \return Subtitle track index or -1 if disabled
	 */
	int get_current_subtitle_track() const { return current_subtitle_track_; }
	
	/**
	 * \brief Loads an external subtitle file
	 * \param subtitle_file Path to subtitle file
	 * \return true if successful
	 */
	bool load_external_subtitle(const std::string& subtitle_file);
	
	/**
	 * \brief Sets subtitle font size
	 * \param scale Font scale factor (1.0 = normal, 0.5-3.0 range)
	 */
	void set_subtitle_font_size(double scale);
	
	/**
	 * \brief Gets current subtitle font size
	 * \return Font scale factor
	 */
	double get_subtitle_font_size() const { return subtitle_font_scale_; }
	
	/**
	 * \brief Gets available audio tracks
	 * \return Vector of audio track information
	 */
	std::vector<struct AudioTrack> get_audio_tracks() const;
	
	/**
	 * \brief Selects an audio track
	 * \param track_index Stream index of audio track
	 */
	void select_audio_track(int track_index);
	
	/**
	 * \brief Gets the currently selected audio track index
	 * \return Audio track index
	 */
	int get_current_audio_track() const { return current_audio_track_; }
	
	/**
	 * \brief Sets audio channel mode
	 * \param mode Channel mode to use
	 */
	void set_audio_channel_mode(AudioChannelMode mode);
	
	/**
	 * \brief Gets current audio channel mode
	 * \return Current channel mode
	 */
	AudioChannelMode get_audio_channel_mode() const;
	
	/**
	 * \brief Adds an external audio file
	 * \param audio_file Path to external audio file
	 * \return true if successful
	 */
	bool add_external_audio(const std::string& audio_file);
	
	/**
	 * \brief Captures the current frame as a screenshot
	 * \param format Output format (default: PNG)
	 * \param filename Optional filename (auto-generated if empty)
	 * \return true if successful
	 */
	bool capture_screenshot(CaptureFormat format = CaptureFormat::PNG, 
	                       const std::string& filename = "");
	
	/**
	 * \brief Gets the current frame for capture
	 * \return Pointer to current frame or nullptr
	 */
	AVFrame* get_current_frame() const { return current_frame_.get(); }
	
	/**
	 * \brief Batch extract frames at regular intervals
	 * \param interval_seconds Interval between frames in seconds
	 * \param format Output format
	 * \param output_prefix Filename prefix (e.g., "frame")
	 * \param start_time Start time in seconds (0 = beginning)
	 * \param end_time End time in seconds (-1 = end of video)
	 * \return Number of frames extracted
	 */
	int batch_extract_frames(double interval_seconds, 
	                         CaptureFormat format = CaptureFormat::PNG,
	                         const std::string& output_prefix = "frame",
	                         double start_time = 0.0,
	                         double end_time = -1.0);
	
private:
	/**
	 * \brief Demultiplexing thread function
	 * 
	 * Reads packets from the media file and distributes them to the
	 * appropriate video or audio packet queues.
	 */
	void demultiplex();
	
	/**
	 * \brief Video decoding thread function
	 * 
	 * Decodes video packets from the video packet queue and pushes
	 * decoded frames to the frame queue.
	 */
	void decode_video();
	
	/**
	 * \brief Audio decoding thread function
	 * 
	 * Decodes audio packets from the audio packet queue, resamples them,
	 * and pushes samples to the audio player.
	 */
	void decode_audio();
	
	/**
	 * \brief Video presentation thread function
	 * 
	 * Pops frames from the frame queue and displays them at the correct
	 * time, synchronized with audio playback.
	 */
	void video();
	
private:
	std::unique_ptr<Demuxer> demuxer_;                    ///< Media file demuxer
	std::unique_ptr<VideoDecoder> video_decoder_;         ///< Software video decoder
	std::unique_ptr<HardwareDecoder> hw_decoder_;         ///< Hardware video decoder
	std::unique_ptr<VideoFilter> video_filter_;           ///< Video filter
	std::unique_ptr<AudioDecoder> audio_decoder_;         ///< Audio decoder
	std::unique_ptr<FormatConverter> format_converter_;   ///< Video format converter
	std::unique_ptr<AudioResampler> audio_resampler_;     ///< Audio resampler
	std::unique_ptr<ResizableDisplay> display_;           ///< Video display
	std::unique_ptr<AudioPlayer> audio_player_;           ///< Audio player
	std::unique_ptr<Timer> timer_;                        ///< Precise timing controller
	std::unique_ptr<PacketQueue> video_packet_queue_;     ///< Queue for video packets
	std::unique_ptr<PacketQueue> audio_packet_queue_;     ///< Queue for audio packets
	std::unique_ptr<FrameQueue> frame_queue_;             ///< Queue for decoded video frames
	std::vector<std::thread> stages_;                     ///< Worker threads
	static const size_t queue_size_;                      ///< Maximum queue size
	std::exception_ptr exception_{};                      ///< Exception from worker threads
	bool has_audio_{false};                               ///< Whether the media has audio
	bool use_hw_decode_{false};                           ///< Whether to use hardware decode
	std::atomic<double> current_position_{0.0};           ///< Current playback position in seconds
	std::atomic<double> seek_target_{-1.0};               ///< Seek target position (-1.0 = no seek pending)
	std::atomic<bool> seeking_{false};                    ///< Whether a seek is in progress
	std::string current_filter_desc_;                     ///< Current filter description
	std::atomic<bool> filter_change_requested_{false};    ///< Whether filter change is pending
	std::string pending_filter_desc_;                     ///< Pending filter description
	int current_subtitle_track_{-1};                      ///< Current subtitle track index (-1 = disabled)
	std::unique_ptr<class SubtitleRenderer> subtitle_renderer_;  ///< Subtitle renderer
	double subtitle_font_scale_{1.0};                     ///< Subtitle font scale
	std::unique_ptr<AudioTrackManager> audio_track_manager_;  ///< Audio track manager
	int current_audio_track_{-1};                         ///< Current audio track index
	std::atomic<bool> audio_track_change_requested_{false};  ///< Whether audio track change is pending
	int pending_audio_track_{-1};                         ///< Pending audio track index
	std::unique_ptr<AVFrame, std::function<void(AVFrame*)>> current_frame_{nullptr, [](AVFrame* f){ av_frame_free(&f); }};  ///< Current frame for capture
	std::mutex frame_mutex_;                              ///< Mutex for frame access
};
