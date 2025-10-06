/**
 * \file audio_player.h
 * \brief Audio playback functionality using miniaudio
 */

#pragma once
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

struct ma_device;

/**
 * \class AudioPlayer
 * \brief Plays audio samples using the miniaudio library
 *
 * This class manages audio playback by buffering samples and feeding them
 * to the audio device. It uses miniaudio for cross-platform audio output.
 */
class AudioPlayer {
public:
  /**
   * \brief Constructs an audio player with the specified parameters
   * \param sample_rate The sample rate in Hz (e.g., 44100, 48000)
   * \param channels The number of audio channels (1 for mono, 2 for stereo)
   */
  AudioPlayer(int sample_rate, int channels);

  /**
   * \brief Destructor - stops playback and cleans up resources
   */
  ~AudioPlayer();

  /**
   * \brief Starts audio playback
   */
  void start();

  /**
   * \brief Stops audio playback
   */
  void stop();

  /**
   * \brief Pushes audio samples to the playback buffer
   * \param samples Pointer to the audio sample data (interleaved float format)
   * \param frame_count Number of frames to push (frame = all channels for one sample)
   */
  void push_samples(const float *samples, size_t frame_count);

  /**
   * \brief Checks if audio is currently playing
   * \return true if playing, false otherwise
   */
  bool is_playing() const;

  /**
   * \brief Gets the number of frames currently buffered
   * \return Number of buffered frames
   */
  size_t buffered_frames() const;

private:
  /**
   * \brief Callback function called by miniaudio to request audio data
   * \param device The audio device
   * \param output Buffer to write audio data to
   * \param input Input buffer (unused)
   * \param frame_count Number of frames requested
   */
  static void data_callback(ma_device *device, void *output, const void *input,
                            unsigned int frame_count);

  std::unique_ptr<ma_device> device_;   ///< Miniaudio device handle
  std::vector<float> buffer_;           ///< Internal audio buffer
  mutable std::mutex buffer_mutex_;     ///< Mutex for thread-safe buffer access
  std::atomic<bool> is_playing_{false}; ///< Playback state flag
  int channels_{};                      ///< Number of audio channels
};
