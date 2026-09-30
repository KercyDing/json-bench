"""Rank the benchmark results published for every platform.

Reads the release archives (``json-bench-<os>-<arch>.zip``), or the directories
they extract to, and prints one ranking per platform plus a merged one. Give it
a directory (or archives) on the command line; the working directory is used by
default. ``--threads N`` picks which process count to rank (1 by default),
``--min-relative F`` hides implementations below that fraction of the best, and
``--png PATH`` also writes the ranking as a chart.

Platforms differ by several times in absolute speed, so every metric is
normalized against the best implementation for the same task and dataset on the
same platform before platforms are joined. The merged ranking therefore measures
relative efficiency, not who drew the fastest runner. Throughput decides every
task but ``get``, which is ranked by operations per second.

Typical use::

    python rank.py ~/Downloads --png ranking.png > ranking.md
"""

import argparse
import csv
import json
import statistics
import sys
import tempfile
import zipfile
from collections import defaultdict
from collections.abc import Sequence
from pathlib import Path

GET_TASK = "Get element"
TYPED_TASKS = ("Encode known data", "Decode known data")
DOM_TASKS = ("Load arbitrary data", "Transform data", GET_TASK)
TASKS = (*TYPED_TASKS, *DOM_TASKS)
Row = dict[tuple[str, str, str], float]


class Score:
    """Relative placement of one implementation over a set of comparisons."""

    def __init__(self) -> None:
        self.relative: list[float] = []
        self.rank: list[int] = []
        self.wins = 0

    def add(self, value: float, best: float, rank: int) -> None:
        self.relative.append(value / best)
        self.rank.append(rank)
        self.wins += rank == 1

    @property
    def median_relative(self) -> float:
        return statistics.median(self.relative)

    @property
    def mean_rank(self) -> float:
        return statistics.fmean(self.rank)


def summaries(paths: Sequence[Path], scratch: Path) -> list[tuple[str, Path]]:
    """Every ``(platform, summary.csv)`` reachable from ``paths``."""
    found: list[tuple[str, Path]] = []
    for path in paths:
        if path.suffix == ".zip":
            target = scratch / path.stem
            with zipfile.ZipFile(path) as archive:
                archive.extractall(target)
            found.extend(summaries([target], scratch))
        elif path.is_dir():
            found.extend((platform_of(item), item) for item in sorted(path.rglob("summary.csv")))
            # An archive beside its own extracted directory would be counted twice.
            archives = [zip for zip in sorted(path.glob("*.zip")) if not (path / zip.stem).is_dir()]
            found.extend(summaries(archives, scratch))
        elif path.exists():
            found.append((platform_of(path), path))
    return found


def platform_of(summary: Path) -> str:
    """The release directory name, or the recorded CPU for a plain result directory."""
    for parent in summary.parents:
        if parent.name.startswith("json-bench-"):
            return parent.name.removeprefix("json-bench-")
    measurements = summary.with_name("measurements.json")
    if measurements.exists():
        try:
            cpu = json.loads(measurements.read_text(encoding="utf-8"))["metadata"]["cpu"]
        except (OSError, ValueError, KeyError, TypeError):
            cpu = None
        if cpu:
            return str(cpu)
    return summary.parent.name


def load(summary: Path, threads: int) -> Row:
    """``(task, dataset, implementation) -> ``ops/s for ``get``, GB/s otherwise.

    A summary holds one row per process count, so only ``threads`` is kept.
    """
    rows: Row = {}
    with summary.open(encoding="utf-8", newline="") as handle:
        for entry in csv.DictReader(handle):
            if entry["task"] not in TASKS or int(entry["threads"]) != threads:
                continue
            column = "ops_per_s" if entry["task"] == GET_TASK else "throughput_gb_s"
            rows[(entry["task"], entry["dataset"], entry["implementation"])] = float(entry[column])
    if not rows:
        raise SystemExit(f"{summary}: no measurements at {threads} thread(s)")
    return rows


def plural(count: int, noun: str) -> str:
    """``count`` followed by its noun, pluralized unless it is one."""
    return f"{count} {noun}" if count == 1 else f"{count} {noun}s"


def ranks(values: dict[str, float]) -> dict[str, int]:
    """Dense ranks, best first; equal metrics share a rank."""
    ordered = sorted(set(values.values()), reverse=True)
    return {name: ordered.index(value) + 1 for name, value in values.items()}


