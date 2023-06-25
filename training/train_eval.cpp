#include <unordered_map>
#include <vector>

#include "train_eval.h"
#include "../engine/Board.h"
#include "../engine/MagicBitboards.h"
#include "../engine/TranspositionTable.h"

using namespace std;

extern const int NUM_PARAMS = 9 * 64 + 7 + 6 + 3 + 24;

vector<double> gradients(NUM_PARAMS, 0);
vector<int> freq(NUM_PARAMS, 0);

extern const int MG_KING_TABLE_IDX = 0,
    EG_KING_TABLE_IDX = 64,
    QUEEN_TABLE_IDX = 128,
    ROOK_TABLE_IDX = 192,
    BISHOP_TABLE_IDX = 256,
    KNIGHT_TABLE_IDX = 320,
    MG_PAWN_TABLE_IDX = 384,
    EG_PAWN_TABLE_IDX = 448,
    PASSED_PAWN_TABLE_IDX = 512,
    KING_SHIELD_IDX = 576,
    PIECE_VALUES_IDX = 579,
    PIECE_ATTACK_WEIGHT_IDX = 586,
    KNIGHT_MOBILITY_IDX = 592,
    KNIGHT_PAWN_CONST_IDX = 593,
    TRAPPED_KNIGHT_PENALTY_IDX = 594,
    KNIGHT_DEF_BY_PAWN_IDX = 595,
    BLOCKING_C_KNIGHT_IDX = 596,
    KNIGHT_PAIR_PENALTY_IDX = 597,
    BISHOP_PAIR_IDX = 598,
    TRAPPED_BISHOP_PENALTY_IDX = 599,
    FIANCHETTO_BONUS_IDX = 600,
    BISHOP_MOBILITY_IDX = 601,
    BLOCKED_BISHOP_PENALTY_IDX = 602,
    ROOK_ON_QUEEN_FILE_IDX = 603,
    ROOK_ON_OPEN_FILE_IDX = 604,
    ROOK_PAWN_CONST_IDX = 605,
    ROOK_ON_SEVENTH_IDX = 606,
    ROOKS_DEF_EACH_OTHER_IDX = 607,
    ROOK_MOBILITY_IDX = 608,
    BLOCKED_ROOK_PENALTY_IDX = 609,
    EARLY_QUEEN_DEVELOPMENT_IDX = 610,
    QUEEN_MOBILITY_IDX = 611,
    DOUBLED_PAWNS_PENALTY_IDX = 612,
    WEAK_PAWN_PENALTY_IDX = 613,
    C_PAWN_PENALTY_IDX = 614,
    TEMPO_BONUS_IDX = 615;

// flipped squares for piece square tables for black
const int TRAIN_FLIPPED[64] = {
    56, 57, 58, 59, 60, 61, 62, 63,
    48, 49, 50, 51, 52, 53, 54, 55,
    40, 41, 42, 43, 44, 45, 46, 47,
    32, 33, 34, 35, 36, 37, 38, 39,
    24, 25, 26, 27, 28, 29, 30, 31,
    16, 17, 18 ,19, 20, 21, 22, 23,
    8, 9, 10, 11 , 12, 13, 14, 15,
    0, 1, 2, 3, 4, 5, 6, 7
};

const int TRAIN_MG_WEIGHT[7] = {0, 0, 1, 1, 2, 4, 0}; 

