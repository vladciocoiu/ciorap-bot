#include <algorithm>
#include <chrono>
#include <math.h>
#include <unordered_map>
#include <iostream>
#include <stack>
#include <cassert>

#include "Evaluate.h"
#include "Board.h"
#include "Search.h"
#include "TranspositionTable.h"
#include "MagicBitboardUtils.h"
#include "UCI.h"
#include "MoveUtils.h"
#include "BoardUtils.h"
#include "Enums.h"
// #include "See.h"

using namespace std;

int killerMoves[256][2];
int history[16][64];
const int HISTORY_MAX = 1e8;

const int INF = 1000000;

const int MATE_EVAL = INF-1;
const int MATE_THRESHOLD = MATE_EVAL/2;

long long startTime, stopTime;
short maxDepth = 256;
bool infiniteTime;

int nodesSearched = 0;
int nodesQ = 0;
bool timeOver = false;

const int N = 256;
int pvArray[(N * N + N) / 2];

void copyPv(int* dest, int* src, int n) {
   while (n-- && (*dest++ = *src++));
}

void showPV(int depth) {
    assert(depth <= N);

    cout << "pv ";
    for(int i = 0; i < depth; i++) {
        if(pvArray[i] == MoveUtils::NO_MOVE) break;
        cout << BoardUtils::moveToString(pvArray[i]) << ' ';
    }
    cout << '\n';
}


// killer moves are quiet moves that cause a beta cutoff and are used for sorting purposes
void storeKiller(short ply, int move) {
    // make sure the moves are different
    if(killerMoves[ply][0] != move) 
        killerMoves[ply][1] = killerMoves[ply][0];

    killerMoves[ply][0] = move;
}


// same as killer moves, but they are saved based on their squares and color
void updateHistory(int move, int depth) {
    int bonus = depth * depth;

    history[(MoveUtils::getColor(move) | MoveUtils::getPiece(move))][MoveUtils::getToSq(move)] += 2 * bonus;
    for(int pc = 0; pc < 16; pc++) {
        for(int sq = 0; sq < 64; sq++) {
            history[pc][sq] -= bonus;
        }
    }

    // cap history points at HISTORY_MAX
    if(history[(MoveUtils::getColor(move) | MoveUtils::getPiece(move))][MoveUtils::getToSq(move)] > HISTORY_MAX) {
        for(int pc = 0; pc < 16; pc++) {
            for(int sq = 0; sq < 64; sq++) {
                history[pc][sq] /= 2;
            }
        }
    }
}

void clearHistory() {
    for(int pc = 0; pc < 16; pc++) {
        for(int sq = 0; sq < 64; sq++) {
            history[pc][sq] = 0;
        }
    }
}

void ageHistory() {
    for(int pc = 0; pc < 16; pc++) {
        for(int sq = 0; sq < 64; sq++) {
            history[pc][sq] /= 8;
        }
    }
}

int captureScore(int move) {
    int score = 0;

    // give huge score boost to captures of the last moved piece
    if(!board.moveStk.empty() && MoveUtils::getToSq(move) == MoveUtils::getToSq(board.moveStk.top())) score += 2000;

    // captured piece value - capturing piece value
    if(MoveUtils::isCapture(move)) score += (PIECE_VALUES[MoveUtils::getCapturedPiece(move)]-
                  PIECE_VALUES[MoveUtils::getPiece(move)]);

    // material gained by promotion
    if(MoveUtils::isPromotion(move)) score += PIECE_VALUES[MoveUtils::getPromotionPiece(move)] - PIECE_VALUES[Pawn];

    return score;
}

int nonCaptureScore(int move) {
    // start with history score
    int score = history[(MoveUtils::getColor(move) | MoveUtils::getPiece(move))][MoveUtils::getToSq(move)];

    // if it is a promotion add huge bonus so that it is searched first
    if(MoveUtils::isPromotion(move)) score += 100000000;

    assert(score <= 1e9);
    return score;
}

bool cmpCapturesInv(int a, int b) {
    return (captureScore(a) < captureScore(b));
}

bool cmpCaptures(int a, int b) {
    return (captureScore(a) > captureScore(b));
}

bool cmpNonCaptures(int a, int b) {
    return nonCaptureScore(a) > nonCaptureScore(b);
}

