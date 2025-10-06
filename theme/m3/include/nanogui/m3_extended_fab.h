/*
    nanogui/m3_extended_fab.h -- M3 Extended FAB

    Based on: https://m3.material.io/components/extended-fab
*/

#pragma once

#include <nanogui/button.h>
#include <nanogui/m3_theme.h>

NAMESPACE_BEGIN(nanogui)

class NANOGUI_EXPORT M3ExtendedFAB : public Button {
public:
    enum class Size {
        Regular,  ///< 56dp height
        Large     ///< 96dp height
    };

    M3ExtendedFAB(Widget *parent, const std::string &label, int icon = 0, 
                  Size size = Size::Regular);

    void set_size_type(Size size);
    Size size_type() const { return m_size_type; }

    void set_expanded(bool expanded) { m_expanded = expanded; }
    bool expanded() const { return m_expanded; }

    void draw(NVGcontext *ctx) override;
    Vector2i preferred_size(NVGcontext *ctx) const;

protected:
    M3Theme *m3_theme() const;

    Size m_size_type;
    bool m_expanded = true;
};

NAMESPACE_END(nanogui)
