#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <cassert>
#include <random>
#include <chrono>
#include <algorithm>
#include <thread>
#include <cstdio>
#include <unistd.h>
#include <sys/wait.h>

#include "../engine/Evaluate.h"
#include "../engine/Board.h"
#include "../engine/Search.h"
#include "../engine/TranspositionTable.h"
#include "../engine/MagicBitboardUtils.h"
#include "../engine/MoveUtils.h"
#include "../engine/BoardUtils.h"
#include "../engine/Enums.h"

using namespace std;

struct PositionData {
    string fen;
    double result; // 1.0 = White win, 0.5 = Draw, 0.0 = Black win
};

vector<PositionData> positions;

// --- PARAMETER OFFSETS IN FLATTENED VECTOR (955 TOTAL) ---
enum ParamOffsets {
    OFF_MG_KING_TABLE = 0,
    OFF_EG_KING_TABLE = 64,
    OFF_MG_QUEEN_TABLE = 128,
    OFF_EG_QUEEN_TABLE = 192,
    OFF_MG_ROOK_TABLE = 256,
    OFF_EG_ROOK_TABLE = 320,
    OFF_MG_BISHOP_TABLE = 384,
    OFF_EG_BISHOP_TABLE = 448,
    OFF_MG_KNIGHT_TABLE = 512,
    OFF_EG_KNIGHT_TABLE = 576,
    OFF_MG_PAWN_TABLE = 640,
    OFF_EG_PAWN_TABLE = 704,
    OFF_MG_PASSED_PAWN_TABLE = 768,
    OFF_EG_PASSED_PAWN_TABLE = 832,

    OFF_MG_PIECE_VALUES = 896, // 7
    OFF_EG_PIECE_VALUES = 903, // 7

    OFF_KING_SHIELD = 910,         // 3
    OFF_PIECE_ATTACK_WEIGHT = 913, // 6

    OFF_KNIGHT_MOBILITY = 919,      // 2
    OFF_KNIGHT_DEF_BY_PAWN = 921,   // 2
    OFF_KNIGHT_PAIR_PENALTY = 923,  // 2
    OFF_BISHOP_PAIR = 925,          // 2
    OFF_BISHOP_MOBILITY = 927,      // 2
    OFF_ROOK_ON_OPEN_FILE = 929,    // 2
    OFF_ROOK_ON_SEVENTH = 931,      // 2
    OFF_ROOKS_DEF_EACH_OTHER = 933, // 2
    OFF_ROOK_MOBILITY = 935,        // 2
    OFF_QUEEN_MOBILITY = 937,       // 2
    OFF_DOUBLED_PAWNS_PENALTY = 939,// 2
    OFF_WEAK_PAWN_PENALTY = 941,    // 2

    OFF_KNIGHT_PAWN_CONST = 943,
    OFF_TRAPPED_KNIGHT_PENALTY = 944,
    OFF_BLOCKING_C_KNIGHT = 945,
    OFF_TRAPPED_BISHOP_PENALTY = 946,
    OFF_FIANCHETTO_BONUS = 947,
    OFF_BLOCKED_BISHOP_PENALTY = 948,
    OFF_ROOK_ON_QUEEN_FILE = 949,
    OFF_ROOK_PAWN_CONST = 950,
    OFF_BLOCKED_ROOK_PENALTY = 951,
    OFF_EARLY_QUEEN_DEVELOPMENT = 952,
    OFF_C_PAWN_PENALTY = 953,
    OFF_TEMPO_BONUS = 954,

    TOTAL_PARAMS = 955
};

void createPosVector(const string& input_file, int maxPos) {
    ifstream fin(input_file);
    if(!fin.is_open()) {
        cerr << "Error opening file: " << input_file << '\n';
        return;
    }

    positions.clear();
    string line;
    cout << "Loading positions from " << input_file << "...\n";
    while(getline(fin, line) && (maxPos <= 0 || (int)positions.size() < maxPos)) {
        if(!line.empty() && line.back() == '\r') line.pop_back();
        if(line.empty()) continue;

        string fen;
        double result = 0.5;

        if(line.find('$') != string::npos) {
            size_t delim = line.find('$');
            fen = line.substr(0, delim);
            string resStr = line.substr(delim + 1);
            if(resStr.find("1-0") != string::npos || resStr == "1.0" || resStr == "1") result = 1.0;
            else if(resStr.find("0-1") != string::npos || resStr == "0.0" || resStr == "0") result = 0.0;
            else result = 0.5;
        } else if(line.find('"') != string::npos) {
            size_t firstQuote = line.find('"');
            size_t secondQuote = line.find('"', firstQuote + 1);
            if(secondQuote == string::npos) continue;
            fen = line.substr(0, firstQuote);
            size_t c9Pos = fen.find(" c9 ");
            if(c9Pos != string::npos) fen = fen.substr(0, c9Pos);
            string resStr = line.substr(firstQuote + 1, secondQuote - firstQuote - 1);
            if(resStr == "1-0" || resStr == "1.0") result = 1.0;
            else if(resStr == "0-1" || resStr == "0.0") result = 0.0;
            else result = 0.5;
        } else if(line.find('[') != string::npos && line.find(']') != string::npos) {
            size_t lbrack = line.find('[');
            size_t rbrack = line.find(']', lbrack + 1);
            fen = line.substr(0, lbrack);
            string resStr = line.substr(lbrack + 1, rbrack - lbrack - 1);
            if(resStr == "1.0" || resStr == "1-0") result = 1.0;
            else if(resStr == "0.0" || resStr == "0-1") result = 0.0;
            else result = 0.5;
        } else {
            continue;
        }

        while(!fen.empty() && (fen.back() == ' ' || fen.back() == '\t')) fen.pop_back();
        if(!fen.empty()) {
            positions.push_back({fen, result});
        }
    }
    cout << "Loaded " << positions.size() << " positions.\n";
}

vector<int> paramsToVector(const EvalParams& p) {
    vector<int> v;
    v.reserve(TOTAL_PARAMS);
    auto addArr = [&](const int* arr, int n) {
        for(int i = 0; i < n; i++) v.push_back(arr[i]);
    };

    addArr(p.mgKingTable, 64);
    addArr(p.egKingTable, 64);
    addArr(p.mgQueenTable, 64);
    addArr(p.egQueenTable, 64);
    addArr(p.mgRookTable, 64);
    addArr(p.egRookTable, 64);
    addArr(p.mgBishopTable, 64);
    addArr(p.egBishopTable, 64);
    addArr(p.mgKnightTable, 64);
    addArr(p.egKnightTable, 64);
    addArr(p.mgPawnTable, 64);
    addArr(p.egPawnTable, 64);
    addArr(p.mgPassedPawnTable, 64);
    addArr(p.egPassedPawnTable, 64);

    addArr(p.mgPieceValues, 7);
    addArr(p.egPieceValues, 7);

    addArr(p.kingShield, 3);
    addArr(p.pieceAttackWeight, 6);

    addArr(p.knightMobility, 2);
    addArr(p.knightDefByPawn, 2);
    addArr(p.knightPairPenalty, 2);
    addArr(p.bishopPair, 2);
    addArr(p.bishopMobility, 2);
    addArr(p.rookOnOpenFile, 2);
    addArr(p.rookOnSeventh, 2);
    addArr(p.rooksDefEachOther, 2);
    addArr(p.rookMobility, 2);
    addArr(p.queenMobility, 2);
    addArr(p.doubledPawnsPenalty, 2);
    addArr(p.weakPawnPenalty, 2);

    v.push_back(p.knightPawnConst);
    v.push_back(p.trappedKnightPenalty);
    v.push_back(p.blockingCKnight);
    v.push_back(p.trappedBishopPenalty);
    v.push_back(p.fianchettoBonus);
    v.push_back(p.blockedBishopPenalty);
    v.push_back(p.rookOnQueenFile);
    v.push_back(p.rookPawnConst);
    v.push_back(p.blockedRookPenalty);
    v.push_back(p.earlyQueenDevelopment);
    v.push_back(p.cPawnPenalty);
    v.push_back(p.tempoBonus);

    assert((int)v.size() == TOTAL_PARAMS);
    return v;
}

EvalParams vectorToParams(const vector<int>& v) {
    assert((int)v.size() == TOTAL_PARAMS);
    EvalParams p{};
    int offset = 0;
    auto readArr = [&](int* arr, int n) {
        for(int i = 0; i < n; i++) arr[i] = v[offset++];
    };

    readArr(p.mgKingTable, 64);
    readArr(p.egKingTable, 64);
    readArr(p.mgQueenTable, 64);
    readArr(p.egQueenTable, 64);
    readArr(p.mgRookTable, 64);
    readArr(p.egRookTable, 64);
    readArr(p.mgBishopTable, 64);
    readArr(p.egBishopTable, 64);
    readArr(p.mgKnightTable, 64);
    readArr(p.egKnightTable, 64);
    readArr(p.mgPawnTable, 64);
    readArr(p.egPawnTable, 64);
    readArr(p.mgPassedPawnTable, 64);
    readArr(p.egPassedPawnTable, 64);

    readArr(p.mgPieceValues, 7);
    readArr(p.egPieceValues, 7);

    readArr(p.kingShield, 3);
    readArr(p.pieceAttackWeight, 6);

    readArr(p.knightMobility, 2);
    readArr(p.knightDefByPawn, 2);
    readArr(p.knightPairPenalty, 2);
    readArr(p.bishopPair, 2);
    readArr(p.bishopMobility, 2);
    readArr(p.rookOnOpenFile, 2);
    readArr(p.rookOnSeventh, 2);
    readArr(p.rooksDefEachOther, 2);
    readArr(p.rookMobility, 2);
    readArr(p.queenMobility, 2);
    readArr(p.doubledPawnsPenalty, 2);
    readArr(p.weakPawnPenalty, 2);

    p.knightPawnConst = v[offset++];
    p.trappedKnightPenalty = v[offset++];
    p.blockingCKnight = v[offset++];
    p.trappedBishopPenalty = v[offset++];
    p.fianchettoBonus = v[offset++];
    p.blockedBishopPenalty = v[offset++];
    p.rookOnQueenFile = v[offset++];
    p.rookPawnConst = v[offset++];
    p.blockedRookPenalty = v[offset++];
    p.earlyQueenDevelopment = v[offset++];
    p.cPawnPenalty = v[offset++];
    p.tempoBonus = v[offset++];

    assert(offset == (int)v.size());
    return p;
}

