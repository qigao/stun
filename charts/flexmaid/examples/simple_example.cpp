#include <flex/modules/flexmaid/flexmaid.h>
#include <iostream>
#include <fstream>

using namespace flex::modules::flexmaid;

int main() {
    std::cout << "=== FlexMaid Simple Example ===\n\n";
    
    FlexMaid maid;
    
    // 示例1：简单流程图
    std::cout << "1. Simple Flowchart:\n";
    std::string flowchart = R"(
        flowchart TD
            A[Start] --> B{Is it working?}
            B -->|Yes| C[Great!]
            B -->|No| D[Fix it]
            D --> B
            C --> E[End]
    )";
    
    std::string svg = maid.mermaid_to_svg(flowchart);
    std::ofstream file1("example_flowchart.svg");
    file1 << svg;
    file1.close();
    std::cout << "   ✓ Generated example_flowchart.svg\n";
    
    // 示例2：序列图
    std::cout << "\n2. Sequence Diagram:\n";
    std::string sequence = R"(
        sequenceDiagram
            Alice --> Bob: Hello Bob!
            Bob --> Alice: Hello Alice!
            Alice --> Bob: How are you?
            Bob --> Alice: I'm good, thanks!
    )";
    
    svg = maid.mermaid_to_svg(sequence);
    std::ofstream file2("example_sequence.svg");
    file2 << svg;
    file2.close();
    std::cout << "   ✓ Generated example_sequence.svg\n";
    
    // 示例3：使用深色主题
    std::cout << "\n3. Dark Theme Example:\n";
    maid.set_theme(Theme::dark());
    
    std::string class_diagram = R"(
        classDiagram
            Animal --> Dog
            Animal --> Cat
            Dog --> Puppy
    )";
    
    svg = maid.mermaid_to_svg(class_diagram);
    std::ofstream file3("example_class_dark.svg");
    file3 << svg;
    file3.close();
    std::cout << "   ✓ Generated example_class_dark.svg (dark theme)\n";
    
    // 示例4：现代主题
    std::cout << "\n4. Modern Theme Example:\n";
    maid.set_theme(Theme::modern());
    
    std::string state = R"(
        stateDiagram-v2
            [*] --> Idle
            Idle --> Processing
            Processing --> Success
            Processing --> Error
            Success --> [*]
            Error --> Retry
            Retry --> Processing
    )";
    
    svg = maid.mermaid_to_svg(state);
    std::ofstream file4("example_state_modern.svg");
    file4 << svg;
    file4.close();
    std::cout << "   ✓ Generated example_state_modern.svg (modern theme)\n";
    
    // 示例5：错误处理
    std::cout << "\n5. Error Handling Example:\n";
    std::string invalid = "this is not valid mermaid syntax";
    auto result = maid.parse(invalid);
    if (!result.success) {
        std::cout << "   ✓ Correctly detected error: " << result.get_error() << "\n";
    }
    
    std::cout << "\n🎉 All examples completed successfully!\n";
    std::cout << "\nGenerated files:\n";
    std::cout << "  - example_flowchart.svg\n";
    std::cout << "  - example_sequence.svg\n";
    std::cout << "  - example_class_dark.svg\n";
    std::cout << "  - example_state_modern.svg\n";
    
    return 0;
}