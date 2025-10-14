#include "player.h"
#include <nanogui.h>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <stdexcept>
#include <string>

int main(int argc, char** argv) {
	try {
		if (argc < 2) {
			std::cerr << "Usage: AVPlayer <video_file|stream_url> [options]\n\n";
			std::cerr << "Options:\n";
			std::cerr << "  --no-hw           Disable hardware acceleration (use software decode)\n";
			std::cerr << "  --filter <desc>   Apply video filter (e.g., \"hflip\", \"eq=contrast=1.5\")\n\n";
			std::cerr << "Examples:\n";
			std::cerr << "  Local files:\n";
			std::cerr << "    AVPlayer video.mp4\n";
			std::cerr << "    AVPlayer C:\\Videos\\movie.mp4\n";
			std::cerr << "    AVPlayer /home/user/videos/movie.mp4\n\n";
			std::cerr << "  HTTP/HTTPS streams:\n";
			std::cerr << "    AVPlayer http://example.com/video.mp4\n";
			std::cerr << "    AVPlayer https://example.com/stream/video.mp4\n\n";
			std::cerr << "  HLS streams (m3u8):\n";
			std::cerr << "    AVPlayer http://example.com/stream/playlist.m3u8\n";
			std::cerr << "    AVPlayer https://example.com/live/stream.m3u8\n\n";
			std::cerr << "  RTSP streams (cameras, live feeds):\n";
			std::cerr << "    AVPlayer rtsp://example.com:554/stream\n";
			std::cerr << "    AVPlayer rtsp://username:password@camera.local/stream1\n\n";
			std::cerr << "  RTMP streams:\n";
			std::cerr << "    AVPlayer rtmp://example.com/live/stream\n\n";
			std::cerr << "  UDP/RTP streams:\n";
			std::cerr << "    AVPlayer udp://@:1234\n";
			std::cerr << "    AVPlayer rtp://239.255.0.1:5004\n\n";
			std::cerr << "  With options:\n";
			std::cerr << "    AVPlayer stream.m3u8 --no-hw\n";
			std::cerr << "    AVPlayer rtsp://camera.local/stream --filter \"eq=contrast=1.5\"\n\n";
			std::cerr << "Keyboard controls:\n";
			std::cerr << "  SPACE - Pause/Resume\n";
			std::cerr << "  ESC   - Quit\n";
			return 1;
		}

		// Parse command-line arguments
		std::string file_path = argv[1];
		bool use_hw_decode = true;
		std::string filter_desc = "";
		
		for (int i = 2; i < argc; i++) {
			std::string arg = argv[i];
			if (arg == "--no-hw") {
				use_hw_decode = false;
			} else if (arg == "--filter" && i + 1 < argc) {
				filter_desc = argv[++i];
			}
		}
		
		// Check if it's a URL or local file
		bool is_url = (file_path.find("://") != std::string::npos);
		
		// Only check file existence for local files
		if (!is_url) {
			std::ifstream file_check(file_path);
			if (!file_check.good()) {
				std::cerr << "Error: File not found: " << file_path << "\n\n";
				std::cerr << "Please provide a valid path to a video file.\n";
				std::cerr << "Current working directory: " << std::filesystem::current_path() << "\n";
				return 1;
			}
			file_check.close();
		} else {
			std::cout << "Opening stream: " << file_path << "\n";
		}

		// Initialize NanoGUI with command-line arguments for SDL
		nanogui::init();

		std::cout << "\n=== AVPlayer Configuration ===\n";
		std::cout << "File: " << file_path << "\n";
		std::cout << "Hardware decode: " << (use_hw_decode ? "enabled" : "disabled") << "\n";
		if (!filter_desc.empty()) {
			std::cout << "Video filter: " << filter_desc << "\n";
		}
		std::cout << "==============================\n\n";

		Player play{file_path, use_hw_decode, filter_desc};
		
		std::cout << "\n=== Playback Info ===\n";
		std::cout << "Decoder: " << play.get_hw_type() << "\n";
		std::cout << "Hardware accelerated: " << (play.is_hardware_accelerated() ? "yes" : "no") << "\n";
		std::cout << "=====================\n\n";
		
		play();
		
		nanogui::shutdown();
	}

	catch (const std::exception &e) {
		std::cerr << "Error: " << e.what() << std::endl;
		nanogui::shutdown();
		return -1;
	}

	return 0;
}