// --- MOVE ORDERING --- 
void sortMoves(int *moves, int num, short ply) {
    // sorting in quiescence search
      if(ply == -1) {
          sort(moves, moves+num, cmpCaptures);
          return;
      }

    int captures[256], nonCaptures[256];
    unsigned int nCaptures = 0, nNonCaptures = 0;

    // find pv move
    int pvIndex = ply * (2 * N + 1 - ply) / 2;
    int pvMoveLegal = false;
    int pvMove = pvArray[pvIndex];

    // find hash move
    int hashMove = transpositionTable.retrieveBestMove();

    // check legality of killer moves
    bool killerLegal[2] = {false, false};
    for(int idx = 0; idx < num; idx++) {
        if(killerMoves[ply][0] == moves[idx]) killerLegal[0] = true;
        if(killerMoves[ply][1] == moves[idx]) killerLegal[1] = true;
        if(pvMove == moves[idx]) pvMoveLegal = true;
    }
    assert(pvMoveLegal || pvMove == MoveUtils::NO_MOVE);

    // split the other moves into captures and non captures for easier sorting
    for(int idx = 0; idx < num; idx++) {
        if((moves[idx] == pvMove) || (moves[idx] == killerMoves[ply][0]) || (moves[idx] == killerMoves[ply][1]) || (moves[idx] == hashMove))
            continue;

        if(MoveUtils::isCapture(moves[idx])) captures[nCaptures++] = moves[idx];
        else nonCaptures[nNonCaptures++] = moves[idx];
    }

    int newNum = 0; // size of sorted array

    // add pv move
    if(pvMove != MoveUtils::NO_MOVE) moves[newNum++] = pvMove;

    // add hash move if it is not the pv move
    if (hashMove != MoveUtils::NO_MOVE && hashMove != pvMove) moves[newNum++] = hashMove;
    
    // add captures sorted by MVV-LVA (only winning / equal captures first)
    sort(captures, captures + nCaptures, cmpCapturesInv); // descending order because we add them from the end
    while(nCaptures && captureScore(captures[nCaptures-1]) >= 0) {
        moves[newNum++] = captures[--nCaptures];
    }

    // add killer moves
    if(killerLegal[0] && (killerMoves[ply][0] != MoveUtils::NO_MOVE) && (killerMoves[ply][0] != pvMove) && (killerMoves[ply][0] != hashMove)) 
        moves[newNum++] = killerMoves[ply][0];
    if(killerLegal[1] && (killerMoves[ply][1] != MoveUtils::NO_MOVE) && (killerMoves[ply][1] != pvMove) && (killerMoves[ply][1] != hashMove)) 
        moves[newNum++] = killerMoves[ply][1];

    // add other quiet moves sorted
    sort(nonCaptures, nonCaptures + nNonCaptures, cmpNonCaptures);
    for(unsigned int idx = 0; idx < nNonCaptures; idx++)
        moves[newNum++] = nonCaptures[idx];

    // add losing captures
    while(nCaptures) {
        moves[newNum++] = captures[--nCaptures];
    }

    assert(newNum == num);
}

// --- QUIESCENCE SEARCH --- 
// only searching for captures at the end of a regular search in order to ensure the engine won't miss obvious tactics
int quiesce(int alpha, int beta) {
    if(!(nodesQ & 4095) && !infiniteTime) {
        long long currTime = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();
        if(currTime >= stopTime) timeOver = true;
    }
    if(timeOver) return 0;
    nodesQ++;

    if(board.isDraw()) return 0;

    int standPat = evaluate();
    if(standPat >= beta) return beta;

    alpha = max(alpha, standPat);

    int moves[256];
    int num = board.generateLegalMoves(moves);

    sortMoves(moves, num, -1);
    for(int idx = 0; idx < num; idx++)  {
        if(!MoveUtils::isCapture(moves[idx]) && !MoveUtils::isPromotion(moves[idx])) continue;

        // if(MoveUtils::isCapture(moves[idx]) && (seeMove(moves[idx]) < 0)) continue;

        // --- DELTA PRUNING --- 
        // we test if each move has the potential to raise alpha
        // if it doesn't, then the position is hopeless so searching deeper won't improve it
        int delta = standPat +  PIECE_VALUES[MoveUtils::getCapturedPiece(moves[idx])] + 200;
        if(MoveUtils::isPromotion(moves[idx])) delta += PIECE_VALUES[MoveUtils::getPromotionPiece(moves[idx])] - PIECE_VALUES[Pawn];

        const int ENDGAME_MATERIAL = 10;
        if((delta <= alpha) && (gamePhase() - MG_WEIGHT[MoveUtils::getCapturedPiece(moves[idx])] >= ENDGAME_MATERIAL)) continue;

        board.makeMove(moves[idx]);
        int score = -quiesce(-beta, -alpha);
        board.unmakeMove(moves[idx]);

        if(timeOver) return 0;

        if(score > alpha) {
            if(score >= beta) return beta;
            alpha = score;
        }
    }

    return alpha;
}

