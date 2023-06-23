#include <unordered_map>

#include "Evaluate.h"
#include "Board.h"
#include "MagicBitboards.h"
#include "TranspositionTable.h"

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

int KING_SAFETY_TABLE[100] = {
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

// piece square tables
int MG_KING_TABLE[64] = {
    40, 50, 30, 10, 10, 30, 50, 40,
    30, 40, 20, 0, 0, 20, 40, 30,
    10, 20, 0, -20, -20, 0, 20, 10,
    0, 10, -10, -30, -30, -10, 10, 0,
    -10, 0, -20, -40, -40, -20, 0, -10,
    -20, -10, -30, -50, -50, -30, -10, -20,
    -30, -20, -40, -60, -60, -40, -20, -30,
    -40, -30, -50, -70, -70, -50, -30, -40
};

int EG_KING_TABLE[64] = {
    -72, -48, -36, -24, -24, -36, -48, -72,
    -48, -24, -12, 0, 0, -12, -24, -48,
    -36, -12, 0, 12, 12, 0, -12, -36,
    -24, 0, 12, 24, 24, 12, 0, -24,
    -24, 0, 12, 24, 24, 12, 0, -24,
    -36, -12, 0, 12, 12, 0, -12, -36,
    -48, -24, -12, 0, 0, -12, -24, -48,
    -72, -48, -36, -24, -24, -36, -48, -72
};

int QUEEN_TABLE[64] = {
    -5, -5, -5, -5, -5, -5, -5, -5,
    0, 0, 1, 1, 1, 1, 0, 0,
    0, 0, 1, 2, 2, 1, 0, 0,
    0, 0, 2, 3, 3, 2, 0, 0,
    0, 0, 2, 3, 3, 2, 0, 0,
    0, 0, 1, 2, 2, 1, 0, 0,
    0, 0, 1, 1, 1, 1, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0
};

int ROOK_TABLE[64] = {
    0, 0, 0, 2, 2, 0, 0, 0,
    -5, 0, 0, 0, 0, 0, 0, -5,
    -5, 0, 0, 0, 0, 0, 0, -5,
    -5, 0, 0, 0, 0, 0, 0, -5,
    -5, 0, 0, 0, 0, 0, 0, -5,
    -5, 0, 0, 0, 0, 0, 0, -5,
    -5, 0, 0, 0, 0, 0, 0, -5,
    5, 5, 5, 5, 5, 5, 5, 5,
};

int BISHOP_TABLE[64] = {
    -4, -4, -12, -4, -4, -12, -4, -4,
    -4, 2, 1, 1, 1, 1, 2, -4,
    -4, 0, 2, 4, 4, 2, 0, -4,
    -4, 0, 4, 6, 6, 4, 0, -4,
    -4, 0, 4, 6, 6, 4, 0, -4,
    -4, 1, 2, 4, 4, 2, 1, -4,
    -4, 0, 0, 0, 0, 0, 0, -4,
    -4, -4, -4, -4, -4, -4, -4, -4,
};

int KNIGHT_TABLE[64] = {
    -8, -12, -8, -8, -8, -8, -12, -8,
    -8, 0, 0, 0, 0, 0, 0, -8,
    -8, 0, 4, 4, 4, 4, 0, -8,
    -8, 0, 4, 8, 8, 4, 0, -8,
    -8, 0, 4, 8, 8, 4, 0, -8,
    -8, 0, 4, 4, 4, 4, 0, -8,
    -8, 0, 1, 2, 2, 1, 0, -8,
    -8, -8, -8, -8, -8, -8, -8, -8,
};

int MG_PAWN_TABLE[64] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    -6, -4, 1, -24, -24, 1, -4, -6,
    -4, -4, 1, 5, 5, 1, -4, -4,
    -6, -4, 5, 10, 10, 5, -4, -6,
    -6, -4, 2, 8, 8, 2, -4, -6,
    -6, -4, 1, 2, 2, 1, -4, -6,
    -6, -4, 1, 1, 1, 1, -4, -6,
    0, 0, 0, 0, 0, 0, 0, 0
};

int EG_PAWN_TABLE[64] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    20, 20, 20, 20, 20, 20, 20, 20,
    20, 20, 20, 20, 20, 20, 20, 20,
    40, 40, 40, 40, 40, 40, 40, 40,
    60, 60, 60, 60, 60, 60, 60, 60,
    80, 80, 80, 80, 80, 80, 80, 80,
    100, 100, 100, 100, 100, 100, 100, 100,
    0, 0, 0, 0, 0, 0, 0, 0,
};

int PASSED_PAWN_TABLE[64] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    20, 20, 20, 20, 20, 20, 20, 20,
    20, 20, 20, 20, 20, 20, 20, 20,
    40, 40, 40, 40, 40, 40, 40, 40,
    60, 60, 60, 60, 60, 60, 60, 60,
    80, 80, 80, 80, 80, 80, 80, 80,
    100, 100, 100, 100, 100, 100, 100, 100,
    0, 0, 0, 0, 0, 0, 0, 0,
};

int KING_SHIELD[3] = {5, 10, 5};

int PIECE_VALUES[7] = {0, 100, 325, 350, 500, 975, 0};

int PIECE_ATTACK_WEIGHT[6] = {0, 0, 2, 2, 3, 5};

