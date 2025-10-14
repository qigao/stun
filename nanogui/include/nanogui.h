/*
    nanogui.h -- Pull in *everything* from NanoGUI

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    The widget drawing code is based on the NanoVG demo application
    by Mikko Mononen.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#if defined(_WIN32)
  #include <process.h> // Required for _beginthreadex on Windows
#endif

#include <nanogui/button.h>
#include <nanogui/canvas.h>
#include <nanogui/checkbox.h>
#include <nanogui/colorwheel.h>
#include <nanogui/combobox.h>
#include <nanogui/common.h>
#include <nanogui/keys.h>
#include <nanogui/formhelper.h>
#include <nanogui/graph.h>
#include <nanogui/icons.h>
#include <nanogui/imagepanel.h>
#include <nanogui/imageview.h>
#include <nanogui/label.h>
#include <nanogui/layout.h>
#include <nanogui/messagedialog.h>
#include <nanogui/metal.h>
#include <nanogui/popup.h>
#include <nanogui/popupbutton.h>
#include <nanogui/progressbar.h>
#include <nanogui/renderpass.h>
#include <nanogui/screen.h>
#include <nanogui/shader.h>
#include <nanogui/slider.h>
#include <nanogui/tabwidget.h>
#include <nanogui/textarea.h>
#include <nanogui/textbox.h>
#include <nanogui/texture.h>
#include <nanogui/theme.h>
#include <nanogui/toolbutton.h>
#include <nanogui/vscrollpanel.h>
#include <nanogui/widget.h>
#include <nanogui/window.h>
