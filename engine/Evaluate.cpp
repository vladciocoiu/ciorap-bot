#include <unordered_map>
#include <algorithm>

#include "Evaluate.h"
#include "Board.h"
#include "MagicBitboardUtils.h"
#include "TranspositionTable.h"
#include "BoardUtils.h"
#include "Enums.h"

using namespace std;

// flipped squares for piece square tables for black
const int FLIPPED[64] = {
    56, 57, 58, 59, 60, 61, 62, 63,
    48, 49, 50, 51, 52, 53, 54, 55,
    40, 41, 42, 43, 44, 45, 46, 47,
    32, 33, 34, 35, 36, 37, 38, 39,
    24, 25, 26, 27, 28, 29, 30, 31,
    16, 17, 18 ,19, 20, 21, 22, 23,
    8, 9, 10, 11 , 12, 13, 14, 15,
    0, 1, 2, 3, 4, 5, 6, 7
};

const int MG_WEIGHT[7] = {0, 0, 1, 1, 2, 4, 0}; 

const int KING_SAFETY_TABLE[100] = {
    0, 0, 1, 2, 3, 5, 7, 9, 12, 15,
    18, 22, 26, 30, 35, 39, 44, 50, 56, 62,
    68,  75,  82,  85,  89,  97, 105, 113, 122, 131,
    140, 150, 169, 180, 191, 202, 213, 225, 237, 248,
    260, 272, 283, 295, 307, 319, 330, 342, 354, 366,
    377, 389, 401, 412, 424, 436, 448, 459, 471, 483,
    494, 500, 500, 500, 500, 500, 500, 500, 500, 500,
    500, 500, 500, 500, 500, 500, 500, 500, 500, 500,
    500, 500, 500, 500, 500, 500, 500, 500, 500, 500,
    500, 500, 500, 500, 500, 500, 500, 500, 500, 500
};

// canonical piece values used for move ordering, delta pruning, and low-material draw recognition
int PIECE_VALUES[7] = {0, 100, 325, 350, 500, 975, 0};