// bonuses and penalties according to various features of the position
int KNIGHT_MOBILITY = 4;
int KNIGHT_PAWN_CONST = 3;
int TRAPPED_KNIGHT_PENALTY = 100;
int KNIGHT_DEF_BY_PAWN = 15;
int BLOCKING_C_KNIGHT = 30;
int KNIGHT_PAIR_PENALTY = 20;

int BISHOP_PAIR = 50;
int TRAPPED_BISHOP_PENALTY = 100;
int FIANCHETTO_BONUS = 20;
int BISHOP_MOBILITY = 5;
int BLOCKED_BISHOP_PENALTY = 50;

int ROOK_ON_QUEEN_FILE = 10;
int ROOK_ON_OPEN_FILE = 20;
int ROOK_PAWN_CONST = 3;
int ROOK_ON_SEVENTH = 30;
int ROOKS_DEF_EACH_OTHER = 5;
int ROOK_MOBILITY = 3;
int BLOCKED_ROOK_PENALTY = 50;

int EARLY_QUEEN_DEVELOPMENT = 20;
int QUEEN_MOBILITY = 2;

int DOUBLED_PAWNS_PENALTY = 40;
int WEAK_PAWN_PENALTY = 15;
int C_PAWN_PENALTY = 25;

int TEMPO_BONUS = 10;


int whiteAttackersCnt, blackAttackersCnt, whiteAttackWeight, blackAttackWeight;
int pawnCntWhite, pawnCntBlack, pieceMaterialWhite, pieceMaterialBlack;

int gamePhase();

int evalPawn(
    int sq, int color, 
    int MG_PAWN_TABLE[64], int EG_PAWN_TABLE[64], int PASSED_PAWN_TABLE[64],
    int& DOUBLED_PAWNS_PENALTY, int& WEAK_PAWN_PENALTY, int& C_PAWN_PENALTY,
    int PIECE_VALUES[7]
);
int evalKnight( 
    int sq, int color, 
    int KNIGHT_TABLE[64], int& KNIGHT_MOBILITY, 
    int& KNIGHT_PAWN_CONST, int& TRAPPED_KNIGHT_PENALTY, int& BLOCKING_C_KNIGHT, int& KNIGHT_DEF_BY_PAWN,
    int PIECE_VALUES[7], int PIECE_ATTACK_WEIGHT[6]
);
int evalBishop(
    int sq, int color, 
    int BISHOP_TABLE[64], int& TRAPPED_BISHOP_PENALTY, 
    int& BLOCKED_BISHOP_PENALTY, int& FIANCHETTO_BONUS, int& BISHOP_MOBILITY,
    int PIECE_VALUES[7], int PIECE_ATTACK_WEIGHT[6]
);
int evalRook(
    int sq, int color, 
    int ROOK_TABLE[64], int& BLOCKED_ROOK_PENALTY,
    int& ROOK_PAWN_CONST, int& ROOK_ON_OPEN_FILE, int& ROOK_ON_SEVENTH, int& ROOKS_DEF_EACH_OTHER,
    int& ROOK_ON_QUEEN_FILE, int& ROOK_MOBILITY,
    int PIECE_VALUES[7], int PIECE_ATTACK_WEIGHT[6]
);
int evalQueen(
    int sq, int color, 
    int QUEEN_TABLE[64], int& EARLY_QUEEN_DEVELOPMENT,
    int& QUEEN_MOBILITY, int PIECE_VALUES[7], int PIECE_ATTACK_WEIGHT[6]
);
int evalPawnStructure(
    bool useHash,
    int MG_PAWN_TABLE[64], int EG_PAWN_TABLE[64], int PASSED_PAWN_TABLE[64],
    int& DOUBLED_PAWNS_PENALTY, int& WEAK_PAWN_PENALTY, int& C_PAWN_PENALTY,
    int PIECE_VALUES[7]
);

int whiteKingShield(int KING_SHIELD[3]), blackKingShield(int KING_SHIELD[3]);

