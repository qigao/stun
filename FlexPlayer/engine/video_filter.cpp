#include "video_filter.h"
#include "ffmpeg.h"
#include <iostream>

VideoFilter::VideoFilter(const std::string& filter_desc, 
                         int width, int height, 
                         AVPixelFormat pix_fmt, 
                         AVRational time_base)
	: filter_desc_(filter_desc), width_(width), height_(height), 
	  pix_fmt_(pix_fmt), time_base_(time_base) {
	
	if (!filter_desc.empty()) {
		init_filter_graph();
	}
}

VideoFilter::~VideoFilter() {
	if (filter_graph_) {
		avfilter_graph_free(&filter_graph_);
	}
}

void VideoFilter::init_filter_graph() {
	char args[512];
	int ret;
	
	const AVFilter* buffersrc = avfilter_get_by_name("buffer");
	const AVFilter* buffersink = avfilter_get_by_name("buffersink");
	AVFilterInOut* outputs = avfilter_inout_alloc();
	AVFilterInOut* inputs = avfilter_inout_alloc();
	
	filter_graph_ = avfilter_graph_alloc();
	if (!outputs || !inputs || !filter_graph_) {
		throw ffmpeg::Error{"Failed to allocate filter graph"};
	}
	
	// Create buffer source
	snprintf(args, sizeof(args),
	         "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=1/1",
	         width_, height_, pix_fmt_, time_base_.num, time_base_.den);
	
	ret = avfilter_graph_create_filter(&buffersrc_ctx_, buffersrc, "in",
	                                   args, nullptr, filter_graph_);
	if (ret < 0) {
		avfilter_inout_free(&outputs);
		avfilter_inout_free(&inputs);
		throw ffmpeg::Error{ret};
	}
	
	// Create buffer sink
	ret = avfilter_graph_create_filter(&buffersink_ctx_, buffersink, "out",
	                                   nullptr, nullptr, filter_graph_);
	if (ret < 0) {
		avfilter_inout_free(&outputs);
		avfilter_inout_free(&inputs);
		throw ffmpeg::Error{ret};
	}
	
	// Set output pixel format
	enum AVPixelFormat pix_fmts[] = { pix_fmt_, AV_PIX_FMT_NONE };
	ret = av_opt_set_int_list(buffersink_ctx_, "pix_fmts", pix_fmts,
	                          AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
	if (ret < 0) {
		avfilter_inout_free(&outputs);
		avfilter_inout_free(&inputs);
		throw ffmpeg::Error{ret};
	}
	
	// Set endpoints for the filter graph
	outputs->name = av_strdup("in");
	outputs->filter_ctx = buffersrc_ctx_;
	outputs->pad_idx = 0;
	outputs->next = nullptr;
	
	inputs->name = av_strdup("out");
	inputs->filter_ctx = buffersink_ctx_;
	inputs->pad_idx = 0;
	inputs->next = nullptr;
	
	// Parse filter description
	ret = avfilter_graph_parse_ptr(filter_graph_, filter_desc_.c_str(),
	                               &inputs, &outputs, nullptr);
	if (ret < 0) {
		avfilter_inout_free(&outputs);
		avfilter_inout_free(&inputs);
		throw ffmpeg::Error{ret};
	}
	
	// Configure the filter graph
	ret = avfilter_graph_config(filter_graph_, nullptr);
	if (ret < 0) {
		avfilter_inout_free(&outputs);
		avfilter_inout_free(&inputs);
		throw ffmpeg::Error{ret};
	}
	
	avfilter_inout_free(&outputs);
	avfilter_inout_free(&inputs);
	
	std::cout << "Video filter initialized: " << filter_desc_ << "\n";
}

bool VideoFilter::filter(AVFrame* input_frame, AVFrame* output_frame) {
	if (!filter_graph_) {
		return false;
	}
	
	// Push frame to filter graph
	int ret = av_buffersrc_add_frame_flags(buffersrc_ctx_, input_frame, 
	                                       AV_BUFFERSRC_FLAG_KEEP_REF);
	if (ret < 0) {
		std::cerr << "Error feeding frame to filter: " << ffmpeg::error_string(ret) << "\n";
		return false;
	}
	
	// Pull filtered frame from filter graph
	ret = av_buffersink_get_frame(buffersink_ctx_, output_frame);
	if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
		return false;
	}
	if (ret < 0) {
		std::cerr << "Error getting filtered frame: " << ffmpeg::error_string(ret) << "\n";
		return false;
	}
	
	return true;
}

int VideoFilter::get_output_width() const {
	if (buffersink_ctx_) {
		return av_buffersink_get_w(buffersink_ctx_);
	}
	return width_;
}

int VideoFilter::get_output_height() const {
	if (buffersink_ctx_) {
		return av_buffersink_get_h(buffersink_ctx_);
	}
	return height_;
}