void printParams(const EvalParams& p, const string& output_file) {
    ofstream fout(output_file);

    auto printTable = [&](const string& comment, const int* table) {
        fout << "    // " << comment << "\n    {\n        ";
        for(int i = 0; i < 64; i++) {
            if(i % 8 == 0 && i > 0) fout << "\n        ";
            fout << table[i] << (i < 63 ? ", " : "");
        }
        fout << "\n    },\n";
    };

    fout << "EvalParams defaultEvalParams = {\n";
    printTable("mgKingTable", p.mgKingTable);
    printTable("egKingTable", p.egKingTable);
    printTable("mgQueenTable", p.mgQueenTable);
    printTable("egQueenTable", p.egQueenTable);
    printTable("mgRookTable", p.mgRookTable);
    printTable("egRookTable", p.egRookTable);
    printTable("mgBishopTable", p.mgBishopTable);
    printTable("egBishopTable", p.egBishopTable);
    printTable("mgKnightTable", p.mgKnightTable);
    printTable("egKnightTable", p.egKnightTable);
    printTable("mgPawnTable", p.mgPawnTable);
    printTable("egPawnTable", p.egPawnTable);
    printTable("mgPassedPawnTable", p.mgPassedPawnTable);
    printTable("egPassedPawnTable", p.egPassedPawnTable);

    auto printArr = [&](const int* arr, int n) {
        fout << "    {";
        for(int i = 0; i < n; i++) fout << arr[i] << (i < n - 1 ? ", " : "");
        fout << "},\n";
    };

    fout << "\n    // mgPieceValues & egPieceValues\n";
    printArr(p.mgPieceValues, 7);
    printArr(p.egPieceValues, 7);

    fout << "\n    // kingShield & pieceAttackWeight\n";
    printArr(p.kingShield, 3);
    printArr(p.pieceAttackWeight, 6);

    auto printPair = [&](const int pair[2], const string& name) {
        fout << "    {" << pair[0] << ", " << pair[1] << "}, // " << name << "\n";
    };

    fout << "\n    // Tapered bonuses & penalties [MG, EG]\n";
    printPair(p.knightMobility, "knightMobility");
    printPair(p.knightDefByPawn, "knightDefByPawn");
    printPair(p.knightPairPenalty, "knightPairPenalty");
    printPair(p.bishopPair, "bishopPair");
    printPair(p.bishopMobility, "bishopMobility");
    printPair(p.rookOnOpenFile, "rookOnOpenFile");
    printPair(p.rookOnSeventh, "rookOnSeventh");
    printPair(p.rooksDefEachOther, "rooksDefEachOther");
    printPair(p.rookMobility, "rookMobility");
    printPair(p.queenMobility, "queenMobility");
    printPair(p.doubledPawnsPenalty, "doubledPawnsPenalty");
    printPair(p.weakPawnPenalty, "weakPawnPenalty");

    fout << "\n    // Untapered bonuses & penalties\n";
    fout << "    " << p.knightPawnConst << ", // knightPawnConst\n";
    fout << "    " << p.trappedKnightPenalty << ", // trappedKnightPenalty\n";
    fout << "    " << p.blockingCKnight << ", // blockingCKnight\n";
    fout << "    " << p.trappedBishopPenalty << ", // trappedBishopPenalty\n";
    fout << "    " << p.fianchettoBonus << ", // fianchettoBonus\n";
    fout << "    " << p.blockedBishopPenalty << ", // blockedBishopPenalty\n";
    fout << "    " << p.rookOnQueenFile << ", // rookOnQueenFile\n";
    fout << "    " << p.rookPawnConst << ", // rookPawnConst\n";
    fout << "    " << p.blockedRookPenalty << ", // blockedRookPenalty\n";
    fout << "    " << p.earlyQueenDevelopment << ", // earlyQueenDevelopment\n";
    fout << "    " << p.cPawnPenalty << ", // cPawnPenalty\n";
    fout << "    " << p.tempoBonus << " // tempoBonus\n";
    fout << "};\n";

    fout.close();
}

// --- SPARSE EVALUATION TRACE EXTRACTION ---

enum ScaleMode : uint8_t {
    SCALE_NORMAL = 0,      // val * coeff
    SCALE_HALF = 1,        // (val / 2) * coeff
    SCALE_FOUR_THIRDS = 2  // ((val * 4) / 3) * coeff
};

enum PhaseTarget : uint8_t {
    TARGET_MG = 0,
    TARGET_EG = 1,
    TARGET_BOTH = 2,
    TARGET_TEMPO = 3
};

struct TraceTerm {
    uint16_t paramIdx;
    int16_t coeff;
    uint8_t scaleMode;
    uint8_t phaseTarget;
};

struct LinearTerm {
    uint16_t paramIdx;
    float weight; // Pre-combined continuous weight (before endgame low-material scale)
};

struct TraceRecord {
    vector<TraceTerm> terms;
    vector<LinearTerm> linearTerms; // Compact merged terms for fast continuous gradient descent
    uint8_t mgWeight = 0;
    uint8_t egWeight = 0;
    uint8_t whiteAttackersCnt = 0;
    uint8_t blackAttackersCnt = 0;
    uint8_t whiteAttackSq[6] = {0, 0, 0, 0, 0, 0};
    uint8_t blackAttackSq[6] = {0, 0, 0, 0, 0, 0};
    uint8_t pawnCntWhite = 0;
    uint8_t pawnCntBlack = 0;
    uint16_t pieceMaterialWhite = 0;
    uint16_t pieceMaterialBlack = 0;
    uint8_t turn = White;
    float result = 0.5f;
};

void addTraceTerm(vector<TraceTerm>& terms, uint16_t paramIdx, int16_t coeff, uint8_t scaleMode, uint8_t phaseTarget) {
    if(coeff == 0) return;
    for(auto& t : terms) {
        if(t.paramIdx == paramIdx && t.scaleMode == scaleMode && t.phaseTarget == phaseTarget) {
            t.coeff += coeff;
            return;
        }
    }
    terms.push_back({paramIdx, coeff, scaleMode, phaseTarget});
}

void extractKingShieldTrace(vector<TraceTerm>& terms, int color) {
    U64 ourPawnsBB = (color == White ? board->whitePiecesBB : board->blackPiecesBB) & board->pawnsBB;
    int sq = (color == White ? board->whiteKingSquare : board->blackKingSquare);
    int c = (color == White ? 1 : -1);

    if(color == White) {
        if(sq % 8 < 3) {
            if(ourPawnsBB & BoardUtils::bits[a2]) addTraceTerm(terms, OFF_KING_SHIELD + 1, c, SCALE_NORMAL, TARGET_MG);
            else if(ourPawnsBB & BoardUtils::bits[a3]) addTraceTerm(terms, OFF_KING_SHIELD + 2, c, SCALE_NORMAL, TARGET_MG);
            else addTraceTerm(terms, OFF_KING_SHIELD + 0, -c, SCALE_NORMAL, TARGET_MG);

            if(ourPawnsBB & BoardUtils::bits[b2]) addTraceTerm(terms, OFF_KING_SHIELD + 1, c, SCALE_NORMAL, TARGET_MG);
            else if(ourPawnsBB & BoardUtils::bits[b3]) addTraceTerm(terms, OFF_KING_SHIELD + 2, c, SCALE_NORMAL, TARGET_MG);
            else addTraceTerm(terms, OFF_KING_SHIELD + 0, -c, SCALE_NORMAL, TARGET_MG);

            if(ourPawnsBB & BoardUtils::bits[c2]) addTraceTerm(terms, OFF_KING_SHIELD + 1, c, SCALE_NORMAL, TARGET_MG);
            else if(ourPawnsBB & BoardUtils::bits[c3]) addTraceTerm(terms, OFF_KING_SHIELD + 2, c, SCALE_NORMAL, TARGET_MG);
            else addTraceTerm(terms, OFF_KING_SHIELD + 0, -c, SCALE_NORMAL, TARGET_MG);
        } else if(sq % 8 > 4) {
            if(ourPawnsBB & BoardUtils::bits[f2]) addTraceTerm(terms, OFF_KING_SHIELD + 1, c, SCALE_NORMAL, TARGET_MG);
            else if(ourPawnsBB & BoardUtils::bits[f3]) addTraceTerm(terms, OFF_KING_SHIELD + 2, c, SCALE_NORMAL, TARGET_MG);
            else addTraceTerm(terms, OFF_KING_SHIELD + 0, -c, SCALE_NORMAL, TARGET_MG);

            if(ourPawnsBB & BoardUtils::bits[g2]) addTraceTerm(terms, OFF_KING_SHIELD + 1, c, SCALE_NORMAL, TARGET_MG);
            else if(ourPawnsBB & BoardUtils::bits[g3]) addTraceTerm(terms, OFF_KING_SHIELD + 2, c, SCALE_NORMAL, TARGET_MG);
            else addTraceTerm(terms, OFF_KING_SHIELD + 0, -c, SCALE_NORMAL, TARGET_MG);

            if(ourPawnsBB & BoardUtils::bits[h2]) addTraceTerm(terms, OFF_KING_SHIELD + 1, c, SCALE_NORMAL, TARGET_MG);
            else if(ourPawnsBB & BoardUtils::bits[h3]) addTraceTerm(terms, OFF_KING_SHIELD + 2, c, SCALE_NORMAL, TARGET_MG);
            else addTraceTerm(terms, OFF_KING_SHIELD + 0, -c, SCALE_NORMAL, TARGET_MG);
        }
    } else {
        if(sq % 8 < 3) {
            if(ourPawnsBB & BoardUtils::bits[a7]) addTraceTerm(terms, OFF_KING_SHIELD + 1, c, SCALE_NORMAL, TARGET_MG);
            else if(ourPawnsBB & BoardUtils::bits[a6]) addTraceTerm(terms, OFF_KING_SHIELD + 2, c, SCALE_NORMAL, TARGET_MG);
            else addTraceTerm(terms, OFF_KING_SHIELD + 0, -c, SCALE_NORMAL, TARGET_MG);

            if(ourPawnsBB & BoardUtils::bits[b7]) addTraceTerm(terms, OFF_KING_SHIELD + 1, c, SCALE_NORMAL, TARGET_MG);
            else if(ourPawnsBB & BoardUtils::bits[b6]) addTraceTerm(terms, OFF_KING_SHIELD + 2, c, SCALE_NORMAL, TARGET_MG);
            else addTraceTerm(terms, OFF_KING_SHIELD + 0, -c, SCALE_NORMAL, TARGET_MG);

            if(ourPawnsBB & BoardUtils::bits[c7]) addTraceTerm(terms, OFF_KING_SHIELD + 1, c, SCALE_NORMAL, TARGET_MG);
            else if(ourPawnsBB & BoardUtils::bits[c6]) addTraceTerm(terms, OFF_KING_SHIELD + 2, c, SCALE_NORMAL, TARGET_MG);
            else addTraceTerm(terms, OFF_KING_SHIELD + 0, -c, SCALE_NORMAL, TARGET_MG);
        } else if(sq % 8 > 4) {
            if(ourPawnsBB & BoardUtils::bits[f7]) addTraceTerm(terms, OFF_KING_SHIELD + 1, c, SCALE_NORMAL, TARGET_MG);
            else if(ourPawnsBB & BoardUtils::bits[f6]) addTraceTerm(terms, OFF_KING_SHIELD + 2, c, SCALE_NORMAL, TARGET_MG);
            else addTraceTerm(terms, OFF_KING_SHIELD + 0, -c, SCALE_NORMAL, TARGET_MG);

            if(ourPawnsBB & BoardUtils::bits[g7]) addTraceTerm(terms, OFF_KING_SHIELD + 1, c, SCALE_NORMAL, TARGET_MG);
            else if(ourPawnsBB & BoardUtils::bits[g6]) addTraceTerm(terms, OFF_KING_SHIELD + 2, c, SCALE_NORMAL, TARGET_MG);
            else addTraceTerm(terms, OFF_KING_SHIELD + 0, -c, SCALE_NORMAL, TARGET_MG);

            if(ourPawnsBB & BoardUtils::bits[h7]) addTraceTerm(terms, OFF_KING_SHIELD + 1, c, SCALE_NORMAL, TARGET_MG);
            else if(ourPawnsBB & BoardUtils::bits[h6]) addTraceTerm(terms, OFF_KING_SHIELD + 2, c, SCALE_NORMAL, TARGET_MG);
            else addTraceTerm(terms, OFF_KING_SHIELD + 0, -c, SCALE_NORMAL, TARGET_MG);
        }
    }
}

