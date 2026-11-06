#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
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
#include "../engine/MoveUtils.h"
#include "../engine/Enums.h"

using namespace std;

struct PositionData {
    string fen;
    double result; // 1.0 = White win, 0.5 = Draw, 0.0 = Black win
};

vector<PositionData> positions;

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

    return v;
}

EvalParams vectorToParams(const vector<int>& v) {
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

    // Merge and shuffle all worker shards
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

    mt19937 rng(1337);
    shuffle(allLines.begin(), allLines.end(), rng);

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