int evaluate(
    bool usePawnHash,

    int MG_KING_TABLE[64], int EG_KING_TABLE[64],
    int QUEEN_TABLE[64], int ROOK_TABLE[64], int BISHOP_TABLE[64], 
    int KNIGHT_TABLE[64], int MG_PAWN_TABLE[64], int EG_PAWN_TABLE[64], int PASSED_PAWN_TABLE[64],

    int KING_SHIELD[3], int PIECE_VALUES[7], int PIECE_ATTACK_WEIGHT[6],

    int& KNIGHT_MOBILITY, int& KNIGHT_PAWN_CONST, int& TRAPPED_KNIGHT_PENALTY,
    int& KNIGHT_DEF_BY_PAWN, int& BLOCKING_C_KNIGHT, int& KNIGHT_PAIR_PENALTY, 

    int& BISHOP_PAIR, int& TRAPPED_BISHOP_PENALTY, int& FIANCHETTO_BONUS, 
    int& BISHOP_MOBILITY, int& BLOCKED_BISHOP_PENALTY,

    int& ROOK_ON_QUEEN_FILE, int& ROOK_ON_OPEN_FILE, int& ROOK_PAWN_CONST,
    int& ROOK_ON_SEVENTH, int& ROOKS_DEF_EACH_OTHER, int& ROOK_MOBILITY,
    int& BLOCKED_ROOK_PENALTY,

    int& EARLY_QUEEN_DEVELOPMENT, int& QUEEN_MOBILITY,

    int& DOUBLED_PAWNS_PENALTY, int& WEAK_PAWN_PENALTY, int& C_PAWN_PENALTY,

    int& TEMPO_BONUS
    ) {

    // reset everything
    whiteAttackersCnt = blackAttackersCnt = 0;
    whiteAttackWeight = blackAttackWeight = 0;
    pawnCntWhite = pawnCntBlack = 0;
    pieceMaterialWhite = pieceMaterialBlack = 0;

    // evaluate pieces independently
    int res = 0;
    for(int sq = 0; sq < 64; sq++) {
        if(board.squares[sq] == Empty) continue;

        int color = (board.squares[sq] & (Black | White));
        int c = (color == White ? 1 : -1);

        if(board.knightsBB & bits[sq]) res += evalKnight(
            sq, color, KNIGHT_TABLE,
            KNIGHT_MOBILITY, KNIGHT_PAWN_CONST, TRAPPED_KNIGHT_PENALTY, 
            BLOCKING_C_KNIGHT, KNIGHT_DEF_BY_PAWN, PIECE_VALUES, PIECE_ATTACK_WEIGHT) * c;

        if(board.bishopsBB & bits[sq]) res += evalBishop(
            sq, color, BISHOP_TABLE, 
            TRAPPED_BISHOP_PENALTY, BLOCKED_BISHOP_PENALTY, 
            FIANCHETTO_BONUS, BISHOP_MOBILITY, PIECE_VALUES, PIECE_ATTACK_WEIGHT) * c;

        if(board.rooksBB & bits[sq]) res += evalRook(
            sq, color, ROOK_TABLE, 
            BLOCKED_ROOK_PENALTY,ROOK_PAWN_CONST, ROOK_ON_OPEN_FILE, 
            ROOK_ON_SEVENTH, ROOKS_DEF_EACH_OTHER, ROOK_ON_QUEEN_FILE, ROOK_MOBILITY, 
            PIECE_VALUES, PIECE_ATTACK_WEIGHT) * c;

        if(board.queensBB & bits[sq]) res += evalQueen(
            sq, color, QUEEN_TABLE, EARLY_QUEEN_DEVELOPMENT,
            QUEEN_MOBILITY, PIECE_VALUES, PIECE_ATTACK_WEIGHT) * c;
    }
    res += evalPawnStructure(
        usePawnHash,
        MG_PAWN_TABLE, EG_PAWN_TABLE, PASSED_PAWN_TABLE,
        DOUBLED_PAWNS_PENALTY, WEAK_PAWN_PENALTY, C_PAWN_PENALTY,
        PIECE_VALUES
    );

    // evaluate kings based on the current game phase (king centralization becomes more important than safety as pieces disappear from the board)
    int mgWeight = min(gamePhase(), 24);
    int egWeight = 24-mgWeight;

    int mgKingScore = whiteKingShield(KING_SHIELD) - blackKingShield(KING_SHIELD);
    int egKingScore = 0;

    // evaluate king safety in the middlegame

    // if only 1 or 2 attackers, we consider the king safe
    if(whiteAttackersCnt <= 2) whiteAttackWeight = 0;
    if(blackAttackersCnt <= 2) blackAttackWeight = 0;

    mgKingScore += KING_SAFETY_TABLE[whiteAttackWeight] - KING_SAFETY_TABLE[blackAttackWeight];
    mgKingScore += MG_KING_TABLE[board.whiteKingSquare] - MG_KING_TABLE[FLIPPED[board.blackKingSquare]];

    egKingScore += EG_KING_TABLE[board.whiteKingSquare] - EG_KING_TABLE[FLIPPED[board.blackKingSquare]];

    res += (mgWeight * mgKingScore + egWeight * egKingScore) / 24;

    // tempo bonus
    if(board.turn == White) res += TEMPO_BONUS;
    else res -= TEMPO_BONUS;

    // add scores for bishop and knight pairs
    if(popcount(board.whitePiecesBB & board.bishopsBB) >= 2) res += BISHOP_PAIR;
    if(popcount(board.blackPiecesBB & board.bishopsBB) >= 2) res -= BISHOP_PAIR;

    if(popcount(board.whitePiecesBB & board.knightsBB) >= 2) res -= KNIGHT_PAIR_PENALTY;
    if(popcount(board.blackPiecesBB & board.knightsBB) >= 2) res += KNIGHT_PAIR_PENALTY;

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
        if(weakerPawns == 0 && weakerPieces == 2*PIECE_VALUES[Knight]) return 0;

        // rook vs minor piece
        if(strongerPieces == PIECE_VALUES[Rook] && (weakerPieces == PIECE_VALUES[Knight] || weakerPieces == PIECE_VALUES[Bishop]) )
            res /= 2;

        // rook and minor vs rook
        if((strongerPieces == PIECE_VALUES[Rook] + PIECE_VALUES[Bishop] || strongerPieces == PIECE_VALUES[Rook] + PIECE_VALUES[Knight])
           && weakerPieces == PIECE_VALUES[Rook])
            res /= 2;
    }
    // return result from the perspective of the side to move
    if(board.turn == Black) res *= -1;

    return res;
}

