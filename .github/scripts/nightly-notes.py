"""Prepend development provenance to the shared release notes."""
import argparse
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--notes", type=Path, required=True)
    parser.add_argument("--sha", required=True)
    parser.add_argument("--repository", required=True)
    parser.add_argument("--run-id", required=True)
    args = parser.parse_args()
    header = (
        "## dev 夜间预发布 / Development nightly prerelease\n\n"
        "此版本来自 `dev`，用于测试开发中的功能。正式发布频道保持独立。\n"
        "Built from `dev` for testing features in development.\n\n"
        f"- Source: [`{args.sha[:12]}`](https://github.com/{args.repository}/commit/{args.sha})\n"
        f"- Build: [Actions run](https://github.com/{args.repository}/actions/runs/{args.run_id})\n\n"
    )
    args.notes.write_text(header + args.notes.read_text(encoding="utf-8"), encoding="utf-8")


if __name__ == "__main__":
    main()