int TRAIN_KING_SAFETY_TABLE[100] = {
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

int trainWhiteAttackersCnt, trainBlackAttackersCnt, trainWhiteAttackWeight, trainBlackAttackWeight;
int trainPawnCntWhite, trainPawnCntBlack, trainPieceMaterialWhite, trainPieceMaterialBlack;

int trainGamePhase();

int trainEvalPawn(
    int sq, int color, 
    int MG_PAWN_TABLE[64], int EG_PAWN_TABLE[64], int PASSED_PAWN_TABLE[64],
    int& DOUBLED_PAWNS_PENALTY, int& WEAK_PAWN_PENALTY, int& C_PAWN_PENALTY,
    int PIECE_VALUES[7]
);
int trainEvalKnight( 
    int sq, int color, 
    int KNIGHT_TABLE[64], int& KNIGHT_MOBILITY, 
    int& KNIGHT_PAWN_CONST, int& TRAPPED_KNIGHT_PENALTY, int& BLOCKING_C_KNIGHT, int& KNIGHT_DEF_BY_PAWN,
    int PIECE_VALUES[7], int PIECE_ATTACK_WEIGHT[6]
);
int trainEvalBishop(
    int sq, int color, 
    int BISHOP_TABLE[64], int& TRAPPED_BISHOP_PENALTY, 
    int& BLOCKED_BISHOP_PENALTY, int& FIANCHETTO_BONUS, int& BISHOP_MOBILITY,
    int PIECE_VALUES[7], int PIECE_ATTACK_WEIGHT[6]
);
int trainEvalRook(
    int sq, int color, 
    int ROOK_TABLE[64], int& BLOCKED_ROOK_PENALTY,
    int& ROOK_PAWN_CONST, int& ROOK_ON_OPEN_FILE, int& ROOK_ON_SEVENTH, int& ROOKS_DEF_EACH_OTHER,
    int& ROOK_ON_QUEEN_FILE, int& ROOK_MOBILITY,
    int PIECE_VALUES[7], int PIECE_ATTACK_WEIGHT[6]
);
int trainEvalQueen(
    int sq, int color, 
    int QUEEN_TABLE[64], int& EARLY_QUEEN_DEVELOPMENT,
    int& QUEEN_MOBILITY, int PIECE_VALUES[7], int PIECE_ATTACK_WEIGHT[6]
);
int trainEvalPawnStructure(
    bool useHash,
    int MG_PAWN_TABLE[64], int EG_PAWN_TABLE[64], int PASSED_PAWN_TABLE[64],
    int& DOUBLED_PAWNS_PENALTY, int& WEAK_PAWN_PENALTY, int& C_PAWN_PENALTY,
    int PIECE_VALUES[7]
);

int trainWhiteKingShield(int KING_SHIELD[3]), trainBlackKingShield(int KING_SHIELD[3]);

int trainEvaluate(
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
    // set gradients vector to 0
    for(int i = 0; i < NUM_PARAMS; i++) gradients[i] = 0;

    // reset everything
    trainWhiteAttackersCnt = trainBlackAttackersCnt = 0;
    trainWhiteAttackWeight = trainBlackAttackWeight = 0;
    trainPawnCntWhite = trainPawnCntBlack = 0;
    trainPieceMaterialWhite = trainPieceMaterialBlack = 0;

    // evaluate pieces independently
    int res = 0;
    for(int sq = 0; sq < 64; sq++) {
        if(board.squares[sq] == Empty) continue;

        int color = (board.squares[sq] & (Black | White));
        int c = (color == White ? 1 : -1);

        if(board.knightsBB & bits[sq]) res += trainEvalKnight(
            sq, color, KNIGHT_TABLE,
            KNIGHT_MOBILITY, KNIGHT_PAWN_CONST, TRAPPED_KNIGHT_PENALTY, 
            BLOCKING_C_KNIGHT, KNIGHT_DEF_BY_PAWN, PIECE_VALUES, PIECE_ATTACK_WEIGHT) * c;

        if(board.bishopsBB & bits[sq]) res += trainEvalBishop(
            sq, color, BISHOP_TABLE, 
            TRAPPED_BISHOP_PENALTY, BLOCKED_BISHOP_PENALTY, 
            FIANCHETTO_BONUS, BISHOP_MOBILITY, PIECE_VALUES, PIECE_ATTACK_WEIGHT) * c;

        if(board.rooksBB & bits[sq]) res += trainEvalRook(
            sq, color, ROOK_TABLE, 
            BLOCKED_ROOK_PENALTY,ROOK_PAWN_CONST, ROOK_ON_OPEN_FILE, 
            ROOK_ON_SEVENTH, ROOKS_DEF_EACH_OTHER, ROOK_ON_QUEEN_FILE, ROOK_MOBILITY, 
            PIECE_VALUES, PIECE_ATTACK_WEIGHT) * c;

        if(board.queensBB & bits[sq]) res += trainEvalQueen(
            sq, color, QUEEN_TABLE, EARLY_QUEEN_DEVELOPMENT,
            QUEEN_MOBILITY, PIECE_VALUES, PIECE_ATTACK_WEIGHT) * c;
    }
    res += trainEvalPawnStructure(
        usePawnHash,
        MG_PAWN_TABLE, EG_PAWN_TABLE, PASSED_PAWN_TABLE,
        DOUBLED_PAWNS_PENALTY, WEAK_PAWN_PENALTY, C_PAWN_PENALTY,
        PIECE_VALUES
    );

    // evaluate kings based on the current game phase (king centralization becomes more important than safety as pieces disappear from the board)
    int mgWeight = min(trainGamePhase(), 24);
    int egWeight = 24-mgWeight;

    int mgKingScore = trainWhiteKingShield(KING_SHIELD) - trainBlackKingShield(KING_SHIELD);
    int egKingScore = 0;

    // evaluate king safety in the middlegame

    // if only 1 or 2 attackers, we consider the king safe
    if(trainWhiteAttackersCnt <= 2) trainWhiteAttackWeight = 0;
    if(trainBlackAttackersCnt <= 2) trainBlackAttackWeight = 0;

    mgKingScore += TRAIN_KING_SAFETY_TABLE[trainWhiteAttackWeight] - TRAIN_KING_SAFETY_TABLE[trainBlackAttackWeight];
    mgKingScore += MG_KING_TABLE[board.whiteKingSquare] - MG_KING_TABLE[TRAIN_FLIPPED[board.blackKingSquare]];    

    egKingScore += EG_KING_TABLE[board.whiteKingSquare] - EG_KING_TABLE[TRAIN_FLIPPED[board.blackKingSquare]];

    res += (mgWeight * mgKingScore + egWeight * egKingScore) / 24;

    // --- UPDATE GRADIENTS ---
    gradients[MG_KING_TABLE_IDX + board.whiteKingSquare] += (double)(mgWeight) / 24.0;
    gradients[EG_KING_TABLE_IDX + board.whiteKingSquare] += (double)(egWeight) / 24.0;
    gradients[MG_KING_TABLE_IDX + TRAIN_FLIPPED[board.blackKingSquare]] -= (double)(mgWeight) / 24.0;
    gradients[EG_KING_TABLE_IDX + TRAIN_FLIPPED[board.blackKingSquare]] -= (double)(egWeight) / 24.0;
    freq[MG_KING_TABLE_IDX + board.whiteKingSquare] += (mgWeight > 0 ? 1 : 0);
    freq[EG_KING_TABLE_IDX + board.whiteKingSquare] += (egWeight > 0 ? 1 : 0);
    freq[MG_KING_TABLE_IDX + TRAIN_FLIPPED[board.blackKingSquare]] += (mgWeight > 0 ? 1 : 0);
    freq[EG_KING_TABLE_IDX + TRAIN_FLIPPED[board.blackKingSquare]] += (egWeight > 0 ? 1 : 0);

    // tempo bonus
    if(board.turn == White) res += TEMPO_BONUS;
    else res -= TEMPO_BONUS;

    // --- UPDATE GRADIENTS ---
    gradients[TEMPO_BONUS_IDX] += (board.turn == White ? 1 : -1);
    freq[TEMPO_BONUS_IDX] += 1;

    // add scores for bishop and knight pairs
    if(popcount(board.whitePiecesBB & board.bishopsBB) >= 2) res += BISHOP_PAIR;
    if(popcount(board.blackPiecesBB & board.bishopsBB) >= 2) res -= BISHOP_PAIR;

    if(popcount(board.whitePiecesBB & board.knightsBB) >= 2) res -= KNIGHT_PAIR_PENALTY;
    if(popcount(board.blackPiecesBB & board.knightsBB) >= 2) res += KNIGHT_PAIR_PENALTY;

    // --- UPDATE GRADIENTS ---
    gradients[BISHOP_PAIR_IDX] += (popcount(board.whitePiecesBB & board.bishopsBB) >= 2 ? 1 : 0);
    gradients[BISHOP_PAIR_IDX] -= (popcount(board.blackPiecesBB & board.bishopsBB) >= 2 ? 1 : 0);
    gradients[KNIGHT_PAIR_PENALTY_IDX] -= (popcount(board.whitePiecesBB & board.knightsBB) >= 2 ? 1 : 0);
    gradients[KNIGHT_PAIR_PENALTY_IDX] += (popcount(board.blackPiecesBB & board.knightsBB) >= 2 ? 1 : 0);
    freq[BISHOP_PAIR_IDX] += (popcount(board.whitePiecesBB & board.bishopsBB) >= 2 ? 1 : 0);
    freq[BISHOP_PAIR_IDX] += (popcount(board.blackPiecesBB & board.bishopsBB) >= 2 ? 1 : 0);
    freq[KNIGHT_PAIR_PENALTY_IDX] += (popcount(board.whitePiecesBB & board.knightsBB) >= 2 ? 1 : 0);
    freq[KNIGHT_PAIR_PENALTY_IDX] += (popcount(board.blackPiecesBB & board.knightsBB) >= 2 ? 1 : 0);

    // low material corrections (adjusting the score for well known draws)

    int strongerSide = White, weakerSide = Black;
    int strongerPawns = trainPawnCntWhite, weakerPawns = trainPawnCntBlack;
    int strongerPieces = trainPieceMaterialWhite, weakerPieces = trainPieceMaterialBlack;
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

    return res;
}

int trainEvalKnight( 
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

    if(color == White) trainPieceMaterialWhite += PIECE_VALUES[Knight];
    else trainPieceMaterialBlack += PIECE_VALUES[Knight];

    // initial piece value and square value
    int eval = PIECE_VALUES[Knight] + KNIGHT_TABLE[(color == White ? sq : TRAIN_FLIPPED[sq])];


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
            trainWhiteAttackersCnt++;
            trainWhiteAttackWeight += PIECE_ATTACK_WEIGHT[Knight] * attackedSquares;
        } else {
            trainBlackAttackersCnt++;
            trainBlackAttackWeight += PIECE_ATTACK_WEIGHT[Knight] * attackedSquares;
        }
    }

    // --- UPDATE GRADIENTS ---
    gradients[KNIGHT_TABLE_IDX + (color == White ? sq : TRAIN_FLIPPED[sq])] += (color == White ? 1 : -1);
    gradients[PIECE_VALUES_IDX + Knight] += (color == White ? 1 : -1);
    gradients[KNIGHT_MOBILITY_IDX] += (popcount(mob) - 4) * (color == White ? 1 : -1);
    gradients[KNIGHT_PAWN_CONST_IDX] += (numberOfPawns - 8) * (color == White ? 1 : -1);
    gradients[PIECE_ATTACK_WEIGHT_IDX + Knight] += attackedSquares * (color == White ? 1 : -1);
    if(color == White) {
        if (sq == a8 && (opponentPawnsBB & (bits[a7] | bits[c7])))
            gradients[TRAPPED_KNIGHT_PENALTY_IDX] -= 1;
        if (sq == a7 && (opponentPawnsBB & (bits[a6] | bits[c6])) && (opponentPawnsBB & (bits[b7] | bits[d7])))
            gradients[TRAPPED_KNIGHT_PENALTY_IDX] -= 1;
        if (sq == h8 && (opponentPawnsBB & (bits[h7] | bits[f7])))
            gradients[TRAPPED_KNIGHT_PENALTY_IDX] -= 1;
        if (sq == h7 && (opponentPawnsBB & (bits[f6] | bits[h6])) && (opponentPawnsBB & (bits[e7] | bits[g7])))
            gradients[TRAPPED_KNIGHT_PENALTY_IDX] -= 1;
        if (sq == c3 && (ourPawnsBB & bits[c2]) && (ourPawnsBB & bits[d4]) && !(ourPawnsBB & bits[e4]))
            gradients[BLOCKING_C_KNIGHT_IDX] -= 1;
    }
    if(color == Black) {
        if (sq == a1 && (opponentPawnsBB & (bits[a2] | bits[c2])))
            gradients[TRAPPED_KNIGHT_PENALTY_IDX] += 1;
        if (sq == a2 && (opponentPawnsBB & (bits[a3] | bits[c3])) && (opponentPawnsBB & (bits[b2] | bits[d2])))
            gradients[TRAPPED_KNIGHT_PENALTY_IDX] += 1;
        if (sq == h1 && (opponentPawnsBB & (bits[h2] | bits[f2])))
            gradients[TRAPPED_KNIGHT_PENALTY_IDX] += 1;
        if (sq == h2 && (opponentPawnsBB & (bits[f3] | bits[h3])) && (opponentPawnsBB & (bits[e2] | bits[g2])))
            gradients[TRAPPED_KNIGHT_PENALTY_IDX] += 1;
        if (sq == c6 && (ourPawnsBB & bits[c7]) && (ourPawnsBB & bits[d5]) && !(ourPawnsBB & bits[e5]))
            gradients[BLOCKING_C_KNIGHT_IDX] += 1;
    }
    gradients[KNIGHT_DEF_BY_PAWN_IDX] += (ourPawnAttacksBB & bits[sq] ? 1 : 0) * (color == White ? 1 : -1);

    freq[KNIGHT_TABLE_IDX + (color == White ? sq : TRAIN_FLIPPED[sq])] += 1;
    freq[PIECE_VALUES_IDX + Knight] += 1;
    freq[KNIGHT_MOBILITY_IDX] += 1;
    freq[KNIGHT_PAWN_CONST_IDX] += 1;
    freq[PIECE_ATTACK_WEIGHT_IDX + Knight] += (attackedSquares ? 1 : 0);
    if(color == White) {
        if (sq == a8 && (opponentPawnsBB & (bits[a7] | bits[c7])))
            freq[TRAPPED_KNIGHT_PENALTY_IDX] += 1;
        if (sq == a7 && (opponentPawnsBB & (bits[a6] | bits[c6])) && (opponentPawnsBB & (bits[b7] | bits[d7])))
            freq[TRAPPED_KNIGHT_PENALTY_IDX] += 1;
        if (sq == h8 && (opponentPawnsBB & (bits[h7] | bits[f7])))
            freq[TRAPPED_KNIGHT_PENALTY_IDX] += 1;
        if (sq == h7 && (opponentPawnsBB & (bits[f6] | bits[h6])) && (opponentPawnsBB & (bits[e7] | bits[g7])))
            freq[TRAPPED_KNIGHT_PENALTY_IDX] += 1;
        if (sq == c3 && (ourPawnsBB & bits[c2]) && (ourPawnsBB & bits[d4]) && !(ourPawnsBB & bits[e4]))
            freq[BLOCKING_C_KNIGHT_IDX] += 1;
    }
    if(color == Black) {
        if (sq == a1 && (opponentPawnsBB & (bits[a2] | bits[c2])))
            freq[TRAPPED_KNIGHT_PENALTY_IDX] += 1;
        if (sq == a2 && (opponentPawnsBB & (bits[a3] | bits[c3])) && (opponentPawnsBB & (bits[b2] | bits[d2])))
            freq[TRAPPED_KNIGHT_PENALTY_IDX] += 1;
        if (sq == h1 && (opponentPawnsBB & (bits[h2] | bits[f2])))
            freq[TRAPPED_KNIGHT_PENALTY_IDX] += 1;
        if (sq == h2 && (opponentPawnsBB & (bits[f3] | bits[h3])) && (opponentPawnsBB & (bits[e2] | bits[g2])))
            freq[TRAPPED_KNIGHT_PENALTY_IDX] += 1;
        if (sq == c6 && (ourPawnsBB & bits[c7]) && (ourPawnsBB & bits[d5]) && !(ourPawnsBB & bits[e5]))
            freq[BLOCKING_C_KNIGHT_IDX] += 1;
    }

    return eval;
}

