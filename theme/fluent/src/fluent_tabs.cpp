/*
    src/fluent_tabs.cpp -- Fluent Design Tabs implementation

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include <nanogui/fluent_tabs.h>
#include <nanogui/theme.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

FluentTabs::FluentTabs(Widget *parent) : TabWidget(parent) {
}

void FluentTabs::draw(NVGcontext *ctx) {
    // Just call parent draw - TabWidget already has good styling
    // We can enhance this later if needed
    TabWidget::draw(ctx);
    
    // TODO: Add Fluent Design indicator animation
    // This would require accessing TabWidget internals or
    // reimplementing the tab header rendering
}

NAMESPACE_END(nanogui)
