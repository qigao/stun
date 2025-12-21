#include <flex/flex.h>
#include <flex/shape.h>
#include <flex/group.h>
#include <flex/artboard.h>
#include <iostream>
#include <cassert>

/**
 * This test verifies that the Flex engine safely handles the scenario where
 * a node is destroyed (e.g., removed from the scene) while it is being 
 * tracked by the event system (e.g., as the pointer_down_node).
 */
void test_node_destruction_during_click() {
    std::cout << "Running test_node_destruction_during_click..." << std::endl;

    flex::init();

    // Create a simple scene
    auto instance = flex::Instance::create(800, 600);
    auto artboard = instance->artboard();

    auto shape = flex::Shape::create();
    shape->set_rect(100, 100);
    shape->set_position(10, 10);
    shape->set_id("target_node");

    artboard->add_child(shape);

    // 1. Trigger pointer down on the node
    std::cout << "Sending pointer down..." << std::endl;
    instance->send_pointer_event(15, 15, true); 
    
    // 2. Remove the node from the scene while pointer is still down
    // In a real app, this might happen in a callback or due to state change
    std::cout << "Removing node from scene..." << std::endl;
    artboard->root()->remove_child(shape.get());
    
    // 3. Reset our shared_ptr to ensure the node is truly destroyed (if Instance doesn't hold it)
    shape.reset();

    // 4. Trigger pointer up
    // This previously caused a crash because Instance held a raw pointer to the destroyed node
    std::cout << "Sending pointer up (should not crash)..." << std::endl;
    instance->send_pointer_event(15, 15, false);

    std::cout << "Test passed!" << std::endl;
    
    flex::shutdown();
}

int main() {
    try {
        test_node_destruction_during_click();
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
    return 0;
}
