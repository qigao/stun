/**
 * \file hw_decoder.h
 * \brief Hardware-accelerated video decoding using FFmpeg
 */

#pragma once
extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavutil/hwcontext.h>
}
#include <string>

/**
 * \class HardwareDecoder
 * \brief Hardware-accelerated video decoder with automatic fallback to software
 * 
 * Supports multiple hardware acceleration APIs:
 * - CUDA/NVDEC (NVIDIA)
 * - VAAPI (Linux/Intel)
 * - VideoToolbox (macOS)
 * - DXVA2/D3D11VA (Windows)
 * - QSV (Intel Quick Sync)
 */
class HardwareDecoder {
public:
	/**
	 * \brief Constructs a hardware decoder with automatic API detection
	 * \param codec_parameters The codec parameters from the demuxer
	 * \param preferred_hw_type Preferred hardware type (empty for auto-detect)
	 */
	HardwareDecoder(AVCodecParameters* codec_parameters, 
	                const std::string& preferred_hw_type = "");
	
	/**
	 * \brief Destructor - cleans up codec and hardware contexts
	 */
	~HardwareDecoder();
	
	/**
	 * \brief Sends a compressed video packet to the decoder
	 * \param packet The video packet to decode
	 * \return true if the packet was successfully sent, false otherwise
	 */
	bool send(AVPacket* packet);
	
	/**
	 * \brief Receives a decoded video frame from the decoder
	 * \param frame The frame to receive decoded video data into
	 * \return true if a frame was successfully received, false otherwise
	 */
	bool receive(AVFrame* frame);
	
	/**
	 * \brief Gets the width of the decoded video frames
	 * \return Frame width in pixels
	 */
	unsigned width() const;
	
	/**
	 * \brief Gets the height of the decoded video frames
	 * \return Frame height in pixels
	 */
	unsigned height() const;
	
	/**
	 * \brief Gets the pixel format of the decoded video frames
	 * \return The AVPixelFormat enum value
	 */
	AVPixelFormat pixel_format() const;
	
	/**
	 * \brief Gets the time base for timestamp calculations
	 * \return The time base as an AVRational
	 */
	AVRational time_base() const;
	
	/**
	 * \brief Checks if hardware acceleration is active
	 * \return true if using hardware decode, false if software fallback
	 */
	bool is_hardware_accelerated() const { return hw_device_ctx_ != nullptr; }
	
	/**
	 * \brief Gets the name of the hardware acceleration method in use
	 * \return Hardware type name (e.g., "cuda", "vaapi") or "software"
	 */
	std::string get_hw_type_name() const;
	
private:
	/**
	 * \brief Attempts to initialize hardware acceleration
	 * \param codec The video codec
	 * \param hw_type Hardware device type to try
	 * \return true if successful, false otherwise
	 */
	bool try_hardware_init(const AVCodec* codec, AVHWDeviceType hw_type);
	
	/**
	 * \brief Finds a suitable hardware decoder for the codec
	 * \param codec_id The codec ID
	 * \param hw_type The hardware type to use
	 * \return Pointer to codec or nullptr if not found
	 */
	const AVCodec* find_hw_decoder(AVCodecID codec_id, AVHWDeviceType hw_type);
	
	/**
	 * \brief Callback for getting hardware pixel format
	 */
	static enum AVPixelFormat get_hw_format(AVCodecContext* ctx, const enum AVPixelFormat* pix_fmts);
	
	AVCodecContext* codec_context_{};      ///< FFmpeg codec context
	AVBufferRef* hw_device_ctx_{};         ///< Hardware device context
	AVPixelFormat hw_pix_fmt_{AV_PIX_FMT_NONE}; ///< Hardware pixel format
	AVHWDeviceType hw_type_{AV_HWDEVICE_TYPE_NONE}; ///< Active hardware type
};
