#include <vector>
#include <string>
#include <iostream>
#include <chrono>
#include <unordered_map>
#include <mutex>
#include <condition_variable>
#include <algorithm>

#include "Board.h"
#include "Search.h"
#include "Evaluate.h"
#include "TranspositionTable.h"
#include "BoardUtils.h"
#include "Enums.h"
#include "UCI.h"

string UCI::engineName = "CiorapBot 0.3";
string UCI::goCommand;
std::condition_variable UCI::cv;
std::mutex UCI::mtx;
bool UCI::startFlag, UCI::quitFlag;

// for showing uci info
string scoreToStr(int score) {
    // if it is a mate, we print "mate" + the number of moves until mate
    if (abs(score) > Search::MATE_THRESHOLD) return "mate " + to_string((score > 0 ? Search::MATE_EVAL - score + 1 : -Search::MATE_EVAL - score - 1) / 2);

    // if the score isn't a mate, we print it in centipawns (relative to the side to move per UCI spec)
    return "cp " + to_string(score);
}

// function to split string into words
vector<string> splitStr(string s) {
    vector<string> ans;
    for(char c: s) {
        if(c == ' ' || !ans.size()) ans.push_back("");
        else ans.back() += c;
    }
    return ans;
}

void UCI::UCICommunication() {
    string inputString;
    while(getline(cin, inputString)) {
        if(!inputString.empty() && inputString.back() == '\r') inputString.pop_back();

        if(inputString == "uci") {
            inputUCI();
        } else if(inputString == "isready") {
            std::unique_lock<std::mutex> lk(UCI::mtx);
            UCI::cv.wait(lk, []{ return !UCI::startFlag; });
            inputIsReady();
        } else if(inputString == "ucinewgame") {
            std::unique_lock<std::mutex> lk(UCI::mtx);
            UCI::cv.wait(lk, []{ return !UCI::startFlag; });
            inputUCINewGame();
        } else if(inputString.substr(0, 8) == "position") {
            std::unique_lock<std::mutex> lk(UCI::mtx);
            UCI::cv.wait(lk, []{ return !UCI::startFlag; });
            inputPosition(inputString);
        } else if(inputString.substr(0, 2) == "go") {
            {
                std::unique_lock<std::mutex> lk(UCI::mtx);
                UCI::cv.wait(lk, []{ return !UCI::startFlag; });
                Search::timeOver = false;
                UCI::goCommand = inputString;
                UCI::startFlag = true;
            }
            UCI::cv.notify_all();
        } else if(inputString.substr(0, 5) == "print") {
            printBoard(inputString.length() <= 6 || inputString.substr(6, 3) != "num");
        } else if(inputString.substr(0, 4) == "eval") {
            printEval();
        } else if(inputString == "stop") {
            Search::timeOver = true;
        } else if(inputString == "quit") {
            break;
        }
    }

    Search::timeOver = true;
    {
        std::lock_guard<std::mutex> lk(UCI::mtx);
        UCI::quitFlag = true;
    }
    UCI::cv.notify_all();
}

void UCI::inputUCI() {
    std::cout << "id name " << engineName << '\n';
    std::cout << "id author Vlad Ciocoiu\n";
    std::cout << "uciok\n";
    std::cout.flush();
}

void UCI::inputIsReady() {
    std::cout << "readyok\n";
    std::cout.flush();
}

// prepare for a new game by clearing hash tables and history/killer tables
void UCI::inputUCINewGame() {
    Search::clearHistory();

    if(board == nullptr) board = new Board();
    else board->clear();

    if(transpositionTable == nullptr) transpositionTable = new TranspositionTable();
    else transpositionTable->clear();
}

