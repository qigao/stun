/*
    examples/example_camera.cpp -- Camera capture and display example

    This example demonstrates how to capture video from a camera using SDL3
    and display it in a NanoGUI window using the ImageView widget.

    Based on SDL's camera example:
    https://github.com/libsdl-org/SDL/blob/main/examples/camera/01-read-and-draw/read-and-draw.c
*/

#include <iostream>
#include <nanogui/button.h>
#include <nanogui/combobox.h>
#include <nanogui/imageview.h>
#include <nanogui/label.h>
#include <nanogui/layout.h>
#include <nanogui/screen.h>
#include <nanogui/texture.h>
#include <nanogui/window.h>

#if defined(NANOGUI_USE_SDL3)
  #include <SDL3/SDL.h>
#else
  #error "This example requires SDL3 backend with camera support"
#endif

using namespace nanogui;

class CameraApp : public Screen {
public:
  CameraApp() : Screen(Vector2i(1280, 720), "Camera Capture Demo") {
    inc_ref();

    // Create main window
    Window *window = new Window(this, "Camera Feed");
    window->set_position(Vector2i(15, 15));
    window->set_layout(new GroupLayout());

    // Status label
    m_status_label = new Label(window, "Initializing camera...", "sans-bold");

    // Camera selection
    new Label(window, "Select Camera:", "sans");
    m_camera_combo = new ComboBox(window, {});
    m_camera_combo->set_callback([this](int index) { select_camera(index); });

    // Resolution selection
    new Label(window, "Resolution:", "sans");
    m_resolution_combo = new ComboBox(window, {"Auto (First Available)"});
    m_resolution_combo->set_callback([this](int index) { m_selected_resolution_index = index; });

    // Control buttons
    Widget *controls = new Widget(window);
    controls->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 6));

    m_start_button = new Button(controls, "Start");
    m_start_button->set_callback([this]() { start_camera(); });

    m_stop_button = new Button(controls, "Stop");
    m_stop_button->set_callback([this]() { stop_camera(); });
    m_stop_button->set_enabled(false);

    Button *refresh_button = new Button(controls, "Refresh");
    refresh_button->set_callback([this]() {
      std::cout << "\nRefreshing camera list..." << std::endl;
      enumerate_cameras();
    });

    // Image view for camera feed
    m_image_view = new ImageView(window);
    m_image_view->set_fixed_size(Vector2i(640, 480));

    // Scale controls
    new Label(window, "Display Scale:", "sans");
    Widget *scale_controls = new Widget(window);
    scale_controls->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 6));

    Button *scale_50 = new Button(scale_controls, "50%");
    scale_50->set_callback([this]() { set_display_scale(0.5f); });

    Button *scale_100 = new Button(scale_controls, "100%");
    scale_100->set_callback([this]() { set_display_scale(1.0f); });

    Button *scale_150 = new Button(scale_controls, "150%");
    scale_150->set_callback([this]() { set_display_scale(1.5f); });

    Button *scale_fit = new Button(scale_controls, "Fit");
    scale_fit->set_callback([this]() { set_display_scale(0.0f); }); // 0 = auto-fit

    perform_layout();

    // Initialize camera system
    std::cout << "Initializing camera subsystem..." << std::endl;
    std::cout << "SDL Version: " << SDL_GetRevision() << std::endl;
    enumerate_cameras();
  }

  virtual ~CameraApp() {
    stop_camera();
    if (m_texture) {
      delete m_texture;
      m_texture = nullptr;
    }
  }

  void enumerate_cameras() {
    int count = 0;
    SDL_CameraID *cameras = SDL_GetCameras(&count);

    std::cout << "SDL_GetCameras returned: " << count << " cameras" << std::endl;
    
    const char *sdl_error = SDL_GetError();
    if (sdl_error && strlen(sdl_error) > 0) {
      std::cout << "SDL Error: " << sdl_error << std::endl;
      SDL_ClearError();
    }

    if (!cameras || count == 0) {
      m_status_label->set_caption("No cameras found! Check connections and permissions.");
      std::cerr << "\n⚠️  No cameras detected!" << std::endl;
      std::cerr << "\nPossible causes:" << std::endl;
      std::cerr << "  1. No camera/webcam is connected" << std::endl;
      std::cerr << "  2. Camera permissions not granted" << std::endl;
      std::cerr << "  3. Camera is in use by another application" << std::endl;
      std::cerr << "  4. SDL3 camera support not available" << std::endl;
      std::cerr << "\nWindows troubleshooting:" << std::endl;
      std::cerr << "  - Settings > Privacy & Security > Camera" << std::endl;
      std::cerr << "  - Enable 'Let apps access your camera'" << std::endl;
      std::cerr << "  - Enable 'Let desktop apps access your camera'" << std::endl;
      std::cerr << "  - Close Zoom, Teams, Skype, or browser tabs using camera" << std::endl;
      std::cerr << "  - Check Device Manager > Cameras" << std::endl;
      std::cerr << "\nAfter fixing, click the 'Refresh' button to rescan." << std::endl;
      
      // Disable controls
      m_camera_combo->set_enabled(false);
      m_start_button->set_enabled(false);
      return;
    }

    std::vector<std::string> camera_names;
    m_camera_ids.clear();

    std::cout << "\nAvailable cameras:" << std::endl;
    for (int i = 0; i < count; i++) {
      const char *name = SDL_GetCameraName(cameras[i]);
      std::string camera_name = name ? name : "Unknown Camera";
      camera_names.push_back(camera_name);
      m_camera_ids.push_back(cameras[i]);
      std::cout << "  [" << i << "] " << camera_name << " (ID: " << cameras[i] << ")" << std::endl;
    }

    SDL_free(cameras);

    m_camera_combo->set_items(camera_names);
    if (!camera_names.empty()) {
      m_camera_combo->set_selected_index(0);
      m_selected_camera_index = 0;
      m_status_label->set_caption("Ready - Select a camera and click Start");
      
      // Populate resolutions for the first camera
      select_camera(0);
    }

    perform_layout();
  }

  void select_camera(int index) {
    if (m_camera) {
      stop_camera();
    }
    m_selected_camera_index = index;

    // Update available resolutions for this camera
    if (index >= 0 && index < (int)m_camera_ids.size()) {
      SDL_CameraID camera_id = m_camera_ids[index];
      int num_specs = 0;
      SDL_CameraSpec **specs = SDL_GetCameraSupportedFormats(camera_id, &num_specs);

      std::vector<std::string> resolution_names;
      resolution_names.push_back("Auto (First Available)");
      m_available_specs.clear();

      if (specs && num_specs > 0) {
        std::cout << "Available resolutions for this camera:" << std::endl;
        for (int i = 0; i < num_specs; i++) {
          char buf[128];
          snprintf(buf, sizeof(buf), "%dx%d @ %s", specs[i]->width, specs[i]->height,
                   SDL_GetPixelFormatName(specs[i]->format));
          resolution_names.push_back(buf);
          m_available_specs.push_back(*specs[i]);
          std::cout << "  [" << i << "] " << buf << std::endl;
        }
        SDL_free(specs);
        std::cout << "Total: " << num_specs << " formats available" << std::endl;
      }

      m_resolution_combo->set_items(resolution_names);
      m_resolution_combo->set_selected_index(0);
      m_selected_resolution_index = 0;
      perform_layout();
    }
  }

  void set_display_scale(float scale) {
    m_display_scale = scale;
    update_image_view_size();
  }

  void update_image_view_size() {
    if (!m_texture)
      return;

    int width = m_camera_spec.width;
    int height = m_camera_spec.height;

    if (m_display_scale == 0.0f) {
      // Auto-fit to window (max 800x600)
      float aspect = (float)width / (float)height;
      if (width > 800 || height > 600) {
        if (aspect > 800.0f / 600.0f) {
          width = 800;
          height = (int)(800 / aspect);
        } else {
          height = 600;
          width = (int)(600 * aspect);
        }
      }
    } else {
      // Apply scale
      width = (int)(width * m_display_scale);
      height = (int)(height * m_display_scale);
    }

    m_image_view->set_fixed_size(Vector2i(width, height));
    m_image_view->center();
    perform_layout();

    std::cout << "Display size: " << width << "x" << height << std::endl;
  }

  void start_camera() {
    if (m_camera) {
      std::cout << "Camera already running" << std::endl;
      return;
    }

    if (m_selected_camera_index < 0 || m_selected_camera_index >= (int)m_camera_ids.size()) {
      m_status_label->set_caption("Please select a camera first");
      return;
    }

    SDL_CameraID camera_id = m_camera_ids[m_selected_camera_index];

    // Determine which spec to use
    SDL_CameraSpec desired_spec;
    bool spec_selected = false;

    if (m_selected_resolution_index > 0 &&
        m_selected_resolution_index <= (int)m_available_specs.size()) {
      // User selected a specific resolution
      desired_spec = m_available_specs[m_selected_resolution_index - 1];
      spec_selected = true;
      std::cout << "Using selected resolution: " << desired_spec.width << "x"
                << desired_spec.height << " @ " << SDL_GetPixelFormatName(desired_spec.format)
                << std::endl;
    } else {
      // Auto-select: prefer RGB formats over YUV
      int num_specs = 0;
      SDL_CameraSpec **specs = SDL_GetCameraSupportedFormats(camera_id, &num_specs);
      if (!specs || num_specs == 0) {
        m_status_label->set_caption("Failed to get camera formats");
        return;
      }

      desired_spec = *specs[0];
      for (int i = 0; i < num_specs; i++) {
        SDL_PixelFormat fmt = specs[i]->format;
        if (fmt == SDL_PIXELFORMAT_RGB24 || fmt == SDL_PIXELFORMAT_BGR24 ||
            fmt == SDL_PIXELFORMAT_RGBA32 || fmt == SDL_PIXELFORMAT_BGRA32) {
          desired_spec = *specs[i];
          std::cout << "Auto-selected RGB format: " << specs[i]->width << "x" << specs[i]->height
                    << " @ " << SDL_GetPixelFormatName(fmt) << std::endl;
          spec_selected = true;
          break;
        }
      }
      SDL_free(specs);
    }

    // Open the camera with desired specification
    m_camera = SDL_OpenCamera(camera_id, &desired_spec);
    if (!m_camera) {
      m_status_label->set_caption(std::string("Failed to open camera: ") + SDL_GetError());
      std::cerr << "Failed to open camera: " << SDL_GetError() << std::endl;
      return;
    }

    // Get the actual format the camera is using (may differ from desired)
    if (!SDL_GetCameraFormat(m_camera, &m_camera_spec)) {
      m_status_label->set_caption("Failed to get camera format");
      SDL_CloseCamera(m_camera);
      m_camera = nullptr;
      return;
    }

    std::cout << "Camera opened: " << m_camera_spec.width << "x" << m_camera_spec.height << " @ "
              << SDL_GetPixelFormatName(m_camera_spec.format) << std::endl;

    // Clean up old texture if it exists
    if (m_texture) {
      delete m_texture;
    }

    // Map SDL pixel format to NanoGUI texture format
    Texture::PixelFormat pixel_format;
    bool format_supported = true;
    
    switch (m_camera_spec.format) {
    case SDL_PIXELFORMAT_RGB24:
      pixel_format = Texture::PixelFormat::RGB;
      break;
    case SDL_PIXELFORMAT_BGR24:
      pixel_format = Texture::PixelFormat::BGR;
      break;
    case SDL_PIXELFORMAT_RGBA32:
    case SDL_PIXELFORMAT_RGBX32:
      pixel_format = Texture::PixelFormat::RGBA;
      break;
    case SDL_PIXELFORMAT_BGRA32:
    case SDL_PIXELFORMAT_BGRX32:
      pixel_format = Texture::PixelFormat::BGRA;
      break;
    default:
      // YUV formats (NV12, YUY2, etc.) need conversion
      std::cerr << "Warning: Camera format " << SDL_GetPixelFormatName(m_camera_spec.format)
                << " requires conversion" << std::endl;
      std::cerr << "Will attempt to convert to RGB..." << std::endl;
      pixel_format = Texture::PixelFormat::RGB;
      format_supported = false;
      break;
    }
    
    // For unsupported formats, we'll need to convert frames
    m_needs_conversion = !format_supported;

    // Create GPU texture to hold camera frames
    // Note: ImageView requires Nearest interpolation mode
    // For YUV formats, we'll convert to RGB on-the-fly
    m_texture =
        new Texture(pixel_format, Texture::ComponentFormat::UInt8,
                    Vector2i(m_camera_spec.width, m_camera_spec.height),
                    Texture::InterpolationMode::Nearest, Texture::InterpolationMode::Nearest);

    // Update the image view with the new texture
    m_image_view->set_image(m_texture);
    update_image_view_size();

    // Update UI state
    m_start_button->set_enabled(false);
    m_stop_button->set_enabled(true);
    m_status_label->set_caption("Camera running");

    perform_layout();
  }

  void stop_camera() {
    if (m_camera) {
      SDL_CloseCamera(m_camera);
      m_camera = nullptr;
      m_start_button->set_enabled(true);
      m_stop_button->set_enabled(false);
      m_status_label->set_caption("Camera stopped");
    }
    m_needs_conversion = false;
    
    // Reset image view to default size
    if (m_image_view) {
      m_image_view->set_fixed_size(Vector2i(640, 480));
      m_image_view->set_image(nullptr);
      perform_layout();
    }
  }

  virtual void draw_contents() override {
    // This is called every frame - update camera feed here
    if (m_camera && m_texture) {
      Uint64 timestampNS = 0;

      // Acquire the latest frame from the camera
      // This is a zero-copy operation - we get a pointer to camera's buffer
      SDL_Surface *frame = SDL_AcquireCameraFrame(m_camera, &timestampNS);

      if (frame) {
        // Handle format conversion if needed (e.g., YUV to RGB)
        if (m_needs_conversion) {
          // Convert YUV frame to RGB format
          SDL_Surface *converted = SDL_ConvertSurface(frame, SDL_PIXELFORMAT_RGB24);
          if (converted) {
            // Upload converted frame data to GPU texture
            m_texture->upload((const uint8_t *)converted->pixels);
            SDL_DestroySurface(converted);
          } else {
            std::cerr << "Frame conversion failed: " << SDL_GetError() << std::endl;
          }
        } else {
          // Direct upload - no conversion needed
          m_texture->upload((const uint8_t *)frame->pixels);
        }

        // Release the frame back to the camera driver
        // Important: must be called to avoid memory leaks
        SDL_ReleaseCameraFrame(m_camera, frame);

        m_frame_count++;
      }
    }

    // Call parent to render the GUI
    Screen::draw_contents();
  }

  virtual bool keyboard_event(int key, int scancode, int action, int modifiers) override {
    if (Screen::keyboard_event(key, scancode, action, modifiers))
      return true;

    // ESC to close
    if (key == SDLK_ESCAPE && action == 1) {
      set_visible(false);
      return true;
    }

    return false;
  }

