#include <flexUI/ui_node_schema.h>
#include <tinytest.hpp>

#include <string>
#include <utility>

namespace {
using flexUI::UiContentModel;
using flexUI::UiDocumentErrorCode;
using flexUI::UiNodeDescriptor;
using flexUI::UiNodeKind;

flexUI::UiNodeDefinition node() {
  flexUI::UiNodeDefinition value;
  value.tag = "sample";
  value.id = "subject";
  value.source = {7, 11, 6};
  return value;
}

UiNodeDescriptor descriptor(UiContentModel content) {
  return {UiNodeKind::Widget, content};
}
} // namespace

spec("FlexUI node content models have no construction side effects") {
  it("accepts only supported descriptor enum values") {
    for (const auto kind : {UiNodeKind::Container, UiNodeKind::Widget}) {
      for (const auto model : {UiContentModel::Empty, UiContentModel::Text,
                              UiContentModel::SingleChild, UiContentModel::Children,
                              UiContentModel::TextAndChildren}) {
        check_true((UiNodeDescriptor{kind, model}.valid()));
      }
    }
    check_false((UiNodeDescriptor{static_cast<UiNodeKind>(99), UiContentModel::Empty}.valid()));
    auto invalid = descriptor(static_cast<UiContentModel>(99));
    check_false(invalid.valid());
    check(invalid.validate_content(node()).code == UiDocumentErrorCode::InvalidNode);
  }

  it("rejects children and every text declaration on empty nodes") {
    auto value = node();
    const auto schema = descriptor(UiContentModel::Empty);
    check_false(static_cast<bool>(schema.validate_content(value)));
    value.children.push_back(node());
    check(schema.validate_content(value).code == UiDocumentErrorCode::InvalidNode);
    value.children.clear();
    for (const char *key : {"text", "content", "bind.text"}) {
      value.properties.emplace(key, std::string{});
      check(schema.validate_content(value).code == UiDocumentErrorCode::InvalidProperty);
      value.properties.clear();
    }
  }

  it("preserves Unicode and embedded NUL in permitted text values") {
    auto value = node();
    const std::string text = std::string(u8"保存😀é") + '\0' + "tail";
    const auto schema = descriptor(UiContentModel::Text);
    check_false(static_cast<bool>(schema.validate_content(value)));
    for (const char *key : {"text", "content", "bind.text"}) {
      value.properties.emplace(key, text);
      check_false(static_cast<bool>(schema.validate_content(value)));
      check_equal(std::get<std::string>(value.properties.at(key)), text);
      value.properties.clear();
    }
    value.children.push_back(node());
    check(schema.validate_content(value).code == UiDocumentErrorCode::InvalidNode);
  }

  it("rejects non-string text and text bindings without coercion") {
    auto value = node();
    const auto schema = descriptor(UiContentModel::Text);
    for (const char *key : {"text", "content", "bind.text"}) {
      value.properties[key] = true;
      check(schema.validate_content(value).code == UiDocumentErrorCode::InvalidProperty);
      value.properties[key] = 1.0F;
      check(schema.validate_content(value).code == UiDocumentErrorCode::InvalidProperty);
      check_true(std::holds_alternative<float>(value.properties.at(key)));
      value.properties.clear();
    }
  }

  it("allows zero or multiple children but not scalar text on child containers") {
    auto value = node();
    const auto schema = descriptor(UiContentModel::Children);
    check_false(static_cast<bool>(schema.validate_content(value)));
    value.children = {node(), node()};
    check_false(static_cast<bool>(schema.validate_content(value)));
    value.properties["text"] = std::string("not child content");
    check(schema.validate_content(value).code == UiDocumentErrorCode::InvalidProperty);
  }

  it("requires exactly one child for a single-child content model") {
    auto value = node();
    const auto schema = descriptor(UiContentModel::SingleChild);
    check(schema.validate_content(value).code == UiDocumentErrorCode::InvalidNode);
    value.children.push_back(node());
    check_false(static_cast<bool>(schema.validate_content(value)));
    value.properties["content"] = std::string{};
    check(schema.validate_content(value).code == UiDocumentErrorCode::InvalidProperty);
    value.properties.clear();
    value.children.push_back(node());
    check(schema.validate_content(value).code == UiDocumentErrorCode::InvalidNode);
  }

  it("permits explicitly declared text together with composite children") {
    auto value = node();
    value.children = {node(), node()};
    value.properties["text"] = std::string("Caption");
    const auto schema = descriptor(UiContentModel::TextAndChildren);
    check_false(static_cast<bool>(schema.validate_content(value)));
    check_equal(value.children.size(), 2);
    value.properties["text"] = false;
    check(schema.validate_content(value).code == UiDocumentErrorCode::InvalidProperty);
  }

  it("reports the offending attribute location or the owning node location") {
    auto value = node();
    value.properties["text"] = std::string{};
    value.property_spans["text"] = {12, 23, 4};
    const auto schema = descriptor(UiContentModel::Empty);
    auto error = schema.validate_content(value);
    check_equal(error.line, 12);
    check_equal(error.column, 23);
    value.property_spans.clear();
    error = schema.validate_content(value);
    check_equal(error.line, 7);
    check_equal(error.column, 11);
    value.properties.clear();
    value.children.push_back(node());
    error = schema.validate_content(value);
    check_equal(error.line, 7);
    check_equal(error.column, 11);
  }

  it("leaves unrelated widget property schemas to their owning validation stage") {
    auto value = node();
    value.properties["value"] = std::string("Input value");
    value.properties["src"] = std::string("image.png");
    value.properties["disabled"] = true;
    check_false(static_cast<bool>(descriptor(UiContentModel::Empty).validate_content(value)));
    check_equal(value.properties.size(), 3);
  }

  it("does not mutate rejected definitions or their source metadata") {
    auto value = node();
    value.properties["text"] = std::string("must survive rejection");
    const auto original = value.properties;
    const auto error = descriptor(UiContentModel::Empty).validate_content(value);
    check(error.code == UiDocumentErrorCode::InvalidProperty);
    check_true(value.properties == original);
    check_equal(value.id, "subject");
    check_equal(value.tag, "sample");
    check_equal(value.source.line, 7);
    check_equal(value.source.column, 11);
    check_equal(value.source.length, 6);
  }
}
