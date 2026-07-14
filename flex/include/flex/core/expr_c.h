#pragma once

#ifdef __cplusplus
extern "C" {
#endif

int flex_expr_eval_f64(const char* expression, double* out_value);
int flex_expr_eval_f32(const char* expression, float* out_value);
int flex_expr_eval_i32(const char* expression, int* out_value);

#ifdef __cplusplus
}
#endif
