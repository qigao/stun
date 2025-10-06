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
#include "audio_decoder.h"
#include "audio_player.h"
#include "audio_resampler.h"
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

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
	 */
	Player(const std::string &file_name);
	
	/**
	 * \brief Starts playback and runs until completion or user quits
	 * 
	 * This method blocks until playback finishes or the user closes the window.
	 */
	void operator()();
	
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
	std::unique_ptr<VideoDecoder> video_decoder_;         ///< Video decoder
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
};