// alpha-beta algorithm with a fail-hard framework and PVS
int alphaBeta(int alpha, int beta, short depth, short ply, bool doNull) {
    assert(depth >= 0);
    assert(ply <= N);

    if(!(nodesSearched & 4095) && !infiniteTime) {
        long long currTime = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();
        if(currTime >= stopTime) timeOver = true;
    }
    if(timeOver) return 0;
    nodesSearched++;

    int pvIndex = ply * (2 * N + 1 - ply) / 2;
    int pvNextIndex = pvIndex + N - ply;

    memset(pvArray + pvIndex, MoveUtils::NO_MOVE, sizeof(int) * (pvNextIndex - pvIndex));


    int hashFlag = TranspositionTable::HASH_F_ALPHA;

    bool isInCheck = board.isInCheck();

    // --- MATE DISTANCE PRUNING --- 
    // if we find mate, we shouldn't look for a better move

    // lower bound
    int matedScore = - MATE_EVAL + ply;
    if(alpha < matedScore) {
        alpha = matedScore;
        if(beta <= matedScore) return matedScore;
    }

    // upper bound
    int mateScore = MATE_EVAL - ply;
    if(beta > mateScore) {
        beta = mateScore;
        if(alpha >= mateScore) return mateScore;
    }

    if(alpha >= beta) return alpha;

    if(board.isDraw()) return 0;

    bool isPV = (beta - alpha > 1);

    // retrieving the hashed move and evaluation if there is any
    int hashScore = transpositionTable.probeHash(depth, alpha, beta);
    if(hashScore != TranspositionTable::VAL_UNKNOWN) {
        // we return hashed info only if it is an exact hit in pv nodes
        if(!isPV || (hashScore > alpha && hashScore < beta)) {
            return hashScore;
        }
    }

    int moves[256];
    int num = board.generateLegalMoves(moves);
    if(num == 0) {
        if(isInCheck) return -mateScore; // checkmate
        return 0; // stalemate
    }


    if(depth <= 0) {
        int q = quiesce(alpha, beta);
        if(timeOver) return 0;
        return q;
    }
    int currBestMove = MoveUtils::NO_MOVE;

    // --- NULL MOVE PRUNING --- 
    // if our position is good, we can pass the turn to the opponent
    // and if that doesn't wreck our position, we don't need to search further
    const int ENDGAME_MATERIAL = 4;
    if(doNull && (!isPV) && (isInCheck == false) && ply && (depth > 3) && (gamePhase() >= ENDGAME_MATERIAL) && (evaluate() >= beta)) {
        board.makeMove(MoveUtils::NO_MOVE);

        short R = 3 + depth / 6;
        int score = -alphaBeta(-beta, -beta + 1, depth - R - 1, ply + 1, false);

        board.unmakeMove(MoveUtils::NO_MOVE);

        if(timeOver) return 0;
        if(score >= beta) return beta;
    }

    // decide if we can apply futility pruning
    // bool fPrune = false;
    // const int F_MARGIN[4] = { 0, 200, 300, 500 };
    // if (depth <= 3 && !isPV && !isInCheck && abs(alpha) < 9000 && evaluate() + F_MARGIN[depth] <= alpha)
    //     fPrune = true;

    int movesSearched = 0;
    
    sortMoves(moves, num, ply);
    for(int idx = 0; idx < num; idx++) {
        if(alpha >= beta) return alpha;

        board.makeMove(moves[idx]);

        // --- FUTILITY PRUNING --- 
        // if a move is bad enough that it wouldn't be able to raise alpha, we just skip it
        // this only applies close to the horizon depth
        // if(fPrune && !MoveUtils::isCapture(moves[idx]) && !MoveUtils::isPromotion(moves[idx]) && !board.isInCheck()) {
        //     board.unmakeMove(moves[idx]);
        //     continue;
        // }
            
        // --- PRINCIPAL VARIATION SEARCH --- 
        // we do a full search only until we find a move that raises alpha and we consider it to be the best
        // for the rest of the moves we start with a quick (null window -> beta = alpha+1) search
        // and only if the move has potential to be the best, we do a full search
        int score;
        if(!movesSearched) {
            score = -alphaBeta(-beta, -alpha, depth-1, ply+1, true);
        } else {
            // --- LATE MOVE REDUCTION --- 
            // we do full searches only for the first moves, and then do a reduced search
            // if the move is potentially good, we do a full search instead
            if(movesSearched >= 2 && !MoveUtils::isCapture(moves[idx]) && !MoveUtils::isPromotion(moves[idx]) && !isInCheck && depth >= 3 && !board.isInCheck()) {
                int reductionDepth = int(sqrt(double(depth-1)) + sqrt(double(movesSearched-1))); 
                if(isPV) reductionDepth = (reductionDepth * 2) / 3;
                reductionDepth = (reductionDepth < depth-1 ? reductionDepth : depth-1);

                score = -alphaBeta(-alpha-1, -alpha, depth - reductionDepth - 1, ply+1, true);
            } else {
                score = alpha + 1; // hack to ensure that full-depth search is done
            }

            if(score > alpha) {
                score = -alphaBeta(-alpha-1, -alpha, depth-1, ply+1, true);
                if(score > alpha && score < beta) {
                    score = -alphaBeta(-beta, -alpha, depth-1, ply+1, true);
                }
            }
        }

        board.unmakeMove(moves[idx]);
        movesSearched++;

        if(timeOver) return 0;
        
        if(score > alpha) {
            currBestMove = moves[idx];

            pvArray[pvIndex] = moves[idx];
            copyPv(pvArray + pvIndex + 1, pvArray + pvNextIndex, N - ply - 1);

            assert(pvArray[pvIndex] != MoveUtils::NO_MOVE);
            assert(pvIndex < pvNextIndex);
            assert(pvNextIndex < (N * N + N) / 2);

            if(score >= beta) {
                transpositionTable.recordHash(depth, beta, TranspositionTable::HASH_F_BETA, currBestMove);

                if(!MoveUtils::isCapture(moves[idx]) && !MoveUtils::isPromotion(moves[idx])) {
                    // store killer moves
                    storeKiller(ply, moves[idx]);

                    // update move history
                    updateHistory(moves[idx], depth);
                }

                return beta;
            }
            hashFlag = TranspositionTable::HASH_F_EXACT;
            alpha = score;
        }
    }
    if(timeOver) return 0;

    transpositionTable.recordHash(depth, alpha, hashFlag, currBestMove);
    return alpha;
}

