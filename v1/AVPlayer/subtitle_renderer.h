/**
 * \file subtitle_renderer.h
 * \brief Subtitle rendering using libass
 */

#pragma once
extern "C" {
	#include <ass/ass.h>
}
#include <string>
#include <vector>
#include <cstdint>

/**
 * \struct SubtitleTrack
 * \brief Information about a subtitle track
 */
struct SubtitleTrack {
	int index;                  ///< Stream index
	std::string language;       ///< Language code (e.g., "eng", "spa")
	std::string title;          ///< Track title/name
	std::string codec_name;     ///< Codec name (e.g., "subrip", "ass")
	bool is_default;            ///< Whether this is the default track
};

/**
 * \class SubtitleRenderer
 * \brief Renders subtitles using libass
 * 
 * Supports various subtitle formats including:
 * - ASS/SSA (Advanced SubStation Alpha)
 * - SRT (SubRip)
 * - WebVTT
 * - DVD/Blu-ray subtitles (via conversion)
 */
class SubtitleRenderer {
public:
	/**
	 * \brief Constructs a subtitle renderer
	 * \param video_width Video frame width
	 * \param video_height Video frame height
	 */
	SubtitleRenderer(int video_width, int video_height);
	
	/**
	 * \brief Destructor - cleans up libass resources
	 */
	~SubtitleRenderer();
	
	/**
	 * \brief Loads subtitle file
	 * \param filename Path to subtitle file
	 * \return true if successful
	 */
	bool load_file(const std::string& filename);
	
	/**
	 * \brief Loads subtitle data from memory
	 * \param data Subtitle data
	 * \param size Data size in bytes
	 * \return true if successful
	 */
	bool load_data(const char* data, size_t size);
	
	/**
	 * \brief Renders subtitles for a specific timestamp
	 * \param timestamp Presentation timestamp in milliseconds
	 * \param image_out Output image buffer (RGBA format)
	 * \param width_out Output image width
	 * \param height_out Output image height
	 * \return true if subtitles were rendered
	 */
	bool render(int64_t timestamp, uint8_t** image_out, int* width_out, int* height_out);
	
	/**
	 * \brief Sets video dimensions (call when video size changes)
	 * \param width New video width
	 * \param height New video height
	 */
	void set_frame_size(int width, int height);
	
	/**
	 * \brief Enables or disables subtitle rendering
	 * \param enabled true to enable, false to disable
	 */
	void set_enabled(bool enabled) { enabled_ = enabled; }
	
	/**
	 * \brief Checks if subtitle rendering is enabled
	 * \return true if enabled
	 */
	bool is_enabled() const { return enabled_; }
	
	/**
	 * \brief Sets subtitle font scale
	 * \param scale Font scale factor (1.0 = normal)
	 */
	void set_font_scale(double scale);
	
	/**
	 * \brief Gets available subtitle tracks from media file
	 * \param format_ctx AVFormatContext from demuxer
	 * \return Vector of subtitle track information
	 */
	static std::vector<SubtitleTrack> get_subtitle_tracks(void* format_ctx);

private:
	ASS_Library* ass_library_{nullptr};     ///< libass library handle
	ASS_Renderer* ass_renderer_{nullptr};   ///< libass renderer
	ASS_Track* ass_track_{nullptr};         ///< Current subtitle track
	int video_width_;                       ///< Video frame width
	int video_height_;                      ///< Video frame height
	bool enabled_{true};                    ///< Whether rendering is enabled
	std::vector<uint8_t> image_buffer_;     ///< Buffer for rendered subtitle image
};