EvalParams defaultEvalParams = {
    // mgKingTable
    {
        40, 50, 30, 10, 10, 30, 50, 40,
        30, 40, 20, 0, 0, 20, 40, 30,
        10, 20, 0, -20, -20, 0, 20, 10,
        0, 10, -10, -30, -30, -10, 10, 0,
        -10, 0, -20, -40, -40, -20, 0, -10,
        -20, -10, -30, -50, -50, -30, -10, -20,
        -30, -20, -40, -60, -60, -40, -20, -30,
        -40, -30, -50, -70, -70, -50, -30, -40
    },
    // egKingTable
    {
        -72, -48, -36, -24, -24, -36, -48, -72,
        -48, -24, -12, 0, 0, -12, -24, -48,
        -36, -12, 0, 12, 12, 0, -12, -36,
        -24, 0, 12, 24, 24, 12, 0, -24,
        -24, 0, 12, 24, 24, 12, 0, -24,
        -36, -12, 0, 12, 12, 0, -12, -36,
        -48, -24, -12, 0, 0, -12, -24, -48,
        -72, -48, -36, -24, -24, -36, -48, -72
    },
    // mgQueenTable
    {
        -5, -5, -5, -5, -5, -5, -5, -5,
        0, 0, 1, 1, 1, 1, 0, 0,
        0, 0, 1, 2, 2, 1, 0, 0,
        0, 0, 2, 3, 3, 2, 0, 0,
        0, 0, 2, 3, 3, 2, 0, 0,
        0, 0, 1, 2, 2, 1, 0, 0,
        0, 0, 1, 1, 1, 1, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0
    },
    // egQueenTable
    {
        -5, -5, -5, -5, -5, -5, -5, -5,
        0, 0, 1, 1, 1, 1, 0, 0,
        0, 0, 1, 2, 2, 1, 0, 0,
        0, 0, 2, 3, 3, 2, 0, 0,
        0, 0, 2, 3, 3, 2, 0, 0,
        0, 0, 1, 2, 2, 1, 0, 0,
        0, 0, 1, 1, 1, 1, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0
    },
    // mgRookTable
    {
        0, 0, 0, 2, 2, 0, 0, 0,
        -5, 0, 0, 0, 0, 0, 0, -5,
        -5, 0, 0, 0, 0, 0, 0, -5,
        -5, 0, 0, 0, 0, 0, 0, -5,
        -5, 0, 0, 0, 0, 0, 0, -5,
        -5, 0, 0, 0, 0, 0, 0, -5,
        -5, 0, 0, 0, 0, 0, 0, -5,
        5, 5, 5, 5, 5, 5, 5, 5
    },
    // egRookTable
    {
        0, 0, 0, 2, 2, 0, 0, 0,
        -5, 0, 0, 0, 0, 0, 0, -5,
        -5, 0, 0, 0, 0, 0, 0, -5,
        -5, 0, 0, 0, 0, 0, 0, -5,
        -5, 0, 0, 0, 0, 0, 0, -5,
        -5, 0, 0, 0, 0, 0, 0, -5,
        -5, 0, 0, 0, 0, 0, 0, -5,
        5, 5, 5, 5, 5, 5, 5, 5
    },
    // mgBishopTable
    {
        -4, -4, -12, -4, -4, -12, -4, -4,
        -4, 2, 1, 1, 1, 1, 2, -4,
        -4, 0, 2, 4, 4, 2, 0, -4,
        -4, 0, 4, 6, 6, 4, 0, -4,
        -4, 0, 4, 6, 6, 4, 0, -4,
        -4, 1, 2, 4, 4, 2, 1, -4,
        -4, 0, 0, 0, 0, 0, 0, -4,
        -4, -4, -4, -4, -4, -4, -4, -4
    },
    // egBishopTable
    {
        -4, -4, -12, -4, -4, -12, -4, -4,
        -4, 2, 1, 1, 1, 1, 2, -4,
        -4, 0, 2, 4, 4, 2, 0, -4,
        -4, 0, 4, 6, 6, 4, 0, -4,
        -4, 0, 4, 6, 6, 4, 0, -4,
        -4, 1, 2, 4, 4, 2, 1, -4,
        -4, 0, 0, 0, 0, 0, 0, -4,
        -4, -4, -4, -4, -4, -4, -4, -4
    },
    // mgKnightTable
    {
        -8, -12, -8, -8, -8, -8, -12, -8,
        -8, 0, 0, 0, 0, 0, 0, -8,
        -8, 0, 4, 4, 4, 4, 0, -8,
        -8, 0, 4, 8, 8, 4, 0, -8,
        -8, 0, 4, 8, 8, 4, 0, -8,
        -8, 0, 4, 4, 4, 4, 0, -8,
        -8, 0, 1, 2, 2, 1, 0, -8,
        -8, -8, -8, -8, -8, -8, -8, -8
    },
    // egKnightTable
    {
        -8, -12, -8, -8, -8, -8, -12, -8,
        -8, 0, 0, 0, 0, 0, 0, -8,
        -8, 0, 4, 4, 4, 4, 0, -8,
        -8, 0, 4, 8, 8, 4, 0, -8,
        -8, 0, 4, 8, 8, 4, 0, -8,
        -8, 0, 4, 4, 4, 4, 0, -8,
        -8, 0, 1, 2, 2, 1, 0, -8,
        -8, -8, -8, -8, -8, -8, -8, -8
    },
    // mgPawnTable
    {
        0, 0, 0, 0, 0, 0, 0, 0,
        -6, -4, 1, -24, -24, 1, -4, -6,
        -4, -4, 1, 5, 5, 1, -4, -4,
        -6, -4, 5, 10, 10, 5, -4, -6,
        -6, -4, 2, 8, 8, 2, -4, -6,
        -6, -4, 1, 2, 2, 1, -4, -6,
        -6, -4, 1, 1, 1, 1, -4, -6,
        0, 0, 0, 0, 0, 0, 0, 0
    },
    // egPawnTable
    {
        0, 0, 0, 0, 0, 0, 0, 0,
        20, 20, 20, 20, 20, 20, 20, 20,
        20, 20, 20, 20, 20, 20, 20, 20,
        40, 40, 40, 40, 40, 40, 40, 40,
        60, 60, 60, 60, 60, 60, 60, 60,
        80, 80, 80, 80, 80, 80, 80, 80,
        100, 100, 100, 100, 100, 100, 100, 100,
        0, 0, 0, 0, 0, 0, 0, 0
    },
    // mgPassedPawnTable
    {
        0, 0, 0, 0, 0, 0, 0, 0,
        20, 20, 20, 20, 20, 20, 20, 20,
        20, 20, 20, 20, 20, 20, 20, 20,
        40, 40, 40, 40, 40, 40, 40, 40,
        60, 60, 60, 60, 60, 60, 60, 60,
        80, 80, 80, 80, 80, 80, 80, 80,
        100, 100, 100, 100, 100, 100, 100, 100,
        0, 0, 0, 0, 0, 0, 0, 0
    },
    // egPassedPawnTable
    {
        0, 0, 0, 0, 0, 0, 0, 0,
        20, 20, 20, 20, 20, 20, 20, 20,
        20, 20, 20, 20, 20, 20, 20, 20,
        40, 40, 40, 40, 40, 40, 40, 40,
        60, 60, 60, 60, 60, 60, 60, 60,
        80, 80, 80, 80, 80, 80, 80, 80,
        100, 100, 100, 100, 100, 100, 100, 100,
        0, 0, 0, 0, 0, 0, 0, 0
    },

    // mgPieceValues & egPieceValues
    {0, 100, 325, 350, 500, 975, 0},
    {0, 100, 325, 350, 500, 975, 0},

    // kingShield & pieceAttackWeight
    {5, 10, 5},
    {0, 0, 2, 2, 3, 5},

    // Tapered bonuses & penalties [MG, EG]
    {4, 4},     // knightMobility
    {15, 15},   // knightDefByPawn
    {20, 20},   // knightPairPenalty

    {50, 50},   // bishopPair
    {5, 5},     // bishopMobility

    {20, 20},   // rookOnOpenFile
    {30, 30},   // rookOnSeventh
    {5, 5},     // rooksDefEachOther
    {3, 3},     // rookMobility

    {2, 2},     // queenMobility

    {40, 40},   // doubledPawnsPenalty
    {15, 15},   // weakPawnPenalty

    // Untapered bonuses & penalties
    3,          // knightPawnConst
    100,        // trappedKnightPenalty
    30,         // blockingCKnight

    100,        // trappedBishopPenalty
    20,         // fianchettoBonus
    50,         // blockedBishopPenalty

    10,         // rookOnQueenFile
    3,          // rookPawnConst
    50,         // blockedRookPenalty

    20,         // earlyQueenDevelopment

    25,         // cPawnPenalty

    10          // tempoBonus
};

