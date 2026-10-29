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

long long Search::stopTime;
bool Search::infiniteTime;

int Search::nodesSearched = 0;
int Search::nodesQ = 0;
bool Search::timeOver = false;

const int Search::MAX_DEPTH = 100;
int Search::currMaxDepth = 100;
int Search::pvArray[(MAX_DEPTH * MAX_DEPTH + MAX_DEPTH) / 2 + MAX_DEPTH];

const int Search::ASP_INCREASE = 50;

int Search::lmrTable[101][256];
bool Search::lmrInitialized = false;
int Search::staticEvalStack[256];

// --- SELECTIVITY PARAMETERS ---
static const int EVAL_NONE = -2000000; // marks plies where the static eval is unknown (in check)
static const int RFP_MAX_DEPTH = 8;
static const int RFP_MARGIN = 85;
static const int LMP_MAX_DEPTH = 6;
static const int SEE_PRUNE_MAX_DEPTH = 8;
static const int SEE_QUIET_MARGIN = 50;
static const int SEE_CAPTURE_MARGIN = 100;

// piece values used only for static exchange evaluation (king must be "infinite")
static const int SEE_VALUES[7] = {0, 100, 325, 350, 500, 975, 20000};

// --- LATE MOVE REDUCTION TABLE ---
// logarithmic reduction formula: grows slowly with both depth and move number
void Search::initLMR() {
    for(int d = 0; d <= 100; d++) {
        for(int m = 0; m < 256; m++) {
            if(d == 0 || m == 0) lmrTable[d][m] = 0;
            else lmrTable[d][m] = int(0.75 + log(double(d)) * log(double(m)) / 2.25);
        }
    }
    lmrInitialized = true;
}

// --- STATIC EXCHANGE EVALUATION ---
// returns all pieces of both colors that attack sq, given the occupancy occ
U64 Search::attackersTo(int sq, U64 occ) {
    U64 whitePawns = (board->pawnsBB & board->whitePiecesBB);
    U64 blackPawns = (board->pawnsBB & board->blackPiecesBB);
    U64 kings = (BoardUtils::bits[board->whiteKingSquare] | BoardUtils::bits[board->blackKingSquare]);

    U64 res = 0;
    res |= (BoardUtils::blackPawnAttacksBB[sq] & whitePawns); // white pawns attack sq from below
    res |= (BoardUtils::whitePawnAttacksBB[sq] & blackPawns); // black pawns attack sq from above
    res |= (BoardUtils::knightAttacksBB[sq] & board->knightsBB);
    res |= (BoardUtils::kingAttacksBB[sq] & kings);
    res |= (MagicBitboardUtils::magicBishopAttacks(occ, sq) & (board->bishopsBB | board->queensBB));
    res |= (MagicBitboardUtils::magicRookAttacks(occ, sq) & (board->rooksBB | board->queensBB));

    return (res & occ);
}

