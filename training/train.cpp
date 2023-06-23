#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <math.h>
#include <unordered_map>

#include "../engine/Evaluate.h"
#include "../engine/Board.h"

using namespace std;

vector<pair<string, double>> positions;

void createPosVector(string input_file, int num) {
    ifstream fin(input_file);

    string str;
    std::cout << "Parsing fens...\n";
    while(getline(fin, str) && positions.size() < num) {
        stringstream test(str);
        string segment;
        vector<string> v;

        while(getline(test, segment, '"')) {
            v.push_back(segment);
        }

        pair<string, double> p = {v[0], 1.0};
        if(v[1] == "0-1") p.second = 0.0;
        if(v[1] == "1/2-1/2") p.second = 0.5;
        positions.push_back(p);

        str = "";
    }
    std::cout << "Parsed fens. Have " << positions.size() << " positions.\n";

    fin.close();
}

void printParams(vector<int>& params, string output_file) {
    assert(params.size() ==  9 * 64 + 7 + 6 + 3 + 24);

    int offset = 0;

    ofstream fout(output_file);

    fout << "int MG_KING_TABLE[64] = {\n   ";
    for(int i = 0; i < 64; i++)  {
        if(i % 8 == 0 && i > 0) fout << "\n   ";
        fout << params[offset++] << (i < 63 ? ", " : "");
    }
    fout << "\n};\n\n";

    fout << "int EG_KING_TABLE[64] = {\n   ";
    for(int i = 0; i < 64; i++)  {
        if(i % 8 == 0 && i > 0) fout << "\n   ";
        fout << params[offset++] << (i < 63 ? ", " : "");
    }
    fout << "\n};\n\n";

    fout << "int QUEEN_TABLE[64] = {\n   ";
    for(int i = 0; i < 64; i++)  {
        if(i % 8 == 0 && i > 0) fout << "\n   ";
        fout << params[offset++] << (i < 63 ? ", " : "");
    }
    fout << "\n};\n\n";

    fout << "int ROOK_TABLE[64] = {\n   ";
    for(int i = 0; i < 64; i++)  {
        if(i % 8 == 0 && i > 0) fout << "\n   ";
        fout << params[offset++] << (i < 63 ? ", " : "");
    }
    fout << "\n};\n\n";

    fout << "int BISHOP_TABLE[64] = {\n   ";
    for(int i = 0; i < 64; i++)  {
        if(i % 8 == 0 && i > 0) fout << "\n   ";
        fout << params[offset++] << (i < 63 ? ", " : "");
    }
    fout << "\n};\n\n";

    fout << "int KNIGHT_TABLE[64] = {\n   ";
    for(int i = 0; i < 64; i++)  {
        if(i % 8 == 0 && i > 0) fout << "\n   ";
        fout << params[offset++] << (i < 63 ? ", " : "");
    }
    fout << "\n};\n\n";

    fout << "int MG_PAWN_TABLE[64] = {\n   ";
    for(int i = 0; i < 64; i++)  {
        if(i % 8 == 0 && i > 0) fout << "\n   ";
        fout << params[offset++] << (i < 63 ? ", " : "");
    }
    fout << "\n};\n\n";

    fout << "int EG_PAWN_TABLE[64] = {\n   ";
    for(int i = 0; i < 64; i++)  {
        if(i % 8 == 0 && i > 0) fout << "\n   ";
        fout << params[offset++] << (i < 63 ? ", " : "");
    }
    fout << "\n};\n\n";

    fout << "int PASSED_PAWN_TABLE[64] = {\n   ";
    for(int i = 0; i < 64; i++)  {
        if(i % 8 == 0 && i > 0) fout << "\n   ";
        fout << params[offset++] << (i < 63 ? ", " : "");
    }
    fout << "\n};\n\n";

    fout << "int KING_SHIELD[3] = {\n   ";
    for(int i = 0; i < 3; i++)  {
        fout << params[offset++] << (i < 2 ? ", " : "");
    }
    fout << "\n};\n\n";

    fout << "int PIECE_VALLUES[7] = {\n   ";
    for(int i = 0; i < 7; i++)  {
        fout << params[offset++] << (i < 6 ? ", " : "");
    }
    fout << "\n};\n\n";

    fout << "int PIECE_ATTACK_WEIGHT[6] = {\n   ";
    for(int i = 0; i < 6; i++)  {
        fout << params[offset++] << (i < 5 ? ", " : "");
    }
    fout << "\n};\n\n";

    fout << "int KNIGHT_MOBILITY = " << params[offset++] << ";\n";
    fout << "int KNIGHT_PAWN_CONST = " << params[offset++] << ";\n";
    fout << "int TRAPPED_KNIGHT_PENALTY = " << params[offset++] << ";\n";
    fout << "int KNIGHT_DEF_BY_PAWN = " << params[offset++] << ";\n";
    fout << "int BLOCKING_C_KNIGHT = " << params[offset++] << ";\n";
    fout << "int KNIGHT_PAIR_PENALTY = " << params[offset++] << ";\n";
    fout << "int BISHOP_PAIR = " << params[offset++] << ";\n";
    fout << "int TRAPPED_BISHOP_PENALTY = " << params[offset++] << ";\n";
    fout << "int FIANCHETTO_BONUS = " << params[offset++] << ";\n";
    fout << "int BISHOP_MOBILITY = " << params[offset++] << ";\n";
    fout << "int BLOCKED_BISHOP_PENALTY = " << params[offset++] << ";\n";
    fout << "int ROOK_ON_QUEEN_FILE = " << params[offset++] << ";\n";
    fout << "int ROOK_ON_OPEN_FILE = " << params[offset++] << ";\n";
    fout << "int ROOK_PAWN_CONST = " << params[offset++] << ";\n";
    fout << "int ROOK_ON_SEVENTH = " << params[offset++] << ";\n";
    fout << "int ROOKS_DEF_EACH_OTHER = " << params[offset++] << ";\n";
    fout << "int ROOK_MOBILITY = " << params[offset++] << ";\n";
    fout << "int BLOCKED_ROOK_PENALTY = " << params[offset++] << ";\n";
    fout << "int EARLY_QUEEN_DEVELOPMENT = " << params[offset++] << ";\n";
    fout << "int QUEEN_MOBILITY = " << params[offset++] << ";\n";
    fout << "int DOUBLED_PAWNS_PENALTY = " << params[offset++] << ";\n";
    fout << "int WEAK_PAWN_PENALTY = " << params[offset++] << ";\n";
    fout << "int C_PAWN_PENALTY = " << params[offset++] << ";\n";
    fout << "int TEMPO_BONUS = " << params[offset++] << ";\n";

    fout.close();
}

