# AVPlayer - FFmpeg Video Player Example

A multi-threaded video player built with NanoGUI and FFmpeg, demonstrating advanced media playback with synchronized audio/video.

## Features

- **Multi-threaded architecture** - Separate threads for demuxing, video decoding, audio decoding, and presentation
- **Synchronized A/V playback** - Precise timing synchronization between audio and video streams
- **Thread-safe queues** - Producer-consumer pattern for efficient data flow
- **YUV to RGB conversion** - CPU-based color space conversion for display
- **Audio playback** - Using miniaudio library for cross-platform audio output
- **Playback controls** - Keyboard shortcuts for pause/resume and quit

## Architecture

### Multi-threaded Pipeline

```
┌─────────────┐     ┌──────────────┐     ┌─────────────┐
│  Demuxer    │────▶│ Video Packet │────▶│   Video     │
│   Thread    │     │    Queue     │     │   Decoder   │
│             │     └──────────────┘     │   Thread    │
│             │                          └─────────────┘
│             │                                 │
│             │                                 ▼
│             │                          ┌─────────────┐
│             │                          │   Frame     │
│             │                          │   Queue     │
│             │                          └─────────────┘
│             │                                 │
│             │                                 ▼
│             │                          ┌─────────────┐
│             │     ┌──────────────┐     │   Video     │
│             │────▶│ Audio Packet │────▶│Presentation │
│             │     │    Queue     │     │   Thread    │
└─────────────┘     └──────────────┘     └─────────────┘
                           │
                           ▼
                    ┌─────────────┐
                    │   Audio     │
                    │   Decoder   │
                    │   Thread    │
                    └─────────────┘
                           │
                           ▼
                    ┌─────────────┐
                    │   Audio     │
                    │   Player    │
                    └─────────────┘
```

### Components

#### Core Classes

- **Player** - Main orchestrator managing all threads and components
- **Demuxer** - Reads media file and extracts packets (libavformat)
- **VideoDecoder** - Decodes video packets to frames (libavcodec)
- **AudioDecoder** - Decodes audio packets to samples (libavcodec)
- **Display** - NanoGUI-based video display with YUV→RGB conversion
- **AudioPlayer** - Cross-platform audio output (miniaudio)

#### Supporting Classes

- **Queue<T>** - Thread-safe bounded queue for packets/frames
- **Timer** - Precise timing for A/V synchronization
- **FormatConverter** - Pixel format conversion (libswscale)
- **AudioResampler** - Audio format conversion (libswresample)

## Building

### Prerequisites

**FFmpeg Development Libraries:**
- Windows: `vcpkg install ffmpeg`
- Linux: `sudo apt-get install libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libswresample-dev`
- macOS: `brew install ffmpeg`

**NanoGUI** (already included in this repository)

### Build Instructions

```bash
# Configure with CMake
cmake -B build -S .

# Build
cmake --build build --target AVPlayer

# Run
./build/bin/AVPlayer path/to/video.mp4
```

If FFmpeg is not found, the AVPlayer target will be skipped automatically.

## Usage

```bash
AVPlayer <video_file> [options]
```

### Options

- `--no-hw` - Disable hardware acceleration (use software decode)
- `--filter <desc>` - Apply video filter (e.g., "hflip", "eq=contrast=1.5")

### Examples

```bash
# Basic playback with hardware acceleration (default)
AVPlayer video.mp4

# Software decoding only
AVPlayer video.mp4 --no-hw

# Apply horizontal flip filter
AVPlayer video.mp4 --filter "hflip"

# Adjust brightness and contrast
AVPlayer video.mp4 --filter "eq=brightness=0.1:contrast=1.2"

# Multiple filters (grayscale + blur)
AVPlayer video.mp4 --filter "hue=s=0,boxblur=2:1"

# Windows paths
AVPlayer.exe C:\path\to\video.mp4
AVPlayer.exe sample.mp4 --filter "vflip"

# Linux/macOS
./AVPlayer /path/to/video.mp4
./AVPlayer sample.mp4 --no-hw
```