TraceRecord extractTraceFromCurrentBoard(double result) {
    TraceRecord rec;
    rec.result = (float)result;
    rec.turn = (uint8_t)board->turn;

    int phase = min(gamePhase(), 24);
    rec.mgWeight = (uint8_t)phase;
    rec.egWeight = (uint8_t)(24 - phase);

    auto& terms = rec.terms;
    terms.reserve(48);

    int numberOfPawns = MagicBitboardUtils::popcount(board->pawnsBB);

    for(int sq = 0; sq < 64; sq++) {
        if(board->squares[sq] == Empty) continue;

        int color = (board->squares[sq] & (Black | White));
        int c = (color == White ? 1 : -1);
        int relSq = (color == White ? sq : FLIPPED[sq]);

        U64 ourPiecesBB = (color == White ? board->whitePiecesBB : board->blackPiecesBB);
        U64 opponentPiecesBB = (color == White ? board->blackPiecesBB : board->whitePiecesBB);
        U64 ourPawnsBB = board->pawnsBB & ourPiecesBB;
        U64 opponentPawnsBB = board->pawnsBB & opponentPiecesBB;
        U64 ourPawnAttacksBB = BoardUtils::pawnAttacks(ourPawnsBB, color);
        U64 opponentPawnAttacksBB = BoardUtils::pawnAttacks(opponentPawnsBB, color ^ (White | Black));
        U64 sqNearKing = (color == White ? BoardUtils::squaresNearBlackKing[board->blackKingSquare]
                                         : BoardUtils::squaresNearWhiteKing[board->whiteKingSquare]);

        // --- KNIGHT ---
        if(board->knightsBB & BoardUtils::bits[sq]) {
            if(color == White) rec.pieceMaterialWhite += PIECE_VALUES[Knight];
            else rec.pieceMaterialBlack += PIECE_VALUES[Knight];

            addTraceTerm(terms, OFF_MG_PIECE_VALUES + Knight, c, SCALE_NORMAL, TARGET_MG);
            addTraceTerm(terms, OFF_EG_PIECE_VALUES + Knight, c, SCALE_NORMAL, TARGET_EG);
            addTraceTerm(terms, OFF_MG_KNIGHT_TABLE + relSq, c, SCALE_NORMAL, TARGET_MG);
            addTraceTerm(terms, OFF_EG_KNIGHT_TABLE + relSq, c, SCALE_NORMAL, TARGET_EG);

            U64 mob = (BoardUtils::knightAttacksBB[sq] ^ (BoardUtils::knightAttacksBB[sq] & (ourPiecesBB | opponentPawnAttacksBB)));
            int mobDiff = MagicBitboardUtils::popcount(mob) - 4;
            addTraceTerm(terms, OFF_KNIGHT_MOBILITY + 0, c * mobDiff, SCALE_NORMAL, TARGET_MG);
            addTraceTerm(terms, OFF_KNIGHT_MOBILITY + 1, c * mobDiff, SCALE_NORMAL, TARGET_EG);

            addTraceTerm(terms, OFF_KNIGHT_PAWN_CONST, c * (numberOfPawns - 8), SCALE_NORMAL, TARGET_BOTH);

            if(color == White) {
                if(sq == a8 && (opponentPawnsBB & (BoardUtils::bits[a7] | BoardUtils::bits[c7])))
                    addTraceTerm(terms, OFF_TRAPPED_KNIGHT_PENALTY, -c, SCALE_NORMAL, TARGET_BOTH);
                if(sq == a7 && (opponentPawnsBB & (BoardUtils::bits[a6] | BoardUtils::bits[c6])) && (opponentPawnsBB & (BoardUtils::bits[b7] | BoardUtils::bits[d7])))
                    addTraceTerm(terms, OFF_TRAPPED_KNIGHT_PENALTY, -c, SCALE_NORMAL, TARGET_BOTH);
                if(sq == h8 && (opponentPawnsBB & (BoardUtils::bits[h7] | BoardUtils::bits[f7])))
                    addTraceTerm(terms, OFF_TRAPPED_KNIGHT_PENALTY, -c, SCALE_NORMAL, TARGET_BOTH);
                if(sq == h7 && (opponentPawnsBB & (BoardUtils::bits[f6] | BoardUtils::bits[h6])) && (opponentPawnsBB & (BoardUtils::bits[e7] | BoardUtils::bits[g7])))
                    addTraceTerm(terms, OFF_TRAPPED_KNIGHT_PENALTY, -c, SCALE_NORMAL, TARGET_BOTH);
                if(sq == c3 && (ourPawnsBB & BoardUtils::bits[c2]) && (ourPawnsBB & BoardUtils::bits[d4]) && !(ourPawnsBB & BoardUtils::bits[e4]))
                    addTraceTerm(terms, OFF_BLOCKING_C_KNIGHT, -c, SCALE_NORMAL, TARGET_BOTH);
            } else {
                if(sq == a1 && (opponentPawnsBB & (BoardUtils::bits[a2] | BoardUtils::bits[c2])))
                    addTraceTerm(terms, OFF_TRAPPED_KNIGHT_PENALTY, -c, SCALE_NORMAL, TARGET_BOTH);
                if(sq == a2 && (opponentPawnsBB & (BoardUtils::bits[a3] | BoardUtils::bits[c3])) && (opponentPawnsBB & (BoardUtils::bits[b2] | BoardUtils::bits[d2])))
                    addTraceTerm(terms, OFF_TRAPPED_KNIGHT_PENALTY, -c, SCALE_NORMAL, TARGET_BOTH);
                if(sq == h1 && (opponentPawnsBB & (BoardUtils::bits[h2] | BoardUtils::bits[f2])))
                    addTraceTerm(terms, OFF_TRAPPED_KNIGHT_PENALTY, -c, SCALE_NORMAL, TARGET_BOTH);
                if(sq == h2 && (opponentPawnsBB & (BoardUtils::bits[f3] | BoardUtils::bits[h3])) && (opponentPawnsBB & (BoardUtils::bits[e2] | BoardUtils::bits[g2])))
                    addTraceTerm(terms, OFF_TRAPPED_KNIGHT_PENALTY, -c, SCALE_NORMAL, TARGET_BOTH);
                if(sq == c6 && (ourPawnsBB & BoardUtils::bits[c7]) && (ourPawnsBB & BoardUtils::bits[d5]) && !(ourPawnsBB & BoardUtils::bits[e5]))
                    addTraceTerm(terms, OFF_BLOCKING_C_KNIGHT, -c, SCALE_NORMAL, TARGET_BOTH);
            }

            if(ourPawnAttacksBB & BoardUtils::bits[sq]) {
                addTraceTerm(terms, OFF_KNIGHT_DEF_BY_PAWN + 0, c, SCALE_NORMAL, TARGET_MG);
                addTraceTerm(terms, OFF_KNIGHT_DEF_BY_PAWN + 1, c, SCALE_NORMAL, TARGET_EG);
            }

            int attackedSquares = MagicBitboardUtils::popcount(BoardUtils::knightAttacksBB[sq] & sqNearKing);
            if(attackedSquares) {
                if(color == White) {
                    rec.whiteAttackersCnt++;
                    rec.whiteAttackSq[Knight] += attackedSquares;
                } else {
                    rec.blackAttackersCnt++;
                    rec.blackAttackSq[Knight] += attackedSquares;
                }
            }
        }

        // --- BISHOP ---
        if(board->bishopsBB & BoardUtils::bits[sq]) {
            if(color == White) rec.pieceMaterialWhite += PIECE_VALUES[Bishop];
            else rec.pieceMaterialBlack += PIECE_VALUES[Bishop];

            addTraceTerm(terms, OFF_MG_PIECE_VALUES + Bishop, c, SCALE_NORMAL, TARGET_MG);
            addTraceTerm(terms, OFF_EG_PIECE_VALUES + Bishop, c, SCALE_NORMAL, TARGET_EG);
            addTraceTerm(terms, OFF_MG_BISHOP_TABLE + relSq, c, SCALE_NORMAL, TARGET_MG);
            addTraceTerm(terms, OFF_EG_BISHOP_TABLE + relSq, c, SCALE_NORMAL, TARGET_EG);

            if(color == White) {
                if(sq == a7 && (opponentPawnsBB & BoardUtils::bits[b6]) && (opponentPawnsBB & BoardUtils::bits[c7]))
                    addTraceTerm(terms, OFF_TRAPPED_BISHOP_PENALTY, -c, SCALE_NORMAL, TARGET_BOTH);
                if(sq == h7 && (opponentPawnsBB & BoardUtils::bits[g6]) && (opponentPawnsBB & BoardUtils::bits[f7]))
                    addTraceTerm(terms, OFF_TRAPPED_BISHOP_PENALTY, -c, SCALE_NORMAL, TARGET_BOTH);
                if(sq == c1 && (ourPawnsBB & BoardUtils::bits[d2]) && (ourPiecesBB & BoardUtils::bits[e3]))
                    addTraceTerm(terms, OFF_BLOCKED_BISHOP_PENALTY, -c, SCALE_NORMAL, TARGET_BOTH);
                if(sq == f1 && (ourPawnsBB & BoardUtils::bits[e2]) && (ourPiecesBB & BoardUtils::bits[d3]))
                    addTraceTerm(terms, OFF_BLOCKED_BISHOP_PENALTY, -c, SCALE_NORMAL, TARGET_BOTH);
            } else {
                if(sq == a2 && (opponentPawnsBB & BoardUtils::bits[b3]) && (opponentPawnsBB & BoardUtils::bits[c2]))
                    addTraceTerm(terms, OFF_TRAPPED_BISHOP_PENALTY, -c, SCALE_NORMAL, TARGET_BOTH);
                if(sq == h2 && (opponentPawnsBB & BoardUtils::bits[g3]) && (opponentPawnsBB & BoardUtils::bits[f2]))
                    addTraceTerm(terms, OFF_TRAPPED_BISHOP_PENALTY, -c, SCALE_NORMAL, TARGET_BOTH);
                if(sq == c8 && (ourPawnsBB & BoardUtils::bits[d7]) && (ourPiecesBB & BoardUtils::bits[e6]))
                    addTraceTerm(terms, OFF_BLOCKED_BISHOP_PENALTY, -c, SCALE_NORMAL, TARGET_BOTH);
                if(sq == f8 && (ourPawnsBB & BoardUtils::bits[e7]) && (ourPiecesBB & BoardUtils::bits[d6]))
                    addTraceTerm(terms, OFF_BLOCKED_BISHOP_PENALTY, -c, SCALE_NORMAL, TARGET_BOTH);
            }

            if(color == White && sq == g2 && (ourPawnsBB & BoardUtils::bits[g3]) && (ourPawnsBB & BoardUtils::bits[f2]))
                addTraceTerm(terms, OFF_FIANCHETTO_BONUS, c, SCALE_NORMAL, TARGET_BOTH);
            if(color == White && sq == b2 && (ourPawnsBB & BoardUtils::bits[b3]) && (ourPawnsBB & BoardUtils::bits[c2]))
                addTraceTerm(terms, OFF_FIANCHETTO_BONUS, c, SCALE_NORMAL, TARGET_BOTH);
            if(color == Black && sq == g7 && (ourPawnsBB & BoardUtils::bits[g6]) && (ourPawnsBB & BoardUtils::bits[f7]))
                addTraceTerm(terms, OFF_FIANCHETTO_BONUS, c, SCALE_NORMAL, TARGET_BOTH);
            if(color == Black && sq == b7 && (ourPawnsBB & BoardUtils::bits[b6]) && (ourPawnsBB & BoardUtils::bits[c7]))
                addTraceTerm(terms, OFF_FIANCHETTO_BONUS, c, SCALE_NORMAL, TARGET_BOTH);

            U64 attacks = MagicBitboardUtils::magicBishopAttacks((board->whitePiecesBB | board->blackPiecesBB), sq);
            int mobility = MagicBitboardUtils::popcount(attacks & ~ourPiecesBB & ~opponentPawnAttacksBB);
            int attackedSquares = MagicBitboardUtils::popcount(attacks & sqNearKing);
            int mobDiff = mobility - 5;

            addTraceTerm(terms, OFF_BISHOP_MOBILITY + 0, c * mobDiff, SCALE_NORMAL, TARGET_MG);
            addTraceTerm(terms, OFF_BISHOP_MOBILITY + 1, c * mobDiff, SCALE_NORMAL, TARGET_EG);

            if(attackedSquares) {
                if(color == White) {
                    rec.whiteAttackersCnt++;
                    rec.whiteAttackSq[Bishop] += attackedSquares;
                } else {
                    rec.blackAttackersCnt++;
                    rec.blackAttackSq[Bishop] += attackedSquares;
                }
            }
        }

        // --- ROOK ---
        if(board->rooksBB & BoardUtils::bits[sq]) {
            if(color == White) rec.pieceMaterialWhite += PIECE_VALUES[Rook];
            else rec.pieceMaterialBlack += PIECE_VALUES[Rook];

            addTraceTerm(terms, OFF_MG_PIECE_VALUES + Rook, c, SCALE_NORMAL, TARGET_MG);
            addTraceTerm(terms, OFF_EG_PIECE_VALUES + Rook, c, SCALE_NORMAL, TARGET_EG);
            addTraceTerm(terms, OFF_MG_ROOK_TABLE + relSq, c, SCALE_NORMAL, TARGET_MG);
            addTraceTerm(terms, OFF_EG_ROOK_TABLE + relSq, c, SCALE_NORMAL, TARGET_EG);

            if(color == White) {
                if((board->whiteKingSquare == f1 || board->whiteKingSquare == g1) && (sq == g1 || sq == h1))
                    addTraceTerm(terms, OFF_BLOCKED_ROOK_PENALTY, -c, SCALE_NORMAL, TARGET_BOTH);
                if((board->whiteKingSquare == c1 || board->whiteKingSquare == b1) && (sq == a1 || sq == b1))
                    addTraceTerm(terms, OFF_BLOCKED_ROOK_PENALTY, -c, SCALE_NORMAL, TARGET_BOTH);
            } else {
                if((board->blackKingSquare == f8 || board->blackKingSquare == g8) && (sq == g8 || sq == h8))
                    addTraceTerm(terms, OFF_BLOCKED_ROOK_PENALTY, -c, SCALE_NORMAL, TARGET_BOTH);
                if((board->blackKingSquare == c8 || board->blackKingSquare == b8) && (sq == a8 || sq == b8))
                    addTraceTerm(terms, OFF_BLOCKED_ROOK_PENALTY, -c, SCALE_NORMAL, TARGET_BOTH);
            }

            addTraceTerm(terms, OFF_ROOK_PAWN_CONST, c * (8 - numberOfPawns), SCALE_NORMAL, TARGET_BOTH);

            U64 currFileBB = BoardUtils::filesBB[sq % 8];
            U64 currRankBB = BoardUtils::ranksBB[sq / 8];
            bool ourBlockingPawns = (currFileBB & ourPawnsBB);
            bool opponentBlockingPawns = (currFileBB & opponentPawnsBB);

            if(!ourBlockingPawns) {
                uint8_t sMode = opponentBlockingPawns ? SCALE_HALF : SCALE_NORMAL;
                addTraceTerm(terms, OFF_ROOK_ON_OPEN_FILE + 0, c, sMode, TARGET_MG);
                addTraceTerm(terms, OFF_ROOK_ON_OPEN_FILE + 1, c, sMode, TARGET_EG);
            }

            int opponentKingSquare = (color == White ? board->blackKingSquare : board->whiteKingSquare);
            int seventhRank = (color == White ? 6 : 1);
            int eighthRank = (color == White ? 7 : 0);

            if(sq / 8 == seventhRank && (opponentKingSquare / 8 == eighthRank || (opponentPawnsBB & BoardUtils::ranksBB[seventhRank]))) {
                addTraceTerm(terms, OFF_ROOK_ON_SEVENTH + 0, c, SCALE_NORMAL, TARGET_MG);
                addTraceTerm(terms, OFF_ROOK_ON_SEVENTH + 1, c, SCALE_NORMAL, TARGET_EG);
            }

            if((board->rooksBB & ourPiecesBB & (currRankBB | currFileBB)) ^ BoardUtils::bits[sq]) {
                addTraceTerm(terms, OFF_ROOKS_DEF_EACH_OTHER + 0, c, SCALE_NORMAL, TARGET_MG);
                addTraceTerm(terms, OFF_ROOKS_DEF_EACH_OTHER + 1, c, SCALE_NORMAL, TARGET_EG);
            }

            if(currFileBB & opponentPiecesBB & board->queensBB) {
                addTraceTerm(terms, OFF_ROOK_ON_QUEEN_FILE, c, SCALE_NORMAL, TARGET_BOTH);
            }

            U64 attacks = MagicBitboardUtils::magicRookAttacks((board->whitePiecesBB | board->blackPiecesBB), sq);
            int mobility = MagicBitboardUtils::popcount(attacks & ~ourPiecesBB & ~opponentPawnAttacksBB);
            int attackedSquares = MagicBitboardUtils::popcount(attacks & sqNearKing);
            int mobDiff = mobility - 7;

            addTraceTerm(terms, OFF_ROOK_MOBILITY + 0, c * mobDiff, SCALE_NORMAL, TARGET_MG);
            addTraceTerm(terms, OFF_ROOK_MOBILITY + 1, c * mobDiff, SCALE_NORMAL, TARGET_EG);

            if(attackedSquares) {
                if(color == White) {
                    rec.whiteAttackersCnt++;
                    rec.whiteAttackSq[Rook] += attackedSquares;
                } else {
                    rec.blackAttackersCnt++;
                    rec.blackAttackSq[Rook] += attackedSquares;
                }
            }
        }

        // --- QUEEN ---
        if(board->queensBB & BoardUtils::bits[sq]) {
            if(color == White) rec.pieceMaterialWhite += PIECE_VALUES[Queen];
            else rec.pieceMaterialBlack += PIECE_VALUES[Queen];

            addTraceTerm(terms, OFF_MG_PIECE_VALUES + Queen, c, SCALE_NORMAL, TARGET_MG);
            addTraceTerm(terms, OFF_EG_PIECE_VALUES + Queen, c, SCALE_NORMAL, TARGET_EG);
            addTraceTerm(terms, OFF_MG_QUEEN_TABLE + relSq, c, SCALE_NORMAL, TARGET_MG);
            addTraceTerm(terms, OFF_EG_QUEEN_TABLE + relSq, c, SCALE_NORMAL, TARGET_EG);

            U64 ourBishopsBB = (board->bishopsBB & ourPiecesBB);
            U64 ourKnightsBB = (board->knightsBB & ourPiecesBB);
            if(color == White && sq / 8 > 1) {
                if(ourKnightsBB & BoardUtils::bits[b1]) addTraceTerm(terms, OFF_EARLY_QUEEN_DEVELOPMENT, -c, SCALE_NORMAL, TARGET_BOTH);
                if(ourBishopsBB & BoardUtils::bits[c1]) addTraceTerm(terms, OFF_EARLY_QUEEN_DEVELOPMENT, -c, SCALE_NORMAL, TARGET_BOTH);
                if(ourBishopsBB & BoardUtils::bits[f1]) addTraceTerm(terms, OFF_EARLY_QUEEN_DEVELOPMENT, -c, SCALE_NORMAL, TARGET_BOTH);
                if(ourKnightsBB & BoardUtils::bits[g1]) addTraceTerm(terms, OFF_EARLY_QUEEN_DEVELOPMENT, -c, SCALE_NORMAL, TARGET_BOTH);
            }
            if(color == Black && sq / 8 < 6) {
                if(ourKnightsBB & BoardUtils::bits[b8]) addTraceTerm(terms, OFF_EARLY_QUEEN_DEVELOPMENT, -c, SCALE_NORMAL, TARGET_BOTH);
                if(ourBishopsBB & BoardUtils::bits[c8]) addTraceTerm(terms, OFF_EARLY_QUEEN_DEVELOPMENT, -c, SCALE_NORMAL, TARGET_BOTH);
                if(ourBishopsBB & BoardUtils::bits[f8]) addTraceTerm(terms, OFF_EARLY_QUEEN_DEVELOPMENT, -c, SCALE_NORMAL, TARGET_BOTH);
                if(ourKnightsBB & BoardUtils::bits[g8]) addTraceTerm(terms, OFF_EARLY_QUEEN_DEVELOPMENT, -c, SCALE_NORMAL, TARGET_BOTH);
            }

            U64 attacks = (MagicBitboardUtils::magicBishopAttacks((board->whitePiecesBB | board->blackPiecesBB), sq)
                         | MagicBitboardUtils::magicRookAttacks((board->whitePiecesBB | board->blackPiecesBB), sq));
            int mobility = MagicBitboardUtils::popcount(attacks & ~ourPiecesBB & ~opponentPawnAttacksBB);
            int attackedSquares = MagicBitboardUtils::popcount(attacks & sqNearKing);
            int mobDiff = mobility - 14;

            addTraceTerm(terms, OFF_QUEEN_MOBILITY + 0, c * mobDiff, SCALE_NORMAL, TARGET_MG);
            addTraceTerm(terms, OFF_QUEEN_MOBILITY + 1, c * mobDiff, SCALE_NORMAL, TARGET_EG);

            if(attackedSquares) {
                if(color == White) {
                    rec.whiteAttackersCnt++;
                    rec.whiteAttackSq[Queen] += attackedSquares;
                } else {
                    rec.blackAttackersCnt++;
                    rec.blackAttackSq[Queen] += attackedSquares;
                }
            }
        }

        // --- PAWN ---
        if(board->pawnsBB & BoardUtils::bits[sq]) {
            if(color == White) rec.pawnCntWhite++;
            else rec.pawnCntBlack++;

            addTraceTerm(terms, OFF_MG_PIECE_VALUES + Pawn, c, SCALE_NORMAL, TARGET_MG);
            addTraceTerm(terms, OFF_EG_PIECE_VALUES + Pawn, c, SCALE_NORMAL, TARGET_EG);
            addTraceTerm(terms, OFF_MG_PAWN_TABLE + relSq, c, SCALE_NORMAL, TARGET_MG);
            addTraceTerm(terms, OFF_EG_PAWN_TABLE + relSq, c, SCALE_NORMAL, TARGET_EG);

            bool weak = true, passed = true, opposed = false;
            int dir = (color == White ? 8 : -8);
            int curSq = sq + dir;
            while(curSq < 64 && curSq >= 0) {
                if(board->pawnsBB & BoardUtils::bits[curSq]) {
                    passed = false;
                    if(ourPiecesBB & BoardUtils::bits[curSq]) {
                        addTraceTerm(terms, OFF_DOUBLED_PAWNS_PENALTY + 0, -c, SCALE_NORMAL, TARGET_MG);
                        addTraceTerm(terms, OFF_DOUBLED_PAWNS_PENALTY + 1, -c, SCALE_NORMAL, TARGET_EG);
                    } else {
                        opposed = true;
                    }
                }
                if(opponentPawnAttacksBB & BoardUtils::bits[curSq]) passed = false;
                curSq += dir;
            }

            curSq = sq;
            while(curSq < 64 && curSq >= 0) {
                if(ourPawnAttacksBB & BoardUtils::bits[curSq]) {
                    weak = false;
                    break;
                }
                curSq -= dir;
            }

            if(passed) {
                uint8_t sMode = (ourPawnAttacksBB & BoardUtils::bits[sq]) ? SCALE_FOUR_THIRDS : SCALE_NORMAL;
                addTraceTerm(terms, OFF_MG_PASSED_PAWN_TABLE + relSq, c, sMode, TARGET_MG);
                addTraceTerm(terms, OFF_EG_PASSED_PAWN_TABLE + relSq, c, sMode, TARGET_EG);
            }

            if(weak) {
                uint8_t sMode = (!opposed) ? SCALE_FOUR_THIRDS : SCALE_NORMAL;
                addTraceTerm(terms, OFF_WEAK_PAWN_PENALTY + 0, -c, sMode, TARGET_MG);
                addTraceTerm(terms, OFF_WEAK_PAWN_PENALTY + 1, -c, sMode, TARGET_EG);
            }

            if(color == White && sq == c2 && (ourPawnsBB & BoardUtils::bits[d4]) && !(ourPawnsBB & BoardUtils::bits[e4]))
                addTraceTerm(terms, OFF_C_PAWN_PENALTY, -c, SCALE_NORMAL, TARGET_BOTH);
            if(color == Black && sq == c7 && (ourPawnsBB & BoardUtils::bits[d5]) && !(ourPawnsBB & BoardUtils::bits[e5]))
                addTraceTerm(terms, OFF_C_PAWN_PENALTY, -c, SCALE_NORMAL, TARGET_BOTH);
        }
    }

    // King PSTs & King Shield
    extractKingShieldTrace(terms, White);
    extractKingShieldTrace(terms, Black);

    addTraceTerm(terms, OFF_MG_KING_TABLE + board->whiteKingSquare, 1, SCALE_NORMAL, TARGET_MG);
    addTraceTerm(terms, OFF_MG_KING_TABLE + FLIPPED[board->blackKingSquare], -1, SCALE_NORMAL, TARGET_MG);
    addTraceTerm(terms, OFF_EG_KING_TABLE + board->whiteKingSquare, 1, SCALE_NORMAL, TARGET_EG);
    addTraceTerm(terms, OFF_EG_KING_TABLE + FLIPPED[board->blackKingSquare], -1, SCALE_NORMAL, TARGET_EG);

    // Bishop & Knight pairs
    if(MagicBitboardUtils::popcount(board->whitePiecesBB & board->bishopsBB) >= 2) {
        addTraceTerm(terms, OFF_BISHOP_PAIR + 0, 1, SCALE_NORMAL, TARGET_MG);
        addTraceTerm(terms, OFF_BISHOP_PAIR + 1, 1, SCALE_NORMAL, TARGET_EG);
    }
    if(MagicBitboardUtils::popcount(board->blackPiecesBB & board->bishopsBB) >= 2) {
        addTraceTerm(terms, OFF_BISHOP_PAIR + 0, -1, SCALE_NORMAL, TARGET_MG);
        addTraceTerm(terms, OFF_BISHOP_PAIR + 1, -1, SCALE_NORMAL, TARGET_EG);
    }

    if(MagicBitboardUtils::popcount(board->whitePiecesBB & board->knightsBB) >= 2) {
        addTraceTerm(terms, OFF_KNIGHT_PAIR_PENALTY + 0, -1, SCALE_NORMAL, TARGET_MG);
        addTraceTerm(terms, OFF_KNIGHT_PAIR_PENALTY + 1, -1, SCALE_NORMAL, TARGET_EG);
    }
    if(MagicBitboardUtils::popcount(board->blackPiecesBB & board->knightsBB) >= 2) {
        addTraceTerm(terms, OFF_KNIGHT_PAIR_PENALTY + 0, 1, SCALE_NORMAL, TARGET_MG);
        addTraceTerm(terms, OFF_KNIGHT_PAIR_PENALTY + 1, 1, SCALE_NORMAL, TARGET_EG);
    }

    // Tempo bonus (from White's perspective)
    addTraceTerm(terms, OFF_TEMPO_BONUS, (board->turn == White ? 1 : -1), SCALE_NORMAL, TARGET_TEMPO);

    // Build merged continuous linearTerms for fast gradient descent
    rec.linearTerms.reserve(terms.size());
    for(const auto& t : terms) {
        float scaleFactor = (t.scaleMode == SCALE_NORMAL) ? 1.0f : (t.scaleMode == SCALE_HALF ? 0.5f : (4.0f / 3.0f));
        float phaseFactor = (t.phaseTarget == TARGET_MG) ? (rec.mgWeight / 24.0f)
                          : (t.phaseTarget == TARGET_EG) ? (rec.egWeight / 24.0f)
                          : 1.0f;
        float w = t.coeff * scaleFactor * phaseFactor;
        if(fabs(w) < 1e-7f) continue;

        bool merged = false;
        for(auto& lt : rec.linearTerms) {
            if(lt.paramIdx == t.paramIdx) {
                lt.weight += w;
                merged = true;
                break;
            }
        }
        if(!merged) {
            rec.linearTerms.push_back({t.paramIdx, w});
        }
    }

    return rec;
}