int whiteAttackersCnt, blackAttackersCnt, whiteAttackWeight, blackAttackWeight;
int pawnCntWhite, pawnCntBlack, pieceMaterialWhite, pieceMaterialBlack;

Score evalPawn(int sq, int color, const EvalParams& p);
Score evalKnight(int sq, int color, const EvalParams& p);
Score evalBishop(int sq, int color, const EvalParams& p);
Score evalRook(int sq, int color, const EvalParams& p);
Score evalQueen(int sq, int color, const EvalParams& p);
Score evalPawnStructure(const EvalParams& p);

int whiteKingShield(const int kingShield[3]);
int blackKingShield(const int kingShield[3]);

int evaluate(const EvalParams& p) {
    // reset everything
    whiteAttackersCnt = blackAttackersCnt = 0;
    whiteAttackWeight = blackAttackWeight = 0;
    pawnCntWhite = pawnCntBlack = 0;
    pieceMaterialWhite = pieceMaterialBlack = 0;

    // evaluate pieces independently
    Score score(0, 0);
    for(int sq = 0; sq < 64; sq++) {
        if(board->squares[sq] == Empty) continue;

        int color = (board->squares[sq] & (Black | White));
        int c = (color == White ? 1 : -1);

        if(board->knightsBB & BoardUtils::bits[sq])
            score += evalKnight(sq, color, p) * c;

        if(board->bishopsBB & BoardUtils::bits[sq])
            score += evalBishop(sq, color, p) * c;

        if(board->rooksBB & BoardUtils::bits[sq])
            score += evalRook(sq, color, p) * c;

        if(board->queensBB & BoardUtils::bits[sq])
            score += evalQueen(sq, color, p) * c;
    }
    score += evalPawnStructure(p);

    // evaluate kings based on the current game phase (king centralization becomes more important than safety as pieces disappear from the board)
    int mgWeight = min(gamePhase(), 24);
    int egWeight = 24 - mgWeight;

    int mgKingScore = whiteKingShield(p.kingShield) - blackKingShield(p.kingShield);
    int egKingScore = 0;

    // evaluate king safety in the middlegame
    // if only 1 or 2 attackers, we consider the king safe
    if(whiteAttackersCnt <= 2) whiteAttackWeight = 0;
    if(blackAttackersCnt <= 2) blackAttackWeight = 0;

    mgKingScore += KING_SAFETY_TABLE[min(max(whiteAttackWeight, 0), 99)] - KING_SAFETY_TABLE[min(max(blackAttackWeight, 0), 99)];
    mgKingScore += p.mgKingTable[board->whiteKingSquare] - p.mgKingTable[FLIPPED[board->blackKingSquare]];

    egKingScore += p.egKingTable[board->whiteKingSquare] - p.egKingTable[FLIPPED[board->blackKingSquare]];

    score.mg += mgKingScore;
    score.eg += egKingScore;

    // add scores for bishop and knight pairs
    if(MagicBitboardUtils::popcount(board->whitePiecesBB & board->bishopsBB) >= 2)
        score += Score(p.bishopPair[0], p.bishopPair[1]);
    if(MagicBitboardUtils::popcount(board->blackPiecesBB & board->bishopsBB) >= 2)
        score -= Score(p.bishopPair[0], p.bishopPair[1]);

    if(MagicBitboardUtils::popcount(board->whitePiecesBB & board->knightsBB) >= 2)
        score -= Score(p.knightPairPenalty[0], p.knightPairPenalty[1]);
    if(MagicBitboardUtils::popcount(board->blackPiecesBB & board->knightsBB) >= 2)
        score += Score(p.knightPairPenalty[0], p.knightPairPenalty[1]);

    int res = (mgWeight * score.mg + egWeight * score.eg) / 24;

    // tempo bonus
    if(board->turn == White) res += p.tempoBonus;
    else res -= p.tempoBonus;

    // low material corrections (adjusting the score for well known draws)
    int strongerSide = White, weakerSide = Black;
    int strongerPawns = pawnCntWhite, weakerPawns = pawnCntBlack;
    int strongerPieces = pieceMaterialWhite, weakerPieces = pieceMaterialBlack;
    if(res < 0) {
        swap(strongerSide, weakerSide);
        swap(strongerPieces, weakerPieces);
        swap(strongerPawns, weakerPawns);
    }

    if(strongerPawns == 0) {
        // weaker side cannot be checkmated
        if(strongerPieces < 400) return 0;
        if(weakerPawns == 0 && weakerPieces == 2 * PIECE_VALUES[Knight]) return 0;

        // rook vs minor piece
        if(strongerPieces == PIECE_VALUES[Rook] && (weakerPieces == PIECE_VALUES[Knight] || weakerPieces == PIECE_VALUES[Bishop]))
            res /= 2;

        // rook and minor vs rook
        if((strongerPieces == PIECE_VALUES[Rook] + PIECE_VALUES[Bishop] || strongerPieces == PIECE_VALUES[Rook] + PIECE_VALUES[Knight])
           && weakerPieces == PIECE_VALUES[Rook])
            res /= 2;
    }

    // return result from the perspective of the side to move
    if(board->turn == Black) res *= -1;

    return res;
}

