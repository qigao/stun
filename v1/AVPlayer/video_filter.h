/**
 * \file video_filter.h
 * \brief Video filtering using FFmpeg's libavfilter
 */

#pragma once
extern "C" {
	#include <libavfilter/avfilter.h>
	#include <libavfilter/buffersink.h>
	#include <libavfilter/buffersrc.h>
	#include <libavutil/opt.h>
}
#include <string>

/**
 * \class VideoFilter
 * \brief Applies video filters using FFmpeg's filter graph
 * 
 * Supports any FFmpeg filter or filter chain, examples:
 * - "hflip" - Horizontal flip
 * - "vflip" - Vertical flip
 * - "rotate=PI/4" - Rotate 45 degrees
 * - "eq=brightness=0.1:contrast=1.2" - Adjust brightness/contrast
 * - "hue=s=0" - Grayscale
 * - "boxblur=2:1" - Blur effect
 * - "unsharp=5:5:1.0:5:5:0.0" - Sharpen
 * - "scale=1280:720" - Resize
 * - "crop=iw/2:ih/2:0:0" - Crop
 * - Multiple filters: "hflip,vflip,eq=contrast=1.5"
 */
class VideoFilter {
public:
	/**
	 * \brief Constructs a video filter with the specified filter description
	 * \param filter_desc Filter description string (e.g., "hflip,vflip")
	 * \param width Input frame width
	 * \param height Input frame height
	 * \param pix_fmt Input pixel format
	 * \param time_base Input time base
	 */
	VideoFilter(const std::string& filter_desc, 
	            int width, int height, 
	            AVPixelFormat pix_fmt, 
	            AVRational time_base);
	
	/**
	 * \brief Destructor - cleans up filter graph
	 */
	~VideoFilter();
	
	/**
	 * \brief Applies the filter to a frame
	 * \param input_frame Input frame to filter
	 * \param output_frame Output frame to receive filtered result
	 * \return true if successful, false otherwise
	 */
	bool filter(AVFrame* input_frame, AVFrame* output_frame);
	
	/**
	 * \brief Checks if the filter is active
	 * \return true if filter is initialized and ready
	 */
	bool is_active() const { return filter_graph_ != nullptr; }
	
	/**
	 * \brief Gets the filter description
	 * \return Filter description string
	 */
	std::string get_description() const { return filter_desc_; }
	
	/**
	 * \brief Gets the output width (may differ from input if filter changes size)
	 * \return Output width in pixels
	 */
	int get_output_width() const;
	
	/**
	 * \brief Gets the output height (may differ from input if filter changes size)
	 * \return Output height in pixels
	 */
	int get_output_height() const;
	
private:
	/**
	 * \brief Initializes the filter graph
	 */
	void init_filter_graph();
	
	std::string filter_desc_;              ///< Filter description string
	int width_;                            ///< Input width
	int height_;                           ///< Input height
	AVPixelFormat pix_fmt_;                ///< Input pixel format
	AVRational time_base_;                 ///< Input time base
	
	AVFilterGraph* filter_graph_{};        ///< Filter graph
	AVFilterContext* buffersrc_ctx_{};     ///< Buffer source context
	AVFilterContext* buffersink_ctx_{};    ///< Buffer sink context
};