**Note:** The video file path can be:
- Relative to the current working directory
- Absolute path
- Use forward slashes (/) or backslashes (\\) on Windows

### Keyboard Controls

- **SPACE** - Pause/Resume playback
- **← Left Arrow** - Seek backward 5 seconds
- **→ Right Arrow** - Seek forward 5 seconds
- **[ Left Bracket** - Decrease playback speed
- **] Right Bracket** - Increase playback speed
- **↑ Up Arrow** - Increase volume by 5%
- **↓ Down Arrow** - Decrease volume by 5%
- **M** - Mute/Unmute audio
- **F** - Toggle fullscreen mode
- **ESC** - Quit player

### Mouse Controls

- **Left-click on progress bar** - Seek to clicked position
- **Right-click** - Show context menu with:
  - Play/Pause
  - Volume slider (0-100%)
  - Mute/Unmute
  - Speed presets (0.25x, 0.5x, 1.0x, 1.5x, 2.0x)
  - **Video Filters** (40+ presets in 8 categories)
  - **Audio Tracks** (select language/quality)
  - **Audio Channels** (Mono/Stereo/L/R/Swap)
  - **Subtitle Tracks** (select language/disable)
  - Fullscreen toggle
  - Quit
- **Hover over progress bar** - Cursor changes to hand pointer

### Supported Formats

AVPlayer supports any format that FFmpeg can decode, including:
- Video: H.264, H.265, VP8, VP9, AV1, MPEG-4, etc.
- Audio: AAC, MP3, Opus, Vorbis, FLAC, etc.
- Containers: MP4, MKV, WebM, AVI, MOV, etc.

## Implementation Details

### Thread Synchronization

The player uses a multi-threaded pipeline with thread-safe queues:

1. **Demuxer thread** reads packets from the file and pushes them to packet queues
2. **Decoder threads** pop packets, decode them, and push frames to frame queues
3. **Presentation thread** pops frames and displays them at the correct time

### Audio/Video Sync

- Audio playback drives the master clock
- Video frames are displayed based on their presentation timestamp (PTS)
- The timer adjusts frame display timing to match audio playback

### Color Space Conversion

Video frames are decoded in YUV420P format and converted to RGB on the CPU:
```cpp
R = Y + 1.370705 * (V - 128)
G = Y - 0.337633 * (U - 128) - 0.698001 * (V - 128)
B = Y + 1.732446 * (U - 128)
```

### Memory Management

- Smart pointers with custom deleters for FFmpeg structures
- RAII pattern ensures proper cleanup
- Move semantics for efficient queue operations

## Troubleshooting

### "Failed to initialize SDL" Error

