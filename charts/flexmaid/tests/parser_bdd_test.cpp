#include <flexmaid/flexmaid.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <bdd-for-c.h>

namespace fs = std::filesystem;
using namespace flex::modules::flexmaid;

#ifndef TEST_FIXTURES_DIR
#define TEST_FIXTURES_DIR "../tests/fixtures"
#endif

static std::string read_file(const fs::path& path) {
    std::ifstream f(path);
    if (!f.is_open()) return "";
    std::stringstream buffer;
    buffer << f.rdbuf();
    return buffer.str();
}

spec("Mermaid Parser BDD Tests") {
    FlexMaid maid;
    fs::path fixtures_path(TEST_FIXTURES_DIR);

    describe("Flowchart") {
        it("should parse nodes and edges correctly") {
            // flowchart LR
            //   A([Start]) --> B{Decision}
            //   B -->|yes| C[/Do thing/]
            //   B -->|no| D[\\Skip\\]
            //   C --> E[[End]]
            //   D --> E
            auto result = maid.parse(read_file(fixtures_path / "flowchart/basic.mmd"));
            check(result.success == true);
            check(result.diagram->type == DiagramType::Flowchart);
            check(result.diagram->get_direction() == "LR");
            
            // 5 nodes: A, B, C, D, E
            check(result.diagram->nodes.size() == 5);
            check(result.diagram->nodes.count("A") == 1);
            check(result.diagram->nodes.count("B") == 1);
            check(result.diagram->nodes.count("E") == 1);
            
            // 5 edges: A->B, B->C, B->D, C->E, D->E
            check(result.diagram->edges.size() == 5);
        }

        it("should parse subgraphs with labels") {
            // subgraph GroupA["Alpha Group"]
            //   A1[One] --> A2[Two]
            // end
            auto result = maid.parse(read_file(fixtures_path / "flowchart/subgraph.mmd"));
            check(result.success == true);
            check(result.diagram->subgraphs.size() == 2);
            
            bool found_alpha = false;
            for (const auto& sg : result.diagram->subgraphs) {
                if (sg.label == "Alpha Group") {
                    found_alpha = true;
                    check(sg.node_ids.size() >= 2);
                }
            }
            check(found_alpha == true);
        }
    }

    describe("Sequence Diagram") {
        it("should parse participants and messages") {
            // participant Alice
            // participant Bob
            // Alice->>Bob: Hello Bob
            // Bob-->>Alice: Hi Alice
            auto result = maid.parse(read_file(fixtures_path / "sequence/basic.mmd"));
            check(result.success == true);
            check(result.diagram->type == DiagramType::Sequence);
            
            // 2 participants
            check(result.diagram->nodes.size() == 2);
            check(result.diagram->nodes.count("Alice") == 1);
            check(result.diagram->nodes.count("Bob") == 1);
            
            // 2 messages
            check(result.diagram->edges.size() == 2);
            
            auto& e1 = result.diagram->edges[0];
            check(e1.from == "Alice");
            check(e1.to == "Bob");
            check(e1.label == "Hello Bob");
            
            auto& e2 = result.diagram->edges[1];
            check(e2.from == "Bob");
            check(e2.to == "Alice");
            check(e2.label == "Hi Alice");
            check(e2.style == EdgeStyle::Dashed);
        }
    }

    describe("Class Diagram") {
        it("should parse class with attributes and methods") {
            // class Animal { +String name, +eat() }
            auto result = maid.parse(read_file(fixtures_path / "class/basic.mmd"));
            check(result.success == true);
            check(result.diagram->type == DiagramType::Class);
            
            // 2 classes: Animal, Dog
            check(result.diagram->nodes.size() == 2);
            check(result.diagram->nodes.count("Animal") == 1);
            check(result.diagram->nodes.count("Dog") == 1);
            
            // Animal has attribute and method
            auto& animal = result.diagram->nodes.at("Animal");
            check(animal.get_prop("attr_0") == "+String name");
            check(animal.get_prop("method_0") == "+eat()");
            
            // 1 edge: Animal <|-- Dog
            check(result.diagram->edges.size() == 1);
            check(result.diagram->edges[0].label == "inherits");
        }

        it("should parse multiplicity correctly") {
            // Class01 "1" *-- "many" Class02 : contains
            auto result = maid.parse(read_file(fixtures_path / "class/multiplicity.mmd"));
            check(result.success == true);
            check(result.diagram->nodes.size() == 2);
            check(result.diagram->edges.size() == 1);
            
            auto& edge = result.diagram->edges[0];
            check(edge.get_prop("from_multiplicity") == "1");
            check(edge.get_prop("to_multiplicity") == "many");
            check(edge.label == "contains");
        }
    }

    describe("State Diagram") {
        it("should parse states and transitions") {
            // [*] --> Idle
            // Idle --> Active : start
            // state "Waiting" as Wait
            auto result = maid.parse(read_file(fixtures_path / "state/basic.mmd"));
            check(result.success == true);
            check(result.diagram->type == DiagramType::State);
            
            // [*], Idle, Active, Wait
            check(result.diagram->nodes.size() >= 4);
            check(result.diagram->nodes.count("Idle") == 1);
            check(result.diagram->nodes.count("Active") == 1);
            
            // Check transition label
            bool found_start = false;
            for (const auto& e : result.diagram->edges) {
                if (e.from == "Idle" && e.to == "Active") {
                    check(e.label == "start");
                    found_start = true;
                }
            }
            check(found_start == true);
        }
    }

    describe("ER Diagram") {
        it("should parse entities and relationships") {
            // CUSTOMER ||--o{ ORDER : places
            // CUSTOMER { string id, string name }
            auto result = maid.parse(read_file(fixtures_path / "er/basic.mmd"));
            check(result.success == true);
            check(result.diagram->type == DiagramType::ER);
            
            check(result.diagram->nodes.count("CUSTOMER") == 1);
            check(result.diagram->nodes.count("ORDER") == 1);
            check(result.diagram->edges.size() >= 1);
        }
    }

    describe("Pie Chart") {
        it("should parse title and data") {
            // pie showData
            //   title Pets
            //   "Dogs" : 10
            auto result = maid.parse(read_file(fixtures_path / "pie/basic.mmd"));
            check(result.success == true);
            check(result.diagram->type == DiagramType::Pie);
            check(result.diagram->get_title() == "Pets");
        }
    }

    describe("Architecture Diagram") {
        it("should parse groups, services and icons") {
            // group api(cloud)[API]
            // service web(server)[Web] in api
            // service db(database)[DB] in api
            // web:R --> L:db
            auto result = maid.parse(read_file(fixtures_path / "architecture/basic.mmd"));
            check(result.success == true);
            
            // 1 group (api)
            check(result.diagram->subgraphs.size() == 1);
            check(result.diagram->subgraphs[0].id == "api");
            check(result.diagram->subgraphs[0].label == "API");
            check(result.diagram->subgraphs[0].node_ids.size() == 2);
            
            // 2 services: web, db
            check(result.diagram->nodes.size() == 2);
            check(result.diagram->nodes.count("web") == 1);
            check(result.diagram->nodes.count("db") == 1);
            
            // Icon property
            check(result.diagram->nodes.at("web").get_prop("icon") == "server");
            check(result.diagram->nodes.at("db").get_prop("icon") == "database");
            
            // 1 edge: web -> db with arrow
            check(result.diagram->edges.size() == 1);
            check(result.diagram->edges[0].from == "web");
            check(result.diagram->edges[0].to == "db");
            check(result.diagram->edges[0].end_decoration == EdgeDecoration::Arrow);
        }
    }

    describe("C4 Diagram") {
        it("should parse boundaries and nodes") {
            // Person(admin, "Admin")
            // System(sys, "System")
            // Rel(admin, sys, "Uses")
            // Boundary(b0, "Boundary") { SystemDb(db, "DB") }
            auto result = maid.parse(read_file(fixtures_path / "c4/basic.mmd"));
            check(result.success == true);
            check(result.diagram->type == DiagramType::C4);
            
            // 4 nodes: admin, sys, db
            check(result.diagram->nodes.size() == 3);
            check(result.diagram->nodes.count("admin") == 1);
            check(result.diagram->nodes.count("sys") == 1);
            check(result.diagram->nodes.count("db") == 1);
            
            // Node types
            check(result.diagram->nodes.at("admin").get_prop("c4_type") == "Person");
            check(result.diagram->nodes.at("sys").get_prop("c4_type") == "System");
            check(result.diagram->nodes.at("db").get_prop("c4_type") == "SystemDb");
            
            // Labels
            check(result.diagram->nodes.at("admin").label == "Admin");
            check(result.diagram->nodes.at("sys").label == "System");
            check(result.diagram->nodes.at("db").label == "DB");
            
            // 1 boundary
            check(result.diagram->subgraphs.size() == 1);
            check(result.diagram->subgraphs[0].id == "b0");
            check(result.diagram->subgraphs[0].label == "Boundary");
            check(result.diagram->subgraphs[0].node_ids.size() == 1);
            
            // 1 edge: admin -> sys
            check(result.diagram->edges.size() == 1);
            check(result.diagram->edges[0].from == "admin");
            check(result.diagram->edges[0].to == "sys");
            check(result.diagram->edges[0].label == "Uses");
        }
    }

    describe("Gantt Chart") {
        it("should parse as Gantt type") {
            auto result = maid.parse(read_file(fixtures_path / "gantt/basic.mmd"));
            check(result.success == true);
            check(result.diagram->type == DiagramType::Gantt);
        }
    }

    describe("GitGraph") {
        it("should parse as GitGraph type") {
            auto result = maid.parse(read_file(fixtures_path / "gitgraph/basic.mmd"));
            check(result.success == true);
            check(result.diagram->type == DiagramType::GitGraph);
        }
    }
}
