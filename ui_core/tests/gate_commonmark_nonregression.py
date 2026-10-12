#!/usr/bin/env python3
"""Gated no-regression check of 256 previously qualified official HTML IDs.

The full 652-case report remains diagnostic; every baseline ID must still
render *exactly* the official CommonMark 0.31.2 expected HTML.
"""
import argparse
import json
from pathlib import Path
import sys

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--survey",type=Path,required=True)
    parser.add_argument("--baseline",type=Path,required=True)
    parser.add_argument("--out",type=Path,required=True)
    args=parser.parse_args()
    data=json.loads(args.survey.read_text(encoding="utf-8"))
    baseline=json.loads(args.baseline.read_text(encoding="utf-8"))
    ids=baseline.get("qualified_official_ids")
    if (data.get("commonmark_version")!="0.31.2" or
        baseline.get("commonmark_version")!="0.31.2" or
        data.get("source")!=baseline.get("source") or
        data.get("total")!=652 or baseline.get("official_total")!=652 or
        not isinstance(ids,list) or len(ids)!=256 or
        any(type(x) is not int or x<1 or x>652 for x in ids) or
        ids!=sorted(set(ids))):
        print("INFRA: immutable CommonMark baseline differs",file=sys.stderr)
        return 2
    rows=data.get("cases")
    if not isinstance(rows,list) or len(rows)!=652:
        print("INFRA: malformed full official survey",file=sys.stderr)
        return 2
    by_id={}
    for row in rows:
        number=int(row["id"].rsplit("-",1)[-1])
        if row["id"]!=f"cm-0.31.2-{number:04d}" or number in by_id:
            print("INFRA: malformed/repeated official example",file=sys.stderr)
            return 2
        by_id[number]=row
    if set(by_id)!=set(range(1,653)):
        print("INFRA: survey example missing or out of range",file=sys.stderr)
        return 2
    regressions=[
        {"id":n,"status":by_id[n].get("native_status"),
         "first_diff":by_id[n].get("first_native_official_diff")}
        for n in ids
        if by_id[n].get("native_status")!="rendered" or
           by_id[n].get("native_matches_official") is not True
    ]
    additional=[n for n in range(1,653) if n not in ids and
                by_id[n].get("native_matches_official") is True]
    result={
        "baseline_total":len(ids),
        "baseline_passed":len(ids)-len(regressions),
        "regressions":regressions,
        "additional_official_matches":additional,
        "official_diagnostic_total":652
    }
    args.out.parent.mkdir(parents=True,exist_ok=True)
    args.out.write_text(json.dumps(result,indent=2)+"\n",encoding="utf-8")
    print(f"Prior official HTML equalities: {result['baseline_passed']}/256;"
          f" additional official matches: {len(additional)}")
    for item in regressions:
        print(f"REGRESSED #{item['id']}: {item['status']}:"
              f" {item['first_diff']}",file=sys.stderr)
    return 1 if regressions else 0

if __name__=="__main__":
    sys.exit(main())
