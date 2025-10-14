# AVPlayer Quick Reference

## 🚀 Quick Start

```bash
AVPlayer video.mp4
```

## ⌨️ Keyboard Shortcuts

| Key | Action |
|-----|--------|
| **SPACE** | Pause/Resume |
| **ESC** | Quit |
| **F** | Fullscreen |
| **M** | Mute/Unmute |
| **S** | Screenshot (PNG) |
| **←** | Seek backward 5s |
| **→** | Seek forward 5s |
| **↑** | Volume +5% |
| **↓** | Volume -5% |
| **[** | Slower (0.25x min) |
| **]** | Faster (4x max) |

## 🖱️ Mouse Controls

| Action | Result |
|--------|--------|
| **Right-click** | Context menu |
| **Click progress bar** | Seek to position |
| **Hover progress bar** | Hand cursor |

## 📋 Context Menu

### Playback
- Play/Pause
- Volume slider
- Mute/Unmute
- Speed (0.25x - 2.0x)

### Video Filters
- **Transform**: Flip, rotate
- **Color**: Brightness, contrast, saturation
- **Effect**: Blur, sharpen, edge detect
- **Quality**: Denoise, deinterlace
- **3D**: Stereo3D conversion
- **Artistic**: Cartoon, vintage, vignette
- **Correction**: Auto levels, white balance
- **Performance**: Scale resolution

### Audio Tracks
- Track 1 (language) [channels, codec]
- Track 2 (language) [channels, codec]
- ...

### Audio Channels
- Original
- Stereo
- Mono
- Left Only
- Right Only
- Swap L/R

### Subtitles
- Disabled
- Track 1 (language)
- Track 2 (language)
- ...
- Load Subtitle File...
- Font Size...

### Other
- Screenshot (S)
- Batch Extract Frames...
- Fullscreen
- Quit

## 💻 Command Line

```bash
# Basic
AVPlayer video.mp4

# Software decode
AVPlayer video.mp4 --no-hw

# With filter
AVPlayer video.mp4 --filter "hflip"

# Multiple filters
AVPlayer video.mp4 --filter "hflip,eq=contrast=1.3"
```

## 🎨 Popular Filters

| Filter | Effect |
|--------|--------|
| `hflip` | Horizontal flip |
| `vflip` | Vertical flip |
| `hue=s=0` | Grayscale |
| `eq=contrast=1.5` | High contrast |
| `boxblur=2:1` | Light blur |
| `unsharp=5:5:1.0` | Sharpen |
| `hqdn3d` | Denoise |
| `yadif` | Deinterlace |
| `stereo3d=sbsl:arcd` | 3D anaglyph |
| `vignette` | Darken edges |

## 🔧 Troubleshooting

| Problem | Solution |
|---------|----------|
| No hardware decode | Update GPU drivers |
| Stuttering | Use `--no-hw` or simpler filter |
| No subtitles | Check if file has subtitle tracks |
| Filter not working | Check FFmpeg filter syntax |

## 📚 Documentation

- `README.md` - Overview
- `AUDIO_FEATURES_GUIDE.md` - Audio tracks and channels
- `FILTERS_AND_SUBTITLES_GUIDE.md` - Detailed filter/subtitle guide
- `FRAME_CAPTURE_GUIDE.md` - Screenshot and frame extraction
- `HARDWARE_DECODE_AND_FILTERS.md` - Hardware acceleration
- `BUILD_GUIDE.md` - Build instructions
- `FEATURE_SUMMARY.md` - Complete feature list

## 🎯 Common Workflows

### Watch Foreign Film
1. Right-click → Audio Tracks → Select language
2. Right-click → Subtitles → Select subtitle track

### Listen with One Earbud
1. Right-click → Audio Channels → Mono

### Enhance Old Video
1. Right-click → Video Filters → Quality → Denoise Medium
2. Right-click → Video Filters → Effect → Sharpen
3. Right-click → Video Filters → Color → High Contrast

### Create Artistic Effect
1. Right-click → Video Filters → Color → Grayscale
2. Right-click → Video Filters → Artistic → Vignette

### Watch 3D Movie
1. Right-click → Video Filters → 3D → Stereo3D: Anaglyph Red-Cyan
2. Put on 3D glasses

### Fix Interlaced Video
1. Right-click → Video Filters → Quality → Deinterlace

## ⚡ Performance Tips

- Hardware decode is **enabled by default**
- Use **lighter filters** for smooth playback
- **Scale down** resolution if needed
- **Close other apps** for best performance
- **Update GPU drivers** regularly

## 🌟 Pro Tips

- **Chain filters** for complex effects
- **Disable filters** by selecting "None"
- **OSD shows** current filter/subtitle
- **Filters work** with hardware decode
- **Subtitles** have minimal performance impact

## 📊 System Requirements

### Minimum
- CPU: Dual-core 2GHz
- RAM: 2GB
- GPU: Any with OpenGL 3.3

### Recommended
- CPU: Quad-core 2.5GHz+
- RAM: 4GB+
- GPU: Hardware decode support (NVIDIA/AMD/Intel)

### For 4K
- CPU: 6-core 3GHz+ (software) or any (hardware)
- RAM: 8GB+
- GPU: Modern with H.265 hardware decode

## 🆘 Getting Help

1. Check console output for errors
2. Try `--no-hw` flag
3. Test with simpler filter
4. Review documentation
5. Check FFmpeg version: `ffmpeg -version`

## 🎬 Supported Formats

### Video Codecs
H.264, H.265, VP8, VP9, AV1, MPEG-4, etc.

### Audio Codecs
AAC, MP3, Opus, Vorbis, FLAC, etc.

### Containers
MP4, MKV, WebM, AVI, MOV, etc.

### Subtitles
ASS, SSA, SRT, WebVTT, DVD, Blu-ray

## 🔗 Quick Links

- FFmpeg Filters: https://ffmpeg.org/ffmpeg-filters.html
- libass: https://github.com/libass/libass
- NanoGUI: https://github.com/mitsuba-renderer/nanogui
