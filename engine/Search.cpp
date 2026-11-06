#include <algorithm>
#include <chrono>
#include <math.h>
#include <unordered_map>
#include <iostream>
#include <stack>
#include <cassert>
#include <cstring>

#include "Evaluate.h"
#include "Board.h"
#include "Search.h"
#include "TranspositionTable.h"
#include "MagicBitboardUtils.h"
#include "UCI.h"
#include "MoveUtils.h"
#include "BoardUtils.h"
#include "Enums.h"

using namespace std;

int Search::bestMove = MoveUtils::NO_MOVE;

int Search::killerMoves[256][2];
int Search::history[16][64];
const int Search::HISTORY_MAX = 1e8;

const int Search::INF = 1000000;
const int Search::MATE_EVAL = INF-1;
const int Search::MATE_THRESHOLD = MATE_EVAL/2;

long long Search::softStopTime;
long long Search::hardStopTime;
bool Search::infiniteTime;

int Search::nodesSearched = 0;
int Search::nodesQ = 0;
bool Search::timeOver = false;

const int Search::MAX_DEPTH = 100;
int Search::currMaxDepth = 100;
int Search::pvArray[(MAX_DEPTH * MAX_DEPTH + MAX_DEPTH) / 2 + MAX_DEPTH];

const int Search::ASP_INCREASE = 50;


// --- MOVE ORDERING ---
int Search::captureScore(int move) {
    int score = 0;

    // give huge score boost to captures of the last moved piece
    if(!board->moveStk.empty() && MoveUtils::getToSq(move) == MoveUtils::getToSq(board->moveStk.top())) score += 2000;

    // captured piece value - capturing piece value
    if(MoveUtils::isCapture(move)) score += (PIECE_VALUES[MoveUtils::getCapturedPiece(move)]-
                  PIECE_VALUES[MoveUtils::getPiece(move)]);

    // material gained by promotion
    if(MoveUtils::isPromotion(move)) score += PIECE_VALUES[MoveUtils::getPromotionPiece(move)] - PIECE_VALUES[Pawn];

    return score;
}

int Search::nonCaptureScore(int move) {
    // start with history score
    int score = history[(MoveUtils::getColor(move) | MoveUtils::getPiece(move))][MoveUtils::getToSq(move)];

    // if it is a promotion add huge bonus so that it is searched first
    if(MoveUtils::isPromotion(move)) score += 100000000;

    assert(score <= 1e9);
    return score;
}

bool Search::cmpCapturesInv(int a, int b) {
    return (captureScore(a) < captureScore(b));
}

bool Search::cmpCaptures(int a, int b) {
    return (captureScore(a) > captureScore(b));
}

bool Search::cmpNonCaptures(int a, int b) {
    return nonCaptureScore(a) > nonCaptureScore(b);
}

