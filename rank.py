"""Rank the benchmark results published for every platform.

Reads the release archives (``json-bench-<os>-<arch>.zip``), or the directories
they extract to, and prints one ranking per platform plus a merged one. Give it
a directory (or archives) on the command line; the working directory is used by
default.

Platforms differ by several times in absolute speed, so every metric is
normalized against the best implementation for the same task and dataset on the
same platform before platforms are joined. The merged ranking therefore measures
relative efficiency, not who drew the fastest runner. Throughput decides every
task but ``get``, which is ranked by operations per second.

Typical use::

    python rank.py ~/Downloads
"""

import argparse
import csv
import statistics
import tempfile
import zipfile
from collections import defaultdict
from collections.abc import Sequence
from pathlib import Path

GET_TASK = "Get element"
TASKS = (
    "Encode known data",
    "Decode known data",
    "Load arbitrary data",
    "Transform data",
    GET_TASK,
)
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
            found.extend(summaries(sorted(path.glob("*.zip")), scratch))
        elif path.exists():
            found.append((platform_of(path), path))
    return found


def platform_of(summary: Path) -> str:
    """The platform label, taken from the enclosing release directory name."""
    for parent in summary.parents:
        if parent.name.startswith("json-bench-"):
            return parent.name.removeprefix("json-bench-")
    return summary.parent.name


def load(summary: Path) -> Row:
    """``(task, dataset, implementation) -> ``ops/s for ``get``, GB/s otherwise."""
    rows: Row = {}
    with summary.open(encoding="utf-8", newline="") as handle:
        for entry in csv.DictReader(handle):
            if entry["task"] not in TASKS:
                continue
            column = "ops_per_s" if entry["task"] == GET_TASK else "throughput_gb_s"
            rows[(entry["task"], entry["dataset"], entry["implementation"])] = float(entry[column])
    return rows


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


def rank_table(scores: dict[str, Score], title: str) -> str:
    """A markdown table of one ranking."""
    order = sorted(scores.items(), key=lambda item: (item[1].mean_rank, -item[1].median_relative))
    lines = [
        f"## {title}",
        "",
        "| # | implementation | median vs best | mean rank | wins | comparisons |",
        "| ---: | --- | ---: | ---: | ---: | ---: |",
    ]
    for position, (implementation, entry) in enumerate(order, start=1):
        lines.append(
            f"| {position} | {implementation} | {entry.median_relative:.2f} | "
            f"{entry.mean_rank:.2f} | {entry.wins} | {len(entry.relative)} |"
        )
    lines.append("")
    return "\n".join(lines)


def task_table(per_platform: dict[str, Row]) -> str:
    """Mean placement per task, with every platform weighing the same."""
    per_task: dict[str, dict[str, list[float]]] = defaultdict(lambda: defaultdict(list))
    for rows in per_platform.values():
        for task in TASKS:
            subset = {key: value for key, value in rows.items() if key[0] == task}
            if not subset:
                continue
            for implementation, entry in score(subset).items():
                per_task[task][implementation].append(entry.median_relative)

    def overall(implementation: str) -> float:
        return statistics.fmean(
            statistics.fmean(per_task[task][implementation])
            for task in TASKS
            if per_task[task].get(implementation)
        )

    lines = [
        "## merged by task",
        "",
        "| implementation | " + " | ".join(TASKS) + " |",
        "| --- | " + " | ".join("---:" for _ in TASKS) + " |",
    ]
    for implementation in sorted(per_task[GET_TASK], key=overall, reverse=True):
        cells = [
            f"{statistics.fmean(per_task[task][implementation]):.2f}"
            if per_task[task].get(implementation)
            else "-"
            for task in TASKS
        ]
        lines.append(f"| {implementation} | " + " | ".join(cells) + " |")
    lines.append("")
    return "\n".join(lines)


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
    return result


def main(argv: Sequence[str] | None = None) -> int:
    args = parser().parse_args(argv)
    with tempfile.TemporaryDirectory() as scratch_name:
        found = summaries(args.paths, Path(scratch_name))
        if not found:
            print("no summary.csv found; pass the release archives or their directory", flush=True)
            return 1

        per_platform: dict[str, Row] = {}
        for platform_name, summary in found:
            per_platform[platform_name] = load(summary)
            print(f"loaded {platform_name}: {summary}", flush=True)

        for platform_name, rows in sorted(per_platform.items()):
            print()
            print(rank_table(score(rows), f"{platform_name} ({len(rows)} measurements)"))

        print(rank_table(pool([score(rows) for rows in per_platform.values()]),
                         f"merged ({len(per_platform)} platforms)"))
        print(task_table(per_platform), end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