// Exact integer evaluation from TraceRecord (returns score from White's perspective)
int evalTraceIntWhite(const TraceRecord& rec, const vector<int>& params) {
    int mg = 0, eg = 0, tempo = 0;

    for(const auto& t : rec.terms) {
        int raw = params[t.paramIdx];
        int scaled = (t.scaleMode == SCALE_NORMAL) ? raw
                   : (t.scaleMode == SCALE_HALF) ? (raw / 2)
                   : ((raw * 4) / 3);
        int val = scaled * t.coeff;
        if(t.phaseTarget == TARGET_MG) mg += val;
        else if(t.phaseTarget == TARGET_EG) eg += val;
        else if(t.phaseTarget == TARGET_BOTH) { mg += val; eg += val; }
        else tempo += val;
    }

    if(rec.whiteAttackersCnt > 2) {
        int wWeight = 0;
        for(int pc = Knight; pc <= Queen; pc++)
            wWeight += params[OFF_PIECE_ATTACK_WEIGHT + pc] * rec.whiteAttackSq[pc];
        mg += KING_SAFETY_TABLE[min(max(wWeight, 0), 99)];
    }
    if(rec.blackAttackersCnt > 2) {
        int bWeight = 0;
        for(int pc = Knight; pc <= Queen; pc++)
            bWeight += params[OFF_PIECE_ATTACK_WEIGHT + pc] * rec.blackAttackSq[pc];
        mg -= KING_SAFETY_TABLE[min(max(bWeight, 0), 99)];
    }

    int res = (rec.mgWeight * mg + rec.egWeight * eg) / 24 + tempo;

    int strongerPawns = rec.pawnCntWhite, weakerPawns = rec.pawnCntBlack;
    int strongerPieces = rec.pieceMaterialWhite, weakerPieces = rec.pieceMaterialBlack;
    if(res < 0) {
        swap(strongerPieces, weakerPieces);
        swap(strongerPawns, weakerPawns);
    }

    if(strongerPawns == 0) {
        if(strongerPieces < 400) return 0;
        if(weakerPawns == 0 && weakerPieces == 2 * PIECE_VALUES[Knight]) return 0;

        if(strongerPieces == PIECE_VALUES[Rook] && (weakerPieces == PIECE_VALUES[Knight] || weakerPieces == PIECE_VALUES[Bishop]))
            res /= 2;

        if((strongerPieces == PIECE_VALUES[Rook] + PIECE_VALUES[Bishop] || strongerPieces == PIECE_VALUES[Rook] + PIECE_VALUES[Knight])
           && weakerPieces == PIECE_VALUES[Rook])
            res /= 2;
    }

    return res;
}

