#include "flexui/document.h"

#include <catch2/catch_test_macros.hpp>

using namespace flexui;

TEST_CASE("FlexDocument - Node creation and lookup", "[document]") {
  FlexDocument doc;

  SECTION("Create node with explicit ID") {
    FlexNodeDesc desc;
    desc.id = "test-node";
    desc.tag = "rect";

    auto &node = doc.appendNode(desc);

    REQUIRE(node.id() == "test-node");
    REQUIRE(node.tag() == "rect");

    auto *found = doc.findNode("test-node");
    REQUIRE(found != nullptr);
    REQUIRE(found->id() == "test-node");
  }

  SECTION("Create node with auto-generated ID") {
    FlexNodeDesc desc;
    desc.tag = "circle";

    auto &node = doc.appendNode(desc);

    REQUIRE(!node.id().empty());
    REQUIRE(node.tag() == "circle");

    auto *found = doc.findNode(node.id());
    REQUIRE(found != nullptr);
    REQUIRE(found == &node);
  }

  SECTION("Create child nodes") {
    FlexNodeDesc parent_desc;
    parent_desc.id = "parent";
    parent_desc.tag = "div";
    auto &parent = doc.appendNode(parent_desc);

    FlexNodeDesc child_desc;
    child_desc.id = "child";
    child_desc.tag = "rect";
    auto &child = doc.appendNode(child_desc, "parent");

    REQUIRE(child.parent() == &parent);
    REQUIRE(parent.children().size() == 1);
    REQUIRE(parent.children()[0]->id() == "child");
  }

  SECTION("Find non-existent node returns nullptr") {
    auto *found = doc.findNode("does-not-exist");
    REQUIRE(found == nullptr);
  }

  SECTION("Duplicate ID throws exception") {
    FlexNodeDesc desc1;
    desc1.id = "duplicate";
    desc1.tag = "rect";
    doc.appendNode(desc1);

    FlexNodeDesc desc2;
    desc2.id = "duplicate";
    desc2.tag = "circle";
    REQUIRE_THROWS_AS(doc.appendNode(desc2), std::runtime_error);
  }
}

TEST_CASE("FlexDocument - Node removal", "[document]") {
  FlexDocument doc;

  SECTION("Remove single node") {
    FlexNodeDesc desc;
    desc.id = "node1";
    desc.tag = "rect";
    doc.appendNode(desc);

    REQUIRE(doc.findNode("node1") != nullptr);

    bool removed = doc.removeNode("node1");
    REQUIRE(removed);
    REQUIRE(doc.findNode("node1") == nullptr);
  }

  SECTION("Remove non-existent node returns false") {
    bool removed = doc.removeNode("does-not-exist");
    REQUIRE(!removed);
  }

  SECTION("Remove node with children") {
    FlexNodeDesc parent_desc;
    parent_desc.id = "parent";
    parent_desc.tag = "div";
    auto &parent = doc.appendNode(parent_desc);

    FlexNodeDesc child1_desc;
    child1_desc.id = "child1";
    child1_desc.tag = "rect";
    doc.appendNode(child1_desc, "parent");

    FlexNodeDesc child2_desc;
    child2_desc.id = "child2";
    child2_desc.tag = "circle";
    doc.appendNode(child2_desc, "parent");

    REQUIRE(parent.children().size() == 2);

    bool removed = doc.removeNode("parent");
    REQUIRE(removed);
    REQUIRE(doc.findNode("parent") == nullptr);
    REQUIRE(doc.findNode("child1") == nullptr);
    REQUIRE(doc.findNode("child2") == nullptr);
  }

  SECTION("Remove child from parent") {
    FlexNodeDesc parent_desc;
    parent_desc.id = "parent";
    parent_desc.tag = "div";
    doc.appendNode(parent_desc);

    FlexNodeDesc child_desc;
    child_desc.id = "child";
    child_desc.tag = "rect";
    doc.appendNode(child_desc, "parent");

    bool removed = doc.removeChild("parent", "child");
    REQUIRE(removed);
    REQUIRE(doc.findNode("child") == nullptr);
    REQUIRE(doc.findNode("parent") != nullptr);
  }

  SECTION("Remove child from wrong parent returns false") {
    FlexNodeDesc parent1_desc;
    parent1_desc.id = "parent1";
    parent1_desc.tag = "div";
    doc.appendNode(parent1_desc);

    FlexNodeDesc parent2_desc;
    parent2_desc.id = "parent2";
    parent2_desc.tag = "div";
    doc.appendNode(parent2_desc);

    FlexNodeDesc child_desc;
    child_desc.id = "child";
    child_desc.tag = "rect";
    doc.appendNode(child_desc, "parent1");

    bool removed = doc.removeChild("parent2", "child");
    REQUIRE(!removed);
    REQUIRE(doc.findNode("child") != nullptr);
  }
}