void Search::sortMoves(int *moves, int num, short ply) {
    // sorting in quiescence search
      if(ply == -1) {
          sort(moves, moves+num, cmpCaptures);
          return;
      }

    int captures[256], nonCaptures[256];
    unsigned int nCaptures = 0, nNonCaptures = 0;

    // find hash moves
    int hashMoveDepth = transpositionTable->retrieveDepthMove();
    int hashMoveReplace = transpositionTable->retrieveReplaceMove();

    // check legality of hash and killer moves
    bool killerLegal[2] = {false, false};
    bool hashDepthLegal = false, hashReplaceLegal = false;
    for(int idx = 0; idx < num; idx++) {
        if(killerMoves[ply][0] == moves[idx]) killerLegal[0] = true;
        if(killerMoves[ply][1] == moves[idx]) killerLegal[1] = true;
        if(hashMoveDepth == moves[idx]) hashDepthLegal = true;
        if(hashMoveReplace == moves[idx]) hashReplaceLegal = true;
    }
    if(!hashDepthLegal) hashMoveDepth = MoveUtils::NO_MOVE;
    if(!hashReplaceLegal) hashMoveReplace = MoveUtils::NO_MOVE;

    // split the other moves into captures and non captures for easier sorting
    for(int idx = 0; idx < num; idx++) {
        if((moves[idx] == killerMoves[ply][0]) 
        || (moves[idx] == killerMoves[ply][1]) 
        || (moves[idx] == hashMoveDepth) 
        || (moves[idx] == hashMoveReplace)) continue;

        if(MoveUtils::isCapture(moves[idx])) captures[nCaptures++] = moves[idx];
        else nonCaptures[nNonCaptures++] = moves[idx];
    }

    int newNum = 0; // size of sorted array

    // add hash moves if they are different from pv move
    if (hashMoveDepth != MoveUtils::NO_MOVE) moves[newNum++] = hashMoveDepth;

    if (hashMoveReplace != MoveUtils::NO_MOVE 
    && hashMoveReplace != hashMoveDepth) moves[newNum++] = hashMoveReplace;
    
    // add captures sorted by MVV-LVA (only winning / equal captures first)
    sort(captures, captures + nCaptures, cmpCapturesInv); // descending order because we add them from the end
    while(nCaptures && captureScore(captures[nCaptures-1]) >= 0) {
        moves[newNum++] = captures[--nCaptures];
    }

    // add killer moves
    if(killerLegal[0] 
    && (killerMoves[ply][0] != MoveUtils::NO_MOVE) 
    && (killerMoves[ply][0] != hashMoveDepth) 
    && (killerMoves[ply][0] != hashMoveReplace)) moves[newNum++] = killerMoves[ply][0];

    if(killerLegal[1] 
    && (killerMoves[ply][1] != MoveUtils::NO_MOVE) 
    && (killerMoves[ply][1] != killerMoves[ply][0])
    && (killerMoves[ply][1] != hashMoveDepth) 
    && (killerMoves[ply][1] != hashMoveReplace)) moves[newNum++] = killerMoves[ply][1];

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
int Search::quiescence(int alpha, int beta) {
    if(!((nodesSearched + nodesQ) & 1023) && !infiniteTime && bestMove != MoveUtils::NO_MOVE) {
        long long currTime = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();
        if(currTime >= hardStopTime) timeOver = true;
    }
    if(timeOver) return 0;
    nodesQ++;

    if(board->isDraw()) return 0;

    int standPat = evaluate();
    if(standPat >= beta && !board->isInCheck()) return beta;

    alpha = max(alpha, standPat);

    int moves[256];
    int num = board->generateLegalMovesQS(moves);

    sortMoves(moves, num, -1);
    for(int idx = 0; idx < num; idx++)  {
        // if(MoveUtils::isCapture(moves[idx]) && (seeMove(moves[idx]) < 0)) continue;

        // --- DELTA PRUNING --- 
        // we test if each move has the potential to raise alpha
        // if it doesn't, then the position is hopeless so searching deeper won't improve it
        int delta = standPat +  PIECE_VALUES[MoveUtils::getCapturedPiece(moves[idx])] + 200;
        if(MoveUtils::isPromotion(moves[idx])) delta += PIECE_VALUES[MoveUtils::getPromotionPiece(moves[idx])] - PIECE_VALUES[Pawn];

        const int ENDGAME_MATERIAL = 10;
        if((delta <= alpha) && (gamePhase() - MG_WEIGHT[MoveUtils::getCapturedPiece(moves[idx])] >= ENDGAME_MATERIAL)) continue;

        board->makeMove(moves[idx]);
        int score = -quiescence(-beta, -alpha);
        board->unmakeMove(moves[idx]);

        if(timeOver) return 0;

        if(score > alpha) {
            if(score >= beta) return beta;
            alpha = score;
        }
    }

    return alpha;
}

// alpha-beta algorithm with a fail-hard framework and PVS
int Search::alphaBeta(int alpha, int beta, short depth, short ply, bool doNull) {
    assert(depth >= 0);
    assert(ply <= MAX_DEPTH);

    int pvIndex = ply * (2 * MAX_DEPTH + 1 - ply) / 2;
    int pvNextIndex = pvIndex + MAX_DEPTH - ply;
    pvArray[pvIndex] = MoveUtils::NO_MOVE;

    if(!((nodesSearched + nodesQ) & 1023) && !infiniteTime && bestMove != MoveUtils::NO_MOVE) {
        long long currTime = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();
        if(currTime >= hardStopTime) timeOver = true;
    }
    if(timeOver) return 0;
    nodesSearched++;

    int hashFlag = TranspositionTable::HASH_F_ALPHA;

    bool isInCheck = board->isInCheck();

    // --- MATE DISTANCE PRUNING --- 
    // if we find mate, we shouldn't look for a better move
    int matedScore = - MATE_EVAL + ply;
    int mateScore = MATE_EVAL - ply;
    if (alpha >= mateScore) return alpha;
    if (beta <= matedScore) return beta;

    if(ply > 0 && board->isDraw()) return 0;

    bool isPV = (beta - alpha > 1);

    // retrieving the hashed move and evaluation if there is any
    int hashScore = transpositionTable->probeHash(depth, alpha, beta, ply);
    if(hashScore != TranspositionTable::VAL_UNKNOWN && !isPV) return hashScore;

    int moves[256];
    int num = board->generateLegalMoves(moves);
    if(num == 0) {
        if(isInCheck) return -mateScore; // checkmate
        return 0; // stalemate
    }

    if(depth <= 0) return quiescence(alpha, beta);

    int currBestMove = MoveUtils::NO_MOVE;

    // --- STATIC NULL MOVE PRUNING ---
    // if our position is so good that we can afford to lose some material
    // we assume this node will fail high and we prune its branch
    int staticScore = evaluate();
    if(!isInCheck && !isPV && abs(beta) < MATE_THRESHOLD) {
        int scoreMargin = 100 * depth;
        if (staticScore - scoreMargin >= beta) {
            return staticScore - scoreMargin;
        }
    }

    // --- NULL MOVE PRUNING --- 
    // if our position is good, we can pass the turn to the opponent
    // and if that doesn't wreck our position, we don't need to search further
    const int ENDGAME_MATERIAL = 4;
    if(doNull && (!isPV) && (isInCheck == false) && ply && (depth > 3) && (gamePhase() >= ENDGAME_MATERIAL) && (staticScore >= beta)) {
        board->makeMove(MoveUtils::NO_MOVE);

        short R = 3 + depth / 6;
        int score = -alphaBeta(-beta, -beta + 1, depth - R - 1, ply + 1, false);

        board->unmakeMove(MoveUtils::NO_MOVE);

        if(timeOver) return 0;
        if(score >= beta && abs(score) < MATE_THRESHOLD) return beta;
    }

    // --- RAZORING --- 
    // if we are close to the horizon and the static eval is bad
    // do a quiescence search and confirm the position will fail low
    // if qsearch score fails low we trust it and return
    if(!isPV && !isInCheck && depth <= 3 && staticScore + 250 * depth <= alpha) {
        int score = quiescence(alpha, beta);
        if(score <= alpha) return score;
    }

    // --- INTERNAL ITERATIVE DEEPENING ---
    // if we don't have a move from the tt, we do a quick search with a reduced depth
    if(depth >= 4 && isPV && transpositionTable->retrieveBestMove() == MoveUtils::NO_MOVE) {
        int score = alphaBeta(alpha, beta, depth - 2, ply, doNull);

        // make sure we have a move in the tt
        if (score <= alpha) score = alphaBeta(-INF, beta, depth - 2, ply, doNull);
        pvArray[pvIndex] = MoveUtils::NO_MOVE;
    }

    // --- FUTILITY PRUNING --- 
    // if a move is bad enough that it wouldn't be able to raise alpha, we just skip it
    // this only applies close to the horizon depth
    bool fPrune = false;
    const int F_MARGIN[5] = { 0, 100, 170, 240, 310 };
    if (depth <= 4 && !isPV && !isInCheck && abs(alpha) < Search::MATE_THRESHOLD) {
        fPrune = (staticScore + F_MARGIN[depth] <= alpha);
    }

    int movesSearched = 0;
    
    sortMoves(moves, num, ply);
    for(int idx = 0; idx < num; idx++) {
        if(alpha >= beta) return alpha;

        board->makeMove(moves[idx]);
            
        // --- PRINCIPAL VARIATION SEARCH --- 
        // we do a full search only until we find a move that raises alpha and we consider it to be the best
        // for the rest of the moves we start with a quick (null window -> beta = alpha+1) search
        // and only if the move has potential to be the best, we do a full search
        int score;
        if(!movesSearched) {
            score = -alphaBeta(-beta, -alpha, depth-1, ply+1, true);
        } else {
            // Futility prune if conditions are met
            if(fPrune && !MoveUtils::isCapture(moves[idx]) && !MoveUtils::isPromotion(moves[idx]) && !board->isInCheck()) {
                board->unmakeMove(moves[idx]);
                continue;
            }

            // --- LATE MOVE REDUCTION --- 
            // we do full searches only for the first moves, and then do a reduced search
            // if the move is potentially good, we do a full search instead
            if(movesSearched >= 2 && !MoveUtils::isCapture(moves[idx]) && !MoveUtils::isPromotion(moves[idx]) && !isInCheck && depth >= 3 && !board->isInCheck()) {
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

        board->unmakeMove(moves[idx]);
        movesSearched++;

        if(timeOver) return 0; // ensure that we at least have a move to print
        
        if(score > alpha) {
            currBestMove = moves[idx];
            assert(currBestMove != MoveUtils::NO_MOVE);

            if(score >= beta) {
                transpositionTable->recordHash(depth, beta, TranspositionTable::HASH_F_BETA, currBestMove, ply);

                if(!MoveUtils::isCapture(moves[idx]) && !MoveUtils::isPromotion(moves[idx])) {
                    // store killer moves
                    storeKiller(ply, moves[idx]);

                    // update move history
                    updateHistory(moves[idx], depth);
                }

                return beta;
            }

            if(isPV) {
                pvArray[pvIndex] = moves[idx];
                copyPv(pvArray + pvIndex + 1, pvArray + pvNextIndex, MAX_DEPTH - ply - 1);

                assert(pvArray[pvIndex] != MoveUtils::NO_MOVE);
            }
            assert(pvIndex < pvNextIndex);
            assert(pvNextIndex < (MAX_DEPTH * MAX_DEPTH + MAX_DEPTH) / 2);

            hashFlag = TranspositionTable::HASH_F_EXACT;
            alpha = score;
        }
    }
    if(timeOver) return 0;

    transpositionTable->recordHash(depth, alpha, hashFlag, currBestMove, ply);
    return alpha;
}

pair<int, int> Search::root() {
    bestMove = MoveUtils::NO_MOVE;

    int alpha = -INF, beta = INF;
    int delta = ASP_INCREASE;
    int eval = 0;

    ageHistory();
    for(short i = 0; i < 256; i++)
        killerMoves[i][0] = killerMoves[i][1] = MoveUtils::NO_MOVE;


    // --- ITERATIVE DEEPENING --- 
    // we start with a depth 1 search and then we increase the depth by 1 every time
    // this helps manage the time because at any point the engine can return the best move found so far
    // also it helps improve move ordering by memorizing the best move that we can search first in the next iteration
    long long currStartTime = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();
    for(short depth = 1; depth <= currMaxDepth; ) {
        nodesSearched = nodesQ = 0;
        memset(pvArray, MoveUtils::NO_MOVE, sizeof(pvArray));

        int curEval = alphaBeta(alpha, beta, depth, 0, false);

        if(timeOver) break;

        // --- ASPIRATION WINDOW --- 
        // we start with a full width search (alpha = -inf and beta = inf), modifying them accordingly
        // if we fall outside the current window, widen once by 150cp before falling back to full width
        if(curEval <= alpha) {
            alpha = (delta > 200) ? -INF : max(curEval - delta, -INF);
            delta *= 3;
            continue;
        }
        if(curEval >= beta) {
            beta = (delta > 200) ? INF : min(curEval + delta, INF);
            delta *= 3;
            continue;
        }

        eval = curEval;
        delta = ASP_INCREASE * 3; // 150cp step if next iteration fails low/high
        alpha = max(eval - ASP_INCREASE, -INF);
        beta = min(eval + ASP_INCREASE, INF);

        if(pvArray[0] != MoveUtils::NO_MOVE) bestMove = pvArray[0];
        UCI::showSearchInfo(depth, nodesSearched+nodesQ, currStartTime, eval);
        long long now = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();
        currStartTime = now;

        depth++; // increase depth only if we are inside the window

        // --- SOFT TIME LIMIT ---
        // Check soft stop after completing a full depth iteration so we don't start
        // a new depth that we likely won't finish
        if(!infiniteTime && now >= softStopTime) break;
    }

    int moveToPlay = bestMove;
    if (moveToPlay == MoveUtils::NO_MOVE) moveToPlay = transpositionTable->retrieveBestMove();
    if (moveToPlay == MoveUtils::NO_MOVE) {
        int moves[256];
        int num = board->generateLegalMoves(moves);
        if (num > 0) moveToPlay = moves[0];
    }
    assert(moveToPlay != MoveUtils::NO_MOVE);

    timeOver = false;
    return {moveToPlay, eval};
}

// --- KILLERS AND HISTORY ---
// killer moves are quiet moves that cause a beta cutoff and are used for sorting purposes
void Search::storeKiller(short ply, int move) {
    // make sure the moves are different
    if(killerMoves[ply][0] != move) 
        killerMoves[ply][1] = killerMoves[ply][0];

    killerMoves[ply][0] = move;
}


// same as killer moves, but they are saved based on their squares and color
void Search::updateHistory(int move, int depth) {
    int bonus = depth * depth;

    history[(MoveUtils::getColor(move) | MoveUtils::getPiece(move))][MoveUtils::getToSq(move)] += 2 * bonus;
    for(int pc = 0; pc < 16; pc++) {
        for(int sq = 0; sq < 64; sq++) {
            history[pc][sq] = max(0, history[pc][sq] - bonus);
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

void Search::clearHistory() {
    for(int pc = 0; pc < 16; pc++) {
        for(int sq = 0; sq < 64; sq++) {
            history[pc][sq] = 0;
        }
    }
}

void Search::ageHistory() {
    for(int pc = 0; pc < 16; pc++) {
        for(int sq = 0; sq < 64; sq++) {
            history[pc][sq] /= 8;
        }
    }
}

// --- PV HELPER FUNCTIONS ---
void Search::copyPv(int* dest, const int* src, int n) {
   while (n-- && (*dest++ = *src++));
   *dest = MoveUtils::NO_MOVE;
}

void Search::showPV(int depth) {
    assert(depth <= MAX_DEPTH);

    cout << "pv ";
    for(int i = 0; i < depth; i++) {
        if(pvArray[i] == MoveUtils::NO_MOVE) break;
        cout << BoardUtils::moveToString(pvArray[i]) << ' ';
    }
    cout << '\n';
    cout.flush();
}
