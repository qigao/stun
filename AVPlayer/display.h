/**
 * \file display.h
 * \brief Video display functionality using NanoGUI with CPU-based YUV to RGB conversion
 */

#pragma once
#include <nanogui/nanogui.h>
#include <nanogui/imageview.h>
#include <array>
#include <memory>
#include <vector>

/**
 * \class VideoScreen
 * \brief NanoGUI screen for displaying video frames
 * 
 * This class extends nanogui::Screen to display video frames. It converts
 * YUV frames to RGB on the CPU and displays them using an ImageView widget.
 */
class VideoScreen : public nanogui::Screen {
public:
	/**
	 * \brief Constructs a video screen with the specified dimensions
	 * \param width Video frame width in pixels
	 * \param height Video frame height in pixels
	 */
	VideoScreen(const unsigned width, const unsigned height);
	
	/**
	 * \brief Updates the displayed frame with new YUV data
	 * \param planes Array of pointers to Y, U, and V plane data
	 * \param pitches Array of pitch (stride) values for each plane
	 */
	void update_frame(std::array<uint8_t*, 3> planes, std::array<size_t, 3> pitches);
	
	/**
	 * \brief Checks if the user has requested to quit
	 * \return true if quit was requested, false otherwise
	 */
	bool get_quit() const { return quit_; }
	
	/**
	 * \brief Checks if playback should continue
	 * \return true if playing, false if paused
	 */
	bool get_play() const { return play_; }
	
	/**
	 * \brief Handles keyboard events for playback control
	 * \param key The key code
	 * \param scancode The platform-specific scancode
	 * \param action The key action (press, release, repeat)
	 * \param modifiers Modifier keys held during the event
	 * \return true if the event was handled
	 */
	virtual bool keyboard_event(int key, int scancode, int action, int modifiers) override;

private:
	/**
	 * \brief Converts YUV frame data to RGB format
	 * \param planes Array of pointers to Y, U, and V plane data
	 * \param pitches Array of pitch (stride) values for each plane
	 */
	void yuv_to_rgb(std::array<uint8_t*, 3> planes, std::array<size_t, 3> pitches);
	
	nanogui::ImageView* image_view_;           ///< Widget for displaying the video
	nanogui::ref<nanogui::Texture> texture_;   ///< GPU texture for the frame
	std::vector<uint8_t> rgb_buffer_;          ///< Buffer for RGB conversion
	unsigned width_;                           ///< Frame width
	unsigned height_;                          ///< Frame height
	bool quit_{false};                         ///< Quit flag
	bool play_{true};                          ///< Play/pause state
};

/**
 * \class Display
 * \brief High-level interface for video display
 * 
 * This class provides a simple interface for displaying video frames,
 * managing the underlying VideoScreen and event handling.
 */
class Display {
private:
	bool quit_{false};                         ///< Quit flag
	bool play_{true};                          ///< Play/pause state
	std::unique_ptr<VideoScreen> screen_;      ///< The video screen

public:
	/**
	 * \brief Constructs a display with the specified dimensions
	 * \param width Video frame width in pixels
	 * \param height Video frame height in pixels
	 */
	Display(const unsigned width, const unsigned height);

	/**
	 * \brief Updates the display with a new video frame
	 * \param planes Array of pointers to Y, U, and V plane data
	 * \param pitches Array of pitch (stride) values for each plane
	 */
	void refresh(std::array<uint8_t*, 3> planes, std::array<size_t, 3> pitches);

	/**
	 * \brief Processes input events
	 */
	void input();

	/**
	 * \brief Checks if the user has requested to quit
	 * \return true if quit was requested, false otherwise
	 */
	bool get_quit();
	
	/**
	 * \brief Checks if playback should continue
	 * \return true if playing, false if paused
	 */
	bool get_play();
};