def score(rows: Row) -> dict[str, Score]:
    """Score every implementation by its placement per ``(task, dataset)``."""
    comparisons: dict[tuple[str, str], dict[str, float]] = defaultdict(dict)
    for (task, dataset, implementation), value in rows.items():
        comparisons[(task, dataset)][implementation] = value

    scores: dict[str, Score] = defaultdict(Score)
    for values in comparisons.values():
        best = max(values.values())
        for implementation, rank in ranks(values).items():
            scores[implementation].add(values[implementation], best, rank)
    return scores


def pool(scored: Sequence[dict[str, Score]]) -> dict[str, Score]:
    """One score per implementation, over the comparisons of every platform."""
    pooled: dict[str, Score] = defaultdict(Score)
    for scores in scored:
        for implementation, entry in scores.items():
            pooled[implementation].relative.extend(entry.relative)
            pooled[implementation].rank.extend(entry.rank)
            pooled[implementation].wins += entry.wins
    return pooled


def kept(scores: dict[str, Score], keep: set[str]) -> dict[str, Score]:
    """The scores of the implementations worth showing."""
    return {name: entry for name, entry in scores.items() if name in keep}


def task_scores(per_platform: dict[str, Row], columns: Sequence[str]) -> dict[str, dict[str, float]]:
    """Mean placement per task, with every platform weighing the same."""
    per_task: dict[str, dict[str, list[float]]] = defaultdict(lambda: defaultdict(list))
    for rows in per_platform.values():
        for task in columns:
            subset = {key: value for key, value in rows.items() if key[0] == task}
            if not subset:
                continue
            for implementation, entry in score(subset).items():
                per_task[task][implementation].append(entry.median_relative)
    return {
        task: {implementation: statistics.fmean(values) for implementation, values in entries.items()}
        for task, entries in per_task.items()
    }


def order_by(scores: dict[str, Score]) -> list[str]:
    """Implementation names, best relative placement first."""
    return [
        name
        for name, _ in sorted(
            scores.items(), key=lambda item: (-item[1].median_relative, item[1].mean_rank)
        )
    ]


def rank_table(scores: dict[str, Score], title: str) -> str:
    """A markdown table of one ranking."""
    lines = [
        f"## {title}",
        "",
        "| # | implementation | median vs best | mean rank | wins | comparisons |",
        "| ---: | --- | ---: | ---: | ---: | ---: |",
    ]
    for position, implementation in enumerate(order_by(scores), start=1):
        entry = scores[implementation]
        lines.append(
            f"| {position} | {implementation} | {entry.median_relative:.2f} | "
            f"{entry.mean_rank:.2f} | {entry.wins} | {len(entry.relative)} |"
        )
    lines.append("")
    return "\n".join(lines)


def task_table(tasks: dict[str, dict[str, float]], columns: Sequence[str]) -> str:
    """A markdown table of the merged placement per task."""
    def overall(implementation: str) -> float:
        return statistics.fmean(
            tasks[task][implementation] for task in columns if tasks[task].get(implementation)
        )

    lines = [
        "## merged by task",
        "",
        "| implementation | " + " | ".join(columns) + " |",
        "| --- | " + " | ".join("---:" for _ in columns) + " |",
    ]
    for implementation in sorted(tasks[GET_TASK], key=overall, reverse=True):
        cells = [
            f"{tasks[task][implementation]:.2f}" if tasks[task].get(implementation) else "-"
            for task in columns
        ]
        lines.append(f"| {implementation} | " + " | ".join(cells) + " |")
    lines.append("")
    return "\n".join(lines)


def scope_name(columns: Sequence[str]) -> str:
    """How to name a task selection in titles."""
    return "all tasks" if tuple(columns) == TASKS else "DOM tasks"


