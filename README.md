# ciorap-bot

This is a UCI-compatible chess engine with ~2000 Elo rating (only tested against [Snowy](https://github.com/JasonCreighton/snowy) so far).

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
- Move ordering using PV-move and MVV-LVA, killer move and history heuristics
- Late move reductions
- Quiescence search with delta-pruning

## Usage

### Compiling and running the engine

```
cd engine
g++ -std=c++20 *.cpp -o ciorap-bot
./ciorap-bot
```

### Training evaluation parameters

```
cd training
g++ -std=c++20 train.cpp train_eval.cpp ../engine/Board.cpp ../engine/UCI.cpp ../engine TranspositionTable.cpp ../engine/Search.cpp ../engine/Moves.cpp ../engine/MagicBitboards.cpp ../engine Evaluate.cpp -o train
./train input_file output_file number_of_positions
```
