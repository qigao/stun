/**
 * \file demuxer.h
 * \brief Media file demultiplexing using FFmpeg's libavformat
 */

#pragma once
#include <string>
extern "C" {
	#include "libavformat/avformat.h"
}

/**
 * \class Demuxer
 * \brief Demultiplexes media files into separate audio and video packet streams
 * 
 * This class wraps FFmpeg's libavformat to open media files and extract
 * individual packets from audio and video streams.
 */
class Demuxer {
public:
	/**
	 * \brief Constructs a demuxer and opens the specified media file
	 * \param file_name Path to the media file to open
	 */
	Demuxer(const std::string &file_name);
	
	/**
	 * \brief Destructor - closes the media file and cleans up resources
	 */
	~Demuxer();
	
	/**
	 * \brief Gets the codec parameters for the video stream
	 * \return Pointer to video codec parameters
	 */
	AVCodecParameters* video_codec_parameters();
	
	/**
	 * \brief Gets the codec parameters for the audio stream
	 * \return Pointer to audio codec parameters, or nullptr if no audio stream
	 */
	AVCodecParameters* audio_codec_parameters();
	
	/**
	 * \brief Gets the index of the video stream
	 * \return Video stream index
	 */
	int video_stream_index() const;
	
	/**
	 * \brief Gets the index of the audio stream
	 * \return Audio stream index, or -1 if no audio stream
	 */
	int audio_stream_index() const;
	
	/**
	 * \brief Gets the time base for the video stream
	 * \return Video stream time base as an AVRational
	 */
	AVRational video_time_base() const;
	
	/**
	 * \brief Gets the time base for the audio stream
	 * \return Audio stream time base as an AVRational
	 */
	AVRational audio_time_base() const;
	
	/**
	 * \brief Gets the container-level time base
	 * \return Container time base as an AVRational
	 */
	AVRational time_base() const;
	
	/**
	 * \brief Reads the next packet from the media file
	 * \param packet Reference to packet structure to fill
	 * \return true if a packet was read successfully, false on end of file or error
	 */
	bool operator()(AVPacket &packet);
	
	/**
	 * \brief Checks if the media file has an audio stream
	 * \return true if audio stream exists, false otherwise
	 */
	bool has_audio() const;

private:
	AVFormatContext* format_context_{};  ///< FFmpeg format context
	int video_stream_index_{};           ///< Index of the video stream
	int audio_stream_index_{-1};         ///< Index of the audio stream (-1 if none)
};
