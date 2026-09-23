#!/usr/bin/env python3
"""Run the two solvers sequentially with identical default search settings.

Only Python's standard library is needed. Build the C++ adapter using the
CMake project in this directory; Julia needs no non-standard dependencies.
"""
import argparse
import csv
import datetime
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import random
import subprocess
import sys
import time

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
FIELDS = ["sequence", "instance", "trial", "seed", "solver", "cost",
          "solver_seconds", "call_seconds", "timeout", "budget_met", "tour"]


def command(*args):
    return subprocess.check_output(args, text=True).strip()


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def source_digest(root, directories):
    h = hashlib.sha256()
    for directory in directories:
        for path in sorted((root / directory).rglob("*")):
            if path.is_file():
                h.update(str(path.relative_to(root)).encode())
                h.update(b"\0")
                h.update(path.read_bytes())
    return h.hexdigest()


class Worker:
    def __init__(self, argv, error_path, env):
        self.error_path = error_path
        self.error = error_path.open("w")
        self.process = subprocess.Popen(argv, stdin=subprocess.PIPE,
                                        stdout=subprocess.PIPE, stderr=self.error,
                                        text=True, bufsize=1, env=env)
        if self.process.stdout.readline().strip() != "ready":
            self.close()
            raise RuntimeError(f"Worker failed to start; see {error_path}")

    def ask(self, *fields):
        if any("\t" in str(x) or "\n" in str(x) for x in fields):
            raise ValueError("Tabs/newlines are not supported in benchmark paths")
        self.process.stdin.write("\t".join(map(str, fields)) + "\n")
        self.process.stdin.flush()
        line = self.process.stdout.readline()
        if not line:
            raise RuntimeError(f"Worker exited; see {self.error_path}")
        parts = line.rstrip("\n").split("\t")
        if parts[0] != fields[0]:
            raise RuntimeError(f"Unexpected worker response: {line!r}")
        return parts[1:]

    def close(self):
        try:
            if self.process.poll() is None:
                self.process.stdin.write("quit\n")
                self.process.stdin.flush()
                self.process.wait(timeout=5)
        except (BrokenPipeError, subprocess.TimeoutExpired):
            self.process.kill()
            self.process.wait()
        finally:
            self.error.close()


