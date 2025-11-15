#include <catch2/catch_test_macros.hpp>
#include <whiteboard/ddf/data_layer.h>

using namespace whiteboard::ddf;

TEST_CASE("DataLayer - Node CRUD operations", "[data_layer]") {
    DataLayer layer;

    SECTION("Add and retrieve node") {
        DataNode node{"node1", "person", {{"name", "John"}, {"age", "30"}}};
        layer.add_node(node);

        auto* retrieved = layer.get_node("node1");
        REQUIRE(retrieved != nullptr);
        REQUIRE(retrieved->id == "node1");
        REQUIRE(retrieved->type == "person");
        REQUIRE(retrieved->properties["name"] == "John");
        REQUIRE(retrieved->properties["age"] == "30");
    }

    SECTION("Update node properties") {
        DataNode node{"node1", "person", {{"name", "John"}, {"age", "30"}}};
        layer.add_node(node);

        layer.update_node("node1", {{"age", "31"}, {"city", "NYC"}});

        auto* updated = layer.get_node("node1");
        REQUIRE(updated != nullptr);
        REQUIRE(updated->properties["name"] == "John");  // Unchanged
        REQUIRE(updated->properties["age"] == "31");     // Updated
        REQUIRE(updated->properties["city"] == "NYC");   // Added
    }

    SECTION("Remove node") {
        DataNode node{"node1", "person", {{"name", "John"}}};
        layer.add_node(node);

        layer.remove_node("node1");

        auto* retrieved = layer.get_node("node1");
        REQUIRE(retrieved == nullptr);
    }

    SECTION("Get non-existent node returns nullptr") {
        auto* retrieved = layer.get_node("nonexistent");
        REQUIRE(retrieved == nullptr);
    }

    SECTION("Update non-existent node does nothing") {
        layer.update_node("nonexistent", {{"key", "value"}});
        auto* retrieved = layer.get_node("nonexistent");
        REQUIRE(retrieved == nullptr);
    }
}

TEST_CASE("DataLayer - Relationship management", "[data_layer]") {
    DataLayer layer;

    SECTION("Add and retrieve relationship") {
        DataRelationship rel{"rel1", "reports_to", "node2", "node1", {{"since", "2020"}}};
        layer.add_relationship(rel);

        auto* retrieved = layer.get_relationship("rel1");
        REQUIRE(retrieved != nullptr);
        REQUIRE(retrieved->id == "rel1");
        REQUIRE(retrieved->type == "reports_to");
        REQUIRE(retrieved->from_node_id == "node2");
        REQUIRE(retrieved->to_node_id == "node1");
        REQUIRE(retrieved->properties["since"] == "2020");
    }

    SECTION("Remove relationship") {
        DataRelationship rel{"rel1", "reports_to", "node2", "node1", {}};
        layer.add_relationship(rel);

        layer.remove_relationship("rel1");

        auto* retrieved = layer.get_relationship("rel1");
        REQUIRE(retrieved == nullptr);
    }

    SECTION("Get relationships for node") {
        DataRelationship rel1{"rel1", "reports_to", "node2", "node1", {}};
        DataRelationship rel2{"rel2", "reports_to", "node3", "node1", {}};
        DataRelationship rel3{"rel3", "peer", "node2", "node4", {}};

        layer.add_relationship(rel1);
        layer.add_relationship(rel2);
        layer.add_relationship(rel3);

        auto rels = layer.get_relationships_for_node("node1");
        REQUIRE(rels.size() == 2);

        auto rels_node2 = layer.get_relationships_for_node("node2");
        REQUIRE(rels_node2.size() == 2);  // rel1 and rel3

        auto rels_node4 = layer.get_relationships_for_node("node4");
        REQUIRE(rels_node4.size() == 1);  // rel3
    }
}

TEST_CASE("DataLayer - Query operations", "[data_layer]") {
    DataLayer layer;

    // Setup test data
    layer.add_node({"node1", "person", {{"name", "John"}, {"dept", "Engineering"}}});
    layer.add_node({"node2", "person", {{"name", "Jane"}, {"dept", "Engineering"}}});
    layer.add_node({"node3", "person", {{"name", "Bob"}, {"dept", "Sales"}}});
    layer.add_node({"node4", "company", {{"name", "Acme Corp"}}});

    SECTION("Query nodes by type") {
        auto persons = layer.query_nodes("person");
        REQUIRE(persons.size() == 3);

        auto companies = layer.query_nodes("company");
        REQUIRE(companies.size() == 1);
        REQUIRE(companies[0]->id == "node4");

        auto nonexistent = layer.query_nodes("nonexistent");
        REQUIRE(nonexistent.empty());
    }

    SECTION("Query nodes by property") {
        auto engineering = layer.query_nodes_by_property("dept", "Engineering");
        REQUIRE(engineering.size() == 2);

        auto sales = layer.query_nodes_by_property("dept", "Sales");
        REQUIRE(sales.size() == 1);
        REQUIRE(sales[0]->id == "node3");

        auto nonexistent = layer.query_nodes_by_property("dept", "Marketing");
        REQUIRE(nonexistent.empty());
    }

    SECTION("Query by non-existent property key") {
        auto result = layer.query_nodes_by_property("nonexistent_key", "value");
        REQUIRE(result.empty());
    }
}

TEST_CASE("DataLayer - Get all nodes", "[data_layer]") {
    DataLayer layer;

    SECTION("Empty layer returns empty vector") {
        auto nodes = layer.get_all_nodes();
        REQUIRE(nodes.empty());
    }

    SECTION("Returns all added nodes") {
        layer.add_node({"node1", "person", {}});
        layer.add_node({"node2", "person", {}});
        layer.add_node({"node3", "company", {}});

        auto nodes = layer.get_all_nodes();
        REQUIRE(nodes.size() == 3);
    }
}

TEST_CASE("DataLayer - Clear operation", "[data_layer]") {
    DataLayer layer;

    layer.add_node({"node1", "person", {}});
    layer.add_node({"node2", "person", {}});
    layer.add_relationship({"rel1", "reports_to", "node2", "node1", {}});

    layer.clear();

    REQUIRE(layer.get_all_nodes().empty());
    REQUIRE(layer.get_relationship("rel1") == nullptr);
}

TEST_CASE("DataLayer - Const correctness", "[data_layer]") {
    DataLayer layer;
    layer.add_node({"node1", "person", {{"name", "John"}}});
    layer.add_relationship({"rel1", "reports_to", "node2", "node1", {}});

    const DataLayer& const_layer = layer;

    SECTION("Const get_node") {
        const auto* node = const_layer.get_node("node1");
        REQUIRE(node != nullptr);
        REQUIRE(node->id == "node1");
    }

    SECTION("Const get_all_nodes") {
        auto nodes = const_layer.get_all_nodes();
        REQUIRE(nodes.size() == 1);
    }

    SECTION("Const query_nodes") {
        auto persons = const_layer.query_nodes("person");
        REQUIRE(persons.size() == 1);
    }

    SECTION("Const query_nodes_by_property") {
        auto johns = const_layer.query_nodes_by_property("name", "John");
        REQUIRE(johns.size() == 1);
    }

    SECTION("Const get_relationship") {
        const auto* rel = const_layer.get_relationship("rel1");
        REQUIRE(rel != nullptr);
        REQUIRE(rel->id == "rel1");
    }

    SECTION("Const get_relationships_for_node") {
        auto rels = const_layer.get_relationships_for_node("node1");
        REQUIRE(rels.size() == 1);
    }
}
