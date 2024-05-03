#include "train.h"
#include "../engine/Enums.h"
#include <string>
#include <cassert>
#include <unordered_map>
#include <vector>
#include <fstream>
#include <iostream>

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

// function to split string into words
std::vector<std::string> splitStr(std::string s) {
    std::vector<std::string> ans;
    for(char c: s) {
        if(c == ' ' || !ans.size()) ans.push_back("");
        else ans.back() += c;
    }
    return ans;
}

int get_half_kp_index(Color perspective, int kingSquare, int pieceSquare, int pieceType, Color pieceColor) {
    const int pIdx = (pieceType) * 2 + (pieceColor != perspective);
    const int orientedKingSquare = (perspective == White ? kingSquare : FLIPPED[kingSquare]);
    const int orientedPieceSquare = (perspective == White ? pieceSquare : FLIPPED[pieceSquare]);

    return orientedPieceSquare + (pIdx + orientedKingSquare * 10) * 64;
}

std::vector<TrainingDataEntry> read_entries(std::string file_name) {
    std::vector<TrainingDataEntry> entries;

    std::ifstream fin(file_name);

    std::string line;
    int idx = 0;
    const int MAX_IDX = 10000000; 
    const int PERCENT = MAX_IDX / 100;
    while(std::getline(fin, line) && idx < MAX_IDX) {
        if(idx % PERCENT == 0) std::cout << "Processing entries from " << file_name << ": " << idx << " / " << MAX_IDX << "\r";

        std::vector<std::string> words = splitStr(line);

        std::string board_string = words[0];
        Color stm = (words[1] == "w" ? White : Black);
        int eval_cp = stoi(words[7]);

        entries.push_back({ board_string, (stm == White ? 1 : 0), eval_cp });

        idx++;
    }
    std::cout << "\nProcessed " << entries.size() << " entries.\n";

    fin.close();

    return entries;
}

// the arrays are of the form [pos, feature, pos, feature...]
// they represent pairs of {pos, feature} indices that need to be kept ordered
// so we do insertion sort when adding a new index
void insert_sort(int* array, int pos) {
    int position_index = array[pos-1];
    int curr_pos = pos - 2;
    while(curr_pos >= 0 && array[curr_pos - 1] == position_index && array[curr_pos] > array[curr_pos + 2]) {
        array[curr_pos] ^= array[curr_pos + 2];
        array[curr_pos + 2] ^= array[curr_pos];
        array[curr_pos] ^= array[curr_pos + 2];
        curr_pos -= 2;
    }
}

const int MAX_ACTIVE_FEATURES = 32;

void fill(struct SparseBatch *batch, const std::vector<TrainingDataEntry>& entries) {
    std::unordered_map<char, int> pieceSymbols = {{'p', Pawn}, {'n', Knight},
    {'b', Bishop}, {'r', Rook}, {'q', Queen}, {'k', King}};

    int pos_index = 0;

    const int SIZE = entries.size(); 
    const int PERCENT = SIZE / 100;

    for(const TrainingDataEntry &t: entries) {
        if(pos_index % PERCENT == 0) std::cout << "Filling entries: " << pos_index << " / " << SIZE << "\r";

        int whiteKingSquare = -1;
        int blackKingSquare = -1;

        int file = 0, rank = 7;
        for(const char &p: t.board_string) {
            if(p == '/') {
                rank--;
                file = 0;
            } else {
                if(p-'0' >= 1 && p-'0' <= 8) {
                    for(char i = 0; i < (p-'0'); i++) {
                        file++;
                    }
                } else {
                    int color = ((p >= 'A' && p <= 'Z') ? White : Black);
                    int type = pieceSymbols[tolower(p)];

                    if(type == King) {
                        if(color == White) whiteKingSquare = rank * 8 + file;
                        if(color == Black) blackKingSquare = FLIPPED[rank * 8 + file];
                    }

                    file++;
                }
            }
        }

        if(whiteKingSquare == -1 || blackKingSquare == -1) {
            batch->size--;
            continue;
        }

        batch->stm[pos_index] = (float)t.stm;
        batch->score[pos_index] = (float)t.eval_cp;

        file = 0, rank = 7;
        for(const char &p: t.board_string) {
            if(p == '/') {
                rank--;
                file = 0;
            } else {
                if(p-'0' >= 1 && p-'0' <= 8) {
                    for(char i = 0; i < (p-'0'); i++) {
                        file++;
                    }
                } else {
                    int color = ((p >= 'A' && p <= 'Z') ? White : Black);
                    int type = pieceSymbols[tolower(p)];

                    if(type == King) {
                        file++;
                        continue;
                    }

                    int white_piece_sq = rank * 8 + file;
                    int black_piece_sq = FLIPPED[white_piece_sq];

                    int white_idx = get_half_kp_index(White, whiteKingSquare, white_piece_sq, type, (Color)color);
                    int black_idx = get_half_kp_index(Black, blackKingSquare, black_piece_sq, type, (Color)color);

                    batch->white_features_indices[2 * batch->num_active_white_features] = pos_index;
                    batch->white_features_indices[2 * batch->num_active_white_features + 1] = white_idx;
                    insert_sort(batch->white_features_indices, 2 * batch->num_active_white_features + 1);
                    batch->num_active_white_features++;

                    batch->black_features_indices[2 * batch->num_active_black_features] = pos_index;
                    batch->black_features_indices[2 * batch->num_active_black_features + 1] = black_idx;
                    insert_sort(batch->black_features_indices, 2 * batch->num_active_black_features + 1);
                    batch->num_active_black_features++;

                    file++;
                }
            }
        }
        pos_index++;
    }

    std::cout << "\nFilled entries. Batch size = " << batch->size << '\n';
}

struct SparseBatch* CreateSparseBatch(const char* file_name) {
    struct SparseBatch* batch = new SparseBatch();

    std::vector<TrainingDataEntry> entries = read_entries(file_name);

    // The number of positions in the batch
    batch->size = entries.size();

    // The total number of white/black active features in the whole batch.
    batch->num_active_white_features = 0;
    batch->num_active_black_features = 0;

    // The side to move for each position. 1 for white, 0 for black.
    // Required for ordering the accumulator slices in the forward pass.
    batch->stm = new float[batch->size];

    // The score for each position. This is the value that we will be teaching the network.
    batch->score = new float[batch->size];

    // The indices of the active features.
    // Why is the size * 2?! The answer is that the indices are 2 dimensional
    // (position_index, feature_index). It's effectively a matrix of size
    // (num_active_*_features, 2).
    // IMPORTANT: We must make sure that the indices are in ascending order.
    // That is first comes the first position, then second, then third,
    // and so on. And within features for one position the feature indices
    // are also in ascending order. Why this is needed will be apparent later.
    batch->white_features_indices = new int[batch->size * MAX_ACTIVE_FEATURES * 2];
    batch->black_features_indices = new int[batch->size * MAX_ACTIVE_FEATURES * 2];

    fill(batch, entries);

    return batch;
};

void DeleteSparseBatch(struct SparseBatch* batch) {
    if (batch != nullptr) {
        delete[] batch->stm;
        delete[] batch->score;
        delete[] batch->white_features_indices;
        delete[] batch->black_features_indices;
    }
}