int trainEvalBishop(
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

    if(color == White) trainPieceMaterialWhite += PIECE_VALUES[Bishop];
    else trainPieceMaterialBlack += PIECE_VALUES[Bishop];

    // initial piece value and square value
    int eval = PIECE_VALUES[Bishop] + BISHOP_TABLE[(color == White ? sq : TRAIN_FLIPPED[sq])];

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
            trainWhiteAttackersCnt++;
            trainWhiteAttackWeight += PIECE_ATTACK_WEIGHT[Bishop] * attackedSquares;
        } else {
            trainBlackAttackersCnt++;
            trainBlackAttackWeight += PIECE_ATTACK_WEIGHT[Bishop] * attackedSquares;
        }
    }

    // --- UPDATE GRADIENTS ---
    gradients[BISHOP_TABLE_IDX + (color == White ? sq : TRAIN_FLIPPED[sq])] += (color == White ? 1 : -1);
    gradients[PIECE_VALUES_IDX + Bishop] += (color == White ? 1 : -1);
    gradients[BISHOP_MOBILITY_IDX] += (mobility-5) * (color == White ? 1 : -1);
    gradients[PIECE_ATTACK_WEIGHT_IDX + Bishop] += attackedSquares * (color == White ? 1 : -1);
    if(color == White) {
        if (sq == a7 && (opponentPawnsBB & bits[b6]) && (opponentPawnsBB & bits[c7]))
            gradients[TRAPPED_BISHOP_PENALTY_IDX] -= 1;
        if (sq == h7 && (opponentPawnsBB & bits[g6]) && (opponentPawnsBB & bits[f7]))
            gradients[TRAPPED_BISHOP_PENALTY_IDX] -= 1;
        if (sq == c1 && (ourPawnsBB & bits[d2]) & (ourPiecesBB & bits[e3]))
            gradients[BLOCKED_BISHOP_PENALTY_IDX] -= 1;
        if (sq == f1 && (ourPawnsBB & bits[e2]) & (ourPiecesBB & bits[d3]))
            gradients[BLOCKED_BISHOP_PENALTY_IDX] -= 1;
    }
    if(color == Black) {
        if (sq == a2 && (opponentPawnsBB & bits[b3]) && (opponentPawnsBB & bits[c2]))
            gradients[TRAPPED_BISHOP_PENALTY_IDX] += 1;
        if (sq == h2 && (opponentPawnsBB & bits[g3]) && (opponentPawnsBB & bits[f2]))
            gradients[TRAPPED_BISHOP_PENALTY_IDX] += 1;
        if (sq == c8 && (ourPawnsBB & bits[d7]) & (ourPiecesBB & bits[e6]))
            gradients[BLOCKED_BISHOP_PENALTY_IDX] += 1;
        if (sq == f8 && (ourPawnsBB & bits[e7]) & (ourPiecesBB & bits[d6]))
            gradients[BLOCKED_BISHOP_PENALTY_IDX] += 1;
    }
    if(color == White && sq == g2 && (ourPawnsBB & bits[g3]) && (ourPawnsBB & bits[f2]))
        gradients[FIANCHETTO_BONUS_IDX] += 1;
    if(color == White && sq == b2 && (ourPawnsBB & bits[b3]) && (ourPawnsBB & bits[c2]))
        gradients[FIANCHETTO_BONUS_IDX] += 1;
    if(color == Black && sq == g7 && (ourPawnsBB & bits[g6]) && (ourPawnsBB & bits[f7]))
       gradients[FIANCHETTO_BONUS_IDX] -= 1;
    if(color == Black && sq == b7 && (ourPawnsBB & bits[b6]) && (ourPawnsBB & bits[c7]))
        gradients[FIANCHETTO_BONUS_IDX] -= 1;

    freq[BISHOP_TABLE_IDX + (color == White ? sq : TRAIN_FLIPPED[sq])] += 1;
    freq[PIECE_VALUES_IDX + Bishop] += 1;
    freq[BISHOP_MOBILITY_IDX] += 1;
    freq[PIECE_ATTACK_WEIGHT_IDX + Bishop] += (attackedSquares ? 1 : 0);
    if(color == White) {
        if (sq == a7 && (opponentPawnsBB & bits[b6]) && (opponentPawnsBB & bits[c7]))
            freq[TRAPPED_BISHOP_PENALTY_IDX] += 1;
        if (sq == h7 && (opponentPawnsBB & bits[g6]) && (opponentPawnsBB & bits[f7]))
            freq[TRAPPED_BISHOP_PENALTY_IDX] += 1;
        if (sq == c1 && (ourPawnsBB & bits[d2]) & (ourPiecesBB & bits[e3]))
            freq[BLOCKED_BISHOP_PENALTY_IDX] += 1;
        if (sq == f1 && (ourPawnsBB & bits[e2]) & (ourPiecesBB & bits[d3]))
            freq[BLOCKED_BISHOP_PENALTY_IDX] += 1;
    }
    if(color == Black) {
        if (sq == a2 && (opponentPawnsBB & bits[b3]) && (opponentPawnsBB & bits[c2]))
            freq[TRAPPED_BISHOP_PENALTY_IDX] += 1;
        if (sq == h2 && (opponentPawnsBB & bits[g3]) && (opponentPawnsBB & bits[f2]))
            freq[TRAPPED_BISHOP_PENALTY_IDX] += 1;
        if (sq == c8 && (ourPawnsBB & bits[d7]) & (ourPiecesBB & bits[e6]))
            freq[BLOCKED_BISHOP_PENALTY_IDX] += 1;
        if (sq == f8 && (ourPawnsBB & bits[e7]) & (ourPiecesBB & bits[d6]))
            freq[BLOCKED_BISHOP_PENALTY_IDX] += 1;
    }
    if(color == White && sq == g2 && (ourPawnsBB & bits[g3]) && (ourPawnsBB & bits[f2]))
        freq[FIANCHETTO_BONUS_IDX] += 1;
    if(color == White && sq == b2 && (ourPawnsBB & bits[b3]) && (ourPawnsBB & bits[c2]))
        freq[FIANCHETTO_BONUS_IDX] += 1;
    if(color == Black && sq == g7 && (ourPawnsBB & bits[g6]) && (ourPawnsBB & bits[f7]))
         freq[FIANCHETTO_BONUS_IDX] += 1;
    if(color == Black && sq == b7 && (ourPawnsBB & bits[b6]) && (ourPawnsBB & bits[c7]))
        freq[FIANCHETTO_BONUS_IDX] += 1;

    return eval;
}

