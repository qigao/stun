/*
 * Flex Engine - Asset Management Implementation
 */

#include "flex/runtime/asset.h"
#include <iostream>

namespace flex {

// ============================================================================
// AssetManager Implementation
// ============================================================================

AssetManager::AssetManager()
    : audio_backend_(std::make_unique<NullAudioBackend>()) {}

AssetManager::~AssetManager() { clear(); }

void AssetManager::set_audio_backend(std::unique_ptr<AudioBackend> backend) {
  audio_backend_ = std::move(backend);
}

void AssetManager::register_audio(const std::string &id, const std::string &path,
                                  const AudioOptions &opts) {
  AssetEntry entry;
  entry.id = id;
  entry.path = path;
  entry.type = AssetType::Audio;
  entry.audio_opts = opts;
  assets_[id] = std::move(entry);
}

void AssetManager::register_image(const std::string &id, const std::string &path,
                                  const ImageOptions &opts) {
  AssetEntry entry;
  entry.id = id;
  entry.path = path;
  entry.type = AssetType::Image;
  entry.image_opts = opts;
  assets_[id] = std::move(entry);
}

void AssetManager::register_font(const std::string &id, const std::string &path,
                                 const FontOptions &opts) {
  AssetEntry entry;
  entry.id = id;
  entry.path = path;
  entry.type = AssetType::Font;
  entry.font_opts = opts;
  assets_[id] = std::move(entry);
}

void AssetManager::register_svg(const std::string &id, const std::string &path) {
  AssetEntry entry;
  entry.id = id;
  entry.path = path;
  entry.type = AssetType::Svg;
  assets_[id] = std::move(entry);
}

void AssetManager::preload_all() {
  for (auto &[id, entry] : assets_) {
    if (!entry.loaded) {
      load_asset(entry);
    }
  }
}

void AssetManager::load_asset(AssetEntry &entry) {
  switch (entry.type) {
  case AssetType::Audio:
    if (entry.audio_opts.preload && audio_backend_) {
      entry.data = audio_backend_->load(entry.path);
      entry.loaded = (entry.data != nullptr);
    }
    break;

  case AssetType::Image:
    // Images are loaded on-demand by the renderer
    entry.loaded = true;
    break;

  case AssetType::Font:
    // Fonts are loaded on-demand by the text renderer
    entry.loaded = true;
    break;

  case AssetType::Svg:
    // SVGs are loaded on-demand
    entry.loaded = true;
    break;
  }
}

const AssetEntry *AssetManager::get(const std::string &id) const {
  auto it = assets_.find(id);
  return (it != assets_.end()) ? &it->second : nullptr;
}

const char *AssetManager::resolve_path(const std::string &id) const {
  auto it = assets_.find(id);
  return (it != assets_.end()) ? it->second.path.c_str() : "";
}

bool AssetManager::has(const std::string &id) const {
  return assets_.find(id) != assets_.end();
}

// -------------------------------------------
// Audio Control
// -------------------------------------------

int AssetManager::play_audio(const std::string &id) {
  auto it = assets_.find(id);
  if (it == assets_.end() || it->second.type != AssetType::Audio) {
    std::cerr << "[AssetManager] Audio asset not found: " << id << "\n";
    return -1;
  }

  auto &entry = it->second;

  // Load if not loaded
  if (!entry.loaded) {
    load_asset(entry);
  }

  if (!entry.data) {
    std::cerr << "[AssetManager] Audio not loaded: " << id << "\n";
    return -1;
  }

  int channel = audio_backend_->play(entry.data, entry.audio_opts.loop,
                                     entry.audio_opts.volume);

  if (channel >= 0) {
    active_channels_[id].push_back(channel);
  }

  return channel;
}

int AssetManager::play_audio(const std::string &id, bool loop, float volume) {
  auto it = assets_.find(id);
  if (it == assets_.end() || it->second.type != AssetType::Audio) {
    return -1;
  }

  auto &entry = it->second;

  if (!entry.loaded) {
    load_asset(entry);
  }

  if (!entry.data) {
    return -1;
  }

  int channel = audio_backend_->play(entry.data, loop, volume);

  if (channel >= 0) {
    active_channels_[id].push_back(channel);
  }

  return channel;
}

void AssetManager::stop_audio_channel(int channel) {
  audio_backend_->stop(channel);

  // Remove from active channels
  for (auto &[id, channels] : active_channels_) {
    channels.erase(std::remove(channels.begin(), channels.end(), channel),
                   channels.end());
  }
}

void AssetManager::stop_audio(const std::string &id) {
  auto it = assets_.find(id);
  if (it == assets_.end() || it->second.type != AssetType::Audio) {
    return;
  }

  // Stop all channels for this asset
  auto channels_it = active_channels_.find(id);
  if (channels_it != active_channels_.end()) {
    for (int channel : channels_it->second) {
      audio_backend_->stop(channel);
    }
    channels_it->second.clear();
  }

  // Also use stop_all in case backend tracks differently
  if (it->second.data) {
    audio_backend_->stop_all(it->second.data);
  }
}

void AssetManager::set_audio_volume(int channel, float volume) {
  audio_backend_->set_volume(channel, volume);
}

bool AssetManager::is_audio_playing(int channel) {
  return audio_backend_->is_playing(channel);
}

// -------------------------------------------
// Bulk Operations
// -------------------------------------------

std::vector<const AssetEntry *> AssetManager::get_all(AssetType type) const {
  std::vector<const AssetEntry *> result;
  for (const auto &[id, entry] : assets_) {
    if (entry.type == type) {
      result.push_back(&entry);
    }
  }
  return result;
}

void AssetManager::clear() {
  // Stop all audio
  for (auto &[id, channels] : active_channels_) {
    for (int channel : channels) {
      audio_backend_->stop(channel);
    }
  }
  active_channels_.clear();

  // Unload all audio
  for (auto &[id, entry] : assets_) {
    if (entry.type == AssetType::Audio && entry.data) {
      audio_backend_->unload(entry.data);
      entry.data = nullptr;
    }
  }

  assets_.clear();
}

// ============================================================================
// SDL2 Audio Backend (conditional)
// ============================================================================

#ifdef FLEX_USE_SDL_AUDIO

#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>

SDLAudioBackend::SDLAudioBackend() {
  if (SDL_Init(SDL_INIT_AUDIO) < 0) {
    std::cerr << "[SDLAudio] SDL init failed: " << SDL_GetError() << "\n";
    return;
  }

  if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
    std::cerr << "[SDLAudio] Mixer init failed: " << Mix_GetError() << "\n";
    return;
  }