double Sigmoid(double ev) {
    return 1.0 / (1.0 + exp(-ev / 400.0));
}

double loss(double ev, double res) {
    double evBounded = (ev > 500 ? 500 : (ev < -500 ? -500 : ev)) / 500.0;
    return (evBounded - res) * (evBounded - res);
}

vector<int> gradientDescent(vector<int> initialGuess) {
    const int nParams = initialGuess.size();
    vector<int> bestParValues = initialGuess;
    int numIterations = 10;
    double learningRate = 0.1;

    int* MG_KING_TABLE[64], *EG_KING_TABLE[64],
        *QUEEN_TABLE[64], *ROOK_TABLE[64], *BISHOP_TABLE[64], 
        *KNIGHT_TABLE[64], *MG_PAWN_TABLE[64], *EG_PAWN_TABLE[64], *PASSED_PAWN_TABLE[64],
        *KING_SHIELD[3], *PIECE_VALUES[7], *PIECE_ATTACK_WEIGHT[6],
        *KNGIHT_MOBILITY, *KNIGHT_PAWN_CONST, *TRAPPED_KNIGHT_PENALTY,
        *KNIGHT_DEF_BY_PAWN, *BLOCKING_C_KNIGHT, *KNIGHT_PAIR_PENALTY, 
        *BISHOP_PAIR, *TRAPPED_BISHOP_PENALTY, *FIANCHETTO_BONUS, 
        *BISHOP_MOBILITY, *BLOCKED_BISHOP_PENALTY,
        *ROOK_ON_QUEEN_FILE, *ROOK_ON_OPEN_FILE, *ROOK_PAWN_CONST,
        *ROOK_ON_SEVENTH, *ROOKS_DEF_EACH_OTHER, *ROOK_MOBILITY,
        *BLOCKED_ROOK_PENALTY,
        *EARLY_QUEEN_DEVELOPMENT, *QUEEN_MOBILITY,
        *DOUBLED_PAWNS_PENALTY, *WEAK_PAWN_PENALTY, *C_PAWN_PENALTY,
        *TEMPO_BONUS;

    int offset = 0;
    for(int i = 0; i < 64; i++) MG_KING_TABLE[i] = &bestParValues[offset++];
    for(int i = 0; i < 64; i++) EG_KING_TABLE[i] = &bestParValues[offset++];
    for(int i = 0; i < 64; i++) QUEEN_TABLE[i] = &bestParValues[offset++];
    for(int i = 0; i < 64; i++) ROOK_TABLE[i] = &bestParValues[offset++];
    for(int i = 0; i < 64; i++) BISHOP_TABLE[i] = &bestParValues[offset++];
    for(int i = 0; i < 64; i++) KNIGHT_TABLE[i] = &bestParValues[offset++];
    for(int i = 0; i < 64; i++) MG_PAWN_TABLE[i] = &bestParValues[offset++];
    for(int i = 0; i < 64; i++) EG_PAWN_TABLE[i] = &bestParValues[offset++];
    for(int i = 0; i < 64; i++) PASSED_PAWN_TABLE[i] = &bestParValues[offset++];

    for(int i = 0; i < 3; i++) KING_SHIELD[i] = &bestParValues[offset++];
    for(int i = 0; i < 7; i++) PIECE_VALUES[i] = &bestParValues[offset++];
    for(int i = 0; i < 6; i++) PIECE_ATTACK_WEIGHT[i] = &bestParValues[offset++];

    KNGIHT_MOBILITY = &bestParValues[offset++];
    KNIGHT_PAWN_CONST = &bestParValues[offset++];
    TRAPPED_KNIGHT_PENALTY = &bestParValues[offset++];
    KNIGHT_DEF_BY_PAWN = &bestParValues[offset++];
    BLOCKING_C_KNIGHT = &bestParValues[offset++];
    KNIGHT_PAIR_PENALTY = &bestParValues[offset++];
    BISHOP_PAIR = &bestParValues[offset++];
    TRAPPED_BISHOP_PENALTY = &bestParValues[offset++];
    FIANCHETTO_BONUS = &bestParValues[offset++];
    BISHOP_MOBILITY = &bestParValues[offset++];
    BLOCKED_BISHOP_PENALTY = &bestParValues[offset++];
    ROOK_ON_QUEEN_FILE = &bestParValues[offset++];
    ROOK_ON_OPEN_FILE = &bestParValues[offset++];
    ROOK_PAWN_CONST = &bestParValues[offset++];
    ROOK_ON_SEVENTH = &bestParValues[offset++];
    ROOKS_DEF_EACH_OTHER = &bestParValues[offset++];
    ROOK_MOBILITY = &bestParValues[offset++];
    BLOCKED_ROOK_PENALTY = &bestParValues[offset++];
    EARLY_QUEEN_DEVELOPMENT = &bestParValues[offset++];
    QUEEN_MOBILITY = &bestParValues[offset++];
    DOUBLED_PAWNS_PENALTY = &bestParValues[offset++];
    WEAK_PAWN_PENALTY = &bestParValues[offset++];
    C_PAWN_PENALTY = &bestParValues[offset++];
    TEMPO_BONUS = &bestParValues[offset++];

    vector<double> gradients(nParams, 0.0);

    for(int iteration = 0; iteration < numIterations; iteration++) {
        std::cout << "Iteration " << iteration << " started.\n";
        for(int paramIdx = 592; paramIdx < nParams; paramIdx++) {
            std::cout << "Calculating gradient for param " << paramIdx << "... ";
            std::cout.flush();

            bestParValues[paramIdx] += 1;
            int loss1 = 0;

            for(pair<string, double> &p: positions) {
                board.loadFenPos(p.first);

                double ev = evaluate(false, 
                    *MG_KING_TABLE, *EG_KING_TABLE,
                    *QUEEN_TABLE, *ROOK_TABLE, *BISHOP_TABLE, 
                    *KNIGHT_TABLE, *MG_PAWN_TABLE, *EG_PAWN_TABLE, *PASSED_PAWN_TABLE,
                    *KING_SHIELD, *PIECE_VALUES, *PIECE_ATTACK_WEIGHT,
                    *KNGIHT_MOBILITY, *KNIGHT_PAWN_CONST, *TRAPPED_KNIGHT_PENALTY,
                    *KNIGHT_DEF_BY_PAWN, *BLOCKING_C_KNIGHT, *KNIGHT_PAIR_PENALTY, 
                    *BISHOP_PAIR, *TRAPPED_BISHOP_PENALTY, *FIANCHETTO_BONUS, 
                    *BISHOP_MOBILITY, *BLOCKED_BISHOP_PENALTY,
                    *ROOK_ON_QUEEN_FILE, *ROOK_ON_OPEN_FILE, *ROOK_PAWN_CONST,
                    *ROOK_ON_SEVENTH, *ROOKS_DEF_EACH_OTHER, *ROOK_MOBILITY,
                    *BLOCKED_ROOK_PENALTY,
                    *EARLY_QUEEN_DEVELOPMENT, *QUEEN_MOBILITY,
                    *DOUBLED_PAWNS_PENALTY, *WEAK_PAWN_PENALTY, *C_PAWN_PENALTY,
                    *TEMPO_BONUS);

                if(board.turn == Black) ev *= -1;

                assert(p.second == 1.0 || p.second == 0.0 || p.second == 0.5);

                loss1 += loss(ev, p.second);
            }

            bestParValues[paramIdx] -= 2;
            int loss2 = 0;

            for(pair<string, double> &p: positions) {
                board.loadFenPos(p.first);

                double ev = evaluate(false, 
                    *MG_KING_TABLE, *EG_KING_TABLE,
                    *QUEEN_TABLE, *ROOK_TABLE, *BISHOP_TABLE, 
                    *KNIGHT_TABLE, *MG_PAWN_TABLE, *EG_PAWN_TABLE, *PASSED_PAWN_TABLE,
                    *KING_SHIELD, *PIECE_VALUES, *PIECE_ATTACK_WEIGHT,
                    *KNGIHT_MOBILITY, *KNIGHT_PAWN_CONST, *TRAPPED_KNIGHT_PENALTY,
                    *KNIGHT_DEF_BY_PAWN, *BLOCKING_C_KNIGHT, *KNIGHT_PAIR_PENALTY, 
                    *BISHOP_PAIR, *TRAPPED_BISHOP_PENALTY, *FIANCHETTO_BONUS, 
                    *BISHOP_MOBILITY, *BLOCKED_BISHOP_PENALTY,
                    *ROOK_ON_QUEEN_FILE, *ROOK_ON_OPEN_FILE, *ROOK_PAWN_CONST,
                    *ROOK_ON_SEVENTH, *ROOKS_DEF_EACH_OTHER, *ROOK_MOBILITY,
                    *BLOCKED_ROOK_PENALTY,
                    *EARLY_QUEEN_DEVELOPMENT, *QUEEN_MOBILITY,
                    *DOUBLED_PAWNS_PENALTY, *WEAK_PAWN_PENALTY, *C_PAWN_PENALTY,
                    *TEMPO_BONUS);

                if(board.turn == Black) ev *= -1;

                assert(p.second == 1.0 || p.second == 0.0 || p.second == 0.5);

                loss2 += loss(ev, p.second);
            }

            gradients[paramIdx] = (loss1 - loss2) / 2;
            bestParValues[paramIdx] += 1; // restore

            std::cout << "Done. Gradient: " << gradients[paramIdx] << "\n";
            std::cout.flush();
        }

        // Update the parameters using the gradients and learning rate
        std::cout << "Updating parameters...\n";
        for (int paramIdx = 0; paramIdx < nParams; paramIdx++) {
            bestParValues[paramIdx] -= static_cast<int>(round(learningRate * gradients[paramIdx]));
        }

        int currLoss = 0;
        for(pair<string, double> &p: positions) {
            board.loadFenPos(p.first);

            double ev = evaluate(false, 
                *MG_KING_TABLE, *EG_KING_TABLE,
                *QUEEN_TABLE, *ROOK_TABLE, *BISHOP_TABLE, 
                *KNIGHT_TABLE, *MG_PAWN_TABLE, *EG_PAWN_TABLE, *PASSED_PAWN_TABLE,
                *KING_SHIELD, *PIECE_VALUES, *PIECE_ATTACK_WEIGHT,
                *KNGIHT_MOBILITY, *KNIGHT_PAWN_CONST, *TRAPPED_KNIGHT_PENALTY,
                *KNIGHT_DEF_BY_PAWN, *BLOCKING_C_KNIGHT, *KNIGHT_PAIR_PENALTY, 
                *BISHOP_PAIR, *TRAPPED_BISHOP_PENALTY, *FIANCHETTO_BONUS, 
                *BISHOP_MOBILITY, *BLOCKED_BISHOP_PENALTY,
                *ROOK_ON_QUEEN_FILE, *ROOK_ON_OPEN_FILE, *ROOK_PAWN_CONST,
                *ROOK_ON_SEVENTH, *ROOKS_DEF_EACH_OTHER, *ROOK_MOBILITY,
                *BLOCKED_ROOK_PENALTY,
                *EARLY_QUEEN_DEVELOPMENT, *QUEEN_MOBILITY,
                *DOUBLED_PAWNS_PENALTY, *WEAK_PAWN_PENALTY, *C_PAWN_PENALTY,
                *TEMPO_BONUS);

            if(board.turn == Black) ev *= -1;

            assert(p.second == 1.0 || p.second == 0.0 || p.second == 0.5);

            currLoss += loss(ev, p.second);
        }
        std::cout << "Current loss: " << currLoss << "\n";

    }
    return bestParValues;
}

