#include "stream_handler.h"
#include <algorithm>
#include <cctype>
#include <iostream>

StreamType StreamHandler::detect_stream_type(const std::string& url) {
	std::string lower_url = url;
	std::transform(lower_url.begin(), lower_url.end(), lower_url.begin(), ::tolower);
	
	// Check protocol prefix
	if (lower_url.find("http://") == 0 || lower_url.find("https://") == 0) {
		// Check for HLS
		if (lower_url.find(".m3u8") != std::string::npos || 
		    lower_url.find(".m3u") != std::string::npos) {
			return StreamType::HLS;
		}
		// Check for DASH
		if (lower_url.find(".mpd") != std::string::npos) {
			return StreamType::DASH;
		}
		return StreamType::HTTP;
	}
	
	if (lower_url.find("rtsp://") == 0) {
		return StreamType::RTSP;
	}
	
	if (lower_url.find("rtmp://") == 0 || lower_url.find("rtmps://") == 0) {
		return StreamType::RTMP;
	}
	
	if (lower_url.find("udp://") == 0) {
		return StreamType::UDP;
	}
	
	if (lower_url.find("tcp://") == 0) {
		return StreamType::TCP;
	}
	
	if (lower_url.find("rtp://") == 0) {
		return StreamType::RTP;
	}
	
	if (lower_url.find("file://") == 0) {
		return StreamType::LOCAL_FILE;
	}
	
	// If no protocol, assume local file
	if (lower_url.find("://") == std::string::npos) {
		return StreamType::LOCAL_FILE;
	}
	
	return StreamType::UNKNOWN;
}

StreamInfo StreamHandler::get_stream_info(const std::string& url) {
	StreamInfo info;
	info.url = url;
	info.type = detect_stream_type(url);
	info.protocol = get_protocol(url);
	info.is_live = is_live_stream(info.type);
	info.supports_seeking = !info.is_live;
	info.timeout_ms = get_recommended_timeout(info.type);
	
	return info;
}

bool StreamHandler::is_valid_stream(const std::string& url) {
	if (url.empty()) {
		return false;
	}
	
	StreamType type = detect_stream_type(url);
	return type != StreamType::UNKNOWN;
}

std::string StreamHandler::get_protocol(const std::string& url) {
	size_t pos = url.find("://");
	if (pos != std::string::npos) {
		return url.substr(0, pos);
	}
	return "file";
}

bool StreamHandler::is_live_stream(StreamType type) {
	switch (type) {
		case StreamType::RTSP:
		case StreamType::RTMP:
		case StreamType::UDP:
		case StreamType::TCP:
		case StreamType::RTP:
			return true;
		case StreamType::HLS:
		case StreamType::DASH:
			// Can be live or VOD, assume live by default
			return true;
		case StreamType::HTTP:
		case StreamType::LOCAL_FILE:
		default:
			return false;
	}
}

int StreamHandler::get_recommended_timeout(StreamType type) {
	switch (type) {
		case StreamType::RTSP:
		case StreamType::RTMP:
			return 10000;  // 10 seconds
		case StreamType::HTTP:
		case StreamType::HLS:
		case StreamType::DASH:
			return 15000;  // 15 seconds
		case StreamType::UDP:
		case StreamType::TCP:
		case StreamType::RTP:
			return 5000;   // 5 seconds
		case StreamType::LOCAL_FILE:
		default:
			return 0;      // No timeout
	}
}

std::string StreamHandler::format_url_for_ffmpeg(const std::string& url) {
	StreamType type = detect_stream_type(url);
	
	// For most streams, return as-is
	// FFmpeg handles protocol-specific options internally
	return url;
}

std::vector<std::string> StreamHandler::get_supported_protocols() {
	return {
		"file",    // Local files
		"http",    // HTTP streams
		"https",   // HTTPS streams
		"rtsp",    // RTSP streams
		"rtmp",    // RTMP streams
		"rtmps",   // RTMP over TLS
		"udp",     // UDP streams
		"tcp",     // TCP streams
		"rtp",     // RTP streams
		"hls",     // HLS (m3u8)
		"dash"     // DASH (mpd)
	};
}

std::vector<std::string> StreamHandler::get_example_urls() {
	return {
		// Local file
		"C:\\Videos\\movie.mp4",
		"/home/user/videos/movie.mp4",
		"file:///path/to/video.mp4",
		
		// HTTP/HTTPS
		"http://example.com/video.mp4",
		"https://example.com/stream/video.mp4",
		
		// HLS
		"http://example.com/stream/playlist.m3u8",
		"https://example.com/live/stream.m3u8",
		
		// DASH
		"http://example.com/stream/manifest.mpd",
		
		// RTSP
		"rtsp://example.com:554/stream",
		"rtsp://username:password@camera.local/stream1",
		
		// RTMP
		"rtmp://example.com/live/stream",
		"rtmps://example.com/live/stream",
		
		// UDP
		"udp://@:1234",
		"udp://239.255.0.1:1234",
		
		// RTP
		"rtp://239.255.0.1:5004"
	};
}
