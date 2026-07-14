/*
 * Flex Engine - Image Node
 *
 * Raster image node for rendering bitmaps.
 */

#pragma once

#include "flex/core/node.h"
#include "flex/core/types.h"
#include "flex/core/allocator.h"
#include <string>

namespace flex {

enum class ImageFit {
  Fill,    // Stretch to fill (may distort)
  Contain, // Fit inside, preserve aspect ratio
  Cover,   // Fill and crop, preserve aspect ratio
};

class Image : public Node {
public:
  using RawPtr = Image*;
  using SharedPtr = std::shared_ptr<Image>;
  using Ptr = RawPtr;

  Image() = default;
  ~Image() override = default;

  static SharedPtr create() { return std::make_shared<Image>(); }
  static RawPtr create(ArenaAllocator& arena) { return arena.create<Image>(); }

  NodeType type() const override { return NodeType::Image; }
  const char *type_name() const override { return "Image"; }

  // Source
  const std::string &src() const { return src_; }
  void set_src(const std::string &src) {
    src_ = src;
    mark_dirty(DirtyFlags::Content | DirtyFlags::Visual);
  }

  // Dimensions
  float width() const { return width_; }
  void set_width(float w) {
    width_ = w;
    mark_dirty(DirtyFlags::Content | DirtyFlags::Bounds);
  }

  float height() const { return height_; }
  void set_height(float h) {
    height_ = h;
    mark_dirty(DirtyFlags::Content | DirtyFlags::Bounds);
  }

  // Fit mode
  ImageFit fit_mode() const { return fit_; }
  void set_fit(ImageFit fit) {
    fit_ = fit;
    mark_dirty(DirtyFlags::Visual);
  }

  // Rendering
  void render(Renderer &renderer) override;

  // Bounds
  Bounds compute_bounds() const override;

private:
  std::string src_;
  float width_ = 0; // 0 = use natural size
  float height_ = 0;
  ImageFit fit_ = ImageFit::Fill;
};

} // namespace flex
