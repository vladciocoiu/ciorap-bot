#!/usr/bin/env python3
"""
Multi-generation self-play Texel tuning pipeline for CiorapBot.

Each generation `g`:
  1. Compiles `texel-trainer` with the current `engine/Evaluate.cpp` (Gen g-1 params).
  2. Generates `positions` new self-play quiet positions (`quiet_selfplay_gen{g}_only.epd`).
  3. Combines the last `max_kept_gens` generations of positions into `quiet_selfplay_gen{g}.epd`
     (preserving contiguous game blocks across the 90% train / 10% validation split)
     and removes `.epd` files from generations older than the sliding window.
  4. Runs Texel tuning (`--tune`) starting from Gen g-1 params -> `tuned_params_gen{g}.cpp`.
  5. Applies `tuned_params_gen{g}.cpp` to `engine/Evaluate.cpp` and compiles
     `build/ciorap-bot-gen{g}.exe` (and updates `build/texel-trainer.exe` for Gen g+1).
"""

import argparse
import os
import platform
import re
import shutil
import subprocess
import sys
from pathlib import Path

# ==============================================================================
# DEFAULT CONFIGURATION (Edit these directly or override via CLI arguments)
# ==============================================================================

OUTPUT_DIR = "./high-quality"       # Base directory for all outputs (<dir>/build/, <dir>/*.epd, <dir>/*.cpp)
START_GEN = 2          # First generation index to run (set >1 to resume from a later gen)
NUM_GENS = 3           # Total number of generations to run (runs START_GEN .. START_GEN + NUM_GENS - 1)
MAX_KEPT_GENS = 3      # Sliding window size: keep positions from at most X generations
NUM_WORKERS = 12       # Parallel worker processes for --datagen (0 = auto-detect cores)

# Per-generation settings:
# You can set a single value (used for all generations) OR a list of values
# (one per generation; if the run has more generations than the list length,
# the last element of the list is repeated for subsequent generations).
SEARCH_DEPTH = [6, 6, 7]
NUM_POSITIONS = [2_000_000, 2_000_000, 1_000_000]
EPOCHS = [500, 600, 600]
LR = [1.0, 0.5, 0.5]
L2 = [3e-8, 1e-8, 5e-9]

# Whether to delete combined `quiet_selfplay_gen{g}.epd` files from previous generations
# once the next generation finishes (shard files outside the X-generation window are always deleted).
CLEANUP_OLD_COMBINED_EPD = False

# Match testing configuration (using cutechess-cli after each generation builds):
RUN_MATCHES = False                  # Bool or list of bools per gen (e.g. True or [False, False, True])
MATCH_OPPONENT = "base"              # "base" (vs ciorap-bot-base), "prev" (vs gen{g-1}), or "both"
MATCH_ROUNDS = 500                   # 500 rounds * 2 games (-repeat) = 1,000 games
MATCH_TC = "1+0.1"                   # Time control for cutechess-cli
MATCH_CONCURRENCY = 12               # Concurrent games in cutechess-cli
OPENING_BOOK = "UHO_XXL_+0.90_+1.19.pgn"
CUTECHESS_CLI = "cutechess-cli"

# ==============================================================================


REPO_ROOT = Path(__file__).resolve().parent.parent
EVALUATE_CPP = REPO_ROOT / "engine" / "Evaluate.cpp"
EXE_EXT = ".exe" if platform.system() == "Windows" else ""


def get_gen_value(setting, gen_offset: int):
    """Return the value for the given 0-based generation offset from a scalar or list."""
    if isinstance(setting, (list, tuple)):
        idx = min(gen_offset, len(setting) - 1)
        return setting[idx]
    return setting


def run_cmd(cmd: list[str], cwd: Path = REPO_ROOT, log_file: Path | None = None):
    """Print and execute a subprocess command, streaming output live to console (and optional log file)."""
    header = f"\n>>> Running: {' '.join(str(c) for c in cmd)}\n"
    print(header, end="", flush=True)
    if log_file is not None:
        with log_file.open("a", encoding="utf-8") as lf:
            lf.write(header)

        proc = subprocess.Popen(
            cmd,
            cwd=str(cwd),
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
            bufsize=1,
        )
        with log_file.open("a", encoding="utf-8") as lf:
            assert proc.stdout is not None
            for line in proc.stdout:
                print(line, end="", flush=True)
                lf.write(line)
        proc.wait()
        if proc.returncode != 0:
            sys.exit(f"Command failed with exit code {proc.returncode}: {' '.join(str(c) for c in cmd)}")
    else:
        result = subprocess.run(cmd, cwd=str(cwd))
        if result.returncode != 0:
            sys.exit(f"Command failed with exit code {result.returncode}: {' '.join(str(c) for c in cmd)}")


