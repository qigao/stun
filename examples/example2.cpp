/*
    examples/example2.cpp -- C++ version of an example application that shows
    how to use the form helper class. For a Python implementation, see
    "../python/example2.py".

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    The widget drawing code is based on the NanoVG demo application
    by Mikko Mononen.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include <nanogui.h>
#include <memory>
#include <iostream>

using namespace nanogui;

enum class ExampleItem {
    Item1 = 0,
    Item2,
    Item3
};

class ExampleFormApp final : public Screen {
public:
    explicit ExampleFormApp(bool use_gl_4_1)
        : Screen(screen_size(use_gl_4_1), screen_caption(use_gl_4_1),
                 /* resizable */ true, /* maximized */ false,
                 /* fullscreen */ false, /* depth_buffer */ true,
                 /* stencil_buffer */ true, /* float_buffer */ false,
                 use_gl_4_1 ? 4u : 3u, use_gl_4_1 ? 1u : 2u) {
        inc_ref();
        build_interface();
    }

private:
    struct FormState {
        bool show_group = true;
        bool bool_value = true;
        int int_value = 12345678;
        double double_value = 3.1415926;
        float float_value = static_cast<float>(double_value);
        std::string text_value = "A string";
        std::string placeholder_value;
        ExampleItem enum_value = ExampleItem::Item2;
        Color color_value = Color(0.5f, 0.5f, 0.7f, 1.f);
    } m_state;

    std::unique_ptr<FormHelper> m_form;
    ref<Window> m_window;

    static Vector2i screen_size(bool use_gl_4_1) {
        return use_gl_4_1 ? Vector2i(500, 700) : Vector2i(500, 700);
    }

    static std::string screen_caption(bool use_gl_4_1) {
        return use_gl_4_1 ? "NanoGUI test [GL 4.1]" : "NanoGUI test";
    }

    void build_interface() {
        m_form = std::make_unique<FormHelper>(this);
        m_window = m_form->add_window(Vector2i(10, 10), "Form helper example");

        m_form->add_group("Basic types");
        m_form->add_variable("bool", m_state.bool_value);
        m_form->add_variable("string", m_state.text_value);
        m_form->add_variable("placeholder", m_state.placeholder_value)->set_placeholder("placeholder");

        m_form->add_group("Validating fields");
        m_form->add_variable("int", m_state.int_value)->set_spinnable(true);
        m_form->add_variable("float", m_state.float_value);
        m_form->add_variable("double", m_state.double_value)->set_spinnable(true);

        m_form->add_group("Complex types");
        auto enum_widget = m_form->add_variable("Enumeration", m_state.enum_value, m_state.show_group);
        enum_widget->set_items({"Item 1", "Item 2", "Item 3"});
        m_form->add_variable("Color", m_state.color_value);

        m_form->add_group("Other widgets");
        m_form->add_button("A button", [] { std::cout << "Button pressed." << std::endl; });

        set_visible(true);
        perform_layout();
        if (m_window)
            m_window->center();
    }
};

int main(int /* argc */, char ** /* argv */) {
    nanogui::init();

    {
        bool use_gl_4_1 = false; // Set to true to create an OpenGL 4.1 context.
        ref<ExampleFormApp> screen = new ExampleFormApp(use_gl_4_1);
        nanogui::run(RunMode::Lazy);
        screen = nullptr;
    }

    nanogui::shutdown();
    return 0;
}