int trainEvalRook(
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

    if(color == White) trainPieceMaterialWhite += PIECE_VALUES[Rook];
    else trainPieceMaterialBlack += PIECE_VALUES[Rook];

    // initial piece value and square value
    int eval = PIECE_VALUES[Rook] + ROOK_TABLE[(color == White ? sq : TRAIN_FLIPPED[sq])];

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
            trainWhiteAttackersCnt++;
            trainWhiteAttackWeight += PIECE_ATTACK_WEIGHT[Rook] * attackedSquares;
        } else {
            trainBlackAttackersCnt++;
            trainBlackAttackWeight += PIECE_ATTACK_WEIGHT[Rook] * attackedSquares;
        }
    }

    // --- UPDATE GRADIENTS ---
    gradients[ROOK_TABLE_IDX + (color == White ? sq : TRAIN_FLIPPED[sq])] += (color == White ? 1 : -1);
    gradients[PIECE_VALUES_IDX + Rook] += (color == White ? 1 : -1);
    gradients[ROOK_PAWN_CONST_IDX] += (8 - numberOfPawns) * (color == White ? 1 : -1);
    gradients[ROOK_ON_OPEN_FILE_IDX] += ((!ourBlockingPawns ? 1 : 0) * (color == White ? 1 : -1)) / (opponentBlockingPawns ? 2 : 1);
    gradients[ROOK_ON_SEVENTH_IDX] += (sq/8 == seventhRank && (opponentKingSquare/8 == eighthRank || (opponentPawnsBB & ranksBB[seventhRank])) ? 1 : 0) * (color == White ? 1 : -1);
    gradients[ROOKS_DEF_EACH_OTHER_IDX] += (((board.rooksBB & ourPiecesBB & (currRankBB | currFileBB)) ^ bits[sq]) ? 1 : 0) * (color == White ? 1 : -1);
    gradients[ROOK_ON_QUEEN_FILE_IDX] += (currFileBB & opponentPiecesBB & board.queensBB ? 1 : 0) * (color == White ? 1 : -1);
    gradients[ROOK_MOBILITY_IDX] += (mobility-7) * (color == White ? 1 : -1);
    gradients[PIECE_ATTACK_WEIGHT_IDX + Rook] += attackedSquares * (color == White ? 1 : -1);
    if(color == White) {
        if((board.whiteKingSquare == f1 || board.whiteKingSquare == g1) && (sq == g1 || sq == h1))
            gradients[BLOCKED_ROOK_PENALTY_IDX] -= 1;
        if((board.whiteKingSquare == c1 || board.whiteKingSquare == b1) && (sq == a1 || sq == b1))
            gradients[BLOCKED_ROOK_PENALTY_IDX] -= 1;
    }
    if (color == Black) {
        if ((board.whiteKingSquare == f8 || board.whiteKingSquare == g8) && (sq == g8 || sq == h8))
            gradients[BLOCKED_ROOK_PENALTY_IDX] += 1;
        if ((board.whiteKingSquare == c8 || board.whiteKingSquare == b8) && (sq == a8 || sq == b8))
            gradients[BLOCKED_ROOK_PENALTY_IDX] += 1;
    }

    freq[ROOK_TABLE_IDX + (color == White ? sq : TRAIN_FLIPPED[sq])] += 1;
    freq[PIECE_VALUES_IDX + Rook] += 1;
    freq[ROOK_PAWN_CONST_IDX] += 1;
    freq[ROOK_ON_OPEN_FILE_IDX] += (!ourBlockingPawns ? 1 : 0);
    freq[ROOK_ON_SEVENTH_IDX] += (sq/8 == seventhRank && (opponentKingSquare/8 == eighthRank || (opponentPawnsBB & ranksBB[seventhRank])) ? 1 : 0);
    freq[ROOKS_DEF_EACH_OTHER_IDX] += (((board.rooksBB & ourPiecesBB & (currRankBB | currFileBB)) ^ bits[sq]) ? 1 : 0);
    freq[ROOK_ON_QUEEN_FILE_IDX] += (currFileBB & opponentPiecesBB & board.queensBB ? 1 : 0);
    freq[ROOK_MOBILITY_IDX] += 1;
    freq[PIECE_ATTACK_WEIGHT_IDX + Rook] += (attackedSquares ? 1 : 0);
    if(color == White) {
        if((board.whiteKingSquare == f1 || board.whiteKingSquare == g1) && (sq == g1 || sq == h1))
            freq[BLOCKED_ROOK_PENALTY_IDX] += 1;
        if((board.whiteKingSquare == c1 || board.whiteKingSquare == b1) && (sq == a1 || sq == b1))
            freq[BLOCKED_ROOK_PENALTY_IDX] += 1;
    }
    if (color == Black) {
        if ((board.whiteKingSquare == f8 || board.whiteKingSquare == g8) && (sq == g8 || sq == h8))
            freq[BLOCKED_ROOK_PENALTY_IDX] += 1;
        if ((board.whiteKingSquare == c8 || board.whiteKingSquare == b8) && (sq == a8 || sq == b8))
            freq[BLOCKED_ROOK_PENALTY_IDX] += 1;
    }

    return eval;
}

