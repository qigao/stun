/**
 * \file audio_track_manager.h
 * \brief Audio track and channel management
 */

#pragma once
extern "C" {
	#include <libavformat/avformat.h>
	#include <libavcodec/avcodec.h>
}
#include <string>
#include <vector>
#include <memory>

/**
 * \struct AudioTrack
 * \brief Information about an audio track
 */
struct AudioTrack {
	int index;                  ///< Stream index
	std::string language;       ///< Language code (e.g., "eng", "spa")
	std::string title;          ///< Track title/name
	std::string codec_name;     ///< Codec name (e.g., "aac", "mp3")
	int channels;               ///< Number of channels
	int sample_rate;            ///< Sample rate in Hz
	int64_t bitrate;            ///< Bitrate in bits/sec
	bool is_default;            ///< Whether this is the default track
	bool is_external;           ///< Whether this is an external audio file
	std::string source_file;    ///< Source file path (for external tracks)
};

/**
 * \enum AudioChannelMode
 * \brief Audio channel output modes
 */
enum class AudioChannelMode {
	STEREO,          ///< Standard stereo (2 channels)
	MONO,            ///< Mono (1 channel, downmix)
	SURROUND_5_1,    ///< 5.1 surround sound
	SURROUND_7_1,    ///< 7.1 surround sound
	LEFT_ONLY,       ///< Left channel only
	RIGHT_ONLY,      ///< Right channel only
	SWAP_CHANNELS,   ///< Swap left and right
	ORIGINAL         ///< Keep original channel layout
};

/**
 * \class AudioTrackManager
 * \brief Manages multiple audio tracks and channel configurations
 */
class AudioTrackManager {
public:
	/**
	 * \brief Constructs an audio track manager
	 */
	AudioTrackManager();
	
	/**
	 * \brief Destructor
	 */
	~AudioTrackManager();
	
	/**
	 * \brief Gets available audio tracks from media file
	 * \param format_ctx AVFormatContext from demuxer
	 * \return Vector of audio track information
	 */
	static std::vector<AudioTrack> get_audio_tracks(void* format_ctx);
	
	/**
	 * \brief Adds an external audio file
	 * \param audio_file Path to external audio file
	 * \return Track index if successful, -1 on error
	 */
	int add_external_audio(const std::string& audio_file);
	
	/**
	 * \brief Removes an external audio track
	 * \param track_index Track index to remove
	 */
	void remove_external_audio(int track_index);
	
	/**
	 * \brief Gets all available tracks (internal + external)
	 * \return Vector of all audio tracks
	 */
	std::vector<AudioTrack> get_all_tracks() const;
	
	/**
	 * \brief Sets the active audio track
	 * \param track_index Track index to activate
	 */
	void set_active_track(int track_index);
	
	/**
	 * \brief Gets the active track index
	 * \return Active track index or -1 if none
	 */
	int get_active_track() const { return active_track_index_; }
	
	/**
	 * \brief Sets the audio channel mode
	 * \param mode Channel mode to use
	 */
	void set_channel_mode(AudioChannelMode mode);
	
	/**
	 * \brief Gets the current channel mode
	 * \return Current channel mode
	 */
	AudioChannelMode get_channel_mode() const { return channel_mode_; }
	
	/**
	 * \brief Gets the channel mode name
	 * \param mode Channel mode
	 * \return Human-readable name
	 */
	static std::string get_channel_mode_name(AudioChannelMode mode);
	
	/**
	 * \brief Checks if an external audio file is loaded
	 * \param track_index Track index to check
	 * \return true if external, false if internal
	 */
	bool is_external_track(int track_index) const;

private:
	std::vector<AudioTrack> internal_tracks_;   ///< Internal audio tracks
	std::vector<AudioTrack> external_tracks_;   ///< External audio tracks
	int active_track_index_{-1};                ///< Currently active track
	AudioChannelMode channel_mode_{AudioChannelMode::ORIGINAL};  ///< Current channel mode
};

/**
 * \class AudioChannelProcessor
 * \brief Processes audio samples for different channel modes
 */
class AudioChannelProcessor {
public:
	/**
	 * \brief Processes audio samples according to channel mode
	 * \param input Input audio samples (interleaved)
	 * \param output Output audio samples (interleaved)
	 * \param num_samples Number of samples per channel
	 * \param input_channels Number of input channels
	 * \param output_channels Number of output channels
	 * \param mode Channel processing mode
	 */
	static void process(const float* input, float* output, 
	                   int num_samples, int input_channels, 
	                   int output_channels, AudioChannelMode mode);

private:
	/**
	 * \brief Downmix to mono
	 */
	static void downmix_to_mono(const float* input, float* output, 
	                           int num_samples, int input_channels);
	
	/**
	 * \brief Extract left channel only
	 */
	static void extract_left(const float* input, float* output, 
	                        int num_samples, int input_channels);
	
	/**
	 * \brief Extract right channel only
	 */
	static void extract_right(const float* input, float* output, 
	                         int num_samples, int input_channels);
	
	/**
	 * \brief Swap left and right channels
	 */
	static void swap_channels(const float* input, float* output, 
	                         int num_samples);
};