  // Allocate channels
  Mix_AllocateChannels(32);
  initialized_ = true;
}

SDLAudioBackend::~SDLAudioBackend() {
  if (initialized_) {
    Mix_CloseAudio();
  }
}

void *SDLAudioBackend::load(const std::string &path) {
  if (!initialized_)
    return nullptr;

  Mix_Chunk *chunk = Mix_LoadWAV(path.c_str());
  if (!chunk) {
    std::cerr << "[SDLAudio] Failed to load: " << path << " - " << Mix_GetError()
              << "\n";
  }
  return chunk;
}

void SDLAudioBackend::unload(void *handle) {
  if (handle) {
    Mix_FreeChunk(static_cast<Mix_Chunk *>(handle));
  }
}

int SDLAudioBackend::play(void *handle, bool loop, float volume) {
  if (!handle || !initialized_)
    return -1;

  auto *chunk = static_cast<Mix_Chunk *>(handle);
  int channel = Mix_PlayChannel(-1, chunk, loop ? -1 : 0);

  if (channel >= 0) {
    Mix_Volume(channel, static_cast<int>(volume * MIX_MAX_VOLUME));
  }

  return channel;
}

void SDLAudioBackend::stop(int channel) {
  if (initialized_ && channel >= 0) {
    Mix_HaltChannel(channel);
  }
}

void SDLAudioBackend::stop_all(void *handle) {
  // SDL_mixer doesn't have a direct way to stop all instances of a chunk
  // We'd need to track channels ourselves, which we do in AssetManager
  (void)handle;
}

void SDLAudioBackend::set_volume(int channel, float volume) {
  if (initialized_ && channel >= 0) {
    Mix_Volume(channel, static_cast<int>(volume * MIX_MAX_VOLUME));
  }
}

bool SDLAudioBackend::is_playing(int channel) {
  if (!initialized_ || channel < 0)
    return false;
  return Mix_Playing(channel) != 0;
}

#endif // FLEX_USE_SDL_AUDIO

} // namespace flex