int trainEvalQueen(
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

    if(color == White) trainPieceMaterialWhite += PIECE_VALUES[Queen];
    else trainPieceMaterialBlack += PIECE_VALUES[Queen];

    // initial piece value and square value
    int eval = PIECE_VALUES[Queen] + QUEEN_TABLE[(color == White ? sq : TRAIN_FLIPPED[sq])];

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
            trainWhiteAttackersCnt++;
            trainWhiteAttackWeight += PIECE_ATTACK_WEIGHT[Queen] * attackedSquares;
        } else {
            trainBlackAttackersCnt++;
            trainBlackAttackWeight += PIECE_ATTACK_WEIGHT[Queen] * attackedSquares;
        }
    }

    // --- UPDATE GRADIENTS ---
    gradients[QUEEN_TABLE_IDX + (color == White ? sq : TRAIN_FLIPPED[sq])] += (color == White ? 1 : -1);
    gradients[PIECE_VALUES_IDX + Queen] += (color == White ? 1 : -1);
    gradients[QUEEN_MOBILITY_IDX] += (mobility-14) * (color == White ? 1 : -1);
    gradients[PIECE_ATTACK_WEIGHT_IDX + Queen] += attackedSquares * (color == White ? 1 : -1);
    if(color == White && sq/8 > 1) {
        if(ourKnightsBB & bits[b1]) gradients[EARLY_QUEEN_DEVELOPMENT_IDX] -= 1;
        if(ourBishopsBB & bits[c1]) gradients[EARLY_QUEEN_DEVELOPMENT_IDX] -= 1;
        if(ourBishopsBB & bits[f1]) gradients[EARLY_QUEEN_DEVELOPMENT_IDX] -= 1;
        if(ourKnightsBB & bits[g1]) gradients[EARLY_QUEEN_DEVELOPMENT_IDX] -= 1;
    }
    if(color == Black && sq/8 < 6) {
        if(ourKnightsBB & bits[b8]) gradients[EARLY_QUEEN_DEVELOPMENT_IDX] += 1;
        if(ourBishopsBB & bits[c8]) gradients[EARLY_QUEEN_DEVELOPMENT_IDX] += 1;
        if(ourBishopsBB & bits[f8]) gradients[EARLY_QUEEN_DEVELOPMENT_IDX] += 1;
        if(ourKnightsBB & bits[g8]) gradients[EARLY_QUEEN_DEVELOPMENT_IDX] += 1;
    }

    freq[QUEEN_TABLE_IDX + (color == White ? sq : TRAIN_FLIPPED[sq])] += 1;
    freq[PIECE_VALUES_IDX + Queen] += 1;
    freq[QUEEN_MOBILITY_IDX] += 1;
    freq[PIECE_ATTACK_WEIGHT_IDX + Queen] += (attackedSquares ? 1 : 0);
    if(color == White && sq/8 > 1) {
        if(ourKnightsBB & bits[b1]) freq[EARLY_QUEEN_DEVELOPMENT_IDX] += 1;
        if(ourBishopsBB & bits[c1]) freq[EARLY_QUEEN_DEVELOPMENT_IDX] += 1;
        if(ourBishopsBB & bits[f1]) freq[EARLY_QUEEN_DEVELOPMENT_IDX] += 1;
        if(ourKnightsBB & bits[g1]) freq[EARLY_QUEEN_DEVELOPMENT_IDX] += 1;
    }
    if(color == Black && sq/8 < 6) {
        if(ourKnightsBB & bits[b8]) freq[EARLY_QUEEN_DEVELOPMENT_IDX] += 1;
        if(ourBishopsBB & bits[c8]) freq[EARLY_QUEEN_DEVELOPMENT_IDX] += 1;
        if(ourBishopsBB & bits[f8]) freq[EARLY_QUEEN_DEVELOPMENT_IDX] += 1;
        if(ourKnightsBB & bits[g8]) freq[EARLY_QUEEN_DEVELOPMENT_IDX] += 1;
    }

    return eval;
}

