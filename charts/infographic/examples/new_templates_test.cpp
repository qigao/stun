#include <flexinfographic.h>
#include <iostream>
#include <fstream>

using namespace flex::modules::infographic;

int main() {
    std::cout << "Testing New Infographic Templates\n";
    std::cout << "=================================\n\n";
    
    FlexInfographic flex;
    
    // 测试 1: 地理信息图表
    std::cout << "1. Testing Geographic Template...\n";
    
    std::string geographic_test = R"(
infographic geographic-regional-data
data
  title Global Sales Distribution
  desc Q4 2024 regional performance
  items
    - label North America
      value 45.2
      desc $45.2M revenue
    - label Europe
      value 32.8
      desc $32.8M revenue
    - label Asia Pacific
      value 28.5
      desc $28.5M revenue
    - label Latin America
      value 12.1
      desc $12.1M revenue
theme
  palette #3b82f6 #10b981 #f59e0b #ef4444
)";
    
    std::cout << "Geographic infographic syntax:\n";
    std::cout << geographic_test << "\n";
    
    std::string svg1 = flex.infographic_to_svg(geographic_test);
    std::ofstream file1("geographic_test.svg");
    file1 << svg1;
    file1.close();
    std::cout << "✓ Saved to geographic_test.svg\n\n";
    
    // 测试 2: 流程图模板
    std::cout << "2. Testing Flowchart Template...\n";
    
    std::string flowchart_test = R"(
infographic flowchart-decision-tree
data
  title Customer Support Decision Tree
  desc Automated support routing system
  items
    - label Start: Customer Contact
    - label Input: Collect Issue Details
    - label Decision: Is it a technical issue?
    - label Process: Route to Tech Support
    - label Process: Route to General Support
    - label Document: Create Support Ticket
    - label End: Issue Resolved
theme
  palette #6366f1 #06b6d4 #10b981 #f59e0b #ef4444 #8b5cf6 #10b981
)";
    
    std::cout << "Flowchart infographic syntax:\n";
    std::cout << flowchart_test << "\n";
    
    std::string svg2 = flex.infographic_to_svg(flowchart_test);
    std::ofstream file2("flowchart_test.svg");
    file2 << svg2;
    file2.close();
    std::cout << "✓ Saved to flowchart_test.svg\n\n";
    
    // 测试 3: 过程模板 - 步骤流程
    std::cout << "3. Testing Process Step Flow Template...\n";
    
    std::string process_step_test = R"(
infographic process-step-by-step
data
  title Product Development Process
  desc From idea to market launch
  items
    - label Research
      desc Market analysis and user research
    - label Design
      desc UI/UX design and prototyping
    - label Development
      desc Code implementation and testing
    - label Launch
      desc Marketing and product release
theme
  palette #8b5cf6 #06b6d4 #10b981 #f59e0b
)";
    
    std::cout << "Process step flow syntax:\n";
    std::cout << process_step_test << "\n";
    
    std::string svg3 = flex.infographic_to_svg(process_step_test);
    std::ofstream file3("process_step_test.svg");
    file3 << svg3;
    file3.close();
    std::cout << "✓ Saved to process_step_test.svg\n\n";
    
    // 测试 4: 过程模板 - 教程布局
    std::cout << "4. Testing Process Tutorial Template...\n";
    
    std::string process_tutorial_test = R"(
infographic process-how-to-guide
data
  title How to Set Up Your Account
  desc Step-by-step account setup guide
  items
    - label Create Account
      desc Sign up with email and password
    - label Verify Email
      desc Check your inbox and click verification link
    - label Complete Profile
      desc Add your personal information and preferences
    - label Start Using
      desc Explore features and begin your journey
theme
  palette #3b82f6 #10b981 #f59e0b #ef4444
)";
    
    std::cout << "Process tutorial syntax:\n";
    std::cout << process_tutorial_test << "\n";
    
    std::string svg4 = flex.infographic_to_svg(process_tutorial_test);
    std::ofstream file4("process_tutorial_test.svg");
    file4 << svg4;
    file4.close();
    std::cout << "✓ Saved to process_tutorial_test.svg\n\n";
    
    std::cout << "🎉 All new template tests completed!\n";
    std::cout << "Check the generated SVG files to verify the new functionality.\n";
    
    return 0;
}