int evaluate() {
    return evaluate(   
        true,

        MG_KING_TABLE, EG_KING_TABLE,
        QUEEN_TABLE, ROOK_TABLE, BISHOP_TABLE, 
        KNIGHT_TABLE, MG_PAWN_TABLE, EG_PAWN_TABLE, PASSED_PAWN_TABLE,

        KING_SHIELD, PIECE_VALUES, PIECE_ATTACK_WEIGHT,

        KNIGHT_MOBILITY, KNIGHT_PAWN_CONST, TRAPPED_KNIGHT_PENALTY,
        KNIGHT_DEF_BY_PAWN, BLOCKING_C_KNIGHT, KNIGHT_PAIR_PENALTY, 

        BISHOP_PAIR, TRAPPED_BISHOP_PENALTY, FIANCHETTO_BONUS, 
        BISHOP_MOBILITY, BLOCKED_BISHOP_PENALTY,

        ROOK_ON_QUEEN_FILE, ROOK_ON_OPEN_FILE, ROOK_PAWN_CONST, ROOK_ON_SEVENTH, 
        ROOKS_DEF_EACH_OTHER, ROOK_MOBILITY, BLOCKED_ROOK_PENALTY,

        EARLY_QUEEN_DEVELOPMENT, QUEEN_MOBILITY,

        DOUBLED_PAWNS_PENALTY, WEAK_PAWN_PENALTY, C_PAWN_PENALTY,

        TEMPO_BONUS);
}

int evalKnight( 
    int sq, int color, 
    int KNIGHT_TABLE[64], int& KNIGHT_MOBILITY, 
    int& KNIGHT_PAWN_CONST, int& TRAPPED_KNIGHT_PENALTY, int& BLOCKING_C_KNIGHT, int& KNIGHT_DEF_BY_PAWN,
    int PIECE_VALUES[7], int PIECE_ATTACK_WEIGHT[6]
) {
    U64 opponentPawnsBB = (board.pawnsBB & (color == White ? board.blackPiecesBB : board.whitePiecesBB));
    U64 ourPawnsBB = (board.pawnsBB ^ opponentPawnsBB);
    U64 ourPiecesBB = (color == White ? board.whitePiecesBB : board.blackPiecesBB);

    U64 ourPawnAttacksBB = pawnAttacks(ourPawnsBB, color);
    U64 opponentPawnAttacksBB = pawnAttacks(opponentPawnsBB, (color ^ (White | Black)));

    if(color == White) pieceMaterialWhite += PIECE_VALUES[Knight];
    else pieceMaterialBlack += PIECE_VALUES[Knight];

    // initial piece value and square value
    int eval = PIECE_VALUES[Knight] + KNIGHT_TABLE[(color == White ? sq : FLIPPED[sq])];


    // mobility bonus
    U64 mob = (knightAttacksBB[sq] ^ (knightAttacksBB[sq] & (ourPiecesBB | opponentPawnAttacksBB)));
    eval += KNIGHT_MOBILITY * (popcount(mob) - 4);

    // decreasing value as pawns disappear
    int numberOfPawns = popcount(board.pawnsBB);
    eval += KNIGHT_PAWN_CONST * (numberOfPawns - 8);

    // traps and blockages
    if(color == White) {
        if(sq == a8 && (opponentPawnsBB & (bits[a7] | bits[c7])))
           eval -= TRAPPED_KNIGHT_PENALTY;
        if(sq == a7 && (opponentPawnsBB & (bits[a6] | bits[c6])) && (opponentPawnsBB & (bits[b7] | bits[d7])))
            eval -= TRAPPED_KNIGHT_PENALTY;

        if(sq == h8 && (opponentPawnsBB & (bits[h7] | bits[f7])))
           eval -= TRAPPED_KNIGHT_PENALTY;
        if(sq == h7 && (opponentPawnsBB & (bits[f6] | bits[h6])) && (opponentPawnsBB & (bits[e7] | bits[g7])))
            eval -= TRAPPED_KNIGHT_PENALTY;

        if(sq == c3 && (ourPawnsBB & bits[c2]) && (ourPawnsBB & bits[d4]) && !(ourPawnsBB & bits[e4]))
            eval -= BLOCKING_C_KNIGHT;
    }
    if(color == Black) {
        if(sq == a1 && (opponentPawnsBB & (bits[a2] | bits[c2])))
           eval -= TRAPPED_KNIGHT_PENALTY;
        if(sq == a2 && (opponentPawnsBB & (bits[a3] | bits[c3])) && (opponentPawnsBB & (bits[b2] | bits[d2])))
            eval -= TRAPPED_KNIGHT_PENALTY;

        if(sq == h1 && (opponentPawnsBB & (bits[h2] | bits[f2])))
           eval -= TRAPPED_KNIGHT_PENALTY;
        if(sq == h2 && (opponentPawnsBB & (bits[f3] | bits[h3])) && (opponentPawnsBB & (bits[e2] | bits[g2])))
            eval -= TRAPPED_KNIGHT_PENALTY;

        if(sq == c6 && (ourPawnsBB & bits[c7]) && (ourPawnsBB & bits[d5]) && !(ourPawnsBB & bits[e5]))
            eval -= BLOCKING_C_KNIGHT;
    }

    // bonus if defended by pawns
    if(ourPawnAttacksBB & bits[sq])
        eval += KNIGHT_DEF_BY_PAWN;

    // attacks
    U64 sqNearKing = (color == White ? squaresNearBlackKing[board.blackKingSquare] : squaresNearWhiteKing[board.whiteKingSquare]);

    int attackedSquares = popcount(knightAttacksBB[sq] & sqNearKing);
    if(attackedSquares) {
        if(color == White) {
            whiteAttackersCnt++;
            whiteAttackWeight += PIECE_ATTACK_WEIGHT[Knight] * attackedSquares;
        } else {
            blackAttackersCnt++;
            blackAttackWeight += PIECE_ATTACK_WEIGHT[Knight] * attackedSquares;
        }
    }

    return eval;
}

