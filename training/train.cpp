#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <cassert>
#include <random>

#include "../engine/Evaluate.h"
#include "../engine/Board.h"
#include "../engine/TranspositionTable.h"
#include "../engine/MagicBitboardUtils.h"

using namespace std;

struct PositionData {
    string fen;
    double result;
};

static vector<PositionData> positions;
static double optK = 1.0;

void createPosVector(const string& input_file, int num) {
    ifstream fin(input_file);
    if(!fin.is_open()) {
        cerr << "Error opening file " << input_file << '\n';
        return;
    }

    positions.clear();
    positions.reserve(num > 0 ? num : 10000);

    string str;
    cout << "Parsing fens...\n";
    while(getline(fin, str) && (num <= 0 || (int)positions.size() < num)) {
        if(str.empty()) continue;

        string fen;
        double result = 0.5;

        // Support both "FEN" "RESULT" and FEN$RESULT formats
        if(str.find('$') != string::npos) {
            size_t delim = str.find('$');
            fen = str.substr(0, delim);
            string resStr = str.substr(delim + 1);
            if(resStr == "1.0" || resStr == "1" || resStr == "1-0") result = 1.0;
            else if(resStr == "0.0" || resStr == "0" || resStr == "0-1") result = 0.0;
            else result = 0.5;
        } else if(str.find('"') != string::npos) {
            stringstream ss(str);
            string segment;
            vector<string> v;
            while(getline(ss, segment, '"')) {
                if(!segment.empty()) v.push_back(segment);
            }
            if(v.size() >= 2) {
                fen = v[0];
                if(v[1] == "1-0" || v[1] == "1.0") result = 1.0;
                else if(v[1] == "0-1" || v[1] == "0.0") result = 0.0;
                else result = 0.5;
            } else {
                continue;
            }
        } else {
            stringstream ss(str);
            string token;
            vector<string> tokens;
            while(ss >> token) tokens.push_back(token);
            if(tokens.size() >= 5) {
                string resStr = tokens.back();
                tokens.pop_back();
                if(resStr == "1.0" || resStr == "1-0" || resStr == "[1.0]") result = 1.0;
                else if(resStr == "0.0" || resStr == "0-1" || resStr == "[0.0]") result = 0.0;
                else result = 0.5;
                fen = "";
                for(size_t i = 0; i < tokens.size(); i++) {
                    if(i > 0) fen += " ";
                    fen += tokens[i];
                }
            } else {
                continue;
            }
        }

        positions.push_back({fen, result});
    }
    cout << "Parsed fens. Loaded " << positions.size() << " positions.\n";
    fin.close();
}

vector<int> paramsToVector(const EvalParams& p) {
    vector<int> v;
    v.reserve(13 * 64 + 14 + 9 + 45);

    auto addTable = [&](const int* table, int size) {
        for(int i = 0; i < size; i++) v.push_back(table[i]);
    };

    addTable(p.mgKingTable, 64);
    addTable(p.egKingTable, 64);
    addTable(p.mgPawnTable, 64);
    addTable(p.egPawnTable, 64);
    addTable(p.passedPawnTable, 64);
    addTable(p.mgKnightTable, 64);
    addTable(p.egKnightTable, 64);
    addTable(p.mgBishopTable, 64);
    addTable(p.egBishopTable, 64);
    addTable(p.mgRookTable, 64);
    addTable(p.egRookTable, 64);
    addTable(p.mgQueenTable, 64);
    addTable(p.egQueenTable, 64);

    addTable(p.mgPieceValues, 7);
    addTable(p.egPieceValues, 7);

    addTable(p.kingShield, 3);
    addTable(p.pieceAttackWeight, 6);

    auto addPair = [&](const int pair[2]) {
        v.push_back(pair[0]);
        v.push_back(pair[1]);
    };

    addPair(p.knightMobility);
    addPair(p.knightPawnConst);
    addPair(p.trappedKnightPenalty);
    addPair(p.knightDefByPawn);
    addPair(p.blockingCKnight);
    addPair(p.knightPairPenalty);

    addPair(p.bishopPair);
    addPair(p.trappedBishopPenalty);
    addPair(p.fianchettoBonus);
    addPair(p.bishopMobility);
    addPair(p.blockedBishopPenalty);

    addPair(p.rookOnQueenFile);
    addPair(p.rookOnOpenFile);
    addPair(p.rookPawnConst);
    addPair(p.rookOnSeventh);
    addPair(p.rooksDefEachOther);
    addPair(p.rookMobility);
    addPair(p.blockedRookPenalty);

    addPair(p.earlyQueenDevelopment);
    addPair(p.queenMobility);

    addPair(p.doubledPawnsPenalty);
    addPair(p.weakPawnPenalty);
    addPair(p.cPawnPenalty);

    v.push_back(p.tempoBonus);

    return v;
}

