#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <math.h>
#include <unordered_map>
#include <iomanip>
#include <algorithm>
#include <random>

#include "train_eval.h"
#include "../engine/Board.h"

const double EPS = 1e-5;

extern const int NUM_PARAMS;

using namespace std;

vector<pair<string, double> > positions;

void createPosVector(string input_file, int num) {
    ifstream fin(input_file);

    string str;
    std::cout << "Parsing fens... ";
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
    std::cout << "Done. Have " << positions.size() << " positions.\n";

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

    fout << "int PIECE_VALUES[7] = {\n   ";
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
    return (Sigmoid(ev) - res) * (Sigmoid(ev) - res);
}

double lossPrime(double ev, double res) {
    return 2.0 * (Sigmoid(ev) - res) * Sigmoid(ev) * (1.0 - Sigmoid(ev));
}

vector<int> randomVector(int size) {
    std::random_device rd;  
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, 100);

    std::vector<int> randomVector(size);

    for (int i = 0; i < size; ++i) {
        randomVector[i] = dist(gen);
    }

    return randomVector;
}

vector<int> gradientDescent(vector<int> initialGuess) {
    assert(initialGuess.size() == NUM_PARAMS);

    vector<int> bestParValues = initialGuess;
    vector<int> currParValues = initialGuess;

    int numEpochs = 1000;
    double learningRate = 300;
    const double lrDecayFactor = 0.8;
    const int lrPatience = 10;
    const int patience = 30;
    int epochsFromLRReduce = 0;
    int epochsWithoutImprovement = 0;
    double bestLoss = 1e9;
    const int batch_size = 512;
    const double MIN_LR = 100;
    const int trainSize = (positions.size() * 8) / 10;
    const int valSize = positions.size() - trainSize;
    int batches = (positions.size() + batch_size - 1) / batch_size;

    int *MG_KING_TABLE[64], *EG_KING_TABLE[64],
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
    for(int i = 0; i < 64; i++) MG_KING_TABLE[i] = &currParValues[offset++];
    for(int i = 0; i < 64; i++) EG_KING_TABLE[i] = &currParValues[offset++];
    for(int i = 0; i < 64; i++) QUEEN_TABLE[i] = &currParValues[offset++];
    for(int i = 0; i < 64; i++) ROOK_TABLE[i] = &currParValues[offset++];
    for(int i = 0; i < 64; i++) BISHOP_TABLE[i] = &currParValues[offset++];
    for(int i = 0; i < 64; i++) KNIGHT_TABLE[i] = &currParValues[offset++];
    for(int i = 0; i < 64; i++) MG_PAWN_TABLE[i] = &currParValues[offset++];
    for(int i = 0; i < 64; i++) EG_PAWN_TABLE[i] = &currParValues[offset++];
    for(int i = 0; i < 64; i++) PASSED_PAWN_TABLE[i] = &currParValues[offset++];

    for(int i = 0; i < 3; i++) KING_SHIELD[i] = &currParValues[offset++];
    for(int i = 0; i < 7; i++) PIECE_VALUES[i] = &currParValues[offset++];
    for(int i = 0; i < 6; i++) PIECE_ATTACK_WEIGHT[i] = &currParValues[offset++];

    KNGIHT_MOBILITY = &currParValues[offset++];
    KNIGHT_PAWN_CONST = &currParValues[offset++];
    TRAPPED_KNIGHT_PENALTY = &currParValues[offset++];
    KNIGHT_DEF_BY_PAWN = &currParValues[offset++];
    BLOCKING_C_KNIGHT = &currParValues[offset++];
    KNIGHT_PAIR_PENALTY = &currParValues[offset++];
    BISHOP_PAIR = &currParValues[offset++];
    TRAPPED_BISHOP_PENALTY = &currParValues[offset++];
    FIANCHETTO_BONUS = &currParValues[offset++];
    BISHOP_MOBILITY = &currParValues[offset++];
    BLOCKED_BISHOP_PENALTY = &currParValues[offset++];
    ROOK_ON_QUEEN_FILE = &currParValues[offset++];
    ROOK_ON_OPEN_FILE = &currParValues[offset++];
    ROOK_PAWN_CONST = &currParValues[offset++];
    ROOK_ON_SEVENTH = &currParValues[offset++];
    ROOKS_DEF_EACH_OTHER = &currParValues[offset++];
    ROOK_MOBILITY = &currParValues[offset++];
    BLOCKED_ROOK_PENALTY = &currParValues[offset++];
    EARLY_QUEEN_DEVELOPMENT = &currParValues[offset++];
    QUEEN_MOBILITY = &currParValues[offset++];
    DOUBLED_PAWNS_PENALTY = &currParValues[offset++];
    WEAK_PAWN_PENALTY = &currParValues[offset++];
    C_PAWN_PENALTY = &currParValues[offset++];
    TEMPO_BONUS = &currParValues[offset++];

    for(int i = 0; i < NUM_PARAMS; i++) freq[i] = 0;

    vector<double> finalGradients(NUM_PARAMS, 0);
    std::random_device rd;
    std::mt19937 rng(rd());

    // Shuffle positions
    std::shuffle(positions.begin(), positions.end(), rng);

    // split into train and val
    vector<pair<string, double> > trainPositions(positions.begin(), positions.begin() + trainSize);
    vector<pair<string, double> > valPositions(positions.begin() + trainSize, positions.end());
    assert(trainPositions.size() == trainSize && valPositions.size() == valSize);

    for(int epoch = 0; epoch < numEpochs; epoch++) {
        std::cout << "Epoch " << epoch + 1 << "/" << numEpochs << ": ";
        std::cout.flush();
        double currLoss = 0;


        for(int batch = 0; batch < batches; batch++) {
            int batch_start_idx = batch * batch_size;
            // set gradients vector to 0
            for(int i = 0; i < NUM_PARAMS; i++) finalGradients[i] = 0;

            for(int posIdx = batch_start_idx; posIdx < min((int)trainPositions.size(), batch_start_idx + batch_size); posIdx++) {
                assert(posIdx < trainPositions.size());
                pair<string, double> p = trainPositions[posIdx];
                board.loadFenPos(p.first);

                double ev = trainEvaluate(false, 
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

                currLoss += loss(ev, p.second);

                // compute gradients
                double lossPrimeVal = lossPrime(ev, p.second);
                for(int i = 0; i < NUM_PARAMS; i++) {
                    finalGradients[i] += lossPrimeVal * gradients[i] / (double)(batch_size);
                }

                // Update the parameters using the gradients and learning rate
                for (int paramIdx = 64 * 8; paramIdx < 64 * 9; paramIdx++) {
                    currParValues[paramIdx] -= (int)(round(learningRate * finalGradients[paramIdx]));
                }
            }
        }

        // compute validation loss
        double valLoss = 0;
        for(auto &p: valPositions) {
            board.loadFenPos(p.first);
            double ev = trainEvaluate(false, 
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
            valLoss += loss(ev, p.second);
        }

        // if(epoch == 0) {
        //     printParams(freq, "freq.txt");
        // }

        std::cout << "train_loss=" << fixed << setprecision(5) << currLoss / (double)(trainPositions.size()) << ", " 
                  << "val_loss=" << fixed << setprecision(5) << valLoss / (double)(valPositions.size()) << ", "
                  << "lr=" << fixed << setprecision(2) << learningRate << "\n";
        std::cout.flush();

        // update lr
        if ((bestLoss - valLoss) > EPS) {
            bestLoss = valLoss;
            bestParValues = currParValues;
            std::cout << "Found new bestLoss=" << fixed << setprecision(5) << bestLoss / (double)valPositions.size() << " printing params...\n";
            std::cout.flush();
            printParams(bestParValues, "best_params.txt");
            epochsFromLRReduce = 0;
            epochsWithoutImprovement = 0;
        } else {
            epochsFromLRReduce++;
            epochsWithoutImprovement++;
            if (epochsFromLRReduce >= lrPatience) {
                learningRate = (learningRate * lrDecayFactor > MIN_LR ? learningRate * lrDecayFactor : MIN_LR);
                epochsFromLRReduce = 0;
            }

            // return early when there is no improvement for a while
            if (epochsWithoutImprovement >= patience) return bestParValues;
        }
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

    vector<int> par = //randomVector(NUM_PARAMS);
    {
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