int evaluate() {
    return evaluate(defaultEvalParams);
}

Score evalKnight(int sq, int color, const EvalParams& p) {
    U64 opponentPawnsBB = (board->pawnsBB & (color == White ? board->blackPiecesBB : board->whitePiecesBB));
    U64 ourPawnsBB = (board->pawnsBB ^ opponentPawnsBB);
    U64 ourPiecesBB = (color == White ? board->whitePiecesBB : board->blackPiecesBB);

    U64 ourPawnAttacksBB = BoardUtils::pawnAttacks(ourPawnsBB, color);
    U64 opponentPawnAttacksBB = BoardUtils::pawnAttacks(opponentPawnsBB, (color ^ (White | Black)));

    if(color == White) pieceMaterialWhite += PIECE_VALUES[Knight];
    else pieceMaterialBlack += PIECE_VALUES[Knight];

    int relSq = (color == White ? sq : FLIPPED[sq]);

    // initial piece value and square value
    Score eval(
        p.mgPieceValues[Knight] + p.mgKnightTable[relSq],
        p.egPieceValues[Knight] + p.egKnightTable[relSq]
    );

    // mobility bonus
    U64 mob = (BoardUtils::knightAttacksBB[sq] ^ (BoardUtils::knightAttacksBB[sq] & (ourPiecesBB | opponentPawnAttacksBB)));
    int mobDiff = MagicBitboardUtils::popcount(mob) - 4;
    eval += Score(p.knightMobility[0] * mobDiff, p.knightMobility[1] * mobDiff);

    // decreasing value as pawns disappear
    int numberOfPawns = MagicBitboardUtils::popcount(board->pawnsBB);
    int pawnAdj = p.knightPawnConst * (numberOfPawns - 8);
    eval += Score(pawnAdj, pawnAdj);

    // traps and blockages
    int untaperedPenalty = 0;
    if(color == White) {
        if(sq == a8 && (opponentPawnsBB & (BoardUtils::bits[a7] | BoardUtils::bits[c7])))
            untaperedPenalty += p.trappedKnightPenalty;
        if(sq == a7 && (opponentPawnsBB & (BoardUtils::bits[a6] | BoardUtils::bits[c6])) && (opponentPawnsBB & (BoardUtils::bits[b7] | BoardUtils::bits[d7])))
            untaperedPenalty += p.trappedKnightPenalty;

        if(sq == h8 && (opponentPawnsBB & (BoardUtils::bits[h7] | BoardUtils::bits[f7])))
            untaperedPenalty += p.trappedKnightPenalty;
        if(sq == h7 && (opponentPawnsBB & (BoardUtils::bits[f6] | BoardUtils::bits[h6])) && (opponentPawnsBB & (BoardUtils::bits[e7] | BoardUtils::bits[g7])))
            untaperedPenalty += p.trappedKnightPenalty;

        if(sq == c3 && (ourPawnsBB & BoardUtils::bits[c2]) && (ourPawnsBB & BoardUtils::bits[d4]) && !(ourPawnsBB & BoardUtils::bits[e4]))
            untaperedPenalty += p.blockingCKnight;
    }
    if(color == Black) {
        if(sq == a1 && (opponentPawnsBB & (BoardUtils::bits[a2] | BoardUtils::bits[c2])))
            untaperedPenalty += p.trappedKnightPenalty;
        if(sq == a2 && (opponentPawnsBB & (BoardUtils::bits[a3] | BoardUtils::bits[c3])) && (opponentPawnsBB & (BoardUtils::bits[b2] | BoardUtils::bits[d2])))
            untaperedPenalty += p.trappedKnightPenalty;

        if(sq == h1 && (opponentPawnsBB & (BoardUtils::bits[h2] | BoardUtils::bits[f2])))
            untaperedPenalty += p.trappedKnightPenalty;
        if(sq == h2 && (opponentPawnsBB & (BoardUtils::bits[f3] | BoardUtils::bits[h3])) && (opponentPawnsBB & (BoardUtils::bits[e2] | BoardUtils::bits[g2])))
            untaperedPenalty += p.trappedKnightPenalty;

        if(sq == c6 && (ourPawnsBB & BoardUtils::bits[c7]) && (ourPawnsBB & BoardUtils::bits[d5]) && !(ourPawnsBB & BoardUtils::bits[e5]))
            untaperedPenalty += p.blockingCKnight;
    }
    eval -= Score(untaperedPenalty, untaperedPenalty);

    // bonus if defended by pawns
    if(ourPawnAttacksBB & BoardUtils::bits[sq])
        eval += Score(p.knightDefByPawn[0], p.knightDefByPawn[1]);

    // attacks
    U64 sqNearKing = (color == White ? BoardUtils::squaresNearBlackKing[board->blackKingSquare] : BoardUtils::squaresNearWhiteKing[board->whiteKingSquare]);

    int attackedSquares = MagicBitboardUtils::popcount(BoardUtils::knightAttacksBB[sq] & sqNearKing);
    if(attackedSquares) {
        if(color == White) {
            whiteAttackersCnt++;
            whiteAttackWeight += p.pieceAttackWeight[Knight] * attackedSquares;
        } else {
            blackAttackersCnt++;
            blackAttackWeight += p.pieceAttackWeight[Knight] * attackedSquares;
        }
    }

    return eval;
}