const int ASP_INCREASE = 50;
pair<int, int> search() {
    timeOver = false;

    int alpha = -INF, beta = INF;
    int eval = 0;

    ageHistory();
    for(short i = 0; i < 256; i++)
        killerMoves[i][0] = killerMoves[i][1] = MoveUtils::NO_MOVE;

    memset(pvArray, MoveUtils::NO_MOVE, sizeof(pvArray));

    // --- ITERATIVE DEEPENING --- 
    // we start with a depth 1 search and then we increase the depth by 1 every time
    // this helps manage the time because at any point the engine can return the best move found so far
    // also it helps improve move ordering by memorizing the best move that we can search first in the next iteration
    long long currStartTime = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();
    for(short depth = 1; depth <= maxDepth; ) {

        nodesSearched = nodesQ = 0;

        int curEval = alphaBeta(alpha, beta, depth, 0, false);

        if(timeOver) break;

        // --- ASPIRATION WINDOW --- 
        // we start with a full width search (alpha = -inf and beta = inf), modifying them accordingly
        // if we fall outside the current window, we do a full width search with the same depth
        // and if we stay inside the window, we only increase it by a small number, so that we can achieve more cutoffs
        if(curEval <= alpha || curEval >= beta) {
            alpha = -INF;
            beta = INF;
            continue;
        }

        eval = curEval;
        alpha = eval - ASP_INCREASE; // increase window for next iteration
        beta = eval + ASP_INCREASE;

        UCI::showSearchInfo(depth, nodesSearched+nodesQ, currStartTime, eval);
        currStartTime = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();

        depth++; // increase depth only if we are inside the window
    }

    return {transpositionTable.retrieveBestMove(), eval};
}