int evalBishop(
    int sq, int color, 
    int BISHOP_TABLE[64], int& TRAPPED_BISHOP_PENALTY, 
    int& BLOCKED_BISHOP_PENALTY, int& FIANCHETTO_BONUS, int& BISHOP_MOBILITY,
    int PIECE_VALUES[7], int PIECE_ATTACK_WEIGHT[6]
) {
    U64 ourPawnsBB = (board.whitePiecesBB & board.pawnsBB);
    U64 opponentPawnsBB = (board.blackPiecesBB & board.pawnsBB);
    if(color == Black) swap(ourPawnsBB, opponentPawnsBB);

    U64 opponentPawnAttacksBB = pawnAttacks(opponentPawnsBB, (color ^ (White | Black)));

    U64 ourPiecesBB = (color == White ? board.whitePiecesBB : board.blackPiecesBB);
    U64 opponentPiecesBB = (color == Black ? board.whitePiecesBB : board.blackPiecesBB);

    int opponentKingSquare = (color == White ? board.blackKingSquare : board.whiteKingSquare);

    if(color == White) pieceMaterialWhite += PIECE_VALUES[Bishop];
    else pieceMaterialBlack += PIECE_VALUES[Bishop];

    // initial piece value and square value
    int eval = PIECE_VALUES[Bishop] + BISHOP_TABLE[(color == White ? sq : FLIPPED[sq])];

    // traps and blockages
    if(color == White) {
        if(sq == a7 && (opponentPawnsBB & bits[b6]) && (opponentPawnsBB & bits[c7]))
            eval -= TRAPPED_BISHOP_PENALTY;
        if(sq == h7 && (opponentPawnsBB & bits[g6]) && (opponentPawnsBB & bits[f7]))
            eval -= TRAPPED_BISHOP_PENALTY;

        if(sq == c1 && (ourPawnsBB & bits[d2]) & (ourPiecesBB & bits[e3]))
            eval -= BLOCKED_BISHOP_PENALTY;
        if(sq == f1 && (ourPawnsBB & bits[e2]) & (ourPiecesBB & bits[d3]))
            eval -= BLOCKED_BISHOP_PENALTY;
    }
    if(color == Black) {
        if(sq == a2 && (opponentPawnsBB & bits[b3]) && (opponentPawnsBB & bits[c2]))
            eval -= TRAPPED_BISHOP_PENALTY;
        if(sq == h2 && (opponentPawnsBB & bits[g3]) && (opponentPawnsBB & bits[f2]))
            eval -= TRAPPED_BISHOP_PENALTY;

        if(sq == c8 && (ourPawnsBB & bits[d7]) & (ourPiecesBB & bits[e6]))
            eval -= BLOCKED_BISHOP_PENALTY;
        if(sq == f8 && (ourPawnsBB & bits[e7]) & (ourPiecesBB & bits[d6]))
            eval -= BLOCKED_BISHOP_PENALTY;
    }

    // fianchetto bonus (bishop on long diagonal on the second rank)
    if(color == White && sq == g2 && (ourPawnsBB & bits[g3]) && (ourPawnsBB & bits[f2])) eval += FIANCHETTO_BONUS;
    if(color == White && sq == b2 && (ourPawnsBB & bits[b3]) && (ourPawnsBB & bits[c2])) eval += FIANCHETTO_BONUS;
    if(color == Black && sq == g7 && (ourPawnsBB & bits[g6]) && (ourPawnsBB & bits[f7])) eval += FIANCHETTO_BONUS;
    if(color == Black && sq == b7 && (ourPawnsBB & bits[b6]) && (ourPawnsBB & bits[c7])) eval += FIANCHETTO_BONUS;

    // mobility and attacks
    U64 sqNearKing = (color == White ? squaresNearBlackKing[board.blackKingSquare] : squaresNearWhiteKing[board.whiteKingSquare]);
    U64 attacks = magicBishopAttacks((board.whitePiecesBB | board.blackPiecesBB), sq);

    int mobility = popcount(attacks & ~ourPiecesBB & ~opponentPawnAttacksBB);
    int attackedSquares = popcount(attacks & sqNearKing);

    eval += BISHOP_MOBILITY * (mobility-5);
    if(attackedSquares) {
        if(color == White) {
            whiteAttackersCnt++;
            whiteAttackWeight += PIECE_ATTACK_WEIGHT[Bishop] * attackedSquares;
        } else {
            blackAttackersCnt++;
            blackAttackWeight += PIECE_ATTACK_WEIGHT[Bishop] * attackedSquares;
        }
    }

    return eval;
}

