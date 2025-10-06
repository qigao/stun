#include "player.h"
#include <nanogui/nanogui.h>
#include <iostream>
#include <stdexcept>
#include <string>

int main(int argc, char** argv) {
	try {
		if (argc != 2) {
			throw std::logic_error{"Not enough arguments. Usage: AVPlayer <video_file>"};
		}

		Player play{argv[1]};
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
