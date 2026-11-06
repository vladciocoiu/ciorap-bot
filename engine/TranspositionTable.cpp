#include <iostream>
#include <vector>
#include <unordered_map>
#include <cassert>

#include "Board.h"
#include "TranspositionTable.h"
#include "Search.h"
#include "MagicBitboardUtils.h"
#include "MoveUtils.h"
#include "BoardUtils.h"
#include "Enums.h"

using namespace std;

typedef unsigned long long U64;

U64 TranspositionTable::pieceZobristNumbers[7][2][64];
U64 TranspositionTable::castleZobristNumbers[16];
U64 TranspositionTable::epZobristNumbers[8];
U64 TranspositionTable::blackTurnZobristNumber;

const int TranspositionTable::VAL_UNKNOWN = -1e9;
const int TranspositionTable::HASH_F_EXACT = 0;
const int TranspositionTable::HASH_F_ALPHA = 1;
const int TranspositionTable::HASH_F_BETA = 2;
const int TranspositionTable::HASH_F_UNKNOWN = -1;

struct TranspositionTable::hashElement {
    U64 key;
    short depth;
    int flags;
    int value;
    int best;
};

TranspositionTable::TranspositionTable(): SIZE(1 << 22), hashTable(new hashElement*[SIZE]) {
    for(int i = 0; i < SIZE; i++) {
        hashTable[i] = new hashElement[2];
    }
    clear();
}

TranspositionTable::~TranspositionTable() {
    for(int i = 0; i < SIZE; i++) delete[] hashTable[i];
    delete[] hashTable;
}

void TranspositionTable::generateZobristHashNumbers() {
    for(int pc = 0; pc < 7; pc++) {
        for(int c = 0; c < 2; c++) {
            for(int sq = 0; sq < 64; sq++) {
                TranspositionTable::pieceZobristNumbers[pc][c][sq] = MagicBitboardUtils::randomULL();
            }
        }
    }
    for(int castle = 0; castle < 16; castle++) {
        TranspositionTable::castleZobristNumbers[castle] = MagicBitboardUtils::randomULL();
    }
    for(int col = 0; col < 8; col++) {
        TranspositionTable::epZobristNumbers[col] = MagicBitboardUtils::randomULL();
    }
    TranspositionTable::blackTurnZobristNumber = MagicBitboardUtils::randomULL();
}

// get the best move from the tt
int TranspositionTable::retrieveBestMove() {
    int index = (board->hashKey & (SIZE-1));
    assert(index >= 0 && index < SIZE);

    hashElement *h = hashTable[index];

    if(h[0].key == board->hashKey && h[0].best != MoveUtils::NO_MOVE) return h[0].best;
    if(h[1].key == board->hashKey) return h[1].best;

    return MoveUtils::NO_MOVE;
}

int TranspositionTable::retrieveDepthMove() {
    int index = (board->hashKey & (SIZE-1));
    assert(index >= 0 && index < SIZE);

    hashElement *h = hashTable[index];

    if(h[0].key == board->hashKey) return h[0].best;

    return MoveUtils::NO_MOVE;
}

int TranspositionTable::retrieveReplaceMove() {
    int index = (board->hashKey & (SIZE-1));
    assert(index >= 0 && index < SIZE);

    hashElement *h = hashTable[index];

    if(h[1].key == board->hashKey) return h[1].best;

    return MoveUtils::NO_MOVE;
}

// check if the stored hash element corresponds to the current position and if it was searched at a good enough depth
int TranspositionTable::probeHash(short depth, int alpha, int beta, int ply) {
    int index = (board->hashKey & (SIZE-1));
    assert(index >= 0 && index < SIZE);

    hashElement *h = hashTable[index];

    for(int i = 0; i < 2; i++) {
        if(h[i].key == board->hashKey) {
            if(h[i].depth >= depth) {
                int score = h[i].value;

                // mate adjustment
                // in the tt we have stored the mate score relative to the current position
                // so we need to adjust it to the root position
                if(score > Search::MATE_THRESHOLD) score -= ply;
                else if(score < -Search::MATE_THRESHOLD) score += ply;

                // bound the score to [alpha, beta] and return if conditions are met
                if ((h[i].flags == HASH_F_EXACT) 
                || (h[i].flags == HASH_F_ALPHA && score <= alpha) 
                || (h[i].flags == HASH_F_BETA && score >= beta)) return min(max(score, alpha), beta);
            }
        }
    }
    return VAL_UNKNOWN;
}

// replace hashed element if replacement conditions are met
void TranspositionTable::recordHash(short depth, int val, int hashF, int best, int ply) {
    if(Search::timeOver) return;

    int index = (board->hashKey & (SIZE-1));
    assert(index >= 0 && index < SIZE);

    hashElement *h = hashTable[index];

    // current score is relative to the root position
    // we want to store it relative to the current position
    if(val > Search::MATE_THRESHOLD) val += ply;
    if(val < -Search::MATE_THRESHOLD) val -= ply;

    // first bucket is depth only
    bool replace = (h[0].depth <= depth);
    if(replace) {
        if(best != MoveUtils::NO_MOVE || h[0].key != board->hashKey) h[0].best = best;
        h[0].key = board->hashKey;
        h[0].value = val;
        h[0].flags = hashF;
        h[0].depth = depth;
    }
    
    // second bucket is always replace
    if(best != MoveUtils::NO_MOVE || h[1].key != board->hashKey) h[1].best = best;
    h[1].key = board->hashKey;
    h[1].value = val;
    h[1].flags = hashF;
    h[1].depth = depth;
}

void TranspositionTable::clear() {
    hashElement newElement = {0, -2, HASH_F_UNKNOWN, 0, MoveUtils::NO_MOVE};
    for(int i = 0; i < SIZE; i++) {
        hashTable[i][0] = hashTable[i][1] = newElement;
    }
}

TranspositionTable *transpositionTable = nullptr;