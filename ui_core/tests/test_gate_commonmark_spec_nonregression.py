#!/usr/bin/env python3
"""Self-contained negative tests for SPEC-policy immutable CommonMark gate."""
import copy
import hashlib
import json
import unittest

from gate_commonmark_spec_nonregression import InvalidEvidence, evaluate


class SpecGateTests(unittest.TestCase):
    def setUp(self):
        rows = [{"id": f"cm-0.31.2-{n:04d}", "markdown": f"case {n}\n",
                 "official_expected_html": f"<p>{n}</p>\n",
                 "native_status": "rendered", "native_html": f"<p>{n}</p>\n",
                 "native_matches_official": True}
                for n in range(1, 653)]
        fixture = [(r["id"], r["markdown"], r["official_expected_html"])
                   for r in rows]
        digest = hashlib.sha256(json.dumps(
            fixture, ensure_ascii=False, separators=(",", ":")
        ).encode("utf-8")).hexdigest()
        self.baseline = {"render_policy": "SpecConformanceHtml",
                         "commonmark_version": "0.31.2", "source": "tools/md/test/spec.txt",
                         "official_total": 652, "official_fixture_sha256": digest,
                         "qualified_official_ids": list(range(1, 650))}
        self.survey = {"render_policy": "SpecConformanceHtml",
                       "commonmark_version": "0.31.2", "source": "tools/md/test/spec.txt",
                       "total": 652, "native_matches_official": 652,
                       "native_unsupported": 0, "native_differences": 0, "cases": rows}

    def test_retains_649_without_auto_promoting_3(self):
        before = copy.deepcopy((self.survey, self.baseline))
        result = evaluate(self.survey, self.baseline)
        self.assertEqual(result["baseline_passed"], 649)
        self.assertEqual(len(result["additional_official_matches"]), 3)
        self.assertEqual(result["regressions"], [])
        self.assertEqual((self.survey, self.baseline), before)

    def test_one_real_baseline_regression_reported(self):
        row = self.survey["cases"][9]
        row["native_html"] = "incorrect"
        row["native_matches_official"] = False
        self.survey.update(native_matches_official=651, native_differences=1)
        result = evaluate(self.survey, self.baseline)
        self.assertEqual(result["baseline_passed"], 648)
        self.assertEqual([r["id"] for r in result["regressions"]], [10])

    def test_safe_policy_cannot_pass_spec_gate(self):
        self.survey.pop("render_policy")
        with self.assertRaises(InvalidEvidence):
            evaluate(self.survey, self.baseline)

    def test_baseline_policy_cannot_change(self):
        self.baseline["render_policy"] = "SafeApplicationHtml"
        with self.assertRaises(InvalidEvidence):
            evaluate(self.survey, self.baseline)

    def test_forged_match_bit_rejected(self):
        self.survey["cases"][0]["native_html"] = "wrong"
        with self.assertRaises(InvalidEvidence):
            evaluate(self.survey, self.baseline)

    def test_unsupported_is_never_pass(self):
        row = self.survey["cases"][0]
        row["native_status"] = "unsupported"
        row["native_html"] = None
        row["native_matches_official"] = False
        self.survey.update(native_matches_official=651, native_unsupported=1)
        result = evaluate(self.survey, self.baseline)
        self.assertEqual(result["baseline_passed"], 648)
        self.assertEqual(result["native_unsupported"], 1)
        row["native_matches_official"] = True
        with self.assertRaises(InvalidEvidence):
            evaluate(self.survey, self.baseline)

    def test_missing_and_duplicate_ids_rejected(self):
        self.survey["cases"][1]["id"] = self.survey["cases"][0]["id"]
        with self.assertRaises(InvalidEvidence):
            evaluate(self.survey, self.baseline)

    def test_fixture_mutation_rejected(self):
        self.survey["cases"][0]["markdown"] += "modified"
        with self.assertRaises(InvalidEvidence):
            evaluate(self.survey, self.baseline)

    def test_unqualified_id_promotion_rejected(self):
        self.baseline["qualified_official_ids"] = list(range(1, 651))
        with self.assertRaises(InvalidEvidence):
            evaluate(self.survey, self.baseline)

    def test_untruthful_aggregate_rejected(self):
        self.survey["native_matches_official"] = 651
        with self.assertRaises(InvalidEvidence):
            evaluate(self.survey, self.baseline)


if __name__ == "__main__":
    unittest.main()