def ensure_cmake_configured(build_dir: Path, log_file: Path | None = None):
    """Configure CMake in `build_dir` if not already configured."""
    build_dir.mkdir(parents=True, exist_ok=True)
    cache_file = build_dir / "CMakeCache.txt"
    if not cache_file.exists():
        cmd = ["cmake", "-S", str(REPO_ROOT), "-B", str(build_dir), "-DCMAKE_BUILD_TYPE=Release"]
        if platform.system() == "Windows":
            cmd.extend(["-G", "MinGW Makefiles"])
        run_cmd(cmd, log_file=log_file)


def build_binaries(build_dir: Path, log_file: Path | None = None):
    """Compile both ciorap-bot-04 and texel-trainer in Release mode inside `build_dir`."""
    ensure_cmake_configured(build_dir, log_file=log_file)
    run_cmd(["cmake", "--build", str(build_dir), "--config", "Release", "-j"], log_file=log_file)


def apply_params_to_evaluate_cpp(params_file: Path):
    """Replace `EvalParams defaultEvalParams = { ... };` in engine/Evaluate.cpp."""
    new_params_code = params_file.read_text(encoding="utf-8").strip()
    if not new_params_code.startswith("EvalParams defaultEvalParams"):
        raise ValueError(f"{params_file} does not start with 'EvalParams defaultEvalParams'")

    eval_text = EVALUATE_CPP.read_text(encoding="utf-8")
    pattern = re.compile(
        r"EvalParams\s+defaultEvalParams\s*=\s*\{.*?\n\};",
        flags=re.DOTALL,
    )
    match = pattern.search(eval_text)
    if not match:
        raise RuntimeError(f"Could not find 'EvalParams defaultEvalParams = {{ ... }};' in {EVALUATE_CPP}")

    updated_text = eval_text[:match.start()] + new_params_code + eval_text[match.end():]
    EVALUATE_CPP.write_text(updated_text, encoding="utf-8")
    print(f"Updated defaultEvalParams in {EVALUATE_CPP} from {params_file.name}")