**Windows:**
1. Make sure `SDL3.dll` is in the same directory as `AVPlayer.exe`
2. Copy from vcpkg: `C:\tools\vcpkg\installed\x64-windows\bin\SDL3.dll` → `build\bin\`
3. Or run: `copy_dlls.bat` (in the ffmpeg directory)

**Linux/macOS:**
- Ensure SDL3 is installed system-wide
- Check `LD_LIBRARY_PATH` (Linux) or `DYLD_LIBRARY_PATH` (macOS)

### "File not found" Error

- Use absolute paths or ensure the video file is in the current working directory
- Check file permissions
- Verify the file format is supported by FFmpeg

### Build Errors

- **FFmpeg not found:** Install FFmpeg development libraries (see Building section)
- **Linker errors:** Ensure all FFmpeg libraries are installed (avcodec, avformat, avutil, swscale, swresample)

## Recent Updates

### ✅ Implemented Features

- **Hardware-Accelerated Decoding** - Automatic GPU decode with CUDA, VAAPI, VideoToolbox, D3D11VA, etc.
- **Streaming Support** - HTTP, HTTPS, HLS, DASH, RTSP, RTMP, UDP, RTP protocols
- **Dynamic Video Filters** - Real-time filter switching via context menu (40+ presets)
- **Multi-Track Audio** - Select audio tracks, channel modes (Mono/Stereo/L/R/Swap)
- **Subtitle Support** - Multi-track subtitle rendering with libass (ASS/SSA/SRT/WebVTT)
- **Filter Presets** - Transform, Color, Effect, 3D, Artistic, Quality categories
- **Intuitive UI** - Context menu, OSD feedback, keyboard shortcuts
- **Resizable Window** - Video automatically scales to fit window while maintaining aspect ratio
- **Letterboxing/Pillarboxing** - Black bars added when needed to preserve aspect ratio
- **Progress Bar / Timeline** - Visual progress indicator with current time / total duration display
- **Seek Controls** - Click progress bar or use Left/Right arrows to jump through video
- **Playback Speed Control** - Adjust speed from 0.25x to 4x with [/] keys or menu presets
- **Volume Control** - Adjust volume 0-100% with Up/Down arrows or slider in menu
- **Mute Toggle** - Instant mute/unmute with M key
- **On-Screen Display (OSD)** - Visual feedback for volume, play/pause, and mute actions
- **Context Menu** - Right-click menu with volume slider and all controls
- **Fullscreen Mode** - Toggle fullscreen with F key or context menu
- **GPU-Accelerated Rendering** - YUV→RGB conversion uses GPU shaders for high performance

See implementation details:
- `CURRENT_STATUS.md` - **Complete feature status and capabilities**
- `STREAMING_GUIDE.md` - **HTTP, HLS, RTSP, RTMP, UDP streaming guide**
- `AUDIO_FEATURES_GUIDE.md` - Audio tracks, channels, and external audio guide
- `FILTERS_AND_SUBTITLES_GUIDE.md` - Dynamic filters and subtitle tracks guide
- `HARDWARE_DECODE_AND_FILTERS.md` - Hardware acceleration and video filters guide
- `RESIZE_FIX.md` - Window resizing and aspect ratio preservation
- `CONTEXT_MENU_GUIDE.md` - Right-click context menu
- `VOLUME_CONTROL_FEATURE.md` - Volume control and OSD system
- `PROGRESS_BAR_FEATURE.md` - Progress bar and time display
- `SEEK_FEATURE.md` - Seeking controls (click and keyboard)
- `PLAYBACK_SPEED_FEATURE.md` - Playback speed control

## Known Limitations

1. **No subtitle support** - Only video and audio streams are processed
2. **Basic error handling** - Limited recovery from decode errors
3. **CPU-based filters** - Video filters run on CPU (GPU filters planned)
4. **Static filter configuration** - Filters must be set at startup

## Future Improvements

- [ ] Add subtitle rendering
- [ ] Support for multiple audio/video tracks
- [ ] Audio pitch correction for speed changes
- [ ] Playlist support
- [ ] GPU-accelerated filters (OpenGL/Vulkan compute shaders)
- [ ] Dynamic filter switching during playback
- [ ] Screenshot capture (S key to save frame)
- [ ] Bookmark/chapter support
- [ ] Video info overlay (codec, resolution, FPS, bitrate)
- [ ] Hover preview on progress bar (thumbnail + timestamp)
- [ ] Audio filters
- [ ] Filter presets and UI controls

## Dependencies

- **FFmpeg** (libavcodec, libavformat, libavfilter, libavutil, libswscale, libswresample)
- **libass** (subtitle rendering)
- **NanoGUI** (UI framework)
- **miniaudio** (audio playback, header-only)
- **SDL3** or **GLFW** (windowing backend via NanoGUI)

### Hardware Acceleration Requirements

- **Windows**: DirectX 11 or DirectX 9 capable GPU
- **Linux**: VAAPI-capable GPU (Intel/AMD) or VDPAU (NVIDIA)
- **macOS**: VideoToolbox (built-in, macOS 10.8+)
- **NVIDIA**: CUDA-capable GPU with recent drivers
- **Intel**: Quick Sync Video support

## License

This example follows the same license as NanoGUI (BSD-style license).

## References

- [FFmpeg Documentation](https://ffmpeg.org/documentation.html)
- [FFmpeg Examples](https://github.com/FFmpeg/FFmpeg/tree/master/doc/examples)
- [miniaudio](https://miniaud.io/)
- [NanoGUI](https://github.com/mitsuba-renderer/nanogui)
