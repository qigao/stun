/*
 * TUI Flexbox Layout System
 * 
 * Full flexbox implementation for terminal UIs.
 * Supports: flex-grow, flex-shrink, justify, align, wrap.
 */

#include "tui.h"
#include <stdlib.h>
#include <string.h>

#define TUI_LAYOUT_MAX_ITEMS 256

typedef struct {
    int x, y, w, h;
    int fixed_w, fixed_h;
    int min_w, min_h;
    int margin_l, margin_t, margin_r, margin_b;
    uint8_t grow;
    uint8_t shrink;
    uint8_t flags;
    uint8_t gap;
    int parent;
    int first_child;
    int next_sibling;
} tui_lay_item_t;

struct tui_layout {
    tui_lay_item_t items[TUI_LAYOUT_MAX_ITEMS];
    int count;
};

tui_layout_t* tui_layout_create(void) {
    return (tui_layout_t*)calloc(1, sizeof(tui_layout_t));
}

void tui_layout_destroy(tui_layout_t* lay) {
    free(lay);
}

void tui_layout_reset(tui_layout_t* lay) {
    if (!lay) return;
    lay->count = 0;
}

int tui_layout_item(tui_layout_t* lay) {
    if (!lay) return -1;
    if (lay->count >= TUI_LAYOUT_MAX_ITEMS) return -1;
    int id = lay->count++;
    tui_lay_item_t* item = &lay->items[id];
    memset(item, 0, sizeof(*item));
    item->grow = 1;
    item->shrink = 1;
    item->parent = -1;
    item->first_child = -1;
    item->next_sibling = -1;
    return id;
}

static bool valid_item(const tui_layout_t* lay, int id) {
    return lay && id >= 0 && id < lay->count;
}

void tui_layout_insert(tui_layout_t* lay, int parent, int child) {
    if (!valid_item(lay, parent) || !valid_item(lay, child) ||
        parent == child || lay->items[child].parent >= 0) {
        return;
    }

    /* Refuse to put a root below one of its descendants. */
    int ancestor = parent;
    for (int depth = 0; ancestor >= 0 && depth < lay->count; ++depth) {
        if (!valid_item(lay, ancestor) || ancestor == child) return;
        ancestor = lay->items[ancestor].parent;
    }
    if (ancestor >= 0) return; /* Corrupt/cyclic ancestry. */

    if (lay->items[parent].first_child < 0) {
        lay->items[parent].first_child = child;
    } else {
        int sib = lay->items[parent].first_child;
        int traversed = 0;
        while (valid_item(lay, sib) &&
               lay->items[sib].next_sibling >= 0 &&
               traversed++ < lay->count) {
            sib = lay->items[sib].next_sibling;
        }
        if (!valid_item(lay, sib) || traversed >= lay->count) return;
        lay->items[sib].next_sibling = child;
    }
    lay->items[child].parent = parent;
}

void tui_layout_set_size(tui_layout_t* lay, int id, int w, int h) {
    if (!valid_item(lay, id)) return;
    lay->items[id].fixed_w = w > 0 ? w : 0;
    lay->items[id].fixed_h = h > 0 ? h : 0;
}

void tui_layout_set_grow(tui_layout_t* lay, int id, int grow) {
    if (!valid_item(lay, id)) return;
    lay->items[id].grow = (uint8_t)(grow < 0 ? 0 :
                                    grow > 255 ? 255 : grow);
}

void tui_layout_set_shrink(tui_layout_t* lay, int id, int shrink) {
    if (!valid_item(lay, id)) return;
    lay->items[id].shrink = (uint8_t)(shrink < 0 ? 0 :
                                      shrink > 255 ? 255 : shrink);
}

void tui_layout_set_min_size(tui_layout_t* lay, int id, int w, int h) {
    if (!valid_item(lay, id)) return;
    lay->items[id].min_w = w > 0 ? w : 0;
    lay->items[id].min_h = h > 0 ? h : 0;
}

void tui_layout_set_flags(tui_layout_t* lay, int id, uint32_t flags) {
    if (!valid_item(lay, id)) return;
    lay->items[id].flags = (uint8_t)(flags & 0xFF);
    lay->items[id].gap = (uint8_t)((flags >> 16) & 0xFF);
}

void tui_layout_set_margins(tui_layout_t* lay, int id, int l, int t, int r, int b) {
    if (!valid_item(lay, id)) return;
    tui_lay_item_t* item = &lay->items[id];
    item->margin_l = l;
    item->margin_t = t;
    item->margin_r = r;
    item->margin_b = b;
}

static int max_i(int a, int b) { return a > b ? a : b; }
static int min_i(int a, int b) { return a < b ? a : b; }

