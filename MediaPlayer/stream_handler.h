/**
 * \file stream_handler.h
 * \brief Stream protocol handler for various input sources
 */

#pragma once
#include <string>
#include <vector>

/**
 * \enum StreamType
 * \brief Types of supported streams
 */
enum class StreamType {
	LOCAL_FILE,      ///< Local file (file://)
	HTTP,            ///< HTTP/HTTPS stream
	RTSP,            ///< RTSP stream
	RTMP,            ///< RTMP stream
	UDP,             ///< UDP stream
	TCP,             ///< TCP stream
	RTP,             ///< RTP stream
	HLS,             ///< HLS (m3u8) stream
	DASH,            ///< DASH (mpd) stream
	UNKNOWN          ///< Unknown/unsupported
};

/**
 * \struct StreamInfo
 * \brief Information about a stream source
 */
struct StreamInfo {
	std::string url;              ///< Full URL/path
	StreamType type;              ///< Stream type
	bool is_live;                 ///< Whether stream is live
	bool supports_seeking;        ///< Whether seeking is supported
	std::string protocol;         ///< Protocol name (http, rtsp, etc.)
	int timeout_ms;               ///< Connection timeout in milliseconds
};

/**
 * \class StreamHandler
 * \brief Handles various stream protocols and input sources
 */
class StreamHandler {
public:
	/**
	 * \brief Detects stream type from URL
	 * \param url Input URL or file path
	 * \return Stream type
	 */
	static StreamType detect_stream_type(const std::string& url);
	
	/**
	 * \brief Gets stream information
	 * \param url Input URL or file path
	 * \return Stream information structure
	 */
	static StreamInfo get_stream_info(const std::string& url);
	
	/**
	 * \brief Checks if URL is valid and accessible
	 * \param url Input URL or file path
	 * \return true if valid and accessible
	 */
	static bool is_valid_stream(const std::string& url);
	
	/**
	 * \brief Gets protocol name from URL
	 * \param url Input URL
	 * \return Protocol name (http, rtsp, file, etc.)
	 */
	static std::string get_protocol(const std::string& url);
	
	/**
	 * \brief Checks if stream is live (non-seekable)
	 * \param type Stream type
	 * \return true if live stream
	 */
	static bool is_live_stream(StreamType type);
	
	/**
	 * \brief Gets recommended timeout for stream type
	 * \param type Stream type
	 * \return Timeout in milliseconds
	 */
	static int get_recommended_timeout(StreamType type);
	
	/**
	 * \brief Formats URL for FFmpeg
	 * \param url Input URL
	 * \return Formatted URL with options
	 */
	static std::string format_url_for_ffmpeg(const std::string& url);
	
	/**
	 * \brief Gets list of supported protocols
	 * \return Vector of protocol names
	 */
	static std::vector<std::string> get_supported_protocols();
	
	/**
	 * \brief Gets example URLs for each protocol
	 * \return Vector of example URLs
	 */
	static std::vector<std::string> get_example_urls();
};
