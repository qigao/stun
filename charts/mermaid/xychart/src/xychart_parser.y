%name XYParser
%token_prefix XY_
%token_type {char*}
%extra_argument { XYParserContext *ctx }

%include {
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xychart/xychart_ast.h"
#include "xychart_parser_gen.h"

typedef struct TextNode {
    char* text;
    struct TextNode* next;
} TextNode;

typedef struct NumberNode {
    double value;
    struct NumberNode* next;
} NumberNode;

static XYStringList convert_text_list(TextNode* head) {
    int count = 0;
    TextNode* curr = head;
    while(curr) { count++; curr = curr->next; }
    
    XYStringList res;
    res.count = count;
    res.items = (char**)malloc(sizeof(char*) * count);
    
    curr = head;
    for(int i = 0; i < count; i++) {
        res.items[i] = curr->text;
        TextNode* next = curr->next;
        free(curr);
        curr = next;
    }
    return res;
}

static XYNumberList convert_number_list(NumberNode* head) {
    int count = 0;
    NumberNode* curr = head;
    while(curr) { count++; curr = curr->next; }
    
    XYNumberList res;
    res.count = count;
    res.items = (double*)malloc(sizeof(double) * count);
    
    curr = head;
    for(int i = 0; i < count; i++) {
        res.items[i] = curr->value;
        NumberNode* next = curr->next;
        free(curr);
        curr = next;
    }
    return res;
}

static double to_double(char* s) {
    if(!s) return 0.0;
    return atof(s);
}
}

%syntax_error {
    if(!ctx->error_message) {
        char buf[128];
        snprintf(buf, 128, "Syntax error at token type %d", yymajor);
        ctx->error_message = strdup(buf);
    }
    ctx->error_count++;
}

%type text_list {TextNode*}
%type number_list {NumberNode*}
%type text_item {char*}
%type series_data {XYNumberList}
%type series_def {struct { char* name; XYNumberList data; }}

start ::= opt_nl XY_KW opt_config document.

opt_nl ::= .
opt_nl ::= opt_nl NL.

opt_config ::= .
opt_config ::= ORIENTATION(O). {
    if (strcmp(O, "horizontal") == 0) xychart_set_orientation(ctx, XY_ORIENTATION_HORIZONTAL);
    else xychart_set_orientation(ctx, XY_ORIENTATION_VERTICAL);
    free(O);
}

document ::= .
document ::= document statement.

statement ::= NL.
statement ::= SEMI.
statement ::= TITLE_KW text_item(T). { xychart_set_title(ctx, T); free(T); }
statement ::= ACC_TITLE_KW COLON text_item(T). { xychart_set_acc_title(ctx, T); free(T); }
statement ::= ACC_DESCR_KW COLON text_item(T). { xychart_set_acc_descr(ctx, T); free(T); }

statement ::= X_AXIS_KW x_axis_def.
statement ::= Y_AXIS_KW y_axis_def.
statement ::= LINE_KW series_def(S). { xychart_add_series(ctx, XY_SERIES_LINE, S.name, S.data); free(S.name); }
statement ::= BAR_KW series_def(S). { xychart_add_series(ctx, XY_SERIES_BAR, S.name, S.data); free(S.name); }

x_axis_def ::= text_item(T). { xychart_set_x_axis_title(ctx, T); free(T); }
x_axis_def ::= text_item(T) x_axis_data. { xychart_set_x_axis_title(ctx, T); free(T); }
x_axis_def ::= x_axis_data.

x_axis_data ::= SQR_START text_list(L) SQR_END. { xychart_set_x_axis_categories(ctx, convert_text_list(L)); }
x_axis_data ::= NUMBER(N1) ARROW NUMBER(N2). { xychart_set_x_axis_range(ctx, to_double(N1), to_double(N2)); free(N1); free(N2); }

y_axis_def ::= text_item(T). { xychart_set_y_axis_title(ctx, T); free(T); }
y_axis_def ::= text_item(T) y_axis_data. { xychart_set_y_axis_title(ctx, T); free(T); }
y_axis_def ::= y_axis_data.


y_axis_data ::= NUMBER(N1) ARROW NUMBER(N2). { xychart_set_y_axis_range(ctx, to_double(N1), to_double(N2)); free(N1); free(N2); }

series_def(R) ::= text_item(T) series_data(D). { R.name = T; R.data = D; }
series_def(R) ::= series_data(D). { R.name = strdup(""); R.data = D; }

series_data(D) ::= SQR_START number_list(L) SQR_END. { D = convert_number_list(L); }

text_list(L) ::= text_item(T). {
    L = (TextNode*)malloc(sizeof(TextNode));
    L->text = T; L->next = NULL;
}
text_list(L) ::= text_item(T) COMMA text_list(Next). {
    L = (TextNode*)malloc(sizeof(TextNode));
    L->text = T; L->next = Next;
}

number_list(L) ::= NUMBER(N). {
    L = (NumberNode*)malloc(sizeof(NumberNode));
    L->value = to_double(N); L->next = NULL;
    free(N);
}
number_list(L) ::= NUMBER(N) COMMA number_list(Next). {
    L = (NumberNode*)malloc(sizeof(NumberNode));
    L->value = to_double(N); L->next = Next;
    free(N);
}

text_item(T) ::= TEXT(X). { T = X; }
text_item(T) ::= NUMBER(X). { T = X; } // Sometimes numbers are used as labels