def solve(worker, path, seed):
    fields = worker.ask("solve", path, seed)
    if len(fields) != 6:
        raise RuntimeError(f"Invalid solve response: {fields}")
    cost, seconds, call_seconds, timeout, budget, tour = fields
    result = dict(cost=int(cost), solver_seconds=float(seconds),
                  call_seconds=float(call_seconds), timeout=int(timeout),
                  budget_met=int(budget), tour=tour)
    if any(not math.isfinite(result[key]) or result[key] <= 0
           for key in ("solver_seconds", "call_seconds")):
        raise RuntimeError("Nonpositive timing")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--julia-repo", type=Path, required=True)
    parser.add_argument("--instances", type=Path, required=True)
    parser.add_argument("--best-known", type=Path, required=True)
    parser.add_argument("--cpp-worker", type=Path, required=True)
    parser.add_argument("--julia", default="julia")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--runs", type=int, default=10)
    parser.add_argument("--schedule-seed", type=int, default=20260914)
    parser.add_argument("--machine", default=platform.machine())
    parser.add_argument("--build-description", default="See CMake Release build configuration")
    parser.add_argument("--only", nargs="*", help="Optional instance names for a smoke run")
    args = parser.parse_args()
    if args.runs < 1:
        parser.error("--runs must be positive")
    args.julia_repo = args.julia_repo.resolve()
    args.instances = args.instances.resolve()
    args.cpp_worker = args.cpp_worker.resolve()
    paths = sorted(args.instances.glob("*.gtsp"))
    if args.only:
        paths = [p for p in paths if p.stem in args.only]
        if len(paths) != len(set(args.only)):
            parser.error("An --only instance was not found")
    if not paths:
        parser.error("No instances found")
    with args.best_known.open(encoding="utf-8-sig", newline="") as f:
        reference = {row[0]: int(row[1].replace(",", ""))
                     for row in list(csv.reader(f))[1:]
                     if len(row) > 1 and row[0] not in ("", "Average")}
    for path in paths:
        if path.stem not in reference:
            parser.error(f"No best-known value for {path.stem}")
    args.output.mkdir(parents=True, exist_ok=True)
    if (args.output / "runs.csv").exists():
        parser.error("Output already has runs.csv; choose a fresh directory")
    env = dict(os.environ, JULIA_NUM_THREADS="1", OPENBLAS_NUM_THREADS="1",
               OMP_NUM_THREADS="1")
    julia_argv = [args.julia, "--startup-file=no", "--threads=1", "--gcthreads=1",
                  str(HERE / "worker.jl"), str(args.julia_repo)]
    metadata = {
        "started_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "platform": platform.platform(), "machine": args.machine,
        "cpp_build": args.build_description,
        "python": platform.python_version(),
        "julia": command(args.julia, "--startup-file=no", "--version"),
        "cpp_commit": command("git", "-C", str(ROOT), "rev-parse", "HEAD"),
        "julia_commit": command("git", "-C", str(args.julia_repo), "rev-parse", "HEAD"),
        "cpp_sources_sha256": source_digest(ROOT, ["src", "include"]),
        "julia_sources_sha256": source_digest(args.julia_repo, ["src"]),
        "cpp_worker_sha256": digest(args.cpp_worker),
        "adapter_sha256": {p.name: digest(p) for p in
                           [HERE / "worker.cpp", HERE / "worker.jl", HERE / "run.py"]},
        "reference_csv_sha256": digest(args.best_known),
        "runs_per_solver_instance": args.runs, "instance_count": len(paths),
        "seeds": list(range(1, args.runs + 1)),
        "schedule_seed": args.schedule_seed,
        "settings": {"mode": "default", "trials": 5, "restarts": 3,
                     "num_iterations_per_set": 60, "max_time": 360, "budget": None},
        "timing": "Single active solver, persistent workers; parse all instances and "
                  "warm each solver before measurement; full Julia GC before each call. "
                  "solver_seconds excludes parsing/startup/output; call_seconds includes "
                  "parsing and the solver call, excludes process startup and validation.",
    }
    (args.output / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")
    workers = {}
    start = time.monotonic()
    try:
        workers["cpp"] = Worker([str(args.cpp_worker)], args.output / "cpp.stderr.log", env)
        workers["julia"] = Worker(julia_argv, args.output / "julia.stderr.log", env)
        manifest = []
        for path in paths:
            c = workers["cpp"].ask("parse", path)
            j = workers["julia"].ask("parse", path)
            if c != j:
                raise RuntimeError(f"Parser disagreement for {path.name}: {c} != {j}")
            n, m, dist, member, sets = map(int, c)
            manifest.append(dict(instance=path.stem, num_vertices=n, num_sets=m,
                                 best_known=reference[path.stem], sha256=digest(path),
                                 matrix_checksum=dist, membership_sum=member,
                                 sets_checksum=sets))
        with (args.output / "instances.csv").open("w", newline="") as f:
            writer = csv.DictWriter(f, fieldnames=list(manifest[0]))
            writer.writeheader()
            writer.writerows(manifest)
        print(f"Parser parity passed for {len(paths)} instances.", flush=True)
        # Full solves compile the same default-mode paths used in measured runs.
        # Warm both a coordinate and an explicit/asymmetric instance if available.
        warm_paths = [p for p in paths if p.stem in ("39rat195", "35ftv170")]
        if not warm_paths:
            warm_paths = paths[:1]
        for path in warm_paths:
            for name, worker in workers.items():
                result = solve(worker, path, 0)
                if name == "julia":
                    workers["cpp"].ask("check", path, result["cost"], result["tour"])
        print("Warm-up complete; starting sequential measured runs.", flush=True)
        measured_start = time.monotonic()
        schedule = [(path, trial) for path in paths for trial in range(1, args.runs + 1)]
        random.Random(args.schedule_seed).shuffle(schedule)
        with (args.output / "runs.csv").open("w", newline="") as f:
            writer = csv.DictWriter(f, fieldnames=FIELDS)
            writer.writeheader()
            sequence = 0
            for pair, (path, trial) in enumerate(schedule):
                order = ("julia", "cpp") if pair % 2 == 0 else ("cpp", "julia")
                for name in order:
                    result = solve(workers[name], path, trial)
                    if name == "julia":
                        workers["cpp"].ask("check", path, result["cost"], result["tour"])
                    sequence += 1
                    writer.writerow(dict(sequence=sequence, instance=path.stem,
                                         trial=trial, seed=trial, solver=name, **result))
                    f.flush()
                    print(f"[{sequence}/{2 * len(schedule)}] {path.stem} trial={trial} "
                          f"{name} cost={result['cost']} time={result['solver_seconds']:.3f}s "
                          f"elapsed={(time.monotonic()-measured_start)/60:.1f}min", flush=True)
        metadata["measured_wall_seconds"] = time.monotonic() - measured_start
        metadata["finished_utc"] = datetime.datetime.now(datetime.timezone.utc).isoformat()
        metadata["total_wall_seconds"] = time.monotonic() - start
        metadata["complete"] = True
        (args.output / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")
    finally:
        for worker in workers.values():
            worker.close()


if __name__ == "__main__":
    main()