// Smooth interpolation of KING_SAFETY_TABLE for continuous evaluation and gradients
inline double smoothKingSafety(double weight) {
    if(weight <= 0.0) return KING_SAFETY_TABLE[0];
    if(weight >= 99.0) return KING_SAFETY_TABLE[99];
    int idx = (int)weight;
    double frac = weight - idx;
    return KING_SAFETY_TABLE[idx] * (1.0 - frac) + KING_SAFETY_TABLE[idx + 1] * frac;
}

inline double smoothKingSafetyDeriv(double weight) {
    if(weight <= 0.0 || weight >= 99.0) return 0.0;
    int idx = (int)weight;
    return (double)(KING_SAFETY_TABLE[idx + 1] - KING_SAFETY_TABLE[idx]);
}

double evalTraceContinuousWhite(const TraceRecord& rec, const vector<double>& params, double& outEndgameScale) {
    double rawScore = 0.0;
    for(const auto& lt : rec.linearTerms) {
        rawScore += params[lt.paramIdx] * lt.weight;
    }

    double mgPhaseFactor = rec.mgWeight / 24.0;
    if(rec.whiteAttackersCnt > 2) {
        double wWeight = 0.0;
        for(int pc = Knight; pc <= Queen; pc++)
            wWeight += params[OFF_PIECE_ATTACK_WEIGHT + pc] * rec.whiteAttackSq[pc];
        rawScore += smoothKingSafety(wWeight) * mgPhaseFactor;
    }
    if(rec.blackAttackersCnt > 2) {
        double bWeight = 0.0;
        for(int pc = Knight; pc <= Queen; pc++)
            bWeight += params[OFF_PIECE_ATTACK_WEIGHT + pc] * rec.blackAttackSq[pc];
        rawScore -= smoothKingSafety(bWeight) * mgPhaseFactor;
    }

    int strongerPawns = rec.pawnCntWhite, weakerPawns = rec.pawnCntBlack;
    int strongerPieces = rec.pieceMaterialWhite, weakerPieces = rec.pieceMaterialBlack;
    if(rawScore < 0.0) {
        swap(strongerPieces, weakerPieces);
        swap(strongerPawns, weakerPawns);
    }

    outEndgameScale = 1.0;
    if(strongerPawns == 0) {
        if(strongerPieces < 400 || (weakerPawns == 0 && weakerPieces == 2 * PIECE_VALUES[Knight])) {
            outEndgameScale = 0.0;
            return 0.0;
        }
        if(strongerPieces == PIECE_VALUES[Rook] && (weakerPieces == PIECE_VALUES[Knight] || weakerPieces == PIECE_VALUES[Bishop])) {
            outEndgameScale = 0.5;
        } else if((strongerPieces == PIECE_VALUES[Rook] + PIECE_VALUES[Bishop] || strongerPieces == PIECE_VALUES[Rook] + PIECE_VALUES[Knight])
                  && weakerPieces == PIECE_VALUES[Rook]) {
            outEndgameScale = 0.5;
        }
    }

    return rawScore * outEndgameScale;
}