int evalRook(
    int sq, int color, 
    int ROOK_TABLE[64], int& BLOCKED_ROOK_PENALTY,
    int& ROOK_PAWN_CONST, int& ROOK_ON_OPEN_FILE, int& ROOK_ON_SEVENTH, int& ROOKS_DEF_EACH_OTHER,
    int& ROOK_ON_QUEEN_FILE, int& ROOK_MOBILITY,
    int PIECE_VALUES[7], int PIECE_ATTACK_WEIGHT[6]
) {
    U64 currFileBB = filesBB[sq%8];
    U64 currRankBB = ranksBB[sq/8];

    U64 ourPiecesBB = (color == White ? board.whitePiecesBB : board.blackPiecesBB);
    U64 opponentPiecesBB = (color == Black ? board.whitePiecesBB : board.blackPiecesBB);
    U64 ourPawnsBB = (board.whitePiecesBB & board.pawnsBB);
    U64 opponentPawnsBB = (board.blackPiecesBB & board.pawnsBB);
    if(color == Black) swap(ourPawnsBB, opponentPawnsBB);

    U64 opponentPawnAttacksBB = pawnAttacks(opponentPawnsBB, (color ^ (White | Black)));

    int opponentKingSquare = (color == White ? board.blackKingSquare : board.whiteKingSquare);

    // in this case seventh rank means the second rank in the opponent's half
    int seventhRank = (color == White ? 6 : 1);
    int eighthRank = (color == White ? 7 : 0);

    if(color == White) pieceMaterialWhite += PIECE_VALUES[Rook];
    else pieceMaterialBlack += PIECE_VALUES[Rook];

    // initial piece value and square value
    int eval = PIECE_VALUES[Rook] + ROOK_TABLE[(color == White ? sq : FLIPPED[sq])];

    // blocked by uncastled king
    if(color == White) {
        if((board.whiteKingSquare == f1 || board.whiteKingSquare == g1) && (sq == g1 || sq == h1))
            eval -= BLOCKED_ROOK_PENALTY;
        if((board.whiteKingSquare == c1 || board.whiteKingSquare == b1) && (sq == a1 || sq == b1))
            eval -= BLOCKED_ROOK_PENALTY;
    }
    if(color == Black) {
        if((board.whiteKingSquare == f8 || board.whiteKingSquare == g8) && (sq == g8 || sq == h8))
            eval -= BLOCKED_ROOK_PENALTY;
        if((board.whiteKingSquare == c8 || board.whiteKingSquare == b8) && (sq == a8 || sq == b8))
            eval -= BLOCKED_ROOK_PENALTY;
    }

    // the rook becomes more valuable as there are less pawns on the board
    int numberOfPawns = popcount(board.pawnsBB);
    eval += ROOK_PAWN_CONST * (8 - numberOfPawns);

    // bonus for a rook on an open or semi open file
    bool ourBlockingPawns = (currFileBB & ourPawnsBB);
    bool opponentBlockingPawns = (currFileBB & opponentPawnsBB);

    if(!ourBlockingPawns) {
        if(opponentBlockingPawns) eval += ROOK_ON_OPEN_FILE/2; // semi open file
        else eval += ROOK_ON_OPEN_FILE; // open file
    }

    // the rook on the seventh rank gets a huge bonus if there are pawns on that rank or if it restricts the king to the eighth rank
    if(sq/8 == seventhRank && (opponentKingSquare/8 == eighthRank || (opponentPawnsBB & ranksBB[seventhRank])))
        eval += ROOK_ON_SEVENTH;

    // small bonus if the rook is defended by another rook
    if((board.rooksBB & ourPiecesBB & (currRankBB | currFileBB)) ^ bits[sq])
        eval += ROOKS_DEF_EACH_OTHER;

    // bonus for a rook that is on the same file as the enemy queen
    if(currFileBB & opponentPiecesBB & board.queensBB) eval += ROOK_ON_QUEEN_FILE;

    // mobility and attacks
    U64 sqNearKing = (color == White ? squaresNearBlackKing[board.blackKingSquare] : squaresNearWhiteKing[board.whiteKingSquare]);
    U64 attacks = magicRookAttacks((board.whitePiecesBB | board.blackPiecesBB), sq);

    int mobility = popcount(attacks & ~ourPiecesBB & ~opponentPawnAttacksBB);
    int attackedSquares = popcount(attacks & sqNearKing);

    eval += ROOK_MOBILITY * (mobility-7);
    if(attackedSquares) {
        if(color == White) {
            whiteAttackersCnt++;
            whiteAttackWeight += PIECE_ATTACK_WEIGHT[Rook] * attackedSquares;
        } else {
            blackAttackersCnt++;
            blackAttackWeight += PIECE_ATTACK_WEIGHT[Rook] * attackedSquares;
        }
    }

    return eval;
}