Score evalBishop(int sq, int color, const EvalParams& p) {
    U64 ourPawnsBB = (board->whitePiecesBB & board->pawnsBB);
    U64 opponentPawnsBB = (board->blackPiecesBB & board->pawnsBB);
    if(color == Black) swap(ourPawnsBB, opponentPawnsBB);

    U64 opponentPawnAttacksBB = BoardUtils::pawnAttacks(opponentPawnsBB, (color ^ (White | Black)));

    U64 ourPiecesBB = (color == White ? board->whitePiecesBB : board->blackPiecesBB);

    if(color == White) pieceMaterialWhite += PIECE_VALUES[Bishop];
    else pieceMaterialBlack += PIECE_VALUES[Bishop];

    int relSq = (color == White ? sq : FLIPPED[sq]);

    // initial piece value and square value
    Score eval(
        p.mgPieceValues[Bishop] + p.mgBishopTable[relSq],
        p.egPieceValues[Bishop] + p.egBishopTable[relSq]
    );

    // traps and blockages
    int untaperedAdj = 0;
    if(color == White) {
        if(sq == a7 && (opponentPawnsBB & BoardUtils::bits[b6]) && (opponentPawnsBB & BoardUtils::bits[c7]))
            untaperedAdj -= p.trappedBishopPenalty;
        if(sq == h7 && (opponentPawnsBB & BoardUtils::bits[g6]) && (opponentPawnsBB & BoardUtils::bits[f7]))
            untaperedAdj -= p.trappedBishopPenalty;

        if(sq == c1 && (ourPawnsBB & BoardUtils::bits[d2]) && (ourPiecesBB & BoardUtils::bits[e3]))
            untaperedAdj -= p.blockedBishopPenalty;
        if(sq == f1 && (ourPawnsBB & BoardUtils::bits[e2]) && (ourPiecesBB & BoardUtils::bits[d3]))
            untaperedAdj -= p.blockedBishopPenalty;
    }
    if(color == Black) {
        if(sq == a2 && (opponentPawnsBB & BoardUtils::bits[b3]) && (opponentPawnsBB & BoardUtils::bits[c2]))
            untaperedAdj -= p.trappedBishopPenalty;
        if(sq == h2 && (opponentPawnsBB & BoardUtils::bits[g3]) && (opponentPawnsBB & BoardUtils::bits[f2]))
            untaperedAdj -= p.trappedBishopPenalty;

        if(sq == c8 && (ourPawnsBB & BoardUtils::bits[d7]) && (ourPiecesBB & BoardUtils::bits[e6]))
            untaperedAdj -= p.blockedBishopPenalty;
        if(sq == f8 && (ourPawnsBB & BoardUtils::bits[e7]) && (ourPiecesBB & BoardUtils::bits[d6]))
            untaperedAdj -= p.blockedBishopPenalty;
    }

    // fianchetto bonus (bishop on long diagonal on the second rank)
    if(color == White && sq == g2 && (ourPawnsBB & BoardUtils::bits[g3]) && (ourPawnsBB & BoardUtils::bits[f2])) untaperedAdj += p.fianchettoBonus;
    if(color == White && sq == b2 && (ourPawnsBB & BoardUtils::bits[b3]) && (ourPawnsBB & BoardUtils::bits[c2])) untaperedAdj += p.fianchettoBonus;
    if(color == Black && sq == g7 && (ourPawnsBB & BoardUtils::bits[g6]) && (ourPawnsBB & BoardUtils::bits[f7])) untaperedAdj += p.fianchettoBonus;
    if(color == Black && sq == b7 && (ourPawnsBB & BoardUtils::bits[b6]) && (ourPawnsBB & BoardUtils::bits[c7])) untaperedAdj += p.fianchettoBonus;

    eval += Score(untaperedAdj, untaperedAdj);

    // mobility and attacks
    U64 sqNearKing = (color == White ? BoardUtils::squaresNearBlackKing[board->blackKingSquare] : BoardUtils::squaresNearWhiteKing[board->whiteKingSquare]);
    U64 attacks = MagicBitboardUtils::magicBishopAttacks((board->whitePiecesBB | board->blackPiecesBB), sq);

    int mobility = MagicBitboardUtils::popcount(attacks & ~ourPiecesBB & ~opponentPawnAttacksBB);
    int attackedSquares = MagicBitboardUtils::popcount(attacks & sqNearKing);

    int mobDiff = mobility - 5;
    eval += Score(p.bishopMobility[0] * mobDiff, p.bishopMobility[1] * mobDiff);
    if(attackedSquares) {
        if(color == White) {
            whiteAttackersCnt++;
            whiteAttackWeight += p.pieceAttackWeight[Bishop] * attackedSquares;
        } else {
            blackAttackersCnt++;
            blackAttackWeight += p.pieceAttackWeight[Bishop] * attackedSquares;
        }
    }

    return eval;
}