int trainWhiteKingShield(int KING_SHIELD[3]) {
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

    // --- UPDATE GRADIENTS ---
    int mgScore = min(24, trainGamePhase());
    if(sq%8 < 3) {
        if(ourPawnsBB & bits[a2]) gradients[KING_SHIELD_IDX] += (double)(mgScore) / 24.0;
        else if(ourPawnsBB & bits[a3]) gradients[KING_SHIELD_IDX+1] += (double)(mgScore) / 24.0;
        else gradients[KING_SHIELD_IDX+2] -= (double)(mgScore) / 24.0;

        if(ourPawnsBB & bits[b2]) gradients[KING_SHIELD_IDX] += (double)(mgScore) / 24.0;
        else if(ourPawnsBB & bits[b3]) gradients[KING_SHIELD_IDX+1] += (double)(mgScore) / 24.0;
        else gradients[KING_SHIELD_IDX+2] -= (double)(mgScore) / 24.0;

        if(ourPawnsBB & bits[c2]) gradients[KING_SHIELD_IDX] += (double)(mgScore) / 24.0;
        else if(ourPawnsBB & bits[c3]) gradients[KING_SHIELD_IDX+1] += (double)(mgScore) / 24.0;
        else gradients[KING_SHIELD_IDX+2] -= (double)(mgScore) / 24.0;
    } else if (sq%8 > 4) {
        if (ourPawnsBB & bits[f2]) gradients[KING_SHIELD_IDX] += (double)(mgScore) / 24.0;
        else if (ourPawnsBB & bits[f3]) gradients[KING_SHIELD_IDX + 1] += (double)(mgScore) / 24.0;
        else gradients[KING_SHIELD_IDX + 2] -= (double)(mgScore) / 24.0;

        if (ourPawnsBB & bits[g2]) gradients[KING_SHIELD_IDX] += (double)(mgScore) / 24.0;
        else if (ourPawnsBB & bits[g3]) gradients[KING_SHIELD_IDX + 1] += (double)(mgScore) / 24.0;
        else gradients[KING_SHIELD_IDX + 2] -= (double)(mgScore) / 24.0;

        if (ourPawnsBB & bits[h2]) gradients[KING_SHIELD_IDX] += (double)(mgScore) / 24.0;
        else if (ourPawnsBB & bits[h3]) gradients[KING_SHIELD_IDX + 1] += (double)(mgScore) / 24.0;
        else gradients[KING_SHIELD_IDX + 2] -= (double)(mgScore) / 24.0;
    }

    if (sq%8 < 3 && mgScore > 0) {
        if (ourPawnsBB & bits[a2]) freq[KING_SHIELD_IDX] += 1;
        else if (ourPawnsBB & bits[a3]) freq[KING_SHIELD_IDX + 1] += 1;
        else freq[KING_SHIELD_IDX + 2] += 1;

        if (ourPawnsBB & bits[b2]) freq[KING_SHIELD_IDX] += 1;
        else if (ourPawnsBB & bits[b3]) freq[KING_SHIELD_IDX + 1] += 1;
        else freq[KING_SHIELD_IDX + 2] += 1;

        if (ourPawnsBB & bits[c2]) freq[KING_SHIELD_IDX] += 1;
        else if (ourPawnsBB & bits[c3]) freq[KING_SHIELD_IDX + 1] += 1;
        else freq[KING_SHIELD_IDX + 2] += 1;
    } else if (sq%8 > 4 && mgScore > 0) {
        if (ourPawnsBB & bits[f2]) freq[KING_SHIELD_IDX] += 1;
        else if (ourPawnsBB & bits[f3]) freq[KING_SHIELD_IDX + 1] += 1;
        else freq[KING_SHIELD_IDX + 2] += 1;

        if (ourPawnsBB & bits[g2]) freq[KING_SHIELD_IDX] += 1;
        else if (ourPawnsBB & bits[g3]) freq[KING_SHIELD_IDX + 1] += 1;
        else freq[KING_SHIELD_IDX + 2] += 1;

        if (ourPawnsBB & bits[h2]) freq[KING_SHIELD_IDX] += 1;
        else if (ourPawnsBB & bits[h3]) freq[KING_SHIELD_IDX + 1] += 1;
        else freq[KING_SHIELD_IDX + 2] += 1;
    }

    return eval;
}