private:
  Label *m_status_label = nullptr;
  ComboBox *m_camera_combo = nullptr;
  ComboBox *m_resolution_combo = nullptr;
  Button *m_start_button = nullptr;
  Button *m_stop_button = nullptr;
  ImageView *m_image_view = nullptr;

  SDL_Camera *m_camera = nullptr;
  SDL_CameraSpec m_camera_spec;
  Texture *m_texture = nullptr;
  bool m_needs_conversion = false;
  float m_display_scale = 1.0f; // 0 = auto-fit

  std::vector<SDL_CameraID> m_camera_ids;
  std::vector<SDL_CameraSpec> m_available_specs;
  int m_selected_camera_index = -1;
  int m_selected_resolution_index = 0;
  int m_frame_count = 0;
};

int main(int argc, char **argv) {
  try {
    std::cout << "\n=== Camera Capture Demo ===" << std::endl;
    std::cout << "SDL Version: " << SDL_GetRevision() << std::endl;
    
    // Check if camera subsystem is available
    std::cout << "\nChecking SDL3 camera support..." << std::endl;
    
    nanogui::init();

    {
      ref<CameraApp> app = new CameraApp();
      app->dec_ref();
      app->set_visible(true);

      std::cout << "\nInstructions:" << std::endl;
      std::cout << "1. Select a camera from the dropdown" << std::endl;
      std::cout << "2. Click 'Start' to begin capture" << std::endl;
      std::cout << "3. Click 'Stop' to pause capture" << std::endl;
      std::cout << "4. Click 'Refresh' to rescan for cameras" << std::endl;
      std::cout << "5. Press ESC to exit" << std::endl;
      std::cout << "===========================\n" << std::endl;

      // Event loop
      while (app->process_events()) {
        app->draw_all();
      }
    }

    nanogui::shutdown();
    std::cout << "\nCamera demo closed successfully" << std::endl;

  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
