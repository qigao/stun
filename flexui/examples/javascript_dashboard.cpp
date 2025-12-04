#include <flexui/screen.h>

int main() {
    flexui::Screen screen(1200, 800, "FlexUI Dashboard - shadcn style");

    // Load CSS and XML from files
    screen.loadCSSFile("js_modules/dashboard/dashboard.css");
    screen.loadXMLFile("js_modules/dashboard/dashboard.xml");

    // Load dashboard JS modules
    screen.loadJSModule("js_modules/dashboard/main.js");

    while (screen.pollEvents()) {
        screen.draw();
    }

    return 0;
}