int trainBlackKingShield(int KING_SHIELD[3]) {
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

    // --- UPDATE GRADIENTS ---
    int mgScore = min(24, trainGamePhase());
    if(sq%8 < 3) {
        if(ourPawnsBB & bits[a7]) gradients[KING_SHIELD_IDX] -= (double)(mgScore) / 24.0;
        else if(ourPawnsBB & bits[a6]) gradients[KING_SHIELD_IDX+1] -= (double)(mgScore) / 24.0;
        else gradients[KING_SHIELD_IDX+2] += (double)(mgScore) / 24.0;

        if(ourPawnsBB & bits[b7]) gradients[KING_SHIELD_IDX] -= (double)(mgScore) / 24.0;
        else if(ourPawnsBB & bits[b6]) gradients[KING_SHIELD_IDX+1] -= (double)(mgScore) / 24.0;
        else gradients[KING_SHIELD_IDX+2] += (double)(mgScore) / 24.0;

        if(ourPawnsBB & bits[c7]) gradients[KING_SHIELD_IDX] -= (double)(mgScore) / 24.0;
        else if(ourPawnsBB & bits[c6]) gradients[KING_SHIELD_IDX+1] -= (double)(mgScore) / 24.0;
        else gradients[KING_SHIELD_IDX+2] += (double)(mgScore) / 24.0;
    } else if (sq%8 > 4) {
        if (ourPawnsBB & bits[f7]) gradients[KING_SHIELD_IDX] -= (double)(mgScore) / 24.0;
        else if (ourPawnsBB & bits[f6]) gradients[KING_SHIELD_IDX + 1] -= (double)(mgScore) / 24.0;
        else gradients[KING_SHIELD_IDX + 2] += (double)(mgScore) / 24.0;

        if (ourPawnsBB & bits[g7]) gradients[KING_SHIELD_IDX] -= (double)(mgScore) / 24.0;
        else if (ourPawnsBB & bits[g6]) gradients[KING_SHIELD_IDX + 1] -= (double)(mgScore) / 24.0;
        else gradients[KING_SHIELD_IDX + 2] += (double)(mgScore) / 24.0;

        if (ourPawnsBB & bits[h7]) gradients[KING_SHIELD_IDX] -= (double)(mgScore) / 24.0;
        else if (ourPawnsBB & bits[h6]) gradients[KING_SHIELD_IDX + 1] -= (double)(mgScore) / 24.0;
        else gradients[KING_SHIELD_IDX + 2] += (double)(mgScore) / 24.0;
    }

    if (sq%8 < 3 && mgScore > 0) {
        if (ourPawnsBB & bits[a7]) freq[KING_SHIELD_IDX] += 1;
        else if (ourPawnsBB & bits[a6]) freq[KING_SHIELD_IDX + 1] += 1;
        else freq[KING_SHIELD_IDX + 2] += 1;

        if (ourPawnsBB & bits[b7]) freq[KING_SHIELD_IDX] += 1;
        else if (ourPawnsBB & bits[b6]) freq[KING_SHIELD_IDX + 1] += 1;
        else freq[KING_SHIELD_IDX + 2] += 1;

        if (ourPawnsBB & bits[c7]) freq[KING_SHIELD_IDX] += 1;
        else if (ourPawnsBB & bits[c6]) freq[KING_SHIELD_IDX + 1] += 1;
        else freq[KING_SHIELD_IDX + 2] += 1;
    } else if (sq%8 > 4 && mgScore > 0) {
        if (ourPawnsBB & bits[f7]) freq[KING_SHIELD_IDX] += 1;
        else if (ourPawnsBB & bits[f6]) freq[KING_SHIELD_IDX + 1] += 1;
        else freq[KING_SHIELD_IDX + 2] += 1;

        if (ourPawnsBB & bits[g7]) freq[KING_SHIELD_IDX] += 1;
        else if (ourPawnsBB & bits[g6]) freq[KING_SHIELD_IDX + 1] += 1;
        else freq[KING_SHIELD_IDX + 2] += 1;

        if (ourPawnsBB & bits[h7]) freq[KING_SHIELD_IDX] += 1;
        else if (ourPawnsBB & bits[h6]) freq[KING_SHIELD_IDX + 1] += 1;
        else freq[KING_SHIELD_IDX + 2] += 1;
    }

    return eval;
}