def combine_window_epds(window_files: list[Path], output_epd: Path):
    """
    Combine multiple single-generation EPD files into `output_epd` such that
    the first 90% of `output_epd` contains the first 90% of each generation's positions
    and the last 10% of `output_epd` contains the last 10% of each generation's positions.
    This keeps contiguous game blocks intact and balances all generations across train/val.
    """
    train_chunks = []
    val_chunks = []
    total_lines = 0

    for shard_path in window_files:
        if not shard_path.exists():
            raise FileNotFoundError(f"Missing generation position file required for window: {shard_path}")
        lines = shard_path.read_text(encoding="utf-8").splitlines(keepends=True)
        n = len(lines)
        val_n = max(1, n // 10)
        train_n = n - val_n
        train_chunks.append(lines[:train_n])
        val_chunks.append(lines[train_n:])
        total_lines += n

    with output_epd.open("w", encoding="utf-8") as out:
        for chunk in train_chunks:
            out.writelines(chunk)
        for chunk in val_chunks:
            out.writelines(chunk)

    print(
        f"Combined {len(window_files)} generation(s) ({total_lines:,} total positions) -> {output_epd.name}"
    )


def parse_cli_list(val_str: str, cast_fn):
    """Parse a comma-separated CLI value into a list of typed values."""
    return [cast_fn(x.strip()) for x in val_str.split(",") if x.strip()]


def parse_bool(val_str: str) -> bool:
    if val_str.lower() in ("1", "true", "yes", "y", "on"):
        return True
    if val_str.lower() in ("0", "false", "no", "n", "off"):
        return False
    raise ValueError(f"Invalid boolean value: {val_str}")


def run_cutechess_match(
    new_name: str,
    new_bin: Path,
    opp_name: str,
    opp_bin: Path,
    pgn_out: Path,
    rounds: int,
    tc: str,
    concurrency: int,
    log_file: Path | None = None,
):
    """Run a cutechess-cli match between `new_bin` and `opp_bin`."""
    book_path = Path(OPENING_BOOK)
    if not book_path.is_absolute():
        book_path = (REPO_ROOT / book_path).resolve()

    cmd = [
        CUTECHESS_CLI,
        "-engine", f"name={new_name}", f"cmd={new_bin}",
        "-engine", f"name={opp_name}", f"cmd={opp_bin}",
        "-each", "proto=uci", f"tc={tc}",
        "-openings", f"file={book_path}", "format=pgn", "order=random", "plies=8",
        "-games", "2",
        "-rounds", str(rounds),
        "-repeat",
        "-concurrency", str(concurrency),
        "-recover",
        "-pgnout", str(pgn_out),
    ]
    run_cmd(cmd, log_file=log_file)


def main():
    parser = argparse.ArgumentParser(
        description="Run multi-generation self-play Texel tuning with a sliding position window."
    )
    parser.add_argument("-o", "--output-dir", type=str, default=OUTPUT_DIR, help=f"Directory to store build/, .epd, and .cpp outputs (default: '{OUTPUT_DIR}')")
    parser.add_argument("--start-gen", type=int, default=START_GEN, help=f"Starting generation number (default: {START_GEN})")
    parser.add_argument("--num-gens", type=int, default=NUM_GENS, help=f"Number of generations to run (default: {NUM_GENS})")
    parser.add_argument("--max-kept-gens", type=int, default=MAX_KEPT_GENS, help=f"Sliding window of generations to keep positions from (default: {MAX_KEPT_GENS})")
    parser.add_argument("--workers", type=int, default=NUM_WORKERS, help=f"Datagen worker processes (default: {NUM_WORKERS})")
    parser.add_argument("--depth", type=str, default=None, help="Search depth (single int or comma-separated list per gen, e.g. '5' or '5,5,6')")
    parser.add_argument("--positions", type=str, default=None, help="Positions per gen (single int or comma-separated list, e.g. '1000000')")
    parser.add_argument("--epochs", type=str, default=None, help="Epochs per gen (single int or comma-separated list, e.g. '500')")
    parser.add_argument("--lr", type=str, default=None, help="Initial learning rate per gen (single float or comma-separated list, e.g. '1.0')")
    parser.add_argument("--l2", type=str, default=None, help="L2 regularization per gen (single float or comma-separated list, e.g. '1e-7,3e-8,1e-8')")
    parser.add_argument("--run-matches", type=str, nargs="?", const="true", default=None, help="Run cutechess-cli matches after each gen (bool or comma-separated list per gen, e.g. '--run-matches' or '--run-matches false,false,true')")
    parser.add_argument("--match-opponent", type=str, choices=["base", "prev", "both"], default=MATCH_OPPONENT, help=f"Opponent for cutechess matches: 'base', 'prev', or 'both' (default: '{MATCH_OPPONENT}')")
    parser.add_argument("--match-rounds", type=int, default=MATCH_ROUNDS, help=f"Number of 2-game opening rounds per match (default: {MATCH_ROUNDS} = {MATCH_ROUNDS * 2} games)")
    parser.add_argument("--match-tc", type=str, default=MATCH_TC, help=f"Time control for matches (default: '{MATCH_TC}')")
    parser.add_argument("--keep-combined-epd", action="store_true", help="Do not delete previous generations' combined quiet_selfplay_gen{g}.epd files")

    args = parser.parse_args()

    out_dir = Path(args.output_dir)
    if not out_dir.is_absolute():
        out_dir = (REPO_ROOT / out_dir).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)
    build_dir = out_dir / "build"
    log_file = out_dir / "training.log"

    depths = parse_cli_list(args.depth, int) if args.depth else SEARCH_DEPTH
    positions_cfg = parse_cli_list(args.positions, int) if args.positions else NUM_POSITIONS
    epochs_cfg = parse_cli_list(args.epochs, int) if args.epochs else EPOCHS
    lr_cfg = parse_cli_list(args.lr, float) if args.lr else LR
    l2_cfg = parse_cli_list(args.l2, float) if args.l2 else L2
    run_matches_cfg = parse_cli_list(args.run_matches, parse_bool) if args.run_matches is not None else RUN_MATCHES
    cleanup_combined = not args.keep_combined_epd and CLEANUP_OLD_COMBINED_EPD

    trainer_bin = build_dir / f"texel-trainer{EXE_EXT}"
    engine_bin = build_dir / f"ciorap-bot-04{EXE_EXT}"

    # Initial build to ensure trainer and baseline engine are up to date
    build_binaries(build_dir, log_file=log_file)

    base_bin = build_dir / f"ciorap-bot-base{EXE_EXT}"
    if args.start_gen == 1 and not base_bin.exists():
        shutil.copy2(engine_bin, base_bin)
        print(f"Saved baseline engine binary -> {base_bin}")

    end_gen = args.start_gen + args.num_gens - 1

    for offset, gen in enumerate(range(args.start_gen, end_gen + 1)):
        depth = get_gen_value(depths, offset)
        num_pos = get_gen_value(positions_cfg, offset)
        epochs = get_gen_value(epochs_cfg, offset)
        lr = get_gen_value(lr_cfg, offset)
        l2 = get_gen_value(l2_cfg, offset)
        do_match = get_gen_value(run_matches_cfg, offset)

        print("\n" + "=" * 72)
        print(f"STARTING GENERATION {gen} (of {args.start_gen}..{end_gen})")
        print(f"  Output dir    : {out_dir}")
        print(f"  New positions : {num_pos:,} (depth={depth}, workers={args.workers})")
        print(f"  Window size   : last {args.max_kept_gens} generation(s)")
        print(f"  Tuning params : epochs={epochs}, lr={lr}, l2={l2:g}")
        print(f"  Run matches   : {do_match}")
        print("=" * 72)

        gen_only_epd = out_dir / f"quiet_selfplay_gen{gen}_only.epd"
        combined_epd = out_dir / f"quiet_selfplay_gen{gen}.epd"
        tuned_params_cpp = out_dir / f"tuned_params_gen{gen}.cpp"
        gen_engine_bin = build_dir / f"ciorap-bot-gen{gen}{EXE_EXT}"

        # 1. Generate new self-play positions evaluated by current engine (Gen gen-1)
        run_cmd([
            str(trainer_bin),
            "--datagen",
            str(gen_only_epd),
            str(num_pos),
            str(args.workers),
            str(depth),
        ], log_file=log_file)

        # 2. Combine positions from the sliding window [max(1, gen - max_kept_gens + 1) .. gen]
        oldest_kept_gen = max(1, gen - args.max_kept_gens + 1)
        window_files = [
            out_dir / f"quiet_selfplay_gen{k}_only.epd"
            for k in range(oldest_kept_gen, gen + 1)
        ]
        combine_window_epds(window_files, combined_epd)

        # Clean up single-generation shards older than `oldest_kept_gen`
        for old_gen in range(1, oldest_kept_gen):
            old_shard = out_dir / f"quiet_selfplay_gen{old_gen}_only.epd"
            if old_shard.exists():
                old_shard.unlink()
                print(f"Removed expired generation shard: {old_shard.name}")

        # 3. Run Texel tuning starting from current defaultEvalParams (Gen gen-1)
        run_cmd([
            str(trainer_bin),
            "--tune",
            str(combined_epd),
            str(tuned_params_cpp),
            str(epochs),
            str(lr),
            f"{l2:g}",
        ], log_file=log_file)

        # Optionally remove previous generation's combined EPD file to save disk space
        if cleanup_combined and gen > 1:
            prev_combined = out_dir / f"quiet_selfplay_gen{gen - 1}.epd"
            if prev_combined.exists():
                prev_combined.unlink()
                print(f"Removed previous combined EPD: {prev_combined.name}")

        # 4. Apply tuned parameters to engine/Evaluate.cpp and rebuild binaries
        apply_params_to_evaluate_cpp(tuned_params_cpp)
        build_binaries(build_dir, log_file=log_file)

        shutil.copy2(engine_bin, gen_engine_bin)
        print(f"Saved Gen {gen} engine binary -> {gen_engine_bin}")

        # 5. Optionally run cutechess-cli test matches for this generation
        if do_match:
            if args.match_opponent in ("base", "both"):
                if base_bin.exists():
                    pgn_out = out_dir / f"texel-gen{gen}-vs-base.pgn"
                    run_cutechess_match(
                        new_name=f"gen{gen}",
                        new_bin=gen_engine_bin,
                        opp_name="base",
                        opp_bin=base_bin,
                        pgn_out=pgn_out,
                        rounds=args.match_rounds,
                        tc=args.match_tc,
                        concurrency=args.workers,
                        log_file=log_file,
                    )
                else:
                    print(f"Warning: {base_bin} not found, skipping match vs base.")

            if args.match_opponent in ("prev", "both") and gen > 1:
                prev_bin = build_dir / f"ciorap-bot-gen{gen - 1}{EXE_EXT}"
                if prev_bin.exists():
                    pgn_out = out_dir / f"texel-gen{gen}-vs-gen{gen - 1}.pgn"
                    run_cutechess_match(
                        new_name=f"gen{gen}",
                        new_bin=gen_engine_bin,
                        opp_name=f"gen{gen - 1}",
                        opp_bin=prev_bin,
                        pgn_out=pgn_out,
                        rounds=args.match_rounds,
                        tc=args.match_tc,
                        concurrency=args.workers,
                        log_file=log_file,
                    )
                else:
                    print(f"Warning: {prev_bin} not found, skipping match vs gen{gen - 1}.")

    print("\n" + "=" * 72)
    print(f"ALL GENERATIONS ({args.start_gen}..{end_gen}) COMPLETED SUCCESSFULLY!")
    print("=" * 72)


if __name__ == "__main__":
    main()