def write_chart(
    path: Path,
    per_platform: dict[str, Row],
    merged: dict[str, Score],
    tasks: dict[str, dict[str, float]],
    columns: Sequence[str],
) -> None:
    """Draw the merged ranking per platform next to the per-task placement."""
    try:
        import matplotlib

        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        raise SystemExit(f"matplotlib is needed for {path}: pip install matplotlib") from None

    platforms = sorted(per_platform)
    platform_scores = {name: score(rows) for name, rows in per_platform.items()}
    order = order_by(merged)

    figure, (left, right) = plt.subplots(
        1, 2, figsize=(14, 6), gridspec_kw={"width_ratios": (3, 2)}
    )
    figure.suptitle(
        f"JSON benchmark ranking over {plural(len(platforms), 'platform')} · {scope_name(columns)}\n"
        "every metric normalized against the best implementation of the same task and dataset",
        fontsize=11,
    )

    width = 0.8 / len(platforms)
    for index, platform_name in enumerate(platforms):
        values = [platform_scores[platform_name][name].median_relative for name in order]
        left.bar(
            [position + index * width for position in range(len(order))],
            values,
            width,
            label=platform_name,
        )
    left.set_title("merged ranking", fontsize=10)
    left.set_ylabel("median vs best")
    left.set_ylim(0, 1.05)
    left.set_xticks(range(len(order)), order, rotation=30, ha="right")
    left.grid(axis="y", alpha=0.3)

    matrix = [[tasks[task].get(name) for task in columns] for name in order]
    image = right.imshow(
        [[float("nan") if value is None else value for value in row] for row in matrix],
        cmap="viridis",
        vmin=0,
        vmax=1,
        aspect="auto",
    )
    right.set_title("merged by task", fontsize=10)
    right.set_xticks(range(len(columns)), [task.replace(" ", "\n") for task in columns], fontsize=8)
    right.set_yticks(range(len(order)), order, fontsize=9)
    for row, name in enumerate(order):
        for column, task in enumerate(columns):
            value = tasks[task].get(name)
            if value is None:
                continue
            right.text(
                column,
                row,
                f"{value:.2f}",
                ha="center",
                va="center",
                fontsize=8,
                color="white" if value < 0.55 else "black",
            )
    figure.colorbar(image, ax=right, shrink=0.85, label="mean vs best")

    handles, labels = left.get_legend_handles_labels()
    figure.legend(handles, labels, loc="lower center", ncols=len(platforms), fontsize=9, frameon=False)

    figure.tight_layout(rect=(0, 0.05, 1, 1))
    figure.savefig(str(path), dpi=160)
    plt.close(figure)


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    result.add_argument(
        "paths",
        nargs="*",
        type=Path,
        default=[Path.cwd()],
        help="release archives, or directories holding them (default: the working directory)",
    )
    result.add_argument(
        "--threads",
        type=int,
        default=1,
        metavar="N",
        help="process count to rank, as measured by bench.py --parallel (default: 1)",
    )
    result.add_argument(
        "--min-relative",
        type=float,
        default=0.0,
        metavar="F",
        help="hide implementations below this fraction of the best (default: 0.0, keep all)",
    )
    result.add_argument(
        "--all-tasks",
        action="store_true",
        help="rank the typed Zig-only tasks together with the DOM tasks",
    )
    result.add_argument(
        "--png",
        type=Path,
        metavar="PATH",
        help="also write the ranking as a chart",
    )
    return result


def main(argv: Sequence[str] | None = None) -> int:
    args = parser().parse_args(argv)
    if args.threads < 1:
        raise SystemExit("--threads must be at least 1")
    with tempfile.TemporaryDirectory() as scratch_name:
        found = summaries(args.paths, Path(scratch_name))
        if not found:
            print("no summary.csv found; pass the release archives or their directory", flush=True)
            return 1

        per_platform: dict[str, Row] = {}
        for platform_name, summary in found:
            per_platform[platform_name] = load(summary, args.threads)
            print(f"loaded {platform_name}: {summary}", flush=True)

        measured = plural(args.threads, "thread")
        columns = TASKS if args.all_tasks else DOM_TASKS
        scope = scope_name(columns)
        ranked = {
            name: {key: value for key, value in rows.items() if key[0] in columns}
            for name, rows in per_platform.items()
        }

        merged = pool([score(rows) for rows in ranked.values()])
        keep = {
            name for name, entry in merged.items() if entry.median_relative >= args.min_relative
        }
        if not keep:
            raise SystemExit(f"nothing reaches {args.min_relative:.2f} of the best")
        if len(keep) < len(merged):
            print(f"showing {len(keep)} of {len(merged)} implementations", flush=True)

        for platform_name, rows in sorted(ranked.items()):
            print()
            print(rank_table(kept(score(rows), keep), f"{platform_name}, {measured}, {scope} ({plural(len(rows), 'comparison')})"))

        print(rank_table(kept(merged, keep), f"merged, {measured}, {scope} ({plural(len(ranked), 'platform')})"))
        tasks = {
            task: {name: value for name, value in entries.items() if name in keep}
            for task, entries in task_scores(per_platform, columns).items()
        }
        print(task_table(tasks, columns), end="")

        if args.png is not None:
            try:
                write_chart(args.png, ranked, kept(merged, keep), tasks, columns)
            except OSError as error:
                # A viewer holding the file open is not a benchmark failure.
                print(f"not overwrote {args.png}: {error.strerror}", file=sys.stderr, flush=True)
            else:
                print(f"wrote {args.png}", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
