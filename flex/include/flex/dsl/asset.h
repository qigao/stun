/*
 * Flex Engine - Asset Management
 *
 * Preload and manage resources (audio, images, fonts).
 * Integrates with state machine for play/stop actions.
 */

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace flex {

// ============================================================================
// Asset Types
// ============================================================================

enum class AssetType {
  Audio,
  Image,
  Font,
  Svg
};

// ============================================================================
// Asset Options
// ============================================================================

struct AudioOptions {
  bool loop = false;
  float volume = 1.0f;
  bool preload = true;
};

struct ImageOptions {
  bool preload = true;
};

struct FontOptions {
  bool preload = true;
};

// ============================================================================
// Asset Entry
// ============================================================================

struct AssetEntry {
  std::string id;
  std::string path;
  AssetType type;

  // Type-specific options
  AudioOptions audio_opts;
  ImageOptions image_opts;
  FontOptions font_opts;

  // Runtime state
  bool loaded = false;
  void* data = nullptr;  // Platform-specific handle
};

// ============================================================================
// Audio Backend Interface
// ============================================================================

class AudioBackend {
public:
  virtual ~AudioBackend() = default;

  // Load audio file, return handle
  virtual void* load(const std::string& path) = 0;

  // Unload audio
  virtual void unload(void* handle) = 0;

  // Play audio (returns channel/instance id)
  virtual int play(void* handle, bool loop, float volume) = 0;

  // Stop audio by channel
  virtual void stop(int channel) = 0;

  // Stop all instances of an audio
  virtual void stop_all(void* handle) = 0;

  // Set volume for channel
  virtual void set_volume(int channel, float volume) = 0;

  // Check if channel is playing
  virtual bool is_playing(int channel) = 0;
};

// ============================================================================
// Null Audio Backend (no-op)
// ============================================================================

class NullAudioBackend : public AudioBackend {
public:
  void* load(const std::string&) override { return nullptr; }
  void unload(void*) override {}
  int play(void*, bool, float) override { return -1; }
  void stop(int) override {}
  void stop_all(void*) override {}
  void set_volume(int, float) override {}
  bool is_playing(int) override { return false; }
};

// ============================================================================
// Asset Manager
// ============================================================================

class AssetManager {
public:
  AssetManager();
  ~AssetManager();

  // Set audio backend (must be called before loading audio assets)
  void set_audio_backend(std::unique_ptr<AudioBackend> backend);

  // Register assets
  void register_audio(const std::string& id, const std::string& path,
                      const AudioOptions& opts = {});
  void register_image(const std::string& id, const std::string& path,
                      const ImageOptions& opts = {});
  void register_font(const std::string& id, const std::string& path,
                     const FontOptions& opts = {});
  void register_svg(const std::string& id, const std::string& path);

  // Preload all registered assets
  void preload_all();

  // Get asset entry
  const AssetEntry* get(const std::string& id) const;

  // Resolve asset path (for compatibility with existing API)
  const char* resolve_path(const std::string& id) const;

  // Check if asset exists
  bool has(const std::string& id) const;

  // -------------------------------------------
  // Audio Control
  // -------------------------------------------

  // Play audio by asset ID
  int play_audio(const std::string& id);

  // Play audio with custom options
  int play_audio(const std::string& id, bool loop, float volume);

  // Stop audio by channel
  void stop_audio_channel(int channel);

  // Stop all instances of an audio asset
  void stop_audio(const std::string& id);

  // Set volume for channel
  void set_audio_volume(int channel, float volume);

  // Check if channel is playing
  bool is_audio_playing(int channel);

  // -------------------------------------------
  // Bulk Operations
  // -------------------------------------------

  // Get all assets of a type
  std::vector<const AssetEntry*> get_all(AssetType type) const;

  // Clear all assets
  void clear();

private:
  std::unordered_map<std::string, AssetEntry> assets_;
  std::unique_ptr<AudioBackend> audio_backend_;

  // Track active audio channels for each asset
  std::unordered_map<std::string, std::vector<int>> active_channels_;

  void load_asset(AssetEntry& entry);
};

// ============================================================================
// SDL2 Audio Backend (optional, requires SDL2_mixer)
// ============================================================================

#ifdef FLEX_USE_SDL_AUDIO

class SDLAudioBackend : public AudioBackend {
public:
  SDLAudioBackend();
  ~SDLAudioBackend() override;

  void* load(const std::string& path) override;
  void unload(void* handle) override;
  int play(void* handle, bool loop, float volume) override;
  void stop(int channel) override;
  void stop_all(void* handle) override;
  void set_volume(int channel, float volume) override;
  bool is_playing(int channel) override;

private:
  bool initialized_ = false;
};

#endif // FLEX_USE_SDL_AUDIO

} // namespace flex
