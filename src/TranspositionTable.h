#pragma once

#ifndef TRANSPOSITIONTABLE_H_
#define TRANSPOSITIONTABLE_H_

int retrieveBestMove();
int probeHash(short depth, int alpha, int beta);
void recordHash(short depth, int val, char hashF, int best);
int retrievePawnEval(U64 pawns);
void recordPawnEval(U64 pawns, int eval);
void generateZobristHashNumbers();
U64 getZobristHashFromCurrPos();
void showPV(short depth);
void clearTT();

extern const int VAL_UNKNOWN;
extern const char HASH_F_ALPHA, HASH_F_BETA, HASH_F_EXACT;

#endif
