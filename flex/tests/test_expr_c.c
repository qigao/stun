#include <flex/core/expr_c.h>
#include <tinytest.h>

spec("Flex Expression C facade") {
    it("links and evaluates all numeric entry points without Runtime") {
        double d = 0.0;
        float f = 0.0f;
        int i = 0;
        check_true(flex_expr_eval_f64("2*3+1", &d));
        check_true(d == 7.0);
        check_true(flex_expr_eval_f32("2*3+1", &f));
        check_true(f == 7.0f);
        check_true(flex_expr_eval_i32("2*3+1", &i));
        check_equal(i, 7);
    }

    it("executes MIR builtin functions rather than accepting numeric prefixes") {
        double value = 0.0;
        check_true(flex_expr_eval_f64("pow(2,3)+sqrt(16)", &value));
        check_true(value == 12.0);
    }

    it("rejects malformed expressions and preserves assigned output") {
        double d = 42.0;
        float f = 43.0f;
        int i = 44;
        check_false(flex_expr_eval_f64("2+", &d));
        check_false(flex_expr_eval_f32("2+", &f));
        check_false(flex_expr_eval_i32("2+", &i));
        check_true(d == 42.0);
        check_true(f == 43.0f);
        check_equal(i, 44);
    }

    it("rejects missing inputs or outputs without overwriting output") {
        double d = 42.0;
        check_false(flex_expr_eval_f64(NULL, &d));
        check_true(d == 42.0);
        check_false(flex_expr_eval_f64("1", NULL));
        check_false(flex_expr_eval_f32("1", NULL));
        check_false(flex_expr_eval_i32("1", NULL));
    }
}
