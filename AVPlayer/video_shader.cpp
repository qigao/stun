#include "video_shader.h"
#include <cstring>

using namespace nanogui;

const char *VideoShader::vertex_shader_ = R"(
#version 330
uniform mat4 mvp;
in vec2 position;
in vec2 texcoord;
out vec2 uv;

void main() {
	gl_Position = mvp * vec4(position, 0.0, 1.0);
	uv = texcoord;
}
)";

const char *VideoShader::fragment_shader_ = R"(
#version 330
uniform sampler2D y_tex;
uniform sampler2D u_tex;
uniform sampler2D v_tex;
in vec2 uv;
out vec4 frag_color;

void main() {
	float y = texture(y_tex, uv).r;
	float u = texture(u_tex, uv).r - 0.5;
	float v = texture(v_tex, uv).r - 0.5;
	
	float r = y + 1.370705 * v;
	float g = y - 0.337633 * u - 0.698001 * v;
	float b = y + 1.732446 * u;
	
	frag_color = vec4(r, g, b, 1.0);
}
)";

VideoShader::VideoShader() {}

VideoShader::~VideoShader() {}

void VideoShader::init(RenderPass *render_pass) { create_shader(render_pass); }

void VideoShader::create_shader(RenderPass *render_pass) {
  shader_ = new Shader(render_pass, "VideoShader", vertex_shader_, fragment_shader_);

  // Set up buffer for fullscreen quad
  float positions[] = {
      -1.0f, 1.0f,  // top-left
      -1.0f, -1.0f, // bottom-left
      1.0f,  -1.0f, // bottom-right
      1.0f,  1.0f   // top-right
  };
  shader_->set_buffer("position", VariableType::Float32, {4, 2}, positions);

  float texcoords[] = {
      0.0f, 0.0f, // top-left
      0.0f, 1.0f, // bottom-left
      1.0f, 1.0f, // bottom-right
      1.0f, 0.0f  // top-right
  };
  shader_->set_buffer("texcoord", VariableType::Float32, {4, 2}, texcoords);

  // Two triangles to form a quad: (0,1,2) and (2,3,0)
  uint32_t indices[] = {0, 1, 2, 2, 3, 0};
  shader_->set_buffer("indices", VariableType::UInt32, {6}, indices);
}

void VideoShader::create_textures() {
  // Create Y texture (full resolution)
  y_texture_ =
      new Texture(Texture::PixelFormat::R, Texture::ComponentFormat::UInt8,
                  Vector2i(video_width_, video_height_), Texture::InterpolationMode::Bilinear,
                  Texture::InterpolationMode::Bilinear, Texture::WrapMode::ClampToEdge);

  // Create U texture (half resolution)
  u_texture_ = new Texture(Texture::PixelFormat::R, Texture::ComponentFormat::UInt8,
                           Vector2i(video_width_ / 2, video_height_ / 2),
                           Texture::InterpolationMode::Bilinear,
                           Texture::InterpolationMode::Bilinear, Texture::WrapMode::ClampToEdge);

  // Create V texture (half resolution)
  v_texture_ = new Texture(Texture::PixelFormat::R, Texture::ComponentFormat::UInt8,
                           Vector2i(video_width_ / 2, video_height_ / 2),
                           Texture::InterpolationMode::Bilinear,
                           Texture::InterpolationMode::Bilinear, Texture::WrapMode::ClampToEdge);

  // Allocate buffers
  y_buffer_.resize(video_width_ * video_height_);
  u_buffer_.resize((video_width_ / 2) * (video_height_ / 2));
  v_buffer_.resize((video_width_ / 2) * (video_height_ / 2));
}

void VideoShader::upload_yuv_frame(std::array<uint8_t *, 3> planes, std::array<size_t, 3> pitches,
                                   unsigned width, unsigned height) {
  if (width != video_width_ || height != video_height_) {
    video_width_ = width;
    video_height_ = height;
    create_textures();
  }

  // Copy Y plane (handle pitch)
  for (unsigned y = 0; y < height; ++y) {
    std::memcpy(y_buffer_.data() + y * width, planes[0] + y * pitches[0], width);
  }
  y_texture_->upload(y_buffer_.data());

  // Copy U plane (handle pitch)
  for (unsigned y = 0; y < height / 2; ++y) {
    std::memcpy(u_buffer_.data() + y * (width / 2), planes[1] + y * pitches[1], width / 2);
  }
  u_texture_->upload(u_buffer_.data());

  // Copy V plane (handle pitch)
  for (unsigned y = 0; y < height / 2; ++y) {
    std::memcpy(v_buffer_.data() + y * (width / 2), planes[2] + y * pitches[2], width / 2);
  }
  v_texture_->upload(v_buffer_.data());
}

void VideoShader::render(const Matrix4f &mvp, const Vector2i &viewport_size) {
  if (!shader_ || !y_texture_)
    return;

  shader_->set_uniform("mvp", mvp);
  shader_->set_texture("y_tex", y_texture_);
  shader_->set_texture("u_tex", u_texture_);
  shader_->set_texture("v_tex", v_texture_);

  shader_->begin();
  shader_->draw_array(Shader::PrimitiveType::Triangle, 0, 6, true);
  shader_->end();
}