TEST_CASE("FlexDocument - Node properties", "[document]") {
  FlexDocument doc;

  SECTION("Node classes") {
    FlexNodeDesc desc;
    desc.id = "node";
    desc.classes = {"class1", "class2"};

    auto &node = doc.appendNode(desc);

    REQUIRE(node.classes().size() == 2);
    REQUIRE(node.hasClass("class1"));
    REQUIRE(node.hasClass("class2"));
    REQUIRE(!node.hasClass("class3"));
  }


  SECTION("Node text") {
    FlexNodeDesc desc;
    desc.id = "node";
    desc.text = "Hello, World!";

    auto &node = doc.appendNode(desc);

    REQUIRE(node.text() == "Hello, World!");
  }
}

TEST_CASE("FlexDocument - Dynamic mutations", "[document]") {
  FlexDocument doc;
  FlexNodeDesc desc;
  desc.id = "node";
  desc.tag = "rect";
  doc.appendNode(desc);

  SECTION("Add class") {
    doc.setClass("node", "active", true);
    auto *node = doc.findNode("node");
    REQUIRE(node->hasClass("active"));
  }

  SECTION("Remove class") {
    FlexNodeDesc desc2;
    desc2.id = "node2";
    desc2.classes = {"class1", "class2"};
    doc.appendNode(desc2);

    doc.setClass("node2", "class1", false);
    auto *node = doc.findNode("node2");
    REQUIRE(!node->hasClass("class1"));
    REQUIRE(node->hasClass("class2"));
  }


  SECTION("Set text") {
    doc.setText("node", "New text");
    auto *node = doc.findNode("node");
    REQUIRE(node->text() == "New text");
  }
}

TEST_CASE("FlexDocument - Traversal", "[document]") {
  FlexDocument doc;

  FlexNodeDesc root1_desc;
  root1_desc.id = "root1";
  root1_desc.tag = "div";
  doc.appendNode(root1_desc);

  FlexNodeDesc root2_desc;
  root2_desc.id = "root2";
  root2_desc.tag = "div";
  doc.appendNode(root2_desc);

  FlexNodeDesc child1_desc;
  child1_desc.id = "child1";
  child1_desc.tag = "rect";
  doc.appendNode(child1_desc, "root1");

  FlexNodeDesc child2_desc;
  child2_desc.id = "child2";
  child2_desc.tag = "circle";
  doc.appendNode(child2_desc, "root1");

  SECTION("Traverse all nodes") {
    int count = 0;
    doc.traverse([&](FlexNode &node) { count++; });

    REQUIRE(count == 4); // 2 roots + 2 children
  }

  SECTION("Const traverse") {
    const auto &const_doc = doc;
    int count = 0;
    const_doc.traverse([&](const FlexNode &node) { count++; });

    REQUIRE(count == 4);
  }
}

TEST_CASE("FlexDocument - Memory safety", "[document][safety]") {
  FlexDocument doc;

  SECTION("Element pointer without renderer returns nullptr") {
    FlexNodeDesc desc;
    desc.id = "node";
    desc.tag = "rect";
    auto &node = doc.appendNode(desc);

    // Without renderer, element() should return nullptr
    REQUIRE(node.element() == nullptr);
  }

  SECTION("Find node after removal returns nullptr") {
    FlexNodeDesc desc;
    desc.id = "temp";
    desc.tag = "rect";
    doc.appendNode(desc);
    doc.removeNode("temp");

    REQUIRE(doc.findNode("temp") == nullptr);
  }
}