// swap-list SEE: returns the expected material balance of the exchange sequence
// started by move on its destination square (from the perspective of the moving side)
int Search::see(int move) {
    if(MoveUtils::isCastle(move)) return 0;

    int from = MoveUtils::getFromSq(move);
    int to = MoveUtils::getToSq(move);
    int us = MoveUtils::getColor(move);

    int gain[32];
    int d = 0;

    U64 occ = (board->whitePiecesBB | board->blackPiecesBB);

    gain[0] = SEE_VALUES[MoveUtils::getCapturedPiece(move)];
    int attackerValue = SEE_VALUES[MoveUtils::getPiece(move)];

    if(MoveUtils::isPromotion(move)) {
        gain[0] += SEE_VALUES[MoveUtils::getPromotionPiece(move)] - SEE_VALUES[Pawn];
        attackerValue = SEE_VALUES[MoveUtils::getPromotionPiece(move)];
    }

    // remove the en passant captured pawn from the occupancy
    if(MoveUtils::isEP(move)) occ ^= BoardUtils::bits[(us == White ? to - 8 : to + 8)];

    U64 fromSet = BoardUtils::bits[from];
    U64 attadef = attackersTo(to, occ);
    const U64 diagSliders = (board->bishopsBB | board->queensBB);
    const U64 lineSliders = (board->rooksBB | board->queensBB);

    const U64 pieceBBs[7] = {0, board->pawnsBB, board->knightsBB, board->bishopsBB,
                             board->rooksBB, board->queensBB,
                             BoardUtils::bits[board->whiteKingSquare] | BoardUtils::bits[board->blackKingSquare]};

    while(fromSet) {
        d++;
        gain[d] = attackerValue - gain[d-1]; // score if the piece we just moved gets captured
        if(max(-gain[d-1], gain[d]) < 0) break; // pruning optimization

        attadef ^= fromSet;
        occ ^= fromSet;

        // reveal x-ray attackers behind the piece that just captured
        attadef |= ((MagicBitboardUtils::magicBishopAttacks(occ, to) & diagSliders)
                  | (MagicBitboardUtils::magicRookAttacks(occ, to) & lineSliders)) & occ;

        // find the least valuable attacker of the side to move
        int side = ((d & 1) ? (us ^ (White | Black)) : us);
        U64 sidePieces = (side == White ? board->whitePiecesBB : board->blackPiecesBB);
        U64 otherPieces = (side == White ? board->blackPiecesBB : board->whitePiecesBB);

        fromSet = 0;
        for(int pc = Pawn; pc <= King; pc++) {
            U64 subset = (attadef & sidePieces & pieceBBs[pc]);
            if(subset) {
                fromSet = (subset & (~subset + 1)); // isolate lowest bit
                attackerValue = SEE_VALUES[pc];

                // the king can only capture if the square is no longer defended
                if(pc == King && (attadef & otherPieces & ~fromSet)) fromSet = 0;
                break;
            }
        }
    }

    // minimax backwards: each side can choose to stop the exchanges if negative
    while(--d) {
        gain[d-1] = -max(-gain[d-1], gain[d]);
    }

    return gain[0];
}


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

    // check legality of killer moves
    bool killerLegal[2] = {false, false};
    for(int idx = 0; idx < num; idx++) {
        if(killerMoves[ply][0] == moves[idx]) killerLegal[0] = true;
        if(killerMoves[ply][1] == moves[idx]) killerLegal[1] = true;
    }

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
int Search::quiescence(int alpha, int beta, short ply) {
    if(!(nodesQ & 4095) && !infiniteTime) {
        long long currTime = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();
        if(currTime >= stopTime) timeOver = true;
    }
    if(timeOver) return 0;
    nodesQ++;

    if(board->isDraw()) return 0;

    // --- TT PROBE ---
    // any stored entry (searched at depth >= 0) is at least as reliable as a quiescence search
    if(transpositionTable != nullptr) {
        int hashScore = transpositionTable->probeHash(0, alpha, beta, ply);
        if(hashScore != TranspositionTable::VAL_UNKNOWN) return hashScore;
    }

    bool inCheck = board->isInCheck();

    int standPat = evaluate();
    if(standPat >= beta && !inCheck) return beta;

    alpha = max(alpha, standPat);

    int moves[256];
    int num = board->generateLegalMovesQS(moves);

    sortMoves(moves, num, -1);
    for(int idx = 0; idx < num; idx++)  {
        // --- DELTA PRUNING --- 
        // we test if each move has the potential to raise alpha
        // if it doesn't, then the position is hopeless so searching deeper won't improve it
        int delta = standPat +  PIECE_VALUES[MoveUtils::getCapturedPiece(moves[idx])] + 200;
        if(MoveUtils::isPromotion(moves[idx])) delta += PIECE_VALUES[MoveUtils::getPromotionPiece(moves[idx])] - PIECE_VALUES[Pawn];

        const int ENDGAME_MATERIAL = 10;
        if((delta <= alpha) && (gamePhase() - MG_WEIGHT[MoveUtils::getCapturedPiece(moves[idx])] >= ENDGAME_MATERIAL)) continue;

        // --- SEE PRUNING ---
        // skip captures that lose material according to the static exchange evaluation
        if(!inCheck && !MoveUtils::isPromotion(moves[idx]) && see(moves[idx]) < 0) continue;

        board->makeMove(moves[idx]);
        int score = -quiescence(-beta, -alpha, ply + 1);
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

    if(!(nodesSearched & 4095) && !infiniteTime) {
        long long currTime = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();
        if(currTime >= stopTime) timeOver = true;
    }
    if(timeOver) return 0;
    nodesSearched++;

    // safety net: extensions could otherwise push the search past the pv / killer array bounds
    if(ply >= MAX_DEPTH - 1) return evaluate();

    int pvIndex = ply * (2 * MAX_DEPTH + 1 - ply) / 2;
    int pvNextIndex = pvIndex + MAX_DEPTH - ply;
    pvArray[pvIndex] = MoveUtils::NO_MOVE;

    int hashFlag = TranspositionTable::HASH_F_ALPHA;

    bool isInCheck = board->isInCheck();

    // --- MATE DISTANCE PRUNING --- 
    // if we find mate, we shouldn't look for a better move
    int matedScore = - MATE_EVAL + ply;
    int mateScore = MATE_EVAL - ply;
    if (alpha >= mateScore) return alpha;
    if (beta <= matedScore) return beta;

    if(board->isDraw()) return 0;

    bool isPV = (beta - alpha > 1);

    // --- CHECK EXTENSION ---
    // positions where we are in check are tactically volatile, so we search them one ply deeper
    // (this also guarantees that we never drop into quiescence search while in check)
    if(isInCheck) depth++;

    // retrieving the hashed move and evaluation if there is any
    int hashScore = transpositionTable->probeHash(depth, alpha, beta, ply);
    if(hashScore != TranspositionTable::VAL_UNKNOWN && !isPV) return hashScore;

    int moves[256];
    int num = board->generateLegalMoves(moves);
    if(num == 0) {
        if(isInCheck) return -mateScore; // checkmate
        return 0; // stalemate
    }

    if(depth <= 0) return quiescence(alpha, beta, ply);

    int currBestMove = MoveUtils::NO_MOVE;

    // --- STATIC EVAL AND IMPROVING HEURISTIC ---
    // "improving" means our static eval is better than it was on our previous turn
    // when we are improving, we prune less aggressively, and when we are not, more aggressively
    int staticScore = (isInCheck ? EVAL_NONE : evaluate());
    staticEvalStack[ply] = staticScore;

    bool improving = false;
    if(!isInCheck) {
        if(ply >= 2 && staticEvalStack[ply-2] != EVAL_NONE) improving = (staticScore > staticEvalStack[ply-2]);
        else if(ply >= 4 && staticEvalStack[ply-4] != EVAL_NONE) improving = (staticScore > staticEvalStack[ply-4]);
        else improving = true;
    }

    // --- REVERSE FUTILITY PRUNING (STATIC NULL MOVE PRUNING) ---
    // if our position is so good that we can afford to lose some material
    // we assume this node will fail high and we prune its branch
    if(!isInCheck && !isPV && depth <= RFP_MAX_DEPTH && abs(beta) < MATE_THRESHOLD) {
        int scoreMargin = RFP_MARGIN * (depth - (improving ? 1 : 0));
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
        int score = -alphaBeta(-beta, -beta + 1, max(0, depth - R - 1), ply + 1, false);

        board->unmakeMove(MoveUtils::NO_MOVE);

        if(timeOver) return 0;
        if(score >= beta && abs(score) < MATE_THRESHOLD) return beta;
    }

    // --- RAZORING --- 
    // if we are close to the horizon and the static eval is bad
    // do a quiescence search and confirm the position will fail low
    // if qsearch score fails low we trust it and return
    if(!isPV && !isInCheck && depth <= 3 && staticScore + 250 * depth <= alpha) {
        int score = quiescence(alpha, beta, ply);
        if(score <= alpha) return score;
    }

    // --- INTERNAL ITERATIVE DEEPENING ---
    // if we don't have a move from the tt, we do a quick search with a reduced depth
    if(depth >= 4 && isPV && transpositionTable->retrieveBestMove() == MoveUtils::NO_MOVE) {
        int score = alphaBeta(alpha, beta, depth - 2, ply + 1, doNull);

        // make sure we have a move in the tt
        if (score <= alpha) score = alphaBeta(-INF, beta, depth - 2, ply + 1, doNull);
    }

    // --- FUTILITY PRUNING --- 
    // if a move is bad enough that it wouldn't be able to raise alpha, we just skip it
    // this only applies close to the horizon depth
    bool fPrune = false;
    const int F_MARGIN[5] = { 0, 100, 170, 240, 310 };
    if (depth <= 4 && !isPV && !isInCheck && abs(alpha) < Search::MATE_THRESHOLD) {
        fPrune = (staticScore + F_MARGIN[depth] <= alpha);
    }

    // --- LATE MOVE PRUNING ---
    // in non-pv nodes close to the horizon, quiet moves sorted late are very unlikely to be good
    // so after a certain number of quiet moves, we skip the remaining ones
    const int lmpLimit = (3 + depth * depth) / (improving ? 1 : 2);

    int movesSearched = 0;
    int quietsSeen = 0;
    
    sortMoves(moves, num, ply);
    for(int idx = 0; idx < num; idx++) {
        if(alpha >= beta) return alpha;

        const int move = moves[idx];
        const bool isQuiet = (!MoveUtils::isCapture(move) && !MoveUtils::isPromotion(move));
        const bool isKiller = (move == killerMoves[ply][0] || move == killerMoves[ply][1]);
        if(isQuiet) quietsSeen++;

        // pruning is only done after we have a move to fall back on, never at the root and never in check
        const bool canPrune = (ply > 0 && movesSearched > 0 && !isInCheck && abs(alpha) < MATE_THRESHOLD);

        // --- SEE PRUNING ---
        // skip moves that lose too much material according to the static exchange evaluation
        // the allowed loss grows with depth, since deeper searches can find compensation
        bool seePrune = false;
        if(canPrune && depth <= SEE_PRUNE_MAX_DEPTH && !MoveUtils::isPromotion(move)) {
            int threshold = (isQuiet ? -SEE_QUIET_MARGIN * depth : -SEE_CAPTURE_MARGIN * depth);
            seePrune = (see(move) < threshold);
        }

        board->makeMove(move);
        const bool givesCheck = board->isInCheck();

        if(canPrune && !givesCheck) {
            // late move pruning
            if(isQuiet && !isPV && depth <= LMP_MAX_DEPTH && quietsSeen > lmpLimit) {
                board->unmakeMove(move);
                continue;
            }

            // futility pruning
            if(fPrune && isQuiet) {
                board->unmakeMove(move);
                continue;
            }

            // see pruning
            if(seePrune) {
                board->unmakeMove(move);
                continue;
            }
        }
            
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
            if(movesSearched >= 2 && isQuiet && !isInCheck && depth >= 3) {
                int R = lmrTable[min((int)depth, 100)][min(movesSearched, 255)];

                if(!isPV) R++; // reduce more in non-pv nodes
                if(!improving) R++; // reduce more when our position is getting worse
                if(isKiller) R--; // killers are likely to be good
                if(givesCheck) R--; // checks are tactically forcing

                // history-aware reduction: moves that recently caused cutoffs are reduced less
                int hist = history[(MoveUtils::getColor(move) | MoveUtils::getPiece(move))][MoveUtils::getToSq(move)];
                if(hist > 0) R--;

                R = max(0, min(R, depth - 1));

                score = -alphaBeta(-alpha-1, -alpha, depth - R - 1, ply+1, true);
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

        board->unmakeMove(move);
        movesSearched++;

        if(timeOver && (ply > 0 || idx > 0)) return 0; // ensure that we at least have a move to print
        
        if(score > alpha) {
            currBestMove = move;
            if(ply == 0) bestMove = currBestMove;
            assert(currBestMove != MoveUtils::NO_MOVE);

            if(isPV) {
                pvArray[pvIndex] = move;
                copyPv(pvArray + pvIndex + 1, pvArray + pvNextIndex, MAX_DEPTH - ply - 1);

                assert(pvArray[pvIndex] != MoveUtils::NO_MOVE);
            }
            assert(pvIndex < pvNextIndex);
            assert(pvNextIndex < (MAX_DEPTH * MAX_DEPTH + MAX_DEPTH) / 2);

            if(score >= beta) {
                transpositionTable->recordHash(depth, beta, TranspositionTable::HASH_F_BETA, currBestMove, ply);

                if(isQuiet) {
                    // store killer moves
                    storeKiller(ply, move);

                    // update move history
                    updateHistory(move, depth);
                }

                return beta;
            }

            hashFlag = TranspositionTable::HASH_F_EXACT;
            alpha = score;
        }
    }
    if(timeOver) return 0;

    transpositionTable->recordHash(depth, alpha, hashFlag, currBestMove, ply);
    return alpha;
}
pair<int, int> Search::root() {
    if(!lmrInitialized) initLMR();

    board->repetitionIndex = 0;
    bestMove = MoveUtils::NO_MOVE;

    timeOver = false;

    int alpha = -INF, beta = INF;
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

    int moveToPlay = bestMove;
    if (moveToPlay == MoveUtils::NO_MOVE) moveToPlay = transpositionTable->retrieveBestMove();
    assert(moveToPlay != MoveUtils::NO_MOVE);

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
}
