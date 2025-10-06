/*
    nanogui/material.h -- Convenience header for Fluent Design components

    This header includes all Fluent Design components for easy integration.
    Include this single header to access all Fluent Design widgets.

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

// Core Fluent Design
#include <nanogui/fluent_theme.h>
#include <nanogui/fluent_easing.h>
#include <nanogui/fluent_typography.h>
#include <nanogui/fluent_ripple.h>

// Fluent Design Materials & Effects
#include <nanogui/fluent_acrylic.h>
#include <nanogui/fluent_mica.h>
#include <nanogui/fluent_reveal.h>
#include <nanogui/fluent_animation.h>

// Buttons & Actions
#include <nanogui/fluent_button.h>
#include <nanogui/fluent_fab.h>
#include <nanogui/fluent_icon_button.h>
#include <nanogui/fluent_switch.h>
#include <nanogui/fluent_checkbox.h>
#include <nanogui/fluent_radio.h>
#include <nanogui/fluent_segmented_button.h>

// Containment
#include <nanogui/fluent_card.h>
#include <nanogui/fluent_dialog.h>
#include <nanogui/fluent_app_bar.h>
#include <nanogui/fluent_menu.h>

// Navigation
#include <nanogui/fluent_tabs.h>
#include <nanogui/fluent_list.h>
#include <nanogui/fluent_list_item.h>
#include <nanogui/fluent_navigation_rail.h>
#include <nanogui/fluent_bottom_navigation.h>
#include <nanogui/fluent_navigation_drawer.h>

// Input
#include <nanogui/fluent_text_field.h>
#include <nanogui/fluent_slider.h>
#include <nanogui/fluent_date_picker.h>
#include <nanogui/fluent_time_picker.h>
#include <nanogui/fluent_autocomplete.h>

// Data Display
#include <nanogui/fluent_avatar.h>
#include <nanogui/fluent_badge.h>
#include <nanogui/fluent_divider.h>
#include <nanogui/fluent_tooltip.h>
#include <nanogui/fluent_data_table.h>
#include <nanogui/fluent_carousel.h>
#include <nanogui/fluent_stepper.h>
#include <nanogui/fluent_tree_view.h>

// Feedback
#include <nanogui/fluent_snackbar.h>
#include <nanogui/fluent_circular_progress.h>
#include <nanogui/fluent_progress_bar.h>
#include <nanogui/fluent_bottom_sheet.h>
#include <nanogui/fluent_banner.h>

// Selection
#include <nanogui/fluent_chip.h>

// Advanced
#include <nanogui/fluent_search_bar.h>
#include <nanogui/fluent_expansion_panel.h>
#include <nanogui/fluent_timeline.h>

/**
 * \file material.h
 * 
 * \brief Convenience header for all Fluent Design components
 * 
 * This header provides a single include for all Fluent Design 3 components.
 * 
 * Usage:
 * \code
 * #include <nanogui/nanogui.h>
 * #include <nanogui/material.h>
 * 
 * using namespace nanogui;
 * 
 * // Create Material-themed application
 * Screen *screen = new Screen(Vector2i(800, 600), "My App");
 * FluentTheme *theme = new FluentTheme(screen->nvg_context());
 * screen->set_theme(theme);
 * 
 * // Use Material components
 * auto *button = new FluentButton(screen, "Click Me", 0, 
 *                                    FluentButton::Style::Filled);
 * \endcode
 */