Score evalRook(int sq, int color, const EvalParams& p) {
    U64 currFileBB = BoardUtils::filesBB[sq%8];
    U64 currRankBB = BoardUtils::ranksBB[sq/8];

    U64 ourPiecesBB = (color == White ? board->whitePiecesBB : board->blackPiecesBB);
    U64 opponentPiecesBB = (color == Black ? board->whitePiecesBB : board->blackPiecesBB);
    U64 ourPawnsBB = (board->whitePiecesBB & board->pawnsBB);
    U64 opponentPawnsBB = (board->blackPiecesBB & board->pawnsBB);
    if(color == Black) swap(ourPawnsBB, opponentPawnsBB);

    U64 opponentPawnAttacksBB = BoardUtils::pawnAttacks(opponentPawnsBB, (color ^ (White | Black)));

    int opponentKingSquare = (color == White ? board->blackKingSquare : board->whiteKingSquare);

    // in this case seventh rank means the second rank in the opponent's half
    int seventhRank = (color == White ? 6 : 1);
    int eighthRank = (color == White ? 7 : 0);

    if(color == White) pieceMaterialWhite += PIECE_VALUES[Rook];
    else pieceMaterialBlack += PIECE_VALUES[Rook];

    int relSq = (color == White ? sq : FLIPPED[sq]);

    // initial piece value and square value
    Score eval(
        p.mgPieceValues[Rook] + p.mgRookTable[relSq],
        p.egPieceValues[Rook] + p.egRookTable[relSq]
    );

    int untaperedAdj = 0;

    // blocked by uncastled king
    if(color == White) {
        if((board->whiteKingSquare == f1 || board->whiteKingSquare == g1) && (sq == g1 || sq == h1))
            untaperedAdj -= p.blockedRookPenalty;
        if((board->whiteKingSquare == c1 || board->whiteKingSquare == b1) && (sq == a1 || sq == b1))
            untaperedAdj -= p.blockedRookPenalty;
    }
    if(color == Black) {
        if((board->blackKingSquare == f8 || board->blackKingSquare == g8) && (sq == g8 || sq == h8))
            untaperedAdj -= p.blockedRookPenalty;
        if((board->blackKingSquare == c8 || board->blackKingSquare == b8) && (sq == a8 || sq == b8))
            untaperedAdj -= p.blockedRookPenalty;
    }

    // the rook becomes more valuable as there are less pawns on the board
    int numberOfPawns = MagicBitboardUtils::popcount(board->pawnsBB);
    untaperedAdj += p.rookPawnConst * (8 - numberOfPawns);

    // bonus for a rook on an open or semi open file
    bool ourBlockingPawns = (currFileBB & ourPawnsBB);
    bool opponentBlockingPawns = (currFileBB & opponentPawnsBB);

    if(!ourBlockingPawns) {
        if(opponentBlockingPawns) eval += Score(p.rookOnOpenFile[0] / 2, p.rookOnOpenFile[1] / 2); // semi open file
        else eval += Score(p.rookOnOpenFile[0], p.rookOnOpenFile[1]); // open file
    }

    // the rook on the seventh rank gets a huge bonus if there are pawns on that rank or if it restricts the king to the eighth rank
    if(sq/8 == seventhRank && (opponentKingSquare/8 == eighthRank || (opponentPawnsBB & BoardUtils::ranksBB[seventhRank])))
        eval += Score(p.rookOnSeventh[0], p.rookOnSeventh[1]);

    // small bonus if the rook is defended by another rook
    if((board->rooksBB & ourPiecesBB & (currRankBB | currFileBB)) ^ BoardUtils::bits[sq])
        eval += Score(p.rooksDefEachOther[0], p.rooksDefEachOther[1]);

    // bonus for a rook that is on the same file as the enemy queen
    if(currFileBB & opponentPiecesBB & board->queensBB) untaperedAdj += p.rookOnQueenFile;

    eval += Score(untaperedAdj, untaperedAdj);

    // mobility and attacks
    U64 sqNearKing = (color == White ? BoardUtils::squaresNearBlackKing[board->blackKingSquare] : BoardUtils::squaresNearWhiteKing[board->whiteKingSquare]);
    U64 attacks = MagicBitboardUtils::magicRookAttacks((board->whitePiecesBB | board->blackPiecesBB), sq);

    int mobility = MagicBitboardUtils::popcount(attacks & ~ourPiecesBB & ~opponentPawnAttacksBB);
    int attackedSquares = MagicBitboardUtils::popcount(attacks & sqNearKing);

    int mobDiff = mobility - 7;
    eval += Score(p.rookMobility[0] * mobDiff, p.rookMobility[1] * mobDiff);
    if(attackedSquares) {
        if(color == White) {
            whiteAttackersCnt++;
            whiteAttackWeight += p.pieceAttackWeight[Rook] * attackedSquares;
        } else {
            blackAttackersCnt++;
            blackAttackWeight += p.pieceAttackWeight[Rook] * attackedSquares;
        }
    }

    return eval;
}