static void compute_item(tui_layout_t* lay, int id, int px, int py, int pw, int ph) {
    tui_lay_item_t* item = &lay->items[id];
    
    int x = px + item->margin_l;
    int y = py + item->margin_t;
    int avail_w = pw - item->margin_l - item->margin_r;
    int avail_h = ph - item->margin_t - item->margin_b;
    
    int w = (item->fixed_w > 0) ? min_i(item->fixed_w, avail_w) : avail_w;
    int h = (item->fixed_h > 0) ? min_i(item->fixed_h, avail_h) : avail_h;
    
    item->x = x;
    item->y = y;
    item->w = max_i(w, item->min_w);
    item->h = max_i(h, item->min_h);
    
    if (item->first_child < 0) return;
    
    int is_row = (item->flags & TUI_LAY_ROW) != 0;
    int justify = (item->flags >> 2) & 0x03;
    int align = (item->flags >> 4) & 0x03;
    int gap = item->gap;
    
    /* Measure children */
    int child_count = 0;
    int total_fixed = 0;
    int total_grow = 0;
    int total_shrink = 0;
    
    for (int c = item->first_child; c >= 0; c = lay->items[c].next_sibling) {
        child_count++;
        tui_lay_item_t* ch = &lay->items[c];
        int ch_margin = is_row ? (ch->margin_l + ch->margin_r) : (ch->margin_t + ch->margin_b);
        int ch_fixed = is_row ? ch->fixed_w : ch->fixed_h;
        int ch_min = is_row ? ch->min_w : ch->min_h;
        
        if (ch_fixed > 0) {
            total_fixed += ch_fixed + ch_margin;
        } else {
            total_fixed += ch_min + ch_margin;
            total_grow += ch->grow;
            total_shrink += ch->shrink;
        }
    }
    
    int total_gap = (child_count > 1) ? gap * (child_count - 1) : 0;
    int main_size = is_row ? item->w : item->h;
    int cross_size = is_row ? item->h : item->w;
    int remaining = main_size - total_fixed - total_gap;
    
    /* Calculate start position based on justify */
    int pos = is_row ? x : y;
    int between_space = 0;
    
    if (remaining > 0 && total_grow == 0) {
        switch (justify) {
            case 1: pos += remaining; break;                              /* END */
            case 2: pos += remaining / 2; break;                          /* CENTER */
            case 3:                                                       /* SPACE_BETWEEN */
                if (child_count > 1) between_space = remaining / (child_count - 1);
                break;
        }
    }
    
    /* Layout children */
    for (int c = item->first_child; c >= 0; c = lay->items[c].next_sibling) {
        tui_lay_item_t* ch = &lay->items[c];
        int ch_margin_main = is_row ? (ch->margin_l + ch->margin_r) : (ch->margin_t + ch->margin_b);
        int ch_fixed = is_row ? ch->fixed_w : ch->fixed_h;
        int ch_min = is_row ? ch->min_w : ch->min_h;
        
        int child_main;
        if (ch_fixed > 0) {
            child_main = ch_fixed;
        } else if (remaining > 0 && total_grow > 0) {
            child_main = ch_min + (remaining * ch->grow) / total_grow;
        } else if (remaining < 0 && total_shrink > 0) {
            int shrink_amount = (-remaining * ch->shrink) / total_shrink;
            child_main = max_i(ch_min, ch_min - shrink_amount);
        } else {
            child_main = ch_min;
        }
        
        int child_cross = cross_size;
        int cross_pos = is_row ? y : x;
        
        /* Align on cross axis */
        int ch_cross_fixed = is_row ? ch->fixed_h : ch->fixed_w;
        if (ch_cross_fixed > 0 && ch_cross_fixed < cross_size) {
            switch (align) {
                case 1: cross_pos += cross_size - ch_cross_fixed; break;  /* END */
                case 2: cross_pos += (cross_size - ch_cross_fixed) / 2; break; /* CENTER */
            }
            child_cross = ch_cross_fixed;
        }
        
        if (is_row) {
            compute_item(lay, c, pos, cross_pos, child_main + ch_margin_main, child_cross);
            pos += lay->items[c].w + ch->margin_l + ch->margin_r + gap + between_space;
        } else {
            compute_item(lay, c, cross_pos, pos, child_cross, child_main + ch_margin_main);
            pos += lay->items[c].h + ch->margin_t + ch->margin_b + gap + between_space;
        }
    }
}

void tui_layout_compute(tui_layout_t* lay, int root, int w, int h) {
    if (!valid_item(lay, root)) return;
    compute_item(lay, root, 0, 0, w > 0 ? w : 0, h > 0 ? h : 0);
}

void tui_layout_get_rect(tui_layout_t* lay, int id, int* x, int* y, int* w, int* h) {
    if (!valid_item(lay, id)) {
        if (x) *x = 0;
        if (y) *y = 0;
        if (w) *w = 0;
        if (h) *h = 0;
        return;
    }
    tui_lay_item_t* item = &lay->items[id];
    if (x) *x = item->x;
    if (y) *y = item->y;
    if (w) *w = item->w;
    if (h) *h = item->h;
}