void UCI::inputPosition(string input) {
    // split input into words
    vector<string> parsedInput = splitStr(input);
    int movesIdx = 0;

    // load the position
    if(parsedInput[1] == "startpos") {
        board->loadFenPos("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
        movesIdx = 2;
    } else {
        string fen;
        for(int i = 2; i <= 5; i++)
            fen += parsedInput[i] + " ";
        
        if(parsedInput.size() > 6) fen += parsedInput[6] + " ";
        if(parsedInput.size() > 7) fen += parsedInput[7] + " ";
        
        board->loadFenPos(fen);
        movesIdx = 8;
    }

    // make the moves
    for(unsigned int i = movesIdx+1; i < parsedInput.size(); i++) {
        int moves[256];
        int num = board->generateLegalMoves(moves);
        for(int idx = 0; idx < num; idx++) 
            if(BoardUtils::moveToString(moves[idx]) == parsedInput[i]) {
                board->makeMove(moves[idx]);
                break;
            }
    }
}

// perft function that returns the number of positions reached from an initial position after a certain depth
long long UCI::moveGenTest(short depth, bool show) {
    if(depth == 0) return 1;

    int moves[256];
    int num = board->generateLegalMoves(moves);

    if(depth == 1) return num;

    long long numPos = 0;

    for(int idx = 0; idx < num; idx++) {
        board->makeMove(moves[idx]);
        long long mv = moveGenTest(depth-1, false);

        if(show) std::cout << BoardUtils::moveToString(moves[idx]) << ": " << mv << '\n';

        numPos += mv;

        board->unmakeMove(moves[idx]);
    }
    return numPos;
}

// show information related to the search such as the depth, nodes, time etc.
void UCI::showSearchInfo(short depth, int nodes, int startTime, int score) {
    // get current time
    int currTime = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();

    // if time is 0 then make it 1 so we wouldn't divide by 0
    int time = (currTime == startTime ? 1 : currTime - startTime); 

    // nodes searched per second
    U64 nps =  1000LL*nodes/time;

    std::cout << "info score " << scoreToStr(score) << " depth " << depth << " nodes " 
         << nodes << " time " << time << " nps " << nps << " ";

    // print principal variation
    Search::showPV(depth);
}

// function that prints the current board
void UCI::printBoard(bool chars) {
    std::cout << "FEN: " << board->getFenFromCurrPos() << '\n';
    if(chars) {
        unordered_map<int, char> pieceSymbols = {{Pawn, 'p'}, {Knight, 'n'},
        {Bishop, 'b'}, {Rook, 'r'}, {Queen, 'q'}, {King, 'k'}};
        
        for(int i = 0; i < 64; i++) {
            if(board->squares[i] == Empty) std::cout << ". ";
            else if(board->squares[i] & 8) std::cout << char(toupper(pieceSymbols[board->squares[i] ^ 8])) << ' ';
            else std::cout << pieceSymbols[board->squares[i]] << ' ';

            if(i%8 == 7) std::cout << '\n';
        }
    } else {
        for(int i = 0; i < 64; i++) {
            std::cout << (int)board->squares[i] << ' ';
            if(i%8 == 7) std::cout << '\n';
        }  
    }
    std::cout << '\n';
}

void UCI::printEval() {
    std::cout << evaluate() * (board->turn == Black ? -1 : 1) << '\n';
}

void UCI::inputGo() {
    while(true) {
        long long time = -1, inc = 0, movesToGo = -1, moveTime = -1;
        short depth = 100;
        Search::infiniteTime = true;

        std::unique_lock<std::mutex> lk(UCI::mtx);
        UCI::cv.wait(lk, []{ return UCI::startFlag || UCI::quitFlag; });

        if(UCI::quitFlag) return;

        vector<string> parsedInput = splitStr(UCI::goCommand);

        // do a perft and then continue if it is requested
        if(parsedInput[1] == "perft") {
            long long startTime = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();
            long long num = moveGenTest(stoi(parsedInput[2]), true);
            long long endTime = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();
            long long time = max(1LL, endTime-startTime);
            long long nps = 1000LL*num/time;

            std::cout << "nodes " << num << " time " << time << " nps " << nps << '\n';
            std::cout.flush();
            UCI::startFlag = false;
            UCI::cv.notify_all();
            continue;
        }

        // loop through words
        for(unsigned int i = 0; i < parsedInput.size(); i++) {
            // only do a quiescence search
            if(parsedInput[i] == "quiescence") {
                int score = Search::quiescence(-1000000, 1000000);
                std::cout << score * (board->turn == Black ? -1 : 1) << '\n';
                std::cout.flush();
                UCI::startFlag = false;
                UCI::cv.notify_all();
                return;
            }

            // get time and increment according to our color 
            if(parsedInput[i] == "wtime" && board->turn == White) {
                time = stoi(parsedInput[i+1]);
            }
            if(parsedInput[i] == "btime" && board->turn == Black) {
                time = stoi(parsedInput[i+1]);
            }
            if(parsedInput[i] == "winc" && board->turn == White) {
                inc = stoi(parsedInput[i+1]);
            }
            if(parsedInput[i] == "binc" && board->turn == Black) {
                inc = stoi(parsedInput[i+1]);
            }

            // get moves to go
            if(parsedInput[i] == "movestogo") {
                movesToGo = stoi(parsedInput[i+1]);
            }
            
            // get move time if any
            if(parsedInput[i] == "movetime") {
                moveTime = stoi(parsedInput[i+1]);
            }

            // get max depth
            if(parsedInput[i] == "depth") {
                depth = stoi(parsedInput[i+1]);
            }
        }
// 
        // if depth is specified, we change the max depth, otherwise we leave it at 100
        Search::currMaxDepth = depth;

        // get current time
        long long startTime = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();
        const long long MOVE_OVERHEAD = 50;

        // if move time is set, we can use it all for the current move (minus overhead)
        if(moveTime != -1) {
            Search::infiniteTime = false;
            long long usable = max(1LL, moveTime - MOVE_OVERHEAD);
            Search::softStopTime = startTime + usable;
            Search::hardStopTime = startTime + usable;
        } else if(time != -1) {
            Search::infiniteTime = false;

            long long usableTime = max(1LL, time - MOVE_OVERHEAD);
            long long mtg = (movesToGo != -1) ? max(1LL, movesToGo) : 30LL;

            // Target optimal time allocation for this move
            long long optTime = (usableTime / mtg) + (inc * 3 / 4);
            optTime = min(optTime, usableTime / 3);
            optTime = max(optTime, 1LL);

            // Soft stop: don't start a new depth if we've already used ~60% of optTime
            long long softTime = max(1LL, optTime * 6 / 10);

            // Hard stop: allow extending up to 2.0x optTime to finish an iteration or resolve a fail-low,
            // strictly capped at 1/3 of remaining usable clock time.
            long long hardTime = min(usableTime / 3, optTime * 2);
            hardTime = max(hardTime, softTime);

            Search::softStopTime = startTime + softTime;
            Search::hardStopTime = startTime + hardTime;
        }

        std::pair<int, int> searchResult = Search::root();
        std::cout << "bestmove " << BoardUtils::moveToString(searchResult.first) << '\n';
        std::cout.flush();

        UCI::startFlag = false;
        UCI::cv.notify_all();
    }
}