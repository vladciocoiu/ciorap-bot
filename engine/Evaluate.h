#pragma once

#ifndef EVALUATE_H_
#define EVALUATE_H_

#include "Board.h"

extern int PIECE_VALUES[7];
extern const int MG_WEIGHT[7];
extern const int FLIPPED[64];
extern const int KING_SAFETY_TABLE[100];

struct Score {
    int mg = 0;
    int eg = 0;

    constexpr Score() = default;
    constexpr Score(int m, int e) : mg(m), eg(e) {}

    constexpr Score& operator+=(const Score& rhs) {
        mg += rhs.mg;
        eg += rhs.eg;
        return *this;
    }

    constexpr Score& operator-=(const Score& rhs) {
        mg -= rhs.mg;
        eg -= rhs.eg;
        return *this;
    }

    constexpr Score operator+(const Score& rhs) const {
        return Score(mg + rhs.mg, eg + rhs.eg);
    }

    constexpr Score operator-(const Score& rhs) const {
        return Score(mg - rhs.mg, eg - rhs.eg);
    }

    constexpr Score operator*(int c) const {
        return Score(mg * c, eg * c);
    }
};

struct EvalParams {
    // Tapered Piece-Square Tables (MG / EG)
    int mgKingTable[64];
    int egKingTable[64];
    int mgQueenTable[64];
    int egQueenTable[64];
    int mgRookTable[64];
    int egRookTable[64];
    int mgBishopTable[64];
    int egBishopTable[64];
    int mgKnightTable[64];
    int egKnightTable[64];
    int mgPawnTable[64];
    int egPawnTable[64];
    int mgPassedPawnTable[64];
    int egPassedPawnTable[64];

    // Tapered Piece Values (Empty, Pawn, Knight, Bishop, Rook, Queen, King)
    int mgPieceValues[7];
    int egPieceValues[7];

    // Middlegame King Safety parameters
    int kingShield[3];
    int pieceAttackWeight[6];

    // Tapered Positional Bonuses & Penalties [0 = MG, 1 = EG]
    int knightMobility[2];
    int knightDefByPawn[2];
    int knightPairPenalty[2];

    int bishopPair[2];
    int bishopMobility[2];

    int rookOnOpenFile[2];
    int rookOnSeventh[2];
    int rooksDefEachOther[2];
    int rookMobility[2];

    int queenMobility[2];

    int doubledPawnsPenalty[2];
    int weakPawnPenalty[2];

    // Untapered Positional / Structural / Opening Bonuses & Penalties
    int knightPawnConst;
    int trappedKnightPenalty;
    int blockingCKnight;

    int trappedBishopPenalty;
    int fianchettoBonus;
    int blockedBishopPenalty;

    int rookOnQueenFile;
    int rookPawnConst;
    int blockedRookPenalty;

    int earlyQueenDevelopment;

    int cPawnPenalty;

    int tempoBonus;
};

extern EvalParams defaultEvalParams;

int gamePhase();
int evaluate();
int evaluate(const EvalParams& params);

#endif
