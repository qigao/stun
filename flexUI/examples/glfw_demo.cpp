/*
 * GLFW Demo - Example using GlfwApp template with Flex DSL
 */

#include <flex/app/glfw_app.h>
#include <flex.h>
#include "backends/thorvg/init.h"

class Demo : public flex::GlfwApp {
public:
    Demo() : GlfwApp("GLFW + Flex Demo", 800, 600) {}

protected:
    bool on_init() override {
        flex::init();
        load_font("Arial", "C:/Windows/Fonts/arial.ttf");
        flex::load_font("sans-serif", "C:/Windows/Fonts/arial.ttf");
        
        def_ = flex::Definition::load_file("hello.flex");
        if (!def_ || def_->has_error()) {
            std::cerr << "Load failed: " << (def_ ? def_->error_message() : "null") << std::endl;
            return false;
        }
        
        inst_ = flex::Instance::create(def_);
        std::cout << "GLFW + Flex Demo ready!" << std::endl;
        return true;
    }

    void on_update(float dt) override {
        if (inst_) inst_->advance(dt);
    }

    void on_render() override {
        if (inst_ && renderer()) {
            renderer()->begin_frame(width(), height(), 1.0f);
            if (inst_->scene()) {
                renderer()->clear(inst_->scene()->background());
            }
            inst_->render(*renderer());
            renderer()->end_frame();
        }
        canvas()->draw();
        canvas()->sync();
    }

private:
    flex::Definition::Ptr def_;
    flex::Instance::Ptr inst_;
};

int main() {
    Demo demo;
    if (!demo.init()) return 1;
    demo.run();
    return 0;
}
