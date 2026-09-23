#!/usr/bin/env python3
"""Validate a completed benchmark and summarize its raw, per-run observations."""
import argparse
import collections
import csv
import json
import math
from pathlib import Path
import statistics


def summarize(directory):
    metadata = json.loads((directory / "metadata.json").read_text())
    if not metadata.get("complete"):
        raise ValueError("Benchmark is not complete")
    with (directory / "instances.csv").open(newline="") as f:
        instances = {row["instance"]: row for row in csv.DictReader(f)}
    with (directory / "runs.csv").open(newline="") as f:
        runs = list(csv.DictReader(f))
    repetitions = metadata["runs_per_solver_instance"]
    expected_count = len(instances) * 2 * repetitions
    if len(instances) != metadata["instance_count"] or len(runs) != expected_count:
        raise ValueError("Incomplete instance/run counts")
    # Keep a compact reference CSV suitable for --best-known when reproducing.
    with (directory / "reference.csv").open("w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["Name", "BestKnown"])
        writer.writerows((name, instances[name]["best_known"]) for name in sorted(instances))
    groups = collections.defaultdict(list)
    keys = set()
    for sequence, run in enumerate(runs, 1):
        key = (run["instance"], run["solver"], int(run["trial"]))
        if key in keys or key[0] not in instances or key[1] not in ("cpp", "julia"):
            raise ValueError(f"Invalid or duplicated run: {key}")
        if int(run["sequence"]) != sequence or int(run["seed"]) != key[2]:
            raise ValueError(f"Incorrect sequence or seed: {key}")
        if not 1 <= key[2] <= repetitions:
            raise ValueError(f"Unexpected trial: {key}")
        keys.add(key)
        for name in ("cost", "timeout", "budget_met", "trial", "seed", "sequence"):
            run[name] = int(run[name])
        for name in ("solver_seconds", "call_seconds"):
            run[name] = float(run[name])
            if not math.isfinite(run[name]) or run[name] <= 0:
                raise ValueError(f"Invalid timing: {key}")
        if run["timeout"] not in (0, 1) or run["budget_met"] not in (0, 1):
            raise ValueError(f"Invalid termination flag: {key}")
        vertices = list(map(int, run["tour"].split(",")))
        instance = instances[key[0]]
        if len(vertices) != int(instance["num_sets"]) or len(set(vertices)) != len(vertices):
            raise ValueError(f"Invalid tour length/duplicates: {key}")
        if any(v < 0 or v >= int(instance["num_vertices"]) for v in vertices):
            raise ValueError(f"Invalid vertex ID: {key}")
        groups[key[:2]].append(run)
    if any(len(group) != repetitions for group in groups.values()):
        raise ValueError("Missing replicates")
    summaries = []
    for name in sorted(instances, key=lambda name: (int(instances[name]["num_sets"]), name)):
        info = instances[name]
        row = dict(instance=name, num_vertices=int(info["num_vertices"]),
                   num_sets=int(info["num_sets"]), best_known=int(info["best_known"]))
        for solver in ("julia", "cpp"):
            group = groups[name, solver]
            costs = [r["cost"] for r in group]
            times = [r["solver_seconds"] for r in group]
            calls = [r["call_seconds"] for r in group]
            values = dict(best=min(costs), mean=statistics.mean(costs),
                          sd=statistics.stdev(costs) if repetitions > 1 else 0,
                          hits=sum(c == row["best_known"] for c in costs),
                          mean_gap_percent=statistics.mean(
                              100 * (c / row["best_known"] - 1) for c in costs),
                          mean_seconds=statistics.mean(times),
                          median_seconds=statistics.median(times),
                          sd_seconds=statistics.stdev(times) if repetitions > 1 else 0,
                          mean_call_seconds=statistics.mean(calls),
                          timeouts=sum(r["timeout"] for r in group))
            row.update({f"{solver}_{k}": v for k, v in values.items()})
        row["speedup"] = row["julia_mean_seconds"] / row["cpp_mean_seconds"]
        summaries.append(row)
    with (directory / "summary.csv").open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(summaries[0]))
        writer.writeheader()
        writer.writerows(summaries)
    aggregate = {"instances": len(instances), "runs_per_solver": len(instances) * repetitions}
    for solver in ("julia", "cpp"):
        sr = [r for r in runs if r["solver"] == solver]
        aggregate[solver] = dict(
            mean_gap_percent=statistics.mean(s[f"{solver}_mean_gap_percent"] for s in summaries),
            median_instance_mean_gap_percent=statistics.median(
                s[f"{solver}_mean_gap_percent"] for s in summaries),
            total_seconds=sum(r["solver_seconds"] for r in sr),
            total_call_seconds=sum(r["call_seconds"] for r in sr),
            best_known_hits=sum(s[f"{solver}_hits"] for s in summaries),
            instances_with_hit=sum(s[f"{solver}_hits"] > 0 for s in summaries),
            instances_all_hits=sum(s[f"{solver}_hits"] == repetitions for s in summaries),
            timeouts=sum(r["timeout"] for r in sr),
            budget_stops=sum(r["budget_met"] for r in sr),
        )
    aggregate["total_time_speedup"] = aggregate["julia"]["total_seconds"] / aggregate["cpp"]["total_seconds"]
    aggregate["total_call_speedup"] = aggregate["julia"]["total_call_seconds"] / aggregate["cpp"]["total_call_seconds"]
    aggregate["geometric_mean_speedup"] = math.exp(statistics.mean(math.log(s["speedup"]) for s in summaries))
    aggregate["median_speedup"] = statistics.median(s["speedup"] for s in summaries)
    aggregate["cpp_faster_instances"] = sum(s["speedup"] > 1 for s in summaries)
    aggregate["quality_wins"] = {
        "julia": sum(s["julia_mean"] < s["cpp_mean"] for s in summaries),
        "cpp": sum(s["cpp_mean"] < s["julia_mean"] for s in summaries),
        "ties": sum(s["cpp_mean"] == s["julia_mean"] for s in summaries),
    }
    (directory / "aggregate.json").write_text(json.dumps(aggregate, indent=2) + "\n")
    table = ["| Instance | Reference best | Julia mean ± SD | C++ mean ± SD | Hits Julia / C++ | Julia (s) | C++ (s) | Speedup |",
             "| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |"]
    for s in summaries:
        table.append(
            f"| {s['instance']} | {s['best_known']} | {s['julia_mean']:.1f} ± {s['julia_sd']:.1f} "
            f"| {s['cpp_mean']:.1f} ± {s['cpp_sd']:.1f} | {s['julia_hits']} / {s['cpp_hits']} "
            f"| {s['julia_mean_seconds']:.3f} | {s['cpp_mean_seconds']:.3f} | {s['speedup']:.2f}× |")
    (directory / "table.md").write_text("\n".join(table) + "\n")
    return aggregate


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    args = parser.parse_args()
    print(json.dumps(summarize(args.directory), indent=2))


if __name__ == "__main__":
    main()