Score evalQueen(int sq, int color, const EvalParams& p) {
    U64 ourPiecesBB = (color == White ? board->whitePiecesBB : board->blackPiecesBB);
    U64 opponentPiecesBB = (color == Black ? board->whitePiecesBB : board->blackPiecesBB);
    U64 ourBishopsBB = (board->bishopsBB & ourPiecesBB);
    U64 ourKnightsBB = (board->knightsBB & ourPiecesBB);

    U64 opponentPawnsBB = (board->pawnsBB & opponentPiecesBB);
    U64 opponentPawnAttacksBB = BoardUtils::pawnAttacks(opponentPawnsBB, (color ^ (White | Black)));

    if(color == White) pieceMaterialWhite += PIECE_VALUES[Queen];
    else pieceMaterialBlack += PIECE_VALUES[Queen];

    int relSq = (color == White ? sq : FLIPPED[sq]);

    // initial piece value and square value
    Score eval(
        p.mgPieceValues[Queen] + p.mgQueenTable[relSq],
        p.egPieceValues[Queen] + p.egQueenTable[relSq]
    );

    // penalty for early development
    int earlyPenalty = 0;
    if(color == White && sq/8 > 1) {
        if(ourKnightsBB & BoardUtils::bits[b1]) earlyPenalty += p.earlyQueenDevelopment;
        if(ourBishopsBB & BoardUtils::bits[c1]) earlyPenalty += p.earlyQueenDevelopment;
        if(ourBishopsBB & BoardUtils::bits[f1]) earlyPenalty += p.earlyQueenDevelopment;
        if(ourKnightsBB & BoardUtils::bits[g1]) earlyPenalty += p.earlyQueenDevelopment;
    }
    if(color == Black && sq/8 < 6) {
        if(ourKnightsBB & BoardUtils::bits[b8]) earlyPenalty += p.earlyQueenDevelopment;
        if(ourBishopsBB & BoardUtils::bits[c8]) earlyPenalty += p.earlyQueenDevelopment;
        if(ourBishopsBB & BoardUtils::bits[f8]) earlyPenalty += p.earlyQueenDevelopment;
        if(ourKnightsBB & BoardUtils::bits[g8]) earlyPenalty += p.earlyQueenDevelopment;
    }
    eval -= Score(earlyPenalty, earlyPenalty);

    // mobility and attacks
    U64 sqNearKing = (color == White ? BoardUtils::squaresNearBlackKing[board->blackKingSquare] : BoardUtils::squaresNearWhiteKing[board->whiteKingSquare]);
    U64 attacks = (MagicBitboardUtils::magicBishopAttacks((board->whitePiecesBB | board->blackPiecesBB), sq)
                 | MagicBitboardUtils::magicRookAttacks((board->whitePiecesBB | board->blackPiecesBB), sq));

    int mobility = MagicBitboardUtils::popcount(attacks & ~ourPiecesBB & ~opponentPawnAttacksBB);
    int attackedSquares = MagicBitboardUtils::popcount(attacks & sqNearKing);

    int mobDiff = mobility - 14;
    eval += Score(p.queenMobility[0] * mobDiff, p.queenMobility[1] * mobDiff);
    if(attackedSquares) {
        if(color == White) {
            whiteAttackersCnt++;
            whiteAttackWeight += p.pieceAttackWeight[Queen] * attackedSquares;
        } else {
            blackAttackersCnt++;
            blackAttackWeight += p.pieceAttackWeight[Queen] * attackedSquares;
        }
    }
    return eval;
}

int whiteKingShield(const int kingShield[3]) {
    U64 ourPawnsBB = (board->whitePiecesBB & board->pawnsBB);
    int sq = board->whiteKingSquare;

    int eval = 0;
    // queen side
    if(sq%8 < 3) {
        if(ourPawnsBB & BoardUtils::bits[a2]) eval += kingShield[1];
        else if(ourPawnsBB & BoardUtils::bits[a3]) eval += kingShield[2];
        else eval -= kingShield[0];

        if(ourPawnsBB & BoardUtils::bits[b2]) eval += kingShield[1];
        else if(ourPawnsBB & BoardUtils::bits[b3]) eval += kingShield[2];
        else eval -= kingShield[0];

        if(ourPawnsBB & BoardUtils::bits[c2]) eval += kingShield[1];
        else if(ourPawnsBB & BoardUtils::bits[c3]) eval += kingShield[2];
        else eval -= kingShield[0];
    }

    // king side
    else if(sq%8 > 4) {
        if(ourPawnsBB & BoardUtils::bits[f2]) eval += kingShield[1];
        else if(ourPawnsBB & BoardUtils::bits[f3]) eval += kingShield[2];
        else eval -= kingShield[0];

        if(ourPawnsBB & BoardUtils::bits[g2]) eval += kingShield[1];
        else if(ourPawnsBB & BoardUtils::bits[g3]) eval += kingShield[2];
        else eval -= kingShield[0];

        if(ourPawnsBB & BoardUtils::bits[h2]) eval += kingShield[1];
        else if(ourPawnsBB & BoardUtils::bits[h3]) eval += kingShield[2];
        else eval -= kingShield[0];
    }
    return eval;
}

int blackKingShield(const int kingShield[3]) {
    U64 ourPawnsBB = (board->blackPiecesBB & board->pawnsBB);
    int sq = board->blackKingSquare;

    int eval = 0;
    // queen side
    if(sq%8 < 3) {
        if(ourPawnsBB & BoardUtils::bits[a7]) eval += kingShield[1];
        else if(ourPawnsBB & BoardUtils::bits[a6]) eval += kingShield[2];
        else eval -= kingShield[0];

        if(ourPawnsBB & BoardUtils::bits[b7]) eval += kingShield[1];
        else if(ourPawnsBB & BoardUtils::bits[b6]) eval += kingShield[2];
        else eval -= kingShield[0];

        if(ourPawnsBB & BoardUtils::bits[c7]) eval += kingShield[1];
        else if(ourPawnsBB & BoardUtils::bits[c6]) eval += kingShield[2];
        else eval -= kingShield[0];
    }

    // king side
    else if(sq%8 > 4) {
        if(ourPawnsBB & BoardUtils::bits[f7]) eval += kingShield[1];
        else if(ourPawnsBB & BoardUtils::bits[f6]) eval += kingShield[2];
        else eval -= kingShield[0];

        if(ourPawnsBB & BoardUtils::bits[g7]) eval += kingShield[1];
        else if(ourPawnsBB & BoardUtils::bits[g6]) eval += kingShield[2];
        else eval -= kingShield[0];

        if(ourPawnsBB & BoardUtils::bits[h7]) eval += kingShield[1];
        else if(ourPawnsBB & BoardUtils::bits[h6]) eval += kingShield[2];
        else eval -= kingShield[0];
    }
    return eval;
}