int evalQueen(
    int sq, int color, 
    int QUEEN_TABLE[64], int& EARLY_QUEEN_DEVELOPMENT, int& QUEEN_MOBILITY,
    int PIECE_VALUES[7], int PIECE_ATTACK_WEIGHT[6]
) {
    U64 ourPiecesBB = (color == White ? board.whitePiecesBB : board.blackPiecesBB);
    U64 opponentPiecesBB = (color == Black ? board.whitePiecesBB : board.blackPiecesBB);
    U64 ourBishopsBB = (board.bishopsBB & ourPiecesBB);
    U64 ourKnightsBB = (board.knightsBB & ourPiecesBB);

    U64 opponentPawnsBB = (board.pawnsBB & opponentPiecesBB);
    U64 opponentPawnAttacksBB = pawnAttacks(opponentPawnsBB, (color ^ (White | Black)));

    int opponentKingSquare = (color == Black ? board.whiteKingSquare : board.blackKingSquare);

    if(color == White) pieceMaterialWhite += PIECE_VALUES[Queen];
    else pieceMaterialBlack += PIECE_VALUES[Queen];

    // initial piece value and square value
    int eval = PIECE_VALUES[Queen] + QUEEN_TABLE[(color == White ? sq : FLIPPED[sq])];

    // penalty for early development
    if(color == White && sq/8 > 1) {
        if(ourKnightsBB & bits[b1]) eval -= EARLY_QUEEN_DEVELOPMENT;
        if(ourBishopsBB & bits[c1]) eval -= EARLY_QUEEN_DEVELOPMENT;
        if(ourBishopsBB & bits[f1]) eval -= EARLY_QUEEN_DEVELOPMENT;
        if(ourKnightsBB & bits[g1]) eval -= EARLY_QUEEN_DEVELOPMENT;
    }
    if(color == Black && sq/8 < 6) {
        if(ourKnightsBB & bits[b8]) eval -= EARLY_QUEEN_DEVELOPMENT;
        if(ourBishopsBB & bits[c8]) eval -= EARLY_QUEEN_DEVELOPMENT;
        if(ourBishopsBB & bits[f8]) eval -= EARLY_QUEEN_DEVELOPMENT;
        if(ourKnightsBB & bits[g8]) eval -= EARLY_QUEEN_DEVELOPMENT;
    }

    // mobility and attacks
    U64 sqNearKing = (color == White ? squaresNearBlackKing[board.blackKingSquare] : squaresNearWhiteKing[board.whiteKingSquare]);
    U64 attacks = (magicBishopAttacks((board.whitePiecesBB | board.blackPiecesBB), sq)
                 | magicRookAttacks((board.whitePiecesBB | board.blackPiecesBB), sq));

    int mobility = popcount(attacks & ~ourPiecesBB & ~opponentPawnAttacksBB);
    int attackedSquares = popcount(attacks & sqNearKing);

    eval += QUEEN_MOBILITY * (mobility-14);
    if(attackedSquares) {
        if(color == White) {
            whiteAttackersCnt++;
            whiteAttackWeight += PIECE_ATTACK_WEIGHT[Queen] * attackedSquares;
        } else {
            blackAttackersCnt++;
            blackAttackWeight += PIECE_ATTACK_WEIGHT[Queen] * attackedSquares;
        }
    }
    return eval;
}

int whiteKingShield(int KING_SHIELD[3]) {
    U64 ourPawnsBB = (board.whitePiecesBB & board.pawnsBB);
    int sq = board.whiteKingSquare;

    int eval = 0;
    // queen side
    if(sq%8 < 3) {
        if(ourPawnsBB & bits[a2]) eval += KING_SHIELD[1];
        else if(ourPawnsBB & bits[a3]) eval += KING_SHIELD[2];
        else eval -= KING_SHIELD[0];

        if(ourPawnsBB & bits[b2]) eval += KING_SHIELD[1];
        else if(ourPawnsBB & bits[b3]) eval += KING_SHIELD[2];
        else eval -= KING_SHIELD[0];

        if(ourPawnsBB & bits[c2]) eval += KING_SHIELD[1];
        else if(ourPawnsBB & bits[c3]) eval += KING_SHIELD[2];
        else eval -= KING_SHIELD[0];
    }

    // king side
    else if(sq%8 > 4) {
        if(ourPawnsBB & bits[f2]) eval += KING_SHIELD[1];
        else if(ourPawnsBB & bits[f3]) eval += KING_SHIELD[2];
        else eval -= KING_SHIELD[0];

        if(ourPawnsBB & bits[g2]) eval += KING_SHIELD[1];
        else if(ourPawnsBB & bits[g3]) eval += KING_SHIELD[2];
        else eval -= KING_SHIELD[0];

        if(ourPawnsBB & bits[h2]) eval += KING_SHIELD[1];
        else if(ourPawnsBB & bits[h3]) eval += KING_SHIELD[2];
        else eval -= KING_SHIELD[0];
    }
    return eval;
}

int blackKingShield(int KING_SHIELD[3]) {
    U64 ourPawnsBB = (board.blackPiecesBB & board.pawnsBB);
    int sq = board.blackKingSquare;

    int eval = 0;
    // queen side
    if(sq%8 < 3) {
        if(ourPawnsBB & bits[a7]) eval += KING_SHIELD[1];
        else if(ourPawnsBB & bits[a6]) eval += KING_SHIELD[2];
        else eval -= KING_SHIELD[0];

        if(ourPawnsBB & bits[b7]) eval += KING_SHIELD[1];
        else if(ourPawnsBB & bits[b6]) eval += KING_SHIELD[2];
        else eval -= KING_SHIELD[0];

        if(ourPawnsBB & bits[c7]) eval += KING_SHIELD[1];
        else if(ourPawnsBB & bits[c6]) eval += KING_SHIELD[2];
        else eval -= KING_SHIELD[0];
    }

    // king side
    else if(sq%8 > 4) {
        if(ourPawnsBB & bits[f7]) eval += KING_SHIELD[1];
        else if(ourPawnsBB & bits[f6]) eval += KING_SHIELD[2];
        else eval -= KING_SHIELD[0];

        if(ourPawnsBB & bits[g7]) eval += KING_SHIELD[1];
        else if(ourPawnsBB & bits[g6]) eval += KING_SHIELD[2];
        else eval -= KING_SHIELD[0];

        if(ourPawnsBB & bits[h7]) eval += KING_SHIELD[1];
        else if(ourPawnsBB & bits[h6]) eval += KING_SHIELD[2];
        else eval -= KING_SHIELD[0];
    }
    return eval;
}

