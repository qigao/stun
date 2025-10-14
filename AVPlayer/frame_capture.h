/**
 * \file frame_capture.h
 * \brief Frame capture and screenshot functionality
 */

#pragma once
extern "C" {
	#include <libavutil/frame.h>
	#include <libavutil/pixfmt.h>
	#include <libavcodec/avcodec.h>
}
#include <string>
#include <vector>

/**
 * \enum CaptureFormat
 * \brief Output format for captured frames
 */
enum class CaptureFormat {
	RGB24,          ///< RGB 24-bit (8 bits per channel)
	RGBA,           ///< RGBA 32-bit (8 bits per channel + alpha)
	YUV420P,        ///< YUV 4:2:0 planar
	YUV444P,        ///< YUV 4:4:4 planar (full chroma)
	PNG,            ///< PNG image file
	JPEG,           ///< JPEG image file
	BMP,            ///< BMP image file
	TIFF            ///< TIFF image file
};

/**
 * \struct CapturedFrame
 * \brief Information about a captured frame
 */
struct CapturedFrame {
	std::vector<uint8_t> data;     ///< Frame data
	int width;                      ///< Frame width
	int height;                     ///< Frame height
	CaptureFormat format;           ///< Data format
	int64_t timestamp;              ///< Timestamp in microseconds
	std::string filename;           ///< Saved filename (if saved)
	size_t data_size;               ///< Size of data in bytes
};

/**
 * \class FrameCapture
 * \brief Captures video frames in various formats
 */
class FrameCapture {
public:
	/**
	 * \brief Captures a frame in specified format
	 * \param frame AVFrame to capture
	 * \param format Output format
	 * \return Captured frame data
	 */
	static CapturedFrame capture_frame(AVFrame* frame, CaptureFormat format);
	
	/**
	 * \brief Saves captured frame to file
	 * \param captured Captured frame data
	 * \param filename Output filename
	 * \return true if successful
	 */
	static bool save_to_file(const CapturedFrame& captured, const std::string& filename);
	
	/**
	 * \brief Captures and saves frame in one operation
	 * \param frame AVFrame to capture
	 * \param filename Output filename (format detected from extension)
	 * \return true if successful
	 */
	static bool capture_and_save(AVFrame* frame, const std::string& filename);
	
	/**
	 * \brief Generates automatic filename with timestamp
	 * \param prefix Filename prefix (e.g., "screenshot")
	 * \param format Output format
	 * \return Generated filename
	 */
	static std::string generate_filename(const std::string& prefix, CaptureFormat format);
	
	/**
	 * \brief Gets file extension for format
	 * \param format Capture format
	 * \return File extension (e.g., ".png", ".jpg")
	 */
	static std::string get_extension(CaptureFormat format);
	
	/**
	 * \brief Detects format from filename extension
	 * \param filename Filename with extension
	 * \return Detected format or PNG as default
	 */
	static CaptureFormat detect_format_from_filename(const std::string& filename);
	
	/**
	 * \brief Converts AVFrame to RGB24
	 * \param frame Input frame
	 * \return RGB24 data
	 */
	static std::vector<uint8_t> frame_to_rgb24(AVFrame* frame);
	
	/**
	 * \brief Converts AVFrame to RGBA
	 * \param frame Input frame
	 * \return RGBA data
	 */
	static std::vector<uint8_t> frame_to_rgba(AVFrame* frame);
	
	/**
	 * \brief Converts AVFrame to YUV420P
	 * \param frame Input frame
	 * \return YUV420P data
	 */
	static std::vector<uint8_t> frame_to_yuv420p(AVFrame* frame);
	
	/**
	 * \brief Converts AVFrame to YUV444P
	 * \param frame Input frame
	 * \return YUV444P data
	 */
	static std::vector<uint8_t> frame_to_yuv444p(AVFrame* frame);
	
	/**
	 * \brief Encodes frame to PNG
	 * \param frame Input frame
	 * \return PNG file data
	 */
	static std::vector<uint8_t> encode_png(AVFrame* frame);
	
	/**
	 * \brief Encodes frame to JPEG
	 * \param frame Input frame
	 * \param quality JPEG quality (1-100)
	 * \return JPEG file data
	 */
	static std::vector<uint8_t> encode_jpeg(AVFrame* frame, int quality = 95);
	
	/**
	 * \brief Encodes frame to BMP
	 * \param frame Input frame
	 * \return BMP file data
	 */
	static std::vector<uint8_t> encode_bmp(AVFrame* frame);

private:
	/**
	 * \brief Converts frame to specified pixel format
	 * \param frame Input frame
	 * \param target_format Target pixel format
	 * \return Converted frame data
	 */
	static std::vector<uint8_t> convert_frame(AVFrame* frame, AVPixelFormat target_format);
	
	/**
	 * \brief Encodes frame to image format
	 * \param frame Input frame
	 * \param codec_id Codec ID (AV_CODEC_ID_PNG, etc.)
	 * \param quality Quality parameter (for JPEG)
	 * \return Encoded image data
	 */
	static std::vector<uint8_t> encode_image(AVFrame* frame, AVCodecID codec_id, int quality = 0);
};
