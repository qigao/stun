/*
    nanogui/m3_circular_progress.h -- Material Design 3 Circular Progress

    Implements M3 circular progress indicator.

    Based on: https://m3.material.io/components/progress-indicators

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#include <nanogui/m3_theme.h>
#include <nanogui/widget.h>


NAMESPACE_BEGIN(nanogui)

/**
 * \class M3CircularProgress m3_circular_progress.h
 * nanogui/m3_circular_progress.h
 *
 * \brief Material Design 3 Circular Progress Indicator
 *
 * Circular progress indicators display progress by animating along a circular
 * track.
 */
class NANOGUI_EXPORT M3CircularProgress : public Widget {
public:
  /// Progress size variants
  enum class Size {
    Small,  ///< 24dp
    Medium, ///< 48dp (default)
    Large   ///< 64dp
  };

  /**
   * \brief Construct an M3 circular progress indicator
   *
   * \param parent Parent widget
   * \param size Progress size
   */
  M3CircularProgress(Widget *parent, Size size = Size::Medium);

  /// Set progress value (0.0 to 1.0, -1 for indeterminate)
  void set_value(float value) { m_value = value; }

  /// Get progress value
  float value() const { return m_value; }

  /// Set size
  void set_size(Size size);

  /// Get size
  Size size() const { return m_size; }

  /// Draw the progress indicator
  void draw(NVGcontext *ctx) override;

  /// Preferred size
  Vector2i preferred_size(NVGcontext *ctx) const;

protected:
  /// Get M3 theme
  M3Theme *m3_theme() const;

  /// Get size in pixels
  int get_pixel_size() const;

  float m_value = -1.0f; // -1 = indeterminate
  Size m_size;
};

NAMESPACE_END(nanogui)
