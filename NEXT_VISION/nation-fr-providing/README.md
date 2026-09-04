# testing-tool

## How to Run

`testing-tool.py` accepts the following command-line arguments:

- `-h`, `--help`: Print help message.
- `-c CONFIG`, `--config CONFIG`: Use `CONFIG` as the **config file**.
- `-i INPUT`, `--input INPUT`: Use `INPUT` as the **input file** (map file).
- `-l LOG`, `--log LOG`: Use `LOG` as the **log file**.
- `-s`, `--stdio`: Read the map from standard input. Without this option and without `-i`, a map is generated instead. The log is written to standard output whenever `-l` is omitted, regardless of this option.
- `-a EXEC1, --exec1 EXEC1`: Use `EXEC1` as the execution command for the LEFT player.
- `-b EXEC2, --exec2 EXEC2`: Use `EXEC2` as the execution command for the RIGHT player.
- `--seed S`: If omitted, a random seed is used.
- `--NP NP`: Use `NP` as the number of zones in the right half. The total number of zones will be $2NP+1$.
- `--KP KP`: Use `KP` as the number of neutral strongholds in the right half. The total number of strongholds will be $2KP+1$.

**Note: `--NP` and `--KP` are generation parameters equal to half the actual `N` and `K` values in the map file's first line.**
**In other words, `NP` and `KP` are the number of zones and neutral strongholds on one half of the battlefield.**
The generated actual `N` satisfies 181 ≤ N ≤ 249, and the odd actual `K` satisfies ceil(√N−1) ≤ K ≤ floor(√N+4).

A map can be provided in one of three ways:
1. Use a pre-generated map file via the `INPUT` key in `config.ini` or the `-i INPUT` option
2. Generate a random map via the `SEED` key in `config.ini` or the `--seed S` option
3. Generate a random map of a specified size via `NP` and `KP` in `config.ini` or the `--NP NP` and `--KP KP` options. Both must be given together.

If multiple options are specified, the earlier one takes higher priority. If none are specified, a random seed is used to generate the map.

For example, to use `input.txt` as the input file, `log.txt` as the log file, `python3 sample-code.py P1` as the LEFT player's command, and `python3 sample-code.py P2` as the RIGHT player's command, run:

```bash
python3 testing-tool.py -i input.txt -l log.txt -a "python3 sample-code.py P1" -b "python3 sample-code.py P2"
```

Or you can generate a map on the fly using `--seed`:

```bash
python3 testing-tool.py --seed 42 -l log.txt -a "python3 sample-code.py P1" -b "python3 sample-code.py P2"
```

### Config File

The config file is a convenient alternative to command-line arguments. It supports the following keys:

```
INPUT=<path to input file>
LOG=<path to log file>
EXEC1=<execution command for the LEFT player>
EXEC2=<execution command for the RIGHT player>
SEED=<map generation seed>
NP=<number of zones in the right half>
KP=<number of neutral strongholds in the right half>
```

If a command-line argument conflicts with a config file value, the command-line argument takes priority.

For example, the run command above can be written as a config file as follows:

```
INPUT=input.txt
LOG=log.txt
EXEC1=python3 sample-code.py P1
EXEC2=python3 sample-code.py P2
```

To use seed-based map generation instead of a pre-generated map file, remove the `INPUT` line and specify `SEED` or the (`NP`, `KP`) pair.
If `SEED` is present, the map size is determined automatically from the seed. If both `NP` and `KP` are also specified, a map of that size is generated.
If `SEED` is absent, a random seed is used.

```
SEED=42
LOG=log.txt
EXEC1=python3 sample-code.py P1
EXEC2=python3 sample-code.py P2
```

Then run with:

```bash
python3 testing-tool.py -c config.ini
```

### Input File (Map File)

The input file describes the **battlefield layout** for the game. It has the following format:

```
N K
x_0 x_1 ... x_{N-1}
y_0 y_1 ... y_{N-1}
p_0 p_1 ... p_{K-1}
a_0 b_{0,0} b_{0,1} ... b_{0,a_0-1}
a_1 b_{1,0} b_{1,1} ... b_{1,a_1-1}
...
a_{N-1} b_{N-1,0} ... b_{N-1,a_{N-1}-1}
```

- First line: number of zones `N` and number of strongholds `K`.
- Second line: `x` coordinates of each zone's center (`N` integers).
- Third line: `y` coordinates of each zone's center (`N` integers).
- Fourth line: zone indices of the `K` neutral strongholds (in ascending order).
- Following `N` lines: for each zone `i`, the number of adjacent zones `a_i` followed by the adjacent zone indices `b_{i,*}`.

### Log File


```
[LEFT "COMMAND: <exec1>"]
[RIGHT "COMMAND: <exec2>"]
MAP
N K
x_0 x_1 ... x_{N-1}
y_0 y_1 ... y_{N-1}
STRONGHOLDS p_0 p_1 ... p_{K-1}
a_0 b_{0,0} b_{0,1} ...
...
a_{N-1} b_{N-1,0} ...
END MAP
TURN t
COMMAND LEFT START
<commands submitted by LEFT>
COMMAND LEFT END
COMMAND RIGHT START
<commands submitted by RIGHT>
COMMAND RIGHT END
TURN t RESULT
TIME LEFT <ms> <token> RIGHT <ms> <token>
<UPGRADE/TRAIN/MOVE/DAMAGE/SIEGE result lines>
END TURN t
...
RESULT <LEFT_WIN/RIGHT_WIN/DRAW> <HQ_DESTROYED/TURN_LIMIT/WA>
```

