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
#include <chrono>
#include "audio_player.h"

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
	 * \brief Handles mouse button events (for context menu)
	 * \param p Mouse position
	 * \param button Button index
	 * \param down True if pressed, false if released
	 * \param modifiers Modifier keys
	 * \return true if the event was handled
	 */
	virtual bool mouse_button_event(const nanogui::Vector2i &p, int button, bool down, int modifiers) override;
	
	/**
	 * \brief Handles mouse motion for cursor change
	 * \param p Mouse position
	 * \param rel Relative mouse movement
	 * \param button Current button state
	 * \param modifiers Modifier keys
	 * \return true if the event was handled
	 */
	virtual bool mouse_motion_event(const nanogui::Vector2i &p, const nanogui::Vector2i &rel, int button, int modifiers) override;
	
	/**
	 * \brief Handles window resize events
	 * \param size New window size
	 * \return true if the event was handled
	 */
	virtual bool resize_event(const nanogui::Vector2i& size) override;
	
	/**
	 * \brief Toggle fullscreen mode
	 */
	void toggle_fullscreen();
	
	/**
	 * \brief Get fullscreen state
	 */
	bool is_fullscreen() const { return fullscreen_; }
	
	/**
	 * \brief Sets the audio player reference for volume control
	 */
	void set_audio_player(class AudioPlayer* player) { audio_player_ = player; }
	
	/**
	 * \brief Sets the volume level
	 */
	void set_volume(float volume);
	
	/**
	 * \brief Gets the current volume level
	 */
	float get_volume() const { return volume_; }
	
	/**
	 * \brief Adjusts volume by a delta
	 */
	void adjust_volume(float delta);
	
	/**
	 * \brief Toggles mute state
	 */
	void toggle_mute();
	
	/**
	 * \brief Gets mute state
	 */
	bool is_muted() const { return muted_; }
	
	/**
	 * \brief Shows an OSD message
	 */
	void show_osd(const std::string &message, float duration = 2.0f);
	
	/**
	 * \brief Updates OSD timer
	 */
	void update_osd();
	
	/**
	 * \brief Draws OSD overlay
	 */
	void draw_osd(NVGcontext* ctx);
	
	/**
	 * \brief Override draw to add OSD and progress bar
	 */
	virtual void draw(NVGcontext *ctx) override;
	
	/**
	 * \brief Sets playback duration and position for progress bar
	 */
	void set_progress(double position, double duration);
	
	/**
	 * \brief Draws progress bar at bottom of screen
	 */
	void draw_progress_bar(NVGcontext* ctx);
	
	/**
	 * \brief Sets the player reference for seeking
	 */
	void set_player(class Player* player) { player_ = player; }
	
	/**
	 * \brief Shows OSD with filter name
	 */
	void show_filter_osd(const std::string& filter_name);
	
	/**
	 * \brief Checks if mouse is over progress bar
	 */
	bool is_mouse_over_progress_bar(const nanogui::Vector2i &p) const;
	
	/**
	 * \brief Sets playback speed
	 */
	void set_playback_speed(double speed);
	
	/**
	 * \brief Gets current playback speed
	 */
	double get_playback_speed() const { return playback_speed_; }
	
	/**
	 * \brief Captures a screenshot of the current frame
	 */
	void capture_screenshot();
	
	/**
	 * \brief Opens file dialog to load external subtitle
	 */
	void load_external_subtitle();
	
	/**
	 * \brief Shows subtitle font size adjustment dialog
	 */
	void show_subtitle_font_dialog();
	
	/**
	 * \brief Shows batch frame extraction dialog
	 */
	void show_batch_extract_dialog();

private:
	/**
	 * \brief Creates and shows the context menu
	 * \param pos Position to show the menu
	 */
	void show_context_menu(const nanogui::Vector2i &pos);
	
private:
	VideoCanvas* canvas_;       ///< Canvas for rendering video
	class AudioPlayer* audio_player_{nullptr};  ///< Audio player reference
	class Player* player_{nullptr};  ///< Player reference for seeking
	unsigned video_width_;      ///< Video frame width
	unsigned video_height_;     ///< Video frame height
	bool quit_{false};          ///< Quit flag
	bool play_{true};           ///< Play/pause state
	bool fullscreen_{false};    ///< Fullscreen state
	nanogui::Vector2i windowed_pos_;   ///< Windowed position
	nanogui::Vector2i windowed_size_;  ///< Windowed size
	float volume_{1.0f};        ///< Volume level (0.0 to 1.0)
	bool muted_{false};         ///< Mute state
	
	// OSD (On-Screen Display)
	std::string osd_message_;   ///< Current OSD message
	float osd_timer_{0.0f};     ///< OSD display timer
	std::chrono::steady_clock::time_point osd_start_time_;  ///< OSD start time
	
	// Progress bar
	double playback_position_{0.0};  ///< Current playback position in seconds
	double playback_duration_{0.0};  ///< Total video duration in seconds
	double playback_speed_{1.0};     ///< Playback speed multiplier
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
	 * \brief Redraws the screen (used when paused to keep UI responsive)
	 */
	void redraw();
	
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
	
	/**
	 * \brief Sets the audio player reference for volume control
	 */
	void set_audio_player(class AudioPlayer* player) {
		if (screen_) {
			screen_->set_audio_player(player);
		}
	}
	
	/**
	 * \brief Updates progress bar with current position and duration
	 */
	void set_progress(double position, double duration) {
		if (screen_) {
			screen_->set_progress(position, duration);
		}
	}
	
	/**
	 * \brief Sets the player reference for seeking
	 */
	void set_player(class Player* player) {
		if (screen_) {
			screen_->set_player(player);
		}
	}
	
	/**
	 * \brief Shows an OSD message
	 */
	void show_osd(const std::string& message, float duration = 2.0f) {
		if (screen_) {
			screen_->show_osd(message, duration);
		}
	}
};
