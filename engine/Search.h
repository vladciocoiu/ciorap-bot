#pragma once

#ifndef SEARCH_H_
#define SEARCH_H_

using namespace std;

class Search {
private:
    static int bestMove;

    static int killerMoves[256][2];
    static int history[16][64];
    static const int HISTORY_MAX;

    static const int INF;

    static int nodesSearched;
    static int nodesQ;

    static int pvArray[];

    static const int ASP_INCREASE;

    // --- MOVE ORDERING ---
    static int captureScore(int move);
    static int nonCaptureScore(int move);
    static bool cmpCapturesInv(int a, int b);
    static bool cmpCaptures(int a, int b);
    static bool cmpNonCaptures(int a, int b);
    static void sortMoves(int *moves, int num, short ply);

    static int alphaBeta(int alpha, int beta, short depth, short ply, bool doNull);

    // --- KILLERS AND HISTORY ---
    static void storeKiller(short ply, int move);
    static void updateHistory(int move, int depth);
    static void ageHistory();

    // --- PV HELPER FUNCTIONS ---
    static void copyPv(int* dest, const int* src, int n);
public:

    static const int MATE_EVAL;
    static const int MATE_THRESHOLD;

    static const int MAX_DEPTH;
    static int currMaxDepth;
    static long long softStopTime;
    static long long hardStopTime;
    static bool infiniteTime;
    static bool timeOver;

    static pair<int, int> root();
    static int quiescence(int alpha, int beta);
    static void clearHistory();
    static void showPV(int depth);
};

#endif