static constexpr double LN10_OVER_400 = 2.30258509299404568402 / 400.0;

static inline double sigmoid(double ev, double k) {
    return 1.0 / (1.0 + exp(-k * LN10_OVER_400 * ev));
}

double computeMSEInt(const vector<TraceRecord>& records, const vector<int>& params, double k) {
    if(records.empty()) return 0.0;
    unsigned int nThreads = max(1u, thread::hardware_concurrency());
    vector<double> partialLoss(nThreads, 0.0);
    vector<thread> workers;

    size_t chunk = (records.size() + nThreads - 1) / nThreads;
    for(unsigned int t = 0; t < nThreads; t++) {
        size_t startIdx = t * chunk;
        size_t endIdx = min(records.size(), startIdx + chunk);
        if(startIdx >= endIdx) continue;

        workers.emplace_back([&, t, startIdx, endIdx]() {
            double localSum = 0.0;
            for(size_t i = startIdx; i < endIdx; i++) {
                double ev = evalTraceIntWhite(records[i], params);
                double diff = sigmoid(ev, k) - records[i].result;
                localSum += diff * diff;
            }
            partialLoss[t] = localSum;
        });
    }
    for(auto& th : workers) th.join();

    double total = 0.0;
    for(double s : partialLoss) total += s;
    return total / records.size();
}

double optimizeK(const vector<TraceRecord>& records, const vector<int>& params) {
    cout << "Optimizing scaling constant K...\n";
    double low = 0.1, high = 3.5;
    for(int iter = 0; iter < 40; iter++) {
        double m1 = low + (high - low) / 3.0;
        double m2 = high - (high - low) / 3.0;
        if(computeMSEInt(records, params, m1) < computeMSEInt(records, params, m2))
            high = m2;
        else
            low = m1;
    }
    double bestK = 0.5 * (low + high);
    cout << "Optimal K = " << fixed << setprecision(6) << bestK << '\n';
    return bestK;
}

vector<bool> buildTrainableMask() {
    vector<bool> mask(TOTAL_PARAMS, true);

    // Pin Empty and King piece values to 0, and pin MG Pawn value to 100
    mask[OFF_MG_PIECE_VALUES + Empty] = false;
    mask[OFF_MG_PIECE_VALUES + Pawn] = false;
    mask[OFF_MG_PIECE_VALUES + King] = false;
    mask[OFF_EG_PIECE_VALUES + Empty] = false;
    mask[OFF_EG_PIECE_VALUES + King] = false;

    // Pin Empty and Pawn attack weights to 0
    mask[OFF_PIECE_ATTACK_WEIGHT + Empty] = false;
    mask[OFF_PIECE_ATTACK_WEIGHT + Pawn] = false;

    // Pin ranks 1 and 8 of Pawn and PassedPawn PSTs to 0
    for(int sq = 0; sq < 64; sq++) {
        if(sq < 8 || sq >= 56) {
            mask[OFF_MG_PAWN_TABLE + sq] = false;
            mask[OFF_EG_PAWN_TABLE + sq] = false;
            mask[OFF_MG_PASSED_PAWN_TABLE + sq] = false;
            mask[OFF_EG_PASSED_PAWN_TABLE + sq] = false;
        }
    }

    return mask;
}

bool verifyTracesAgainstEngine(const string& inputFile, int maxCheck = 0) {
    createPosVector(inputFile, maxCheck);
    if(positions.empty()) {
        cerr << "No positions found in " << inputFile << '\n';
        return false;
    }

    vector<int> baseVec = paramsToVector(defaultEvalParams);
    int mismatches = 0;

    for(size_t i = 0; i < positions.size(); i++) {
        board->loadFenPos(positions[i].fen);
        int engineScore = evaluate(defaultEvalParams);
        int engineWhiteScore = (board->turn == Black) ? -engineScore : engineScore;

        TraceRecord rec = extractTraceFromCurrentBoard(positions[i].result);
        int traceWhiteScore = evalTraceIntWhite(rec, baseVec);

        if(engineWhiteScore != traceWhiteScore) {
            if(mismatches < 10) {
                cerr << "Mismatch at position " << i << " (" << positions[i].fen << "): "
                     << "engine=" << engineWhiteScore << " vs trace=" << traceWhiteScore << '\n';
            }
            mismatches++;
        }
    }

    if(mismatches == 0) {
        cout << "[PASS] Verified 100% bit-exact match between TraceEvaluator and evaluate() across "
             << positions.size() << " positions!\n";
        return true;
    } else {
        cerr << "[FAIL] Found " << mismatches << " mismatches out of " << positions.size() << " positions.\n";
        return false;
    }
}

