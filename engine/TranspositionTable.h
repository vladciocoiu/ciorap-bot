#pragma once

#ifndef TRANSPOSITIONTABLE_H_
#define TRANSPOSITIONTABLE_H_

class TranspositionTable {
private:
    const int SIZE;
    struct hashElement;
    hashElement *hashTable;
public:
    TranspositionTable();
    ~TranspositionTable();

    static U64 pieceZobristNumbers[7][2][64];
    static U64 castleZobristNumbers[16];
    static U64 epZobristNumbers[8];
    static U64 blackTurnZobristNumber;

    static const int VAL_UNKNOWN;
    static const int HASH_F_ALPHA, HASH_F_BETA, HASH_F_EXACT, HASH_F_UNKNOWN;

    int retrieveBestMove();
    int retrieveDepthMove();
    int retrieveReplaceMove();
    
    int probeHash(short depth, int alpha, int beta, int ply);
    void recordHash(short depth, int val, int hashF, int best, int ply);
    void clear();

    static void generateZobristHashNumbers();
};

extern TranspositionTable *transpositionTable;

#endif
