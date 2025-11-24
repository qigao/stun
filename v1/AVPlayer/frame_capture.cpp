#include "frame_capture.h"
#include "ffmpeg.h"
extern "C" {
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

CapturedFrame FrameCapture::capture_frame(AVFrame *frame, CaptureFormat format) {
  CapturedFrame captured;
  captured.width = frame->width;
  captured.height = frame->height;
  captured.format = format;
  captured.timestamp = frame->pts;

  switch (format) {
  case CaptureFormat::RGB24:
    captured.data = frame_to_rgb24(frame);
    break;
  case CaptureFormat::RGBA:
    captured.data = frame_to_rgba(frame);
    break;
  case CaptureFormat::YUV420P:
    captured.data = frame_to_yuv420p(frame);
    break;
  case CaptureFormat::YUV444P:
    captured.data = frame_to_yuv444p(frame);
    break;
  case CaptureFormat::PNG:
    captured.data = encode_png(frame);
    break;
  case CaptureFormat::JPEG:
    captured.data = encode_jpeg(frame, 95);
    break;
  case CaptureFormat::BMP:
    captured.data = encode_bmp(frame);
    break;
  case CaptureFormat::TIFF:
    // TIFF encoding similar to PNG
    captured.data = encode_image(frame, AV_CODEC_ID_TIFF, 0);
    break;
  }

  captured.data_size = captured.data.size();
  return captured;
}

bool FrameCapture::save_to_file(const CapturedFrame &captured, const std::string &filename) {
  std::ofstream file(filename, std::ios::binary);
  if (!file.is_open()) {
    std::cerr << "Failed to open file for writing: " << filename << "\n";
    return false;
  }

  file.write(reinterpret_cast<const char *>(captured.data.data()), captured.data.size());
  file.close();

  std::cout << "Frame saved: " << filename << " (" << captured.data_size << " bytes)\n";
  return true;
}

bool FrameCapture::capture_and_save(AVFrame *frame, const std::string &filename) {
  CaptureFormat format = detect_format_from_filename(filename);
  CapturedFrame captured = capture_frame(frame, format);
  return save_to_file(captured, filename);
}

std::string FrameCapture::generate_filename(const std::string &prefix, CaptureFormat format) {
  auto now = std::chrono::system_clock::now();
  auto time = std::chrono::system_clock::to_time_t(now);
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

  std::stringstream ss;
  ss << prefix << "_" << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S") << "_"
     << std::setfill('0') << std::setw(3) << ms.count() << get_extension(format);

  return ss.str();
}

std::string FrameCapture::get_extension(CaptureFormat format) {
  switch (format) {
  case CaptureFormat::PNG:
    return ".png";
  case CaptureFormat::JPEG:
    return ".jpg";
  case CaptureFormat::BMP:
    return ".bmp";
  case CaptureFormat::TIFF:
    return ".tiff";
  case CaptureFormat::RGB24:
    return ".rgb";
  case CaptureFormat::RGBA:
    return ".rgba";
  case CaptureFormat::YUV420P:
    return ".yuv";
  case CaptureFormat::YUV444P:
    return ".yuv";
  default:
    return ".dat";
  }
}

CaptureFormat FrameCapture::detect_format_from_filename(const std::string &filename) {
  std::string lower = filename;
  std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

  // Helper lambda to check if string ends with suffix (C++17 compatible)
  auto ends_with = [](const std::string &str, const std::string &suffix) {
    if (suffix.size() > str.size())
      return false;
    return str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
  };

  if (ends_with(lower, ".png"))
    return CaptureFormat::PNG;
  if (ends_with(lower, ".jpg") || ends_with(lower, ".jpeg"))
    return CaptureFormat::JPEG;
  if (ends_with(lower, ".bmp"))
    return CaptureFormat::BMP;
  if (ends_with(lower, ".tiff") || ends_with(lower, ".tif"))
    return CaptureFormat::TIFF;
  if (ends_with(lower, ".rgb"))
    return CaptureFormat::RGB24;
  if (ends_with(lower, ".rgba"))
    return CaptureFormat::RGBA;
  if (ends_with(lower, ".yuv"))
    return CaptureFormat::YUV420P;

  return CaptureFormat::PNG; // Default
}

std::vector<uint8_t> FrameCapture::convert_frame(AVFrame *frame, AVPixelFormat target_format) {
  SwsContext *sws_ctx = sws_getContext(
      frame->width, frame->height, static_cast<AVPixelFormat>(frame->format), frame->width,
      frame->height, target_format, SWS_BILINEAR, nullptr, nullptr, nullptr);

  if (!sws_ctx) {
    std::cerr << "Failed to create conversion context\n";
    return {};
  }

  // Allocate output frame
  AVFrame *out_frame = av_frame_alloc();
  out_frame->width = frame->width;
  out_frame->height = frame->height;
  out_frame->format = target_format;

  av_frame_get_buffer(out_frame, 0);

  // Convert
  sws_scale(sws_ctx, frame->data, frame->linesize, 0, frame->height, out_frame->data,
            out_frame->linesize);

  // Copy data to vector
  int data_size = av_image_get_buffer_size(target_format, frame->width, frame->height, 1);
  std::vector<uint8_t> result(data_size);

  av_image_copy_to_buffer(result.data(), data_size, out_frame->data, out_frame->linesize,
                          target_format, frame->width, frame->height, 1);

  av_frame_free(&out_frame);
  sws_freeContext(sws_ctx);

  return result;
}

std::vector<uint8_t> FrameCapture::frame_to_rgb24(AVFrame *frame) {
  return convert_frame(frame, AV_PIX_FMT_RGB24);
}

std::vector<uint8_t> FrameCapture::frame_to_rgba(AVFrame *frame) {
  return convert_frame(frame, AV_PIX_FMT_RGBA);
}

std::vector<uint8_t> FrameCapture::frame_to_yuv420p(AVFrame *frame) {
  return convert_frame(frame, AV_PIX_FMT_YUV420P);
}

std::vector<uint8_t> FrameCapture::frame_to_yuv444p(AVFrame *frame) {
  return convert_frame(frame, AV_PIX_FMT_YUV444P);
}

std::vector<uint8_t> FrameCapture::encode_image(AVFrame *frame, AVCodecID codec_id, int quality) {
  // Validate frame
  if (!frame || frame->width <= 0 || frame->height <= 0) {
    std::cerr << "Invalid frame for encoding (width: " << (frame ? frame->width : 0)
              << ", height: " << (frame ? frame->height : 0) << ")\n";
    return {};
  }

  const AVCodec *codec = avcodec_find_encoder(codec_id);
  if (!codec) {
    std::cerr << "Codec not found for encoding\n";
    return {};
  }

  AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);
  if (!codec_ctx) {
    std::cerr << "Failed to allocate codec context\n";
    return {};
  }

  codec_ctx->width = frame->width;
  codec_ctx->height = frame->height;
  codec_ctx->pix_fmt = AV_PIX_FMT_RGB24;
  codec_ctx->time_base = {1, 25};

  // Set quality for JPEG
  if (codec_id == AV_CODEC_ID_MJPEG && quality > 0) {
    codec_ctx->qmin = codec_ctx->qmax = quality;
  }

  if (avcodec_open2(codec_ctx, codec, nullptr) < 0) {
    std::cerr << "Failed to open codec\n";
    avcodec_free_context(&codec_ctx);
    return {};
  }

  // Convert frame to RGB24 if needed
  AVFrame *rgb_frame = av_frame_alloc();
  rgb_frame->width = frame->width;
  rgb_frame->height = frame->height;
  rgb_frame->format = AV_PIX_FMT_RGB24;
  av_frame_get_buffer(rgb_frame, 0);

  SwsContext *sws_ctx = sws_getContext(
      frame->width, frame->height, static_cast<AVPixelFormat>(frame->format), frame->width,
      frame->height, AV_PIX_FMT_RGB24, SWS_BILINEAR, nullptr, nullptr, nullptr);

  sws_scale(sws_ctx, frame->data, frame->linesize, 0, frame->height, rgb_frame->data,
            rgb_frame->linesize);
  sws_freeContext(sws_ctx);

  // Encode frame
  AVPacket *pkt = av_packet_alloc();
  int ret = avcodec_send_frame(codec_ctx, rgb_frame);

  std::vector<uint8_t> result;

  if (ret >= 0) {
    ret = avcodec_receive_packet(codec_ctx, pkt);
    if (ret >= 0) {
      result.assign(pkt->data, pkt->data + pkt->size);
    }
  }

  av_packet_free(&pkt);
  av_frame_free(&rgb_frame);
  avcodec_free_context(&codec_ctx);

  return result;
}

std::vector<uint8_t> FrameCapture::encode_png(AVFrame *frame) {
  return encode_image(frame, AV_CODEC_ID_PNG, 0);
}

std::vector<uint8_t> FrameCapture::encode_jpeg(AVFrame *frame, int quality) {
  return encode_image(frame, AV_CODEC_ID_MJPEG, quality);
}

std::vector<uint8_t> FrameCapture::encode_bmp(AVFrame *frame) {
  return encode_image(frame, AV_CODEC_ID_BMP, 0);
}