int main(int argc, char **argv) {
    if(argc != 4) {
        std::cout << "Wrong number of arguments\n";
        return 0;
    }

    init();
    createPosVector(argv[1], atoi(argv[3]));

    vector<int> par = {
    40, 50, 30, 10, 10, 30, 50, 40,
    30, 40, 20, 0, 0, 20, 40, 30,
    10, 20, 0, -20, -20, 0, 20, 10,
    0, 10, -10, -30, -30, -10, 10, 0,
    -10, 0, -20, -40, -40, -20, 0, -10,
    -20, -10, -30, -50, -50, -30, -10, -20,
    -30, -20, -40, -60, -60, -40, -20, -30,
    -40, -30, -50, -70, -70, -50, -30, -40,
    -72, -48, -36, -24, -24, -36, -48, -72,
    -48, -24, -12, 0, 0, -12, -24, -48,
    -36, -12, 0, 12, 12, 0, -12, -36,
    -24, 0, 12, 24, 24, 12, 0, -24,
    -24, 0, 12, 24, 24, 12, 0, -24,
    -36, -12, 0, 12, 12, 0, -12, -36,
    -48, -24, -12, 0, 0, -12, -24, -48,
    -72, -48, -36, -24, -24, -36, -48, -72,
    -5, -5, -5, -5, -5, -5, -5, -5,
    0, 0, 1, 1, 1, 1, 0, 0,
    0, 0, 1, 2, 2, 1, 0, 0,
    0, 0, 2, 3, 3, 2, 0, 0,
    0, 0, 2, 3, 3, 2, 0, 0,
    0, 0, 1, 2, 2, 1, 0, 0,
    0, 0, 1, 1, 1, 1, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 2, 2, 0, 0, 0,
    -5, 0, 0, 0, 0, 0, 0, -5,
    -5, 0, 0, 0, 0, 0, 0, -5,
    -5, 0, 0, 0, 0, 0, 0, -5,
    -5, 0, 0, 0, 0, 0, 0, -5,
    -5, 0, 0, 0, 0, 0, 0, -5,
    -5, 0, 0, 0, 0, 0, 0, -5,
    5, 5, 5, 5, 5, 5, 5, 5,
    -4, -4, -12, -4, -4, -12, -4, -4,
    -4, 2, 1, 1, 1, 1, 2, -4,
    -4, 0, 2, 4, 4, 2, 0, -4,
    -4, 0, 4, 6, 6, 4, 0, -4,
    -4, 0, 4, 6, 6, 4, 0, -4,
    -4, 1, 2, 4, 4, 2, 1, -4,
    -4, 0, 0, 0, 0, 0, 0, -4,
    -4, -4, -4, -4, -4, -4, -4, -4,
    -8, -12, -8, -8, -8, -8, -12, -8,
    -8, 0, 0, 0, 0, 0, 0, -8,
    -8, 0, 4, 4, 4, 4, 0, -8,
    -8, 0, 4, 8, 8, 4, 0, -8,
    -8, 0, 4, 8, 8, 4, 0, -8,
    -8, 0, 4, 4, 4, 4, 0, -8,
    -8, 0, 1, 2, 2, 1, 0, -8,
    -8, -8, -8, -8, -8, -8, -8, -8,
    0, 0, 0, 0, 0, 0, 0, 0,
    -6, -4, 1, -24, -24, 1, -4, -6,
    -4, -4, 1, 5, 5, 1, -4, -4,
    -6, -4, 5, 10, 10, 5, -4, -6,
    -6, -4, 2, 8, 8, 2, -4, -6,
    -6, -4, 1, 2, 2, 1, -4, -6,
    -6, -4, 1, 1, 1, 1, -4, -6,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    20, 20, 20, 20, 20, 20, 20, 20,
    20, 20, 20, 20, 20, 20, 20, 20,
    40, 40, 40, 40, 40, 40, 40, 40,
    60, 60, 60, 60, 60, 60, 60, 60,
    80, 80, 80, 80, 80, 80, 80, 80,
    100, 100, 100, 100, 100, 100, 100, 100,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    20, 20, 20, 20, 20, 20, 20, 20,
    20, 20, 20, 20, 20, 20, 20, 20,
    40, 40, 40, 40, 40, 40, 40, 40,
    60, 60, 60, 60, 60, 60, 60, 60,
    80, 80, 80, 80, 80, 80, 80, 80,
    100, 100, 100, 100, 100, 100, 100, 100,
    0, 0, 0, 0, 0, 0, 0, 0,
    5, 10, 5,
    0, 100, 325, 350, 500, 975, 0,
    0, 0, 2, 2, 3, 5,
    4, 3, 100, 15, 30, 20, 50, 100, 20, 5, 50, 10, 20, 3, 30, 5, 3, 50, 20, 2, 40, 15, 25, 10};


    vector<int> newPar = gradientDescent(par);

    printParams(newPar, argv[2]);

    return 0;
}