int runTexelTuning(const string& inputFile, const string& outputFile, int epochs, double lr, double l2Lambda) {
    createPosVector(inputFile, 0);
    if(positions.empty()) {
        cerr << "No positions loaded from " << inputFile << '\n';
        return 1;
    }

    cout << "Extracting sparse evaluation traces for " << positions.size() << " positions...\n";
    auto t0 = chrono::steady_clock::now();

    vector<int> initVec = paramsToVector(defaultEvalParams);
    vector<TraceRecord> allRecords;
    allRecords.reserve(positions.size());

    int mismatches = 0;
    for(size_t i = 0; i < positions.size(); i++) {
        board->loadFenPos(positions[i].fen);
        TraceRecord rec = extractTraceFromCurrentBoard(positions[i].result);

        // Verify exact match on first 5,000 positions during extraction
        if(i < 5000) {
            int engScore = evaluate(defaultEvalParams);
            if(board->turn == Black) engScore = -engScore;
            if(engScore != evalTraceIntWhite(rec, initVec)) mismatches++;
        }
        allRecords.push_back(std::move(rec));
    }

    // Free FEN strings to save memory
    positions.clear();
    positions.shrink_to_fit();

    auto t1 = chrono::steady_clock::now();
    cout << "Extracted " << allRecords.size() << " traces in "
         << fixed << setprecision(2) << chrono::duration<double>(t1 - t0).count() << "s.\n";

    if(mismatches > 0) {
        cerr << "ERROR: Trace verification failed with " << mismatches << " mismatches!\n";
        return 1;
    }
    cout << "Verified 100% exact trace match against engine evaluate().\n";

    // Split 90% Train / 10% Validation by contiguous game blocks before shuffling
    // so positions from the same game do not leak between train and validation sets
    size_t valSize = max((size_t)1, allRecords.size() / 10);
    size_t trainSize = allRecords.size() - valSize;

    vector<TraceRecord> trainSet(
        std::make_move_iterator(allRecords.begin()),
        std::make_move_iterator(allRecords.begin() + trainSize)
    );
    vector<TraceRecord> valSet(
        std::make_move_iterator(allRecords.begin() + trainSize),
        std::make_move_iterator(allRecords.end())
    );
    allRecords.clear();
    allRecords.shrink_to_fit();

    mt19937 rng(42);
    shuffle(trainSet.begin(), trainSet.end(), rng);

    cout << "Dataset split: " << trainSet.size() << " train | " << valSet.size() << " validation.\n";

    double optK = optimizeK(trainSet, initVec);

    double initTrainMSE = computeMSEInt(trainSet, initVec, optK);
    double initValMSE = computeMSEInt(valSet, initVec, optK);
    cout << "Initial Train MSE: " << setprecision(7) << initTrainMSE
         << " | Initial Val MSE: " << initValMSE << '\n';

    vector<bool> trainable = buildTrainableMask();
    vector<double> w(TOTAL_PARAMS);
    vector<double> wPrior(TOTAL_PARAMS);
    for(int i = 0; i < TOTAL_PARAMS; i++) {
        w[i] = initVec[i];
        wPrior[i] = initVec[i];
    }

    vector<double> m(TOTAL_PARAMS, 0.0);
    vector<double> v(TOTAL_PARAMS, 0.0);
    const double beta1 = 0.9;
    const double beta2 = 0.999;
    const double eps = 1e-8;

    vector<int> bestIntParams = initVec;
    double bestValMSE = initValMSE;
    int bestEpoch = 0;

    unsigned int nThreads = max(1u, thread::hardware_concurrency());
    vector<vector<double>> threadGrads(nThreads, vector<double>(TOTAL_PARAMS, 0.0));

    auto tuneStart = chrono::steady_clock::now();

    for(int epoch = 1; epoch <= epochs; epoch++) {
        for(unsigned int t = 0; t < nThreads; t++) {
            fill(threadGrads[t].begin(), threadGrads[t].end(), 0.0);
        }

        vector<thread> workers;
        size_t chunk = (trainSet.size() + nThreads - 1) / nThreads;
        const double kScale = optK * LN10_OVER_400;

        for(unsigned int t = 0; t < nThreads; t++) {
            size_t startIdx = t * chunk;
            size_t endIdx = min(trainSet.size(), startIdx + chunk);
            if(startIdx >= endIdx) continue;

            workers.emplace_back([&, t, startIdx, endIdx, kScale]() {
                auto& localGrad = threadGrads[t];
                for(size_t i = startIdx; i < endIdx; i++) {
                    const auto& rec = trainSet[i];
                    double egScale = 1.0;
                    double ev = evalTraceContinuousWhite(rec, w, egScale);
                    if(egScale == 0.0) continue;

                    double sig = sigmoid(ev, optK);
                    double diff = sig - rec.result;
                    double dLoss_dEv = 2.0 * diff * sig * (1.0 - sig) * kScale * egScale;

                    for(const auto& lt : rec.linearTerms) {
                        localGrad[lt.paramIdx] += dLoss_dEv * lt.weight;
                    }

                    // Gradients for pieceAttackWeight via smooth King Safety derivative
                    if(rec.whiteAttackersCnt > 2) {
                        double wWeight = 0.0;
                        for(int pc = Knight; pc <= Queen; pc++)
                            wWeight += w[OFF_PIECE_ATTACK_WEIGHT + pc] * rec.whiteAttackSq[pc];
                        double dKS = smoothKingSafetyDeriv(wWeight) * (rec.mgWeight / 24.0);
                        for(int pc = Knight; pc <= Queen; pc++) {
                            if(rec.whiteAttackSq[pc])
                                localGrad[OFF_PIECE_ATTACK_WEIGHT + pc] += dLoss_dEv * dKS * rec.whiteAttackSq[pc];
                        }
                    }
                    if(rec.blackAttackersCnt > 2) {
                        double bWeight = 0.0;
                        for(int pc = Knight; pc <= Queen; pc++)
                            bWeight += w[OFF_PIECE_ATTACK_WEIGHT + pc] * rec.blackAttackSq[pc];
                        double dKS = smoothKingSafetyDeriv(bWeight) * (rec.mgWeight / 24.0);
                        for(int pc = Knight; pc <= Queen; pc++) {
                            if(rec.blackAttackSq[pc])
                                localGrad[OFF_PIECE_ATTACK_WEIGHT + pc] -= dLoss_dEv * dKS * rec.blackAttackSq[pc];
                        }
                    }
                }
            });
        }
        for(auto& th : workers) th.join();

        double invN = 1.0 / trainSet.size();
        double biasCorr1 = 1.0 - pow(beta1, epoch);
        double biasCorr2 = 1.0 - pow(beta2, epoch);

        vector<int> roundedParams(TOTAL_PARAMS);
        for(int p = 0; p < TOTAL_PARAMS; p++) {
            if(!trainable[p]) {
                roundedParams[p] = initVec[p];
                continue;
            }

            double g = 0.0;
            for(unsigned int t = 0; t < nThreads; t++) g += threadGrads[t][p];
            g = g * invN + 2.0 * l2Lambda * (w[p] - wPrior[p]);

            m[p] = beta1 * m[p] + (1.0 - beta1) * g;
            v[p] = beta2 * v[p] + (1.0 - beta2) * (g * g);

            double mHat = m[p] / biasCorr1;
            double vHat = v[p] / biasCorr2;

            w[p] -= lr * mHat / (sqrt(vHat) + eps);
            if(p >= OFF_PIECE_ATTACK_WEIGHT && p < OFF_PIECE_ATTACK_WEIGHT + 6) {
                w[p] = max(0.0, w[p]);
            }
            roundedParams[p] = (int)llround(w[p]);
        }

        double valMSE = computeMSEInt(valSet, roundedParams, optK);
        if(valMSE < bestValMSE) {
            bestValMSE = valMSE;
            bestIntParams = roundedParams;
            bestEpoch = epoch;
        }

        if(epoch == 1 || epoch % 25 == 0 || epoch == epochs) {
            double trainMSE = computeMSEInt(trainSet, roundedParams, optK);
            auto now = chrono::steady_clock::now();
            double elapsed = chrono::duration<double>(now - tuneStart).count();
            cout << "Epoch " << setw(4) << epoch << "/" << epochs
                 << " | Train MSE: " << setprecision(7) << trainMSE
                 << " | Val MSE: " << valMSE
                 << " | Best Val: " << bestValMSE << " (ep " << bestEpoch << ")"
                 << " [" << setprecision(1) << elapsed << "s]\n";
        }
    }

    EvalParams bestEvalParams = vectorToParams(bestIntParams);
    printParams(bestEvalParams, outputFile);

    cout << "\nTuning finished!\n"
         << "  Initial Val MSE : " << setprecision(7) << initValMSE << "\n"
         << "  Best Val MSE    : " << bestValMSE << " (at epoch " << bestEpoch << ")\n"
         << "  Improvement     : " << (initValMSE - bestValMSE) << "\n"
         << "  Saved C++ params: " << outputFile << '\n';
    return 0;
}

// --- SELF-PLAY DATA GENERATION ---

static const string STARTPOS_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
static const int MAX_POSITIONS_PER_GAME = 25;
static const int OPENING_RANDOM_PLIES = 8;

bool setupRandomOpening(mt19937& rng) {
    board->loadFenPos(STARTPOS_FEN);
    Search::infiniteTime = true;
    Search::timeOver = false;

    uniform_int_distribution<int> pctDist(0, 99);

    for(int ply = 0; ply < OPENING_RANDOM_PLIES; ply++) {
        int moves[256];
        int num = board->generateLegalMoves(moves);
        if(num == 0 || board->isDraw()) return false;

        int moveScores[256];
        int bestScore = -1000000;

        for(int i = 0; i < num; i++) {
            board->makeMove(moves[i]);
            int sc = -Search::quiescence(-1000000, 1000000);
            board->unmakeMove(moves[i]);
            moveScores[i] = sc;
            if(sc > bestScore) bestScore = sc;
        }

        // 85% of the time pick a move within 120cp of the best move;
        // 15% of the time allow a wider margin (300cp) to sample suboptimal squares/penalties
        int margin = (pctDist(rng) < 85) ? 120 : 300;
        vector<int> candidates;
        for(int i = 0; i < num; i++) {
            if(moveScores[i] >= bestScore - margin) {
                candidates.push_back(moves[i]);
            }
        }
        if(candidates.empty()) return false;

        uniform_int_distribution<size_t> pickDist(0, candidates.size() - 1);
        board->makeMove(candidates[pickDist(rng)]);
    }

    int moves[256];
    if(board->generateLegalMoves(moves) == 0 || board->isDraw()) return false;

    int exitScore = Search::quiescence(-1000000, 1000000);
    return abs(exitScore) <= 600;
}

