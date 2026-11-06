# ciorap-bot

This is a UCI-compatible chess engine with ~2100 Elo rating.

### Features

- Hybrid move generation using bitboards and mailbox

####

- Piece evaluation using piece-square tables and mobility bonus
- Pawn structure evaluation that recognizes passed / weak / doubled pawns
- King safety evaluation
- All evaluation parameters tuned using supervised learning

####

- Move searching using alpha-beta algorithm and Principal Variation Search
- Iterative deepening and aspiration windows
- Transposition table using Zobrist hash
- Move ordering using PV-move, Static Exchange Evaluation (SEE), MVV-LVA, killer move and history heuristics
- Late move reductions, null move pruning, razoring, and futility pruning
- Quiescence search with SEE and delta-pruning
- Soft and hard time management limits

## Usage

### Compiling and running the engine

```
cd engine
g++ -std=c++20 -O3 *.cpp -o ciorap-bot
./ciorap-bot
```
