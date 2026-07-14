#ifndef FLEXCHART_COMMON_TYPES_H
#define FLEXCHART_COMMON_TYPES_H
#include <stdlib.h>

typedef struct ChartDataValue {
    char** keys;
    char** values;
    int count;
    struct ChartDataValue* next;
} ChartDataValue;

typedef struct ChartData {
    char* source;
    ChartDataValue* values;
} ChartData;

void chart_data_free(ChartData* data);
void chart_data_value_free(ChartDataValue* v);

#endif
