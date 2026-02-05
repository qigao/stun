#include "hw_decoder.h"
#include "ffmpeg.h"
#include <iostream>

// Static member for hardware pixel format callback
static AVPixelFormat hw_pix_fmt_global = AV_PIX_FMT_NONE;

enum AVPixelFormat HardwareDecoder::get_hw_format(AVCodecContext* ctx, const enum AVPixelFormat* pix_fmts) {
	for (const enum AVPixelFormat* p = pix_fmts; *p != AV_PIX_FMT_NONE; p++) {
		if (*p == hw_pix_fmt_global) {
			return *p;
		}
	}
	std::cerr << "Failed to get HW surface format.\n";
	return AV_PIX_FMT_NONE;
}

const AVCodec* HardwareDecoder::find_hw_decoder(AVCodecID codec_id, AVHWDeviceType hw_type) {
	const AVCodec* codec = nullptr;
	void* iter = nullptr;
	
	// Iterate through all decoders for this codec
	while ((codec = av_codec_iterate(&iter))) {
		if (codec->id != codec_id || !av_codec_is_decoder(codec)) {
			continue;
		}
		
		// Check if this decoder supports the hardware type
		for (int i = 0;; i++) {
			const AVCodecHWConfig* config = avcodec_get_hw_config(codec, i);
			if (!config) {
				break;
			}
			
			if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
			    config->device_type == hw_type) {
				return codec;
			}
		}
	}
	
	return nullptr;
}

bool HardwareDecoder::try_hardware_init(const AVCodec* codec, AVHWDeviceType hw_type) {
	// Find hardware configuration
	for (int i = 0;; i++) {
		const AVCodecHWConfig* config = avcodec_get_hw_config(codec, i);
		if (!config) {
			return false;
		}
		
		if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
		    config->device_type == hw_type) {
			hw_pix_fmt_ = config->pix_fmt;
			hw_pix_fmt_global = hw_pix_fmt_;
			break;
		}
	}
	
	// Create hardware device context
	int ret = av_hwdevice_ctx_create(&hw_device_ctx_, hw_type, nullptr, nullptr, 0);
	if (ret < 0) {
		std::cerr << "Failed to create " << av_hwdevice_get_type_name(hw_type) 
		          << " device context: " << ffmpeg::error_string(ret) << "\n";
		return false;
	}
	
	codec_context_->hw_device_ctx = av_buffer_ref(hw_device_ctx_);
	codec_context_->get_format = get_hw_format;
	hw_type_ = hw_type;
	
	std::cout << "Hardware acceleration enabled: " << av_hwdevice_get_type_name(hw_type) << "\n";
	return true;
}

HardwareDecoder::HardwareDecoder(AVCodecParameters* codec_parameters, 
                                 const std::string& preferred_hw_type) {
	const AVCodec* codec = nullptr;
	AVHWDeviceType hw_type = AV_HWDEVICE_TYPE_NONE;
	
	// Try preferred hardware type first
	if (!preferred_hw_type.empty()) {
		hw_type = av_hwdevice_find_type_by_name(preferred_hw_type.c_str());
		if (hw_type != AV_HWDEVICE_TYPE_NONE) {
			codec = find_hw_decoder(codec_parameters->codec_id, hw_type);
		}
	}
	
	// Auto-detect hardware acceleration if no preference or preferred failed
	if (!codec) {
		// Try hardware types in order of preference
		const AVHWDeviceType hw_priority[] = {
#ifdef _WIN32
			AV_HWDEVICE_TYPE_D3D11VA,
			AV_HWDEVICE_TYPE_DXVA2,
#elif __APPLE__
			AV_HWDEVICE_TYPE_VIDEOTOOLBOX,
#else
			AV_HWDEVICE_TYPE_VAAPI,
			AV_HWDEVICE_TYPE_VDPAU,
#endif
			AV_HWDEVICE_TYPE_CUDA,
			AV_HWDEVICE_TYPE_QSV,
			AV_HWDEVICE_TYPE_NONE
		};
		
		for (int i = 0; hw_priority[i] != AV_HWDEVICE_TYPE_NONE; i++) {
			codec = find_hw_decoder(codec_parameters->codec_id, hw_priority[i]);
			if (codec) {
				hw_type = hw_priority[i];
				std::cout << "Found hardware decoder: " << codec->name 
				          << " (" << av_hwdevice_get_type_name(hw_type) << ")\n";
				break;
			}
		}
	}
	
	// Fallback to software decoder
	if (!codec) {
		std::cout << "No hardware decoder found, using software decoding\n";
		codec = avcodec_find_decoder(codec_parameters->codec_id);
		if (!codec) {
			throw ffmpeg::Error{"Unsupported video codec"};
		}
	}
	
	// Allocate codec context
	codec_context_ = avcodec_alloc_context3(codec);
	if (!codec_context_) {
		throw ffmpeg::Error{"Couldn't allocate video codec context"};
	}
	
	ffmpeg::check(avcodec_parameters_to_context(codec_context_, codec_parameters));
	
	// Try to initialize hardware acceleration
	if (hw_type != AV_HWDEVICE_TYPE_NONE) {
		if (!try_hardware_init(codec, hw_type)) {
			std::cerr << "Hardware acceleration init failed, falling back to software\n";
			hw_device_ctx_ = nullptr;
			hw_type_ = AV_HWDEVICE_TYPE_NONE;
		}
	}
	
	// Open codec
	ffmpeg::check(avcodec_open2(codec_context_, codec, nullptr));
}

HardwareDecoder::~HardwareDecoder() {
	avcodec_free_context(&codec_context_);
	if (hw_device_ctx_) {
		av_buffer_unref(&hw_device_ctx_);
	}
}

bool HardwareDecoder::send(AVPacket* packet) {
	auto ret = avcodec_send_packet(codec_context_, packet);
	if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
		return false;
	} else {
		ffmpeg::check(ret);
		return true;
	}
}

bool HardwareDecoder::receive(AVFrame* frame) {
	auto ret = avcodec_receive_frame(codec_context_, frame);
	if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
		return false;
	} else {
		ffmpeg::check(ret);
		
		// If hardware frame, transfer to system memory
		if (frame->format == hw_pix_fmt_ && hw_device_ctx_) {
			AVFrame* sw_frame = av_frame_alloc();
			if (!sw_frame) {
				throw ffmpeg::Error{"Failed to allocate software frame"};
			}
			
			ret = av_hwframe_transfer_data(sw_frame, frame, 0);
			if (ret < 0) {
				av_frame_free(&sw_frame);
				throw ffmpeg::Error{ret};
			}
			
			av_frame_copy_props(sw_frame, frame);
			av_frame_unref(frame);
			av_frame_move_ref(frame, sw_frame);
			av_frame_free(&sw_frame);
		}
		
		return true;
	}
}

unsigned HardwareDecoder::width() const {
	return codec_context_->width;
}

unsigned HardwareDecoder::height() const {
	return codec_context_->height;
}

AVPixelFormat HardwareDecoder::pixel_format() const {
	return codec_context_->pix_fmt;
}

AVRational HardwareDecoder::time_base() const {
	return codec_context_->time_base;
}

std::string HardwareDecoder::get_hw_type_name() const {
	if (hw_type_ == AV_HWDEVICE_TYPE_NONE) {
		return "software";
	}
	return av_hwdevice_get_type_name(hw_type_);
}