EvalParams vectorToParams(const vector<int>& v) {
    EvalParams p;
    int offset = 0;

    auto readTable = [&](int* table, int size) {
        for(int i = 0; i < size; i++) table[i] = v[offset++];
    };

    readTable(p.mgKingTable, 64);
    readTable(p.egKingTable, 64);
    readTable(p.mgPawnTable, 64);
    readTable(p.egPawnTable, 64);
    readTable(p.passedPawnTable, 64);
    readTable(p.mgKnightTable, 64);
    readTable(p.egKnightTable, 64);
    readTable(p.mgBishopTable, 64);
    readTable(p.egBishopTable, 64);
    readTable(p.mgRookTable, 64);
    readTable(p.egRookTable, 64);
    readTable(p.mgQueenTable, 64);
    readTable(p.egQueenTable, 64);

    readTable(p.mgPieceValues, 7);
    readTable(p.egPieceValues, 7);

    readTable(p.kingShield, 3);
    readTable(p.pieceAttackWeight, 6);

    auto readPair = [&](int pair[2]) {
        pair[0] = v[offset++];
        pair[1] = v[offset++];
    };

    readPair(p.knightMobility);
    readPair(p.knightPawnConst);
    readPair(p.trappedKnightPenalty);
    readPair(p.knightDefByPawn);
    readPair(p.blockingCKnight);
    readPair(p.knightPairPenalty);

    readPair(p.bishopPair);
    readPair(p.trappedBishopPenalty);
    readPair(p.fianchettoBonus);
    readPair(p.bishopMobility);
    readPair(p.blockedBishopPenalty);

    readPair(p.rookOnQueenFile);
    readPair(p.rookOnOpenFile);
    readPair(p.rookPawnConst);
    readPair(p.rookOnSeventh);
    readPair(p.rooksDefEachOther);
    readPair(p.rookMobility);
    readPair(p.blockedRookPenalty);

    readPair(p.earlyQueenDevelopment);
    readPair(p.queenMobility);

    readPair(p.doubledPawnsPenalty);
    readPair(p.weakPawnPenalty);
    readPair(p.cPawnPenalty);

    p.tempoBonus = v[offset++];
    return p;
}

void printParams(const vector<int>& params, const string& output_file) {
    EvalParams p = vectorToParams(params);
    ofstream fout(output_file);

    auto printTable = [&](const string& name, const int* table, int size) {
        fout << "int " << name << "[" << size << "] = {\n   ";
        for(int i = 0; i < size; i++) {
            if(i % 8 == 0 && i > 0) fout << "\n   ";
            fout << table[i] << (i < size - 1 ? ", " : "");
        }
        fout << "\n};\n\n";
    };

    printTable("MG_KING_TABLE", p.mgKingTable, 64);
    printTable("EG_KING_TABLE", p.egKingTable, 64);
    printTable("MG_PAWN_TABLE", p.mgPawnTable, 64);
    printTable("EG_PAWN_TABLE", p.egPawnTable, 64);
    printTable("PASSED_PAWN_TABLE", p.passedPawnTable, 64);
    printTable("MG_KNIGHT_TABLE", p.mgKnightTable, 64);
    printTable("EG_KNIGHT_TABLE", p.egKnightTable, 64);
    printTable("MG_BISHOP_TABLE", p.mgBishopTable, 64);
    printTable("EG_BISHOP_TABLE", p.egBishopTable, 64);
    printTable("MG_ROOK_TABLE", p.mgRookTable, 64);
    printTable("EG_ROOK_TABLE", p.egRookTable, 64);
    printTable("MG_QUEEN_TABLE", p.mgQueenTable, 64);
    printTable("EG_QUEEN_TABLE", p.egQueenTable, 64);

    printTable("MG_PIECE_VALUES", p.mgPieceValues, 7);
    printTable("EG_PIECE_VALUES", p.egPieceValues, 7);

    printTable("KING_SHIELD", p.kingShield, 3);
    printTable("PIECE_ATTACK_WEIGHT", p.pieceAttackWeight, 6);

    auto printPair = [&](const string& name, const int pair[2]) {
        fout << "int " << name << "[2] = {" << pair[0] << ", " << pair[1] << "};\n";
    };

    printPair("KNIGHT_MOBILITY", p.knightMobility);
    printPair("KNIGHT_PAWN_CONST", p.knightPawnConst);
    printPair("TRAPPED_KNIGHT_PENALTY", p.trappedKnightPenalty);
    printPair("KNIGHT_DEF_BY_PAWN", p.knightDefByPawn);
    printPair("BLOCKING_C_KNIGHT", p.blockingCKnight);
    printPair("KNIGHT_PAIR_PENALTY", p.knightPairPenalty);

    printPair("BISHOP_PAIR", p.bishopPair);
    printPair("TRAPPED_BISHOP_PENALTY", p.trappedBishopPenalty);
    printPair("FIANCHETTO_BONUS", p.fianchettoBonus);
    printPair("BISHOP_MOBILITY", p.bishopMobility);
    printPair("BLOCKED_BISHOP_PENALTY", p.blockedBishopPenalty);

    printPair("ROOK_ON_QUEEN_FILE", p.rookOnQueenFile);
    printPair("ROOK_ON_OPEN_FILE", p.rookOnOpenFile);
    printPair("ROOK_PAWN_CONST", p.rookPawnConst);
    printPair("ROOK_ON_SEVENTH", p.rookOnSeventh);
    printPair("ROOKS_DEF_EACH_OTHER", p.rooksDefEachOther);
    printPair("ROOK_MOBILITY", p.rookMobility);
    printPair("BLOCKED_ROOK_PENALTY", p.blockedRookPenalty);

    printPair("EARLY_QUEEN_DEVELOPMENT", p.earlyQueenDevelopment);
    printPair("QUEEN_MOBILITY", p.queenMobility);

    printPair("DOUBLED_PAWNS_PENALTY", p.doubledPawnsPenalty);
    printPair("WEAK_PAWN_PENALTY", p.weakPawnPenalty);
    printPair("C_PAWN_PENALTY", p.cPawnPenalty);

    fout << "int TEMPO_BONUS = " << p.tempoBonus << ";\n";
    fout.close();
}