- `[LEFT ...]`, `[RIGHT ...]`: Shows the commands used to launch LEFT and RIGHT.
- `MAP` through `END MAP`: Records the battlefield layout, same as the map file. The stronghold line starts with `STRONGHOLDS ...`.
- `TURN t` through `END TURN t`: Records the commands submitted and their results for day `t`.
- `COMMAND <LEFT/RIGHT> START` through `COMMAND <LEFT/RIGHT> END`: Command lines submitted by the given player. If no commands were submitted, these two lines appear consecutively with nothing in between.
- After `TURN t RESULT`: Records time usage, remaining tokens, and UPGRADE/TRAIN/MOVE/DAMAGE/SIEGE results. Result types that did not occur are omitted.
- `DAMAGE <CAUSE> <warrior_id> <damage>`: A warrior took damage. `CAUSE` is `TURRET`, `COMBAT`, or `HUNGER`. `damage` is the amount of HP lost (not remaining HP).
- `SIEGE <side> <region> <damage>`: A building took siege damage. `damage` is the amount of HP lost (not remaining HP).
- `RESULT <LEFT_WIN/RIGHT_WIN/DRAW> <HQ_DESTROYED/TURN_LIMIT/WA>`: Game result. The reason for ending is one of: HQ destroyed (`HQ_DESTROYED`), day 400 reached (`TURN_LIMIT`), or wrong answer / timeout (`WA`).
- `# Debug <LEFT/RIGHT>: <msg>`: A line printed by LEFT or RIGHT to standard error (stderr).

## Wire Protocol

This section documents the messages exchanged between the interactor and each player process: what the interactor writes to the player's standard input, and what the player writes back on its standard output.

### Handshake (READY)

At game start each player receives:

```
READY (LEFT | RIGHT)
N K
x_0 x_1 ... x_{N-1}
y_0 y_1 ... y_{N-1}
p_0 p_1 ... p_{K-1}
a_0 b_{0,0} b_{0,1} ... b_{0,a_0-1}
...
a_{N-1} b_{N-1,0} ... b_{N-1,a_{N-1}-1}
```

The map format is identical to the Input File format above. The player must reply with `OK` within 1000 ms.

### Turn Start

```
START TURN T
```

Sent at the beginning of each turn. The player must reply with a command block ending in `END` within 100 ms (excess time is deducted from overtime tokens).

### Command Block (player → interactor)

```
COMMAND
MOVE <id> <region>
TRAIN <n>
UPGRADE <region>
...
END
```

Commands may be submitted in any order. `TRAIN` may appear at most once per turn.

### Result Block (interactor → player)

After both players submit their commands, each player receives a result block. The block scope is one player's perspective — the event sections carry **only that player's own events**. Throughout the result block `A` refers to LEFT and `B` refers to RIGHT.

```
TURN T
TIME T_self R_self T_opp R_opp
UPGRADE N
<side> <region>
...
TRAIN N
<id_1> ... <id_N>
MOVE N
<id> <new_region>
...
DAMAGE N
<cause> <id> <damage>
...
SIEGE N
<side> <region> <damage>
...
WARRIOR W
<id> <region> <hp>
...
BUILDING B
<side> <region> <kind> <level> <hp>
...
END
```

When a section's count is 0 the following data line(s) for that section are omitted entirely. This applies to every section that carries a count.

**TURN / TIME**

- `TURN T`: turn number.
- `TIME T_self R_self T_opp R_opp`: milliseconds used this turn and remaining overtime tokens, for self then opponent.

**UPGRADE N** (own player only)

Buildings built, upgraded, or repaired this turn by this player. The opponent's upgrades are not included.

- Each row: `<side> <region>`. Rows are sorted by region ascending.

**TRAIN N** (own player only)

Warriors trained this turn by this player.

- One line listing all `N` new warrior IDs. Warrior IDs are globally unique and assigned in increasing suffix order per side (e.g. `A4`, `A5`, `B7`).

**MOVE N** (own player only)

Warriors that completed a move this turn, belonging to this player.

- Each row: `<id> <new_region>`. Rows are sorted by warrior ID.

**DAMAGE N** (own player only)

Damage events suffered by this player's warriors this turn.

- Each row: `<cause> <id> <damage>`. `cause` is `TURRET`, `COMBAT`, or `HUNGER`. `damage` is HP lost (not remaining HP). Rows are sorted by cause group (TURRET → COMBAT → HUNGER), then by warrior ID within each group.

**SIEGE N** (own player only)

Siege damage taken by this player's buildings this turn.

- Each row: `<side> <region> <damage>`. `damage` is HP lost (not remaining HP). Rows are sorted by region ascending.

**Visibility**

A player determines their own visible set from the adjacency list and the positions of their own units at the end of the turn.

- Distance is measured in hops along the adjacency list — the number of zones on the shortest such path — not by Euclidean distance.
- Every unit (warrior, BASE, HQ) sees every zone within 2 hops, including the zone it stands in.
- The visible set is the union over all of the player's own units.

**WARRIOR W**

Snapshot of warriors visible to this player at end-of-turn.

- Includes **all** of this player's own warriors (regardless of visibility).
- Includes **enemy warriors** only if they are in a visible zone.
- Each row: `<id> <region> <hp>`. Rows are sorted by warrior ID (LEFT warriors `A*` before RIGHT warriors `B*`, then by numeric suffix ascending).

**BUILDING B**

Snapshot of buildings visible to this player at end-of-turn.

- Includes **all** of this player's own buildings (regardless of visibility).
- Includes **enemy buildings** only if they are in a visible zone.
- Each row: `<side> <region> <kind> <level> <hp>`. `kind` is `HQ` or `BASE`. Rows are sorted by region ascending.

### Game End

```
FINISH
```

When the game ends for any reason (HQ destroyed, turn limit reached, or a rule violation), the interactor sends `FINISH` instead of the next result block. **Upon receiving `FINISH` the player process must exit immediately** — it must not attempt to read a result block or issue any further output.
