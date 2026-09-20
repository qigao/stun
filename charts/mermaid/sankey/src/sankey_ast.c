#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "sankey/sankey_ast.h"

#include "json_parser.h"

static char* copy_string(const char* s) {
    if(!s) return NULL;
    return strdup(s);
}

typedef struct NodeList {
    char* id;
    struct NodeList* next;
} NodeList;

static void add_unique_node(NodeList** head, const char* id) {
    if(!id) return;
    
    // Check if exists
    NodeList* cur = *head;
    while(cur) {
        if(strcmp(cur->id, id) == 0) return;
        cur = cur->next;
    }

    // New node
    NodeList* n = (NodeList*)malloc(sizeof(NodeList));
    n->id = strdup(id);
    n->next = NULL;

    // Insert sorted
    if(!*head || strcmp((*head)->id, id) > 0) {
        n->next = *head;
        *head = n;
    } else {
        cur = *head;
        while(cur->next && strcmp(cur->next->id, id) < 0) {
            cur = cur->next;
        }
        n->next = cur->next;
        cur->next = n;
    }
}

static void free_nodelist(NodeList* head) {
    while(head) {
        NodeList* next = head->next;
        free(head->id);
        free(head);
        head = next;
    }
}

// ... existing code ...

char* sankey_to_json(SankeyDiagram* d) {
    if(!d) {
        json_value_t *null_val = json_create_null();
        size_t len;
        char *s = json_serialize_pretty_crlf(null_val, &len);
        json_free(&null_val);
        return s;
    }

    NodeList* nodes_head = NULL;
    json_value_t *root = json_create_object();
    json_value_t *links = json_create_array();

    SankeyLink* l = d->links;
    while(l) {
        json_value_t *link = json_create_object();
        json_object_set_string(link, "source", l->source ? l->source : "");
        json_object_set_string(link, "target", l->target ? l->target : "");
        json_object_set_number(link, "value", l->value);
        json_array_add(links, link);

        add_unique_node(&nodes_head, l->source);
        add_unique_node(&nodes_head, l->target);

        l = l->next;
    }
    json_object_add(root, "links", links);

    json_value_t *nodes_obj = json_create_object();
    NodeList* curr = nodes_head;
    while(curr) {
        json_value_t *node = json_create_object();
        json_object_set_string(node, "id", curr->id);
        json_object_set_string(node, "label", curr->id);
        // "title": curr->id ? title is optional, usually label is enough or same
        
        json_object_add(nodes_obj, curr->id, node);
        curr = curr->next;
    }
    json_object_add(root, "nodes", nodes_obj);
    json_object_set_string(root, "type", "sankey");

    size_t out_len;
    char *json_str = json_serialize_pretty_crlf(root, &out_len);
    
    json_free(&root);
    free_nodelist(nodes_head);

    return json_str;
}

SankeyDiagram* sankey_create_diagram() {
    SankeyDiagram* d = (SankeyDiagram*)malloc(sizeof(SankeyDiagram));
    if(d) {
        memset(d, 0, sizeof(SankeyDiagram));
    }
    return d;
}

void sankey_free_diagram(SankeyDiagram* d) {
    if(!d) return;
    SankeyLink* l = d->links;
    while(l) {
        SankeyLink* next = l->next;
        free(l->source);
        free(l->target);
        free(l);
        l = next;
    }
    free(d);
}

void sankey_add_link(SankeyParserContext* ctx, const char* source, const char* target, double value) {
    if(!ctx || !ctx->diagram) return;
    
    SankeyLink* l = (SankeyLink*)malloc(sizeof(SankeyLink));
    if(l) {
        l->source = copy_string(source);
        l->target = copy_string(target);
        l->value = value;
        l->next = NULL;
        
        if(!ctx->diagram->links) {
            ctx->diagram->links = l;
        } else {
            SankeyLink* last = ctx->diagram->links;
            while(last->next) last = last->next;
            last->next = l;
        }
    }
}
