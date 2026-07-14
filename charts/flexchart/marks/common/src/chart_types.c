#include "flexchart/common/chart_types.h"
#include <string.h>

void chart_data_value_free(ChartDataValue* v) {
    while (v) {
        ChartDataValue* next = v->next;
        for (int i = 0; i < v->count; i++) {
            free(v->keys[i]);
            free(v->values[i]);
        }
        free(v->keys);
        free(v->values);
        free(v);
        v = next;
    }
}

void chart_data_free(ChartData* data) {
    if (!data) return;
    free(data->source);
    chart_data_value_free(data->values);
    free(data);
}