void runDatagenWorker(int workerId, const string& shardFile, int targetPositions, int searchDepth) {
    uint64_t seed = chrono::high_resolution_clock::now().time_since_epoch().count();
    seed ^= (uint64_t)getpid() * 0x9e3779b97f4a7c15ULL + (uint64_t)workerId * 1337ULL;
    mt19937 rng(seed);

    ofstream fout(shardFile);
    if(!fout.is_open()) {
        cerr << "Worker " << workerId << " failed to open " << shardFile << '\n';
        return;
    }

    int collectedPositions = 0;
    int gamesPlayed = 0;
    int whiteWins = 0, blackWins = 0, draws = 0;
    auto startTime = chrono::steady_clock::now();

    while(collectedPositions < targetPositions) {
        Search::clearHistory();
        if(!setupRandomOpening(rng)) continue;

        vector<string> gameQuietFens;
        gameQuietFens.reserve(64);

        string outcome = "1/2-1/2";
        int winAdjStreak = 0;
        int lastWinSign = 0;
        int drawAdjStreak = 0;

        for(int ply = OPENING_RANDOM_PLIES; ply < 400; ply++) {
            int moves[256];
            int num = board->generateLegalMoves(moves);
            if(num == 0) {
                if(board->isInCheck()) {
                    outcome = (board->turn == White) ? "0-1" : "1-0";
                } else {
                    outcome = "1/2-1/2";
                }
                break;
            }
            if(board->isDraw()) {
                outcome = "1/2-1/2";
                break;
            }

            // Record position if quiet
            if(!board->isInCheck()) {
                Search::infiniteTime = true;
                Search::timeOver = false;
                int staticEv = evaluate();
                int qEv = Search::quiescence(-1000000, 1000000);
                if(staticEv == qEv && abs(staticEv) < 3000) {
                    gameQuietFens.push_back(board->getFenFromCurrPos());
                }
            }

            // Search move to fixed depth with stdout muted
            Search::currMaxDepth = searchDepth;
            Search::infiniteTime = true;
            Search::timeOver = false;

            cout.setstate(ios_base::failbit);
            pair<int, int> searchRes = Search::root();
            cout.clear();

            int bestMove = searchRes.first;
            int score = searchRes.second;
            int whiteScore = (board->turn == White) ? score : -score;

            board->makeMove(bestMove);

            // Win/Loss adjudication: |whiteScore| >= 1000 cp for 4 consecutive plies
            if(abs(whiteScore) >= 1000) {
                int sign = (whiteScore > 0) ? 1 : -1;
                if(sign == lastWinSign) winAdjStreak++;
                else {
                    lastWinSign = sign;
                    winAdjStreak = 1;
                }
                if(winAdjStreak >= 4) {
                    outcome = (sign > 0) ? "1-0" : "0-1";
                    break;
                }
            } else {
                winAdjStreak = 0;
                lastWinSign = 0;
            }

            // Draw adjudication: |whiteScore| <= 15 cp for 8 consecutive plies after ply 80
            if(ply >= 80 && abs(whiteScore) <= 15) {
                drawAdjStreak++;
                if(drawAdjStreak >= 8) {
                    outcome = "1/2-1/2";
                    break;
                }
            } else {
                drawAdjStreak = 0;
            }
        }

        if(outcome == "1-0") whiteWins++;
        else if(outcome == "0-1") blackWins++;
        else draws++;
        gamesPlayed++;

        // Cap at MAX_POSITIONS_PER_GAME (25) quiet positions per game
        if((int)gameQuietFens.size() > MAX_POSITIONS_PER_GAME) {
            shuffle(gameQuietFens.begin(), gameQuietFens.end(), rng);
            gameQuietFens.resize(MAX_POSITIONS_PER_GAME);
        }

        for(const string& fen : gameQuietFens) {
            if(collectedPositions >= targetPositions) break;
            fout << fen << " \"" << outcome << "\"\n";
            collectedPositions++;
        }

        if(workerId == 0 && gamesPlayed % 20 == 0) {
            auto now = chrono::steady_clock::now();
            double elapsedSec = max(0.001, chrono::duration<double>(now - startTime).count());
            double posPerSec = collectedPositions / elapsedSec;
            cout << "[Worker 0] Games: " << gamesPlayed
                 << " | Positions: " << collectedPositions << "/" << targetPositions
                 << " (" << (int)posPerSec << " pos/s per worker)"
                 << " | W/D/L: " << whiteWins << "/" << draws << "/" << blackWins << '\n';
            cout.flush();
        }
    }

    fout.close();
}

int runSelfPlayDatagen(const string& outputFile, int totalPositions, int numWorkers, int searchDepth) {
    if(numWorkers <= 0) {
        numWorkers = max(1u, thread::hardware_concurrency());
    }
    cout << "Starting self-play data generation:\n"
         << "  Output file      : " << outputFile << "\n"
         << "  Target positions : " << totalPositions << "\n"
         << "  Workers          : " << numWorkers << "\n"
         << "  Search depth     : " << searchDepth << "\n"
         << "  Max pos / game   : " << MAX_POSITIONS_PER_GAME << "\n";

    auto startTime = chrono::steady_clock::now();
    vector<pid_t> children;
    vector<string> shardFiles;

    int baseQuota = totalPositions / numWorkers;
    int remainder = totalPositions % numWorkers;

    for(int w = 0; w < numWorkers; w++) {
        int quota = baseQuota + (w < remainder ? 1 : 0);
        if(quota <= 0) continue;

        string shard = outputFile + ".part." + to_string(w);
        shardFiles.push_back(shard);

        pid_t pid = fork();
        if(pid < 0) {
            cerr << "Failed to fork worker " << w << '\n';
            return 1;
        } else if(pid == 0) {
            // Child process
            runDatagenWorker(w, shard, quota, searchDepth);
            _exit(0);
        } else {
            children.push_back(pid);
        }
    }

    // Wait for all worker processes to complete
    for(pid_t pid : children) {
        int status = 0;
        waitpid(pid, &status, 0);
    }

    // Merge all worker shards (keeping games contiguous for clean train/val split)
    vector<string> allLines;
    allLines.reserve(totalPositions);

    for(const string& shard : shardFiles) {
        ifstream fin(shard);
        string line;
        while(getline(fin, line)) {
            if(!line.empty()) allLines.push_back(line);
        }
        fin.close();
        remove(shard.c_str());
    }

    ofstream fout(outputFile);
    if(!fout.is_open()) {
        cerr << "Error opening output file: " << outputFile << '\n';
        return 1;
    }
    for(const string& line : allLines) {
        fout << line << '\n';
    }
    fout.close();

    auto endTime = chrono::steady_clock::now();
    double totalSec = max(0.001, chrono::duration<double>(endTime - startTime).count());
    cout << "Self-play datagen complete! Wrote " << allLines.size() << " shuffled quiet positions to "
         << outputFile << " in " << totalSec << "s (" << (int)(allLines.size() / totalSec) << " pos/s).\n";
    return 0;
}

int runFilterQuiet(const string& inputFile, const string& outputFile) {
    createPosVector(inputFile, 0);
    if(positions.empty()) {
        cerr << "No positions loaded from " << inputFile << '\n';
        return 1;
    }

    Search::infiniteTime = true;
    Search::timeOver = false;

    vector<string> quietLines;
    quietLines.reserve(positions.size());

    for(size_t i = 0; i < positions.size(); i++) {
        board->loadFenPos(positions[i].fen);
        if(board->isInCheck()) continue;

        int staticEv = evaluate();
        int qEv = Search::quiescence(-1000000, 1000000);
        if(staticEv == qEv && abs(staticEv) < 3000) {
            string outcome = "1/2-1/2";
            if(positions[i].result > 0.75) outcome = "1-0";
            else if(positions[i].result < 0.25) outcome = "0-1";
            quietLines.push_back(positions[i].fen + " \"" + outcome + "\"");
        }

        if((i + 1) % 10000 == 0) {
            cout << "Checked " << (i + 1) << "/" << positions.size()
                 << " positions (kept " << quietLines.size() << " quiet)...\n";
        }
    }

    mt19937 rng(1337);
    shuffle(quietLines.begin(), quietLines.end(), rng);

    ofstream fout(outputFile);
    for(const string& line : quietLines) {
        fout << line << '\n';
    }
    fout.close();

    cout << "Filtered " << positions.size() << " -> " << quietLines.size()
         << " quiet positions written to " << outputFile << ".\n";
    return 0;
}

void printUsage(const char* prog) {
    cout << "Usage:\n"
         << "  " << prog << " --datagen <output_file> <num_positions> [num_workers] [depth]\n"
         << "  " << prog << " --tune <input_file> <output_params_file> [epochs=300] [lr=1.0] [l2=1e-7]\n"
         << "  " << prog << " --verify <input_file>\n"
         << "  " << prog << " --filter-quiet <input_file> <output_file>\n";
}

int main(int argc, char **argv) {
    board = new Board();
    transpositionTable = new TranspositionTable();
    init();

    if(argc >= 2 && string(argv[1]) == "--datagen") {
        if(argc < 4) {
            printUsage(argv[0]);
            delete transpositionTable;
            delete board;
            return 1;
        }
        string outputFile = argv[2];
        int totalPositions = atoi(argv[3]);
        int numWorkers = (argc >= 5) ? atoi(argv[4]) : (int)max(1u, thread::hardware_concurrency());
        int searchDepth = (argc >= 6) ? atoi(argv[5]) : 5;

        int rc = runSelfPlayDatagen(outputFile, totalPositions, numWorkers, searchDepth);
        delete transpositionTable;
        delete board;
        return rc;
    }

    if(argc >= 2 && string(argv[1]) == "--verify") {
        if(argc < 3) {
            printUsage(argv[0]);
            delete transpositionTable;
            delete board;
            return 1;
        }
        bool ok = verifyTracesAgainstEngine(argv[2]);
        delete transpositionTable;
        delete board;
        return ok ? 0 : 1;
    }

    if(argc >= 2 && string(argv[1]) == "--tune") {
        if(argc < 4) {
            printUsage(argv[0]);
            delete transpositionTable;
            delete board;
            return 1;
        }
        string inputFile = argv[2];
        string outputFile = argv[3];
        int epochs = (argc >= 5) ? atoi(argv[4]) : 300;
        double lr = (argc >= 6) ? atof(argv[5]) : 1.0;
        double l2 = (argc >= 7) ? atof(argv[6]) : 1e-7;

        int rc = runTexelTuning(inputFile, outputFile, epochs, lr, l2);
        delete transpositionTable;
        delete board;
        return rc;
    }

    if(argc >= 2 && string(argv[1]) == "--filter-quiet") {
        if(argc < 4) {
            printUsage(argv[0]);
            delete transpositionTable;
            delete board;
            return 1;
        }
        int rc = runFilterQuiet(argv[2], argv[3]);
        delete transpositionTable;
        delete board;
        return rc;
    }

    vector<int> vec = paramsToVector(defaultEvalParams);
    EvalParams roundTrip = vectorToParams(vec);
    assert(paramsToVector(roundTrip) == vec);
    cout << "Texel trainer initialized (" << vec.size() << " total parameters).\n";
    printUsage(argv[0]);

    delete transpositionTable;
    delete board;
    return 0;
}
