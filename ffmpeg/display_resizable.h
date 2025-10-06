/**
 * \file display_resizable.h
 * \brief Resizable video display using GPU-accelerated YUV to RGB conversion
 */

#pragma once
#include <nanogui/nanogui.h>
#include <nanogui/canvas.h>
#include "video_shader.h"
#include <array>
#include <memory>

/**
 * \class VideoCanvas
 * \brief Canvas widget for rendering video frames using custom shaders
 * 
 * This class extends nanogui::Canvas to render video frames using a custom
 * shader that performs YUV to RGB conversion on the GPU.
 */
class VideoCanvas : public nanogui::Canvas {
public:
	/**
	 * \brief Constructs a video canvas with the specified dimensions
	 * \param parent Parent widget
	 * \param video_width Video frame width in pixels
	 * \param video_height Video frame height in pixels
	 */
	VideoCanvas(nanogui::Widget *parent, unsigned video_width, unsigned video_height);
	
	/**
	 * \brief Updates the displayed frame with new YUV data
	 * \param planes Array of pointers to Y, U, and V plane data
	 * \param pitches Array of pitch (stride) values for each plane
	 */
	void update_frame(std::array<uint8_t*, 3> planes, std::array<size_t, 3> pitches);
	
	/**
	 * \brief Sets the canvas size for rendering
	 * \param new_size New canvas dimensions
	 */
	void set_canvas_size(const nanogui::Vector2i& new_size);
	
	/**
	 * \brief Draws the video frame contents
	 */
	virtual void draw_contents() override;

private:
	std::unique_ptr<VideoShader> video_shader_;  ///< Custom shader for YUV to RGB conversion
	unsigned video_width_;                       ///< Video frame width
	unsigned video_height_;                      ///< Video frame height
};

/**
 * \class ResizableVideoScreen
 * \brief Resizable NanoGUI screen for displaying video frames
 * 
 * This class extends nanogui::Screen to provide a resizable window for
 * video playback with GPU-accelerated rendering.
 */
class ResizableVideoScreen : public nanogui::Screen {
public:
	/**
	 * \brief Constructs a resizable video screen with the specified dimensions
	 * \param video_width Video frame width in pixels
	 * \param video_height Video frame height in pixels
	 */
	ResizableVideoScreen(const unsigned video_width, const unsigned video_height);
	
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
	
	/**
	 * \brief Handles window resize events
	 * \param size New window size
	 * \return true if the event was handled
	 */
	virtual bool resize_event(const nanogui::Vector2i& size) override;

private:
	VideoCanvas* canvas_;       ///< Canvas for rendering video
	unsigned video_width_;      ///< Video frame width
	unsigned video_height_;     ///< Video frame height
	bool quit_{false};          ///< Quit flag
	bool play_{true};           ///< Play/pause state
};

/**
 * \class ResizableDisplay
 * \brief High-level interface for resizable video display
 * 
 * This class provides a simple interface for displaying video frames in a
 * resizable window with GPU-accelerated rendering.
 */
class ResizableDisplay {
private:
	bool quit_{false};                                ///< Quit flag
	bool play_{true};                                 ///< Play/pause state
	std::unique_ptr<ResizableVideoScreen> screen_;    ///< The resizable video screen

public:
	/**
	 * \brief Constructs a resizable display with the specified dimensions
	 * \param width Video frame width in pixels
	 * \param height Video frame height in pixels
	 */
	ResizableDisplay(const unsigned width, const unsigned height);

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
