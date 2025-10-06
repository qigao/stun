/**
 * \file video_shader.h
 * \brief Custom shader for GPU-accelerated YUV to RGB video rendering
 */

#pragma once
#include <nanogui/nanogui.h>
#include <nanogui/texture.h>
#include <nanogui/shader.h>
#include <nanogui/renderpass.h>
#include <array>
#include <string>
#include <vector>

/**
 * \class VideoShader
 * \brief Custom shader for rendering YUV video frames with GPU conversion to RGB
 * 
 * This class manages GPU textures and shaders to efficiently convert YUV video
 * frames to RGB on the GPU and render them to the screen.
 */
class VideoShader {
public:
	/**
	 * \brief Constructs a video shader
	 */
	VideoShader();
	
	/**
	 * \brief Destructor - cleans up shader and texture resources
	 */
	~VideoShader();
	
	/**
	 * \brief Initializes the shader with the specified render pass
	 * \param render_pass The NanoGUI render pass to use
	 */
	void init(nanogui::RenderPass* render_pass);
	
	/**
	 * \brief Uploads a YUV frame to GPU textures
	 * \param planes Array of pointers to Y, U, and V plane data
	 * \param pitches Array of pitch (stride) values for each plane
	 * \param width Frame width in pixels
	 * \param height Frame height in pixels
	 */
	void upload_yuv_frame(std::array<uint8_t*, 3> planes, std::array<size_t, 3> pitches, 
	                      unsigned width, unsigned height);
	
	/**
	 * \brief Renders the video frame to the screen
	 * \param mvp Model-view-projection matrix for rendering
	 * \param viewport_size Size of the viewport in pixels
	 */
	void render(const nanogui::Matrix4f& mvp, const nanogui::Vector2i& viewport_size);
	
	/**
	 * \brief Gets the width of the current video frame
	 * \return Frame width in pixels
	 */
	unsigned video_width() const { return video_width_; }
	
	/**
	 * \brief Gets the height of the current video frame
	 * \return Frame height in pixels
	 */
	unsigned video_height() const { return video_height_; }

private:
	/**
	 * \brief Creates and compiles the YUV to RGB shader
	 * \param render_pass The render pass to create the shader for
	 */
	void create_shader(nanogui::RenderPass* render_pass);
	
	/**
	 * \brief Creates GPU textures for Y, U, and V planes
	 */
	void create_textures();
	
	nanogui::ref<nanogui::Shader> shader_;      ///< The YUV to RGB shader
	nanogui::ref<nanogui::Texture> y_texture_;  ///< Texture for Y (luminance) plane
	nanogui::ref<nanogui::Texture> u_texture_;  ///< Texture for U (chrominance) plane
	nanogui::ref<nanogui::Texture> v_texture_;  ///< Texture for V (chrominance) plane
	
	std::vector<uint8_t> y_buffer_;             ///< CPU buffer for Y plane
	std::vector<uint8_t> u_buffer_;             ///< CPU buffer for U plane
	std::vector<uint8_t> v_buffer_;             ///< CPU buffer for V plane
	
	unsigned video_width_{0};                   ///< Current frame width
	unsigned video_height_{0};                  ///< Current frame height
	
	static const char* vertex_shader_;          ///< Vertex shader source code
	static const char* fragment_shader_;        ///< Fragment shader source code
};