static inline double sigmoid(double ev, double k) {
    return 1.0 / (1.0 + exp(-k * ev / 400.0));
}

double computeMSE(const EvalParams& params, double k, size_t maxPos = 0) {
    double totalError = 0.0;
    size_t count = maxPos == 0 ? positions.size() : min(maxPos, positions.size());
    if(count == 0) return 0.0;

    for(size_t i = 0; i < count; i++) {
        board->loadFenPos(positions[i].fen);
        double ev = evaluate(params);
        // evaluate() returns score from side-to-move's perspective
        // Convert score to White's perspective for match results
        if(board->turn == Black) ev = -ev;

        double sig = sigmoid(ev, k);
        double diff = sig - positions[i].result;
        totalError += diff * diff;
    }
    return totalError / count;
}

double optimizeK(const EvalParams& params) {
    cout << "Optimizing K for Texel tuning...\n";
    double low = 0.1, high = 4.0;
    for(int it = 0; it < 30; it++) {
        double m1 = low + (high - low) / 3.0;
        double m2 = high - (high - low) / 3.0;

        double e1 = computeMSE(params, m1, 5000);
        double e2 = computeMSE(params, m2, 5000);

        if(e1 < e2) high = m2;
        else low = m1;
    }
    double bestK = (low + high) / 2.0;
    cout << "Optimal K found: " << bestK << '\n';
    return bestK;
}

