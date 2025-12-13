#include "subtitle_renderer.h"
extern "C" {
	#include <libavformat/avformat.h>
	#include <libavcodec/avcodec.h>
}
#include <iostream>
#include <cstring>

SubtitleRenderer::SubtitleRenderer(int video_width, int video_height)
	: video_width_(video_width), video_height_(video_height) {
	
	// Initialize libass library
	ass_library_ = ass_library_init();
	if (!ass_library_) {
		std::cerr << "Failed to initialize libass library\n";
		return;
	}
	
	// Set message callback (optional, for debugging)
	ass_set_message_cb(ass_library_, [](int level, const char* fmt, va_list args, void*) {
		if (level < 4) {  // Only show warnings and errors
			vfprintf(stderr, fmt, args);
		}
	}, nullptr);
	
	// Create renderer
	ass_renderer_ = ass_renderer_init(ass_library_);
	if (!ass_renderer_) {
		std::cerr << "Failed to initialize libass renderer\n";
		return;
	}
	
	// Set renderer parameters
	ass_set_frame_size(ass_renderer_, video_width_, video_height_);
	ass_set_fonts(ass_renderer_, nullptr, "sans-serif", 
	              ASS_FONTPROVIDER_AUTODETECT, nullptr, 1);
	ass_set_hinting(ass_renderer_, ASS_HINTING_LIGHT);
}

SubtitleRenderer::~SubtitleRenderer() {
	if (ass_track_) {
		ass_free_track(ass_track_);
	}
	if (ass_renderer_) {
		ass_renderer_done(ass_renderer_);
	}
	if (ass_library_) {
		ass_library_done(ass_library_);
	}
}

bool SubtitleRenderer::load_file(const std::string& filename) {
	if (!ass_library_) {
		return false;
	}
	
	// Free existing track
	if (ass_track_) {
		ass_free_track(ass_track_);
		ass_track_ = nullptr;
	}
	
	// Load subtitle file
	ass_track_ = ass_read_file(ass_library_, const_cast<char*>(filename.c_str()), nullptr);
	if (!ass_track_) {
		std::cerr << "Failed to load subtitle file: " << filename << "\n";
		return false;
	}
	
	std::cout << "Loaded subtitle file: " << filename << "\n";
	return true;
}

bool SubtitleRenderer::load_data(const char* data, size_t size) {
	if (!ass_library_) {
		return false;
	}
	
	// Free existing track
	if (ass_track_) {
		ass_free_track(ass_track_);
		ass_track_ = nullptr;
	}
	
	// Create new track
	ass_track_ = ass_new_track(ass_library_);
	if (!ass_track_) {
		return false;
	}
	
	// Process subtitle data
	ass_process_data(ass_track_, const_cast<char*>(data), size);
	
	return true;
}

bool SubtitleRenderer::render(int64_t timestamp, uint8_t** image_out, 
                               int* width_out, int* height_out) {
	if (!enabled_ || !ass_renderer_ || !ass_track_) {
		return false;
	}
	
	// Render subtitles for this timestamp
	int detect_change = 0;
	ASS_Image* img = ass_render_frame(ass_renderer_, ass_track_, timestamp, &detect_change);
	
	if (!img) {
		return false;
	}
	
	// Allocate output buffer if needed
	size_t buffer_size = video_width_ * video_height_ * 4;  // RGBA
	if (image_buffer_.size() != buffer_size) {
		image_buffer_.resize(buffer_size);
	}
	
	// Clear buffer
	std::memset(image_buffer_.data(), 0, buffer_size);
	
	// Composite subtitle images
	while (img) {
		if (img->w > 0 && img->h > 0) {
			uint8_t r = (img->color >> 24) & 0xFF;
			uint8_t g = (img->color >> 16) & 0xFF;
			uint8_t b = (img->color >> 8) & 0xFF;
			uint8_t a = 255 - (img->color & 0xFF);
			
			for (int y = 0; y < img->h; y++) {
				for (int x = 0; x < img->w; x++) {
					int dst_x = img->dst_x + x;
					int dst_y = img->dst_y + y;
					
					if (dst_x >= 0 && dst_x < video_width_ && 
					    dst_y >= 0 && dst_y < video_height_) {
						uint8_t alpha = img->bitmap[y * img->stride + x];
						if (alpha > 0) {
							int idx = (dst_y * video_width_ + dst_x) * 4;
							float blend = (alpha * a) / 65025.0f;
							image_buffer_[idx + 0] = r;
							image_buffer_[idx + 1] = g;
							image_buffer_[idx + 2] = b;
							image_buffer_[idx + 3] = static_cast<uint8_t>(blend * 255);
						}
					}
				}
			}
		}
		img = img->next;
	}
	
	*image_out = image_buffer_.data();
	*width_out = video_width_;
	*height_out = video_height_;
	
	return true;
}

void SubtitleRenderer::set_frame_size(int width, int height) {
	video_width_ = width;
	video_height_ = height;
	
	if (ass_renderer_) {
		ass_set_frame_size(ass_renderer_, width, height);
	}
}

void SubtitleRenderer::set_font_scale(double scale) {
	if (ass_renderer_) {
		ass_set_font_scale(ass_renderer_, scale);
	}
}

std::vector<SubtitleTrack> SubtitleRenderer::get_subtitle_tracks(void* format_ctx_ptr) {
	std::vector<SubtitleTrack> tracks;
	
	AVFormatContext* format_ctx = static_cast<AVFormatContext*>(format_ctx_ptr);
	if (!format_ctx) {
		return tracks;
	}
	
	for (unsigned i = 0; i < format_ctx->nb_streams; i++) {
		AVStream* stream = format_ctx->streams[i];
		if (stream->codecpar->codec_type == AVMEDIA_TYPE_SUBTITLE) {
			SubtitleTrack track;
			track.index = i;
			
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
			
			// Check if default
			track.is_default = (stream->disposition & AV_DISPOSITION_DEFAULT) != 0;
			
			tracks.push_back(track);
		}
	}
	
	return tracks;
}