// evaluate every pawn independently
Score evalPawnStructure(const EvalParams& p) {
    U64 whitePawns = (board->pawnsBB & board->whitePiecesBB);
    U64 blackPawns = (board->pawnsBB & board->blackPiecesBB);

    Score eval(0, 0);
    while(whitePawns) {
        int sq = MagicBitboardUtils::bitscanForward(whitePawns);
        eval += evalPawn(sq, White, p);
        whitePawns &= (whitePawns - 1);
    }
    while(blackPawns) {
        int sq = MagicBitboardUtils::bitscanForward(blackPawns);
        eval -= evalPawn(sq, Black, p);
        blackPawns &= (blackPawns - 1);
    }

    return eval;
}

Score evalPawn(int sq, int color, const EvalParams& p) {
    U64 ourPiecesBB = (color == White ? board->whitePiecesBB : board->blackPiecesBB);

    U64 opponentPawnsBB = (board->pawnsBB & (color == White ? board->blackPiecesBB : board->whitePiecesBB));
    U64 ourPawnsBB = (board->pawnsBB & (color == White ? board->whitePiecesBB : board->blackPiecesBB));

    U64 opponentPawnAttacksBB = BoardUtils::pawnAttacks(opponentPawnsBB, (color == White ? Black : White));
    U64 ourPawnAttacksBB = BoardUtils::pawnAttacks(ourPawnsBB, color);

    bool weak = true, passed = true, opposed = false;

    if(color == White) pawnCntWhite++;
    else pawnCntBlack++;

    int relSq = (color == White ? sq : FLIPPED[sq]);

    // initial pawn value + square value
    Score eval(
        p.mgPieceValues[Pawn] + p.mgPawnTable[relSq],
        p.egPieceValues[Pawn] + p.egPawnTable[relSq]
    );
    int dir = (color == White ? 8 : -8);

    // check squares in front of the pawn to see if it is passed or opposed/doubled
    int curSq = sq + dir;
    while(curSq < 64 && curSq >= 0) {
        if(board->pawnsBB & BoardUtils::bits[curSq]) {
            passed = false;
            if(ourPiecesBB & BoardUtils::bits[curSq])
                eval -= Score(p.doubledPawnsPenalty[0], p.doubledPawnsPenalty[1]);
            else opposed = true;
        }
        if(opponentPawnAttacksBB & BoardUtils::bits[curSq]) passed = false;

        curSq += dir;
    }

    // check squares behind the pawn to see if it can be protected by other pawns
    curSq = sq;
    while(curSq < 64 && curSq >= 0) {
        if(ourPawnAttacksBB & BoardUtils::bits[curSq]) {
            weak = false;
            break;
        }
        curSq -= dir;
    }

    // bonus for passed pawns, bigger bonus for protected passers
    // the bonus is also bigger if the pawn is more advanced
    if(passed) {
        int mgBonus = p.mgPassedPawnTable[relSq];
        int egBonus = p.egPassedPawnTable[relSq];
        if(ourPawnAttacksBB & BoardUtils::bits[sq]) {
            mgBonus = (mgBonus * 4) / 3;
            egBonus = (egBonus * 4) / 3;
        }

        eval += Score(mgBonus, egBonus);
    }

    // penalty for weak (backward or isolated) pawns and bigger penalty if they are on a semi open file
    if(weak) {
        int mgPenalty = p.weakPawnPenalty[0];
        int egPenalty = p.weakPawnPenalty[1];
        if(!opposed) {
            mgPenalty = (mgPenalty * 4) / 3;
            egPenalty = (egPenalty * 4) / 3;
        }

        eval -= Score(mgPenalty, egPenalty);
    }

    // penalty for having a pawn on d4 and not having a pawn on c4 or c3 in a d4 opening
    if(color == White && sq == c2 && (ourPawnsBB & BoardUtils::bits[d4]) && !(ourPawnsBB & BoardUtils::bits[e4]))
        eval -= Score(p.cPawnPenalty, p.cPawnPenalty);
    if(color == Black && sq == c7 && (ourPawnsBB & BoardUtils::bits[d5]) && !(ourPawnsBB & BoardUtils::bits[e5]))
        eval -= Score(p.cPawnPenalty, p.cPawnPenalty);

    return eval;
}

int gamePhase() {
    return MagicBitboardUtils::popcount(board->knightsBB) * MG_WEIGHT[Knight] 
         + MagicBitboardUtils::popcount(board->bishopsBB) * MG_WEIGHT[Bishop] 
         + MagicBitboardUtils::popcount(board->rooksBB) * MG_WEIGHT[Rook]
         + MagicBitboardUtils::popcount(board->queensBB) * MG_WEIGHT[Queen]; 
}
