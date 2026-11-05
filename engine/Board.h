#pragma once

#ifndef BOARD_H_
#define BOARD_H_

#include <string>
#include <stack>

using namespace std;

typedef unsigned long long U64;

class Board {
private:
    stack<int> epStk, castleStk, halfmoveStk;

    int pseudoLegalMoves[512];
    short generatePseudoLegalMoves();
    short generatePseudoLegalMovesQS();

    void updateHashKey(int move);
    void updatePieceInBB(int piece, int color, int sq);
    void movePieceInBB(int piece, int color, int from, int to);
    bool checkRepetition();
public:
    Board();
    ~Board();

    int squares[64], turn;
    int whiteKingSquare, blackKingSquare;
    int ep;
    int halfmoveClock;
    int repetitionIndex;
    int castleRights; // bit 0 -> white short, 1 -> white long, 2 -> black short, 3 -> black long

    U64 pawnsBB, knightsBB, bishopsBB, rooksBB, queensBB;
    U64 blackPiecesBB, whitePiecesBB;
    U64 hashKey;

    stack<int> moveStk;
    U64 *repetitionMap;

    void clear();


    void loadFenPos(string input);
    string getFenFromCurrPos();
    U64 getZobristHashFromCurrPos();

    U64 attacksTo(int sq);
    bool isAttacked(int sq);
    bool isInCheck();
    bool isDraw();

    int generateLegalMoves(int *moves);
    int generateLegalMovesQS(int *moves);

    void makeMove(int move);
    void unmakeMove(int move);
};

void init();

extern Board *board;

#endif
