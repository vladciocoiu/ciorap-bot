#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cmath>
#include <cassert>

#include "../engine/Evaluate.h"
#include "../engine/Board.h"
#include "../engine/TranspositionTable.h"

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

int main(int argc, char **argv) {
    board = new Board();
    transpositionTable = new TranspositionTable();
    init();

    vector<int> vec = paramsToVector(defaultEvalParams);
    EvalParams roundTrip = vectorToParams(vec);
    assert(paramsToVector(roundTrip) == vec);
    cout << "Texel trainer initialized (" << vec.size() << " total parameters).\n";

    delete transpositionTable;
    delete board;
    return 0;
}