// evaluate every pawn independently but store the full pawn structure evaluation in the hash map
int evalPawnStructure(
    bool useHash,
    int MG_PAWN_TABLE[64], int EG_PAWN_TABLE[64], int PASSED_PAWN_TABLE[64],
    int& DOUBLED_PAWNS_PENALTY, int& WEAK_PAWN_PENALTY, int& C_PAWN_PENALTY,
    int PIECE_VALUES[7]
) {
    U64 whitePawns = (board.pawnsBB & board.whitePiecesBB);
    U64 blackPawns = (board.pawnsBB & board.blackPiecesBB);

    int eval;
    if(useHash) {
        eval = retrievePawnEval();
        if(eval != VAL_UNKNOWN) return eval;
    }

    eval = 0;
    while(whitePawns) {
        int sq = bitscanForward(whitePawns);
        eval += evalPawn(
            sq, White, MG_PAWN_TABLE, EG_PAWN_TABLE, PASSED_PAWN_TABLE,
            DOUBLED_PAWNS_PENALTY, WEAK_PAWN_PENALTY, C_PAWN_PENALTY, PIECE_VALUES);
        whitePawns &= (whitePawns-1);
    }
    while(blackPawns) {
        int sq = bitscanForward(blackPawns);
        eval -= evalPawn(
            sq, Black, MG_PAWN_TABLE, EG_PAWN_TABLE, PASSED_PAWN_TABLE,
            DOUBLED_PAWNS_PENALTY, WEAK_PAWN_PENALTY, C_PAWN_PENALTY, PIECE_VALUES);
        blackPawns &= (blackPawns-1);
    }

    if(useHash) recordPawnEval(eval);

    return eval;
}

int evalPawn(
    int sq, int color, 
    int MG_PAWN_TABLE[64], int EG_PAWN_TABLE[64], int PASSED_PAWN_TABLE[64],
    int& DOUBLED_PAWNS_PENALTY, int& WEAK_PAWN_PENALTY, int& C_PAWN_PENALTY,
    int PIECE_VALUES[7]
) {
    U64 ourPiecesBB = (color == White ? board.whitePiecesBB : board.blackPiecesBB);

    U64 opponentPawnsBB = (board.pawnsBB & (color == White ? board.blackPiecesBB : board.whitePiecesBB));
    U64 ourPawnsBB = (board.pawnsBB & (color == White ? board.whitePiecesBB : board.blackPiecesBB));

    U64 opponentPawnAttacksBB = pawnAttacks(opponentPawnsBB, (color == White ? Black : White));
    U64 ourPawnAttacksBB = pawnAttacks(ourPawnsBB, color);

    bool weak = true, passed = true, opposed = false;

    if(color == White) pawnCntWhite ++;
    else pawnCntBlack ++;

    // initial pawn value + square value
    int mgWeight = min(gamePhase(), 24);
    int egWeight = 24-mgWeight;
    int pst_eval = (
        mgWeight * MG_PAWN_TABLE[(color == White ? sq : FLIPPED[sq])] 
        + egWeight * EG_PAWN_TABLE[(color == White ? sq : FLIPPED[sq])]
    ) / 24;

    int eval = PIECE_VALUES[Pawn] + pst_eval;
    int dir = (color == White ? 8 : -8);

    // check squares in front of the pawn to see if it is passed or opposed/doubled
    int curSq = sq+dir;
    while(curSq < 64 && curSq >= 0) {
        if(board.pawnsBB & bits[curSq]) {
            passed = false;
            if(ourPiecesBB & bits[curSq]) eval -= DOUBLED_PAWNS_PENALTY;
            else opposed = true;
        }
        if(opponentPawnAttacksBB & bits[curSq]) passed = false;

        curSq += dir;
    }

    // check squares behind the pawn to see if it can be protected by other pawns
    curSq = sq;
    while(curSq < 64 && curSq >= 0) {
        if(ourPawnAttacksBB & curSq) {
            weak = false;
            break;
        }
        curSq -= dir;
    }

    // bonus for passed pawns, bigger bonus for protected passers
    // the bonus is also bigger if the pawn is more advanced
    if(passed) {
        int bonus = PASSED_PAWN_TABLE[(color == White ? sq : FLIPPED[sq])];
        if(ourPawnAttacksBB & bits[sq]) bonus = (bonus*4)/3;

        eval += bonus;
    }

    // penalty for weak (backward or isolated) pawns and bigger penalty if they are on a semi open file
    if(weak) {
        int penalty = WEAK_PAWN_PENALTY;
        if(!opposed) penalty = (penalty*4)/3;

        eval -= penalty;
    }

    // penalty for having a pawn on d4 and not having a pawn on c4 or c3 in a d4 opening
    if(color == White && sq == c2 && (ourPawnsBB & bits[d4]) && !(ourPawnsBB & bits[e4]))
        eval -= C_PAWN_PENALTY;
    if(color == Black && sq == c7 && (ourPawnsBB & bits[d5]) && !(ourPawnsBB & bits[e5]))
        eval -= C_PAWN_PENALTY;

    return eval;
}

int gamePhase() {
    return popcount(board.knightsBB) * MG_WEIGHT[Knight] 
         + popcount(board.bishopsBB) * MG_WEIGHT[Bishop] 
         + popcount(board.rooksBB) * MG_WEIGHT[Rook]
         + popcount(board.queensBB) * MG_WEIGHT[Queen]; 
}