// Texel tuning optimizer using mini-batches and numerical gradients
vector<int> trainParameters(const EvalParams& initialGuess, int epochs = 20) {
    EvalParams currentParams = initialGuess;
    vector<int> parVec = paramsToVector(currentParams);
    const size_t nParams = parVec.size();

    optK = optimizeK(currentParams);
    double bestMSE = computeMSE(currentParams, optK);
    cout << "Initial MSE: " << bestMSE << '\n';

    vector<double> continuousParams(nParams);
    for(size_t i = 0; i < nParams; i++) continuousParams[i] = parVec[i];

    vector<double> m(nParams, 0.0);
    vector<double> v(nParams, 0.0);

    const double alpha = 1.0;
    const double beta1 = 0.9;
    const double beta2 = 0.999;
    const double eps = 1e-8;

    mt19937 rng(1337);

    for(int epoch = 0; epoch < epochs; epoch++) {
        cout << "--- Epoch " << epoch + 1 << "/" << epochs << " ---" << '\n';
        shuffle(positions.begin(), positions.end(), rng);

        const size_t batchSize = min((size_t)2048, positions.size());
        vector<double> grad(nParams, 0.0);

        // Approximate gradient over mini-batch
        for(size_t pIdx = 0; pIdx < nParams; pIdx++) {
            // Check parameter sensitivity with step delta
            parVec[pIdx] += 1;
            EvalParams pPlus = vectorToParams(parVec);
            double errPlus = computeMSE(pPlus, optK, batchSize);

            parVec[pIdx] -= 2;
            EvalParams pMinus = vectorToParams(parVec);
            double errMinus = computeMSE(pMinus, optK, batchSize);

            parVec[pIdx] += 1; // restore
            grad[pIdx] = (errPlus - errMinus) / 2.0;
        }

        // Adam update
        for(size_t i = 0; i < nParams; i++) {
            m[i] = beta1 * m[i] + (1.0 - beta1) * grad[i];
            v[i] = beta2 * v[i] + (1.0 - beta2) * (grad[i] * grad[i]);

            double mHat = m[i] / (1.0 - pow(beta1, epoch + 1));
            double vHat = v[i] / (1.0 - pow(beta2, epoch + 1));

            continuousParams[i] -= alpha * mHat / (sqrt(vHat) + eps);
            parVec[i] = (int)round(continuousParams[i]);
        }

        currentParams = vectorToParams(parVec);
        double currentMSE = computeMSE(currentParams, optK);
        cout << "Epoch " << epoch + 1 << " complete. MSE: " << currentMSE << '\n';
    }

    return parVec;
}

void runSanityTests() {
    cout << "Running evaluation sanity tests...\n";

    struct TestCase {
        string fen;
        string name;
        int minExpected;
        int maxExpected;
    };

    vector<TestCase> testCases = {
        {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", "Starting position (White to move)", 0, 30},
        {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 0 1", "Starting position (Black to move)", -30, 0},
        {"rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1", "After 1. e4", -50, 50},
        {"r1bqkb1r/pppp1ppp/2n2n2/4p3/4P3/2N2N2/PPPP1PPP/R1BQKB1R w KQkq - 4 4", "Four Knights symmetrical", -15, 30},
        {"rnb1kbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", "White up Queen", 800, 1400},
        {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNB1KBNR w KQkq - 0 1", "White down Queen", -1400, -800},
        {"8/8/8/4k3/8/8/4K3/4R3 w - - 0 1", "KR vs K endgame", 400, 800},
        {"8/8/8/4k3/8/8/4K3/8 w - - 0 1", "K vs K draw", 0, 15},
        {"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", "Kiwipete complex tactical", -200, 200}
    };

    bool allPassed = true;
    for(const auto& tc : testCases) {
        board->loadFenPos(tc.fen);
        int score = evaluate();
        int phase = gamePhase();

        bool inBounds = (score >= tc.minExpected && score <= tc.maxExpected);
        cout << "  [" << (inBounds ? "PASS" : "FAIL") << "] " << tc.name
             << ": eval=" << score << " (phase=" << phase << "/24) expected in ["
             << tc.minExpected << ", " << tc.maxExpected << "]\n";

        if(!inBounds) allPassed = false;
    }

    if(allPassed) {
        cout << "All sanity tests PASSED!\n";
    } else {
        cerr << "Some sanity tests FAILED!\n";
    }
}

int main(int argc, char **argv) {
    board = new Board();
    transpositionTable = new TranspositionTable();
    init();

    if(argc >= 2 && string(argv[1]) == "--test") {
        runSanityTests();
        delete transpositionTable;
        delete board;
        return 0;
    }

    if(argc < 3) {
        cout << "Usage: " << argv[0] << " <positions_file> <output_params_file> [max_positions] [epochs]\n";
        cout << "   or: " << argv[0] << " --test\n";
        delete transpositionTable;
        delete board;
        return 0;
    }

    string inputFile = argv[1];
    string outputFile = argv[2];
    int maxPos = (argc >= 4) ? atoi(argv[3]) : 0;
    int epochs = (argc >= 5) ? atoi(argv[4]) : 15;

    createPosVector(inputFile, maxPos);

    if(positions.empty()) {
        cerr << "No positions found in " << inputFile << '\n';
        delete transpositionTable;
        delete board;
        return 1;
    }

    EvalParams initialGuess = defaultEvalParams;
    vector<int> tunedVec = trainParameters(initialGuess, epochs);

    printParams(tunedVec, outputFile);
    cout << "Finished tuning! Parameters saved to " << outputFile << '\n';

    delete transpositionTable;
    delete board;
    return 0;
}