// evaluate every pawn independently but store the full pawn structure evaluation in the hash map
int trainEvalPawnStructure(
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
        eval += trainEvalPawn(
            sq, White, MG_PAWN_TABLE, EG_PAWN_TABLE, PASSED_PAWN_TABLE,
            DOUBLED_PAWNS_PENALTY, WEAK_PAWN_PENALTY, C_PAWN_PENALTY, PIECE_VALUES);
        whitePawns &= (whitePawns-1);
    }
    while(blackPawns) {
        int sq = bitscanForward(blackPawns);
        eval -= trainEvalPawn(
            sq, Black, MG_PAWN_TABLE, EG_PAWN_TABLE, PASSED_PAWN_TABLE,
            DOUBLED_PAWNS_PENALTY, WEAK_PAWN_PENALTY, C_PAWN_PENALTY, PIECE_VALUES);
        blackPawns &= (blackPawns-1);
    }

    if(useHash) recordPawnEval(eval);

    return eval;
}

int trainEvalPawn(
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

    if(color == White) trainPawnCntWhite ++;
    else trainPawnCntBlack ++;

    // initial pawn value + square value
    int mgWeight = min(trainGamePhase(), 24);
    int egWeight = 24-mgWeight;
    int pst_eval = (
        mgWeight * MG_PAWN_TABLE[(color == White ? sq : TRAIN_FLIPPED[sq])] 
        + egWeight * EG_PAWN_TABLE[(color == White ? sq : TRAIN_FLIPPED[sq])]
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
        int bonus = PASSED_PAWN_TABLE[(color == White ? sq : TRAIN_FLIPPED[sq])];
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

    // --- UPDATE GRADIENTS ---
    gradients[PIECE_VALUES_IDX + Pawn] += (color == White ? 1 : -1);
    gradients[MG_PAWN_TABLE_IDX + (color == White ? sq : TRAIN_FLIPPED[sq])] += (color == White ? 1 : -1) * (double)(mgWeight) / 24.0;
    gradients[EG_PAWN_TABLE_IDX + (color == White ? sq : TRAIN_FLIPPED[sq])] += (color == White ? 1 : -1) * (double)(egWeight) / 24.0;
    gradients[PASSED_PAWN_TABLE_IDX + (color == White ? sq : TRAIN_FLIPPED[sq])] += (passed ? 1 : 0) * (color == White ? 1 : -1) * (ourPawnAttacksBB & bits[sq] ? (double)(4 / 3) : 1);
    gradients[WEAK_PAWN_PENALTY_IDX] += (weak ? -1 : 0) * (color == White ? 1 : -1) * ((!opposed) ? (double)(4 / 3) : 1);
    curSq = sq+dir;
    while(curSq < 64 && curSq >= 0) {
        if(board.pawnsBB & bits[curSq]) {
            gradients[DOUBLED_PAWNS_PENALTY_IDX] += ((ourPiecesBB & bits[curSq]) ? -1 : 0) * (color == White ? 1 : -1);
        }
        curSq += dir;
    }
    if(color == White && sq == c2 && (ourPawnsBB & bits[d4]) && !(ourPawnsBB & bits[e4]))
        gradients[C_PAWN_PENALTY_IDX] += -1;
    if(color == Black && sq == c7 && (ourPawnsBB & bits[d5]) && !(ourPawnsBB & bits[e5]))
        gradients[C_PAWN_PENALTY_IDX] += 1;
    
    freq[PIECE_VALUES_IDX + Pawn] += 1;
    freq[MG_PAWN_TABLE_IDX + (color == White ? sq : TRAIN_FLIPPED[sq])] += (mgWeight > 0 ? 1 : 0);
    freq[EG_PAWN_TABLE_IDX + (color == White ? sq : TRAIN_FLIPPED[sq])] += (egWeight > 0 ? 1 : 0);
    freq[PASSED_PAWN_TABLE_IDX + (color == White ? sq : TRAIN_FLIPPED[sq])] += (passed ? 1 : 0);
    freq[WEAK_PAWN_PENALTY_IDX] += (weak ? 1 : 0);
    curSq = sq+dir;
    while(curSq < 64 && curSq >= 0) {
        if(board.pawnsBB & bits[curSq]) {
            freq[DOUBLED_PAWNS_PENALTY_IDX] += ((ourPiecesBB & bits[curSq]) ? 1 : 0);
        }
        curSq += dir;
    }
    if(color == White && sq == c2 && (ourPawnsBB & bits[d4]) && !(ourPawnsBB & bits[e4]))
        freq[C_PAWN_PENALTY_IDX] += 1;
    if(color == Black && sq == c7 && (ourPawnsBB & bits[d5]) && !(ourPawnsBB & bits[e5]))
        freq[C_PAWN_PENALTY_IDX] += 1;

    return eval;
}

int trainGamePhase() {
    return popcount(board.knightsBB) * TRAIN_MG_WEIGHT[Knight] 
         + popcount(board.bishopsBB) * TRAIN_MG_WEIGHT[Bishop] 
         + popcount(board.rooksBB) * TRAIN_MG_WEIGHT[Rook]
         + popcount(board.queensBB) * TRAIN_MG_WEIGHT[Queen]; 
}
