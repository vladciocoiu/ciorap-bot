#include <vector>
#include <string>
#include <iostream>
#include <chrono>
#include <unordered_map>
#include <mutex>
#include <condition_variable>

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
    if (abs(score) > Search::MATE_THRESHOLD) return "mate " + to_string((score > 0 ? Search::MATE_EVAL - score + 1 : -Search::MATE_EVAL - score) / 2);

    // the score is initially relative to the side to move, so we change it to be positive for white
    if(board->turn == Black) score *= -1;

    // if the score isn't a mate, we print it in centipawns
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
    while(true) {
        string inputString;
        getline(cin, inputString);

        if(inputString == "uci") {
            inputUCI();
        } else if(inputString == "isready") {
            inputIsReady();
        } else if(inputString == "ucinewgame") {
            inputUCINewGame();
        } else if(inputString.substr(0, 8) == "position") {
            inputPosition(inputString);
        } else if(inputString.substr(0, 2) == "go") {
            {
                std::lock_guard<std::mutex> lk(UCI::mtx);
                UCI::goCommand = inputString;
                UCI::startFlag = true;
            }
            UCI::cv.notify_one();
        } else if(inputString.substr(0, 5) == "print") {
            printBoard(inputString.length() <= 6 || inputString.substr(6, 3) != "num");
        } else if(inputString.substr(0, 4) == "eval") {
            printEval();
        } else if(inputString == "stop") {
            Search::timeOver = true;
        } else if(inputString == "quit") {
            Search::timeOver = true;
            {
                std::lock_guard<std::mutex> lk(UCI::mtx);
                UCI::goCommand = inputString;
                UCI::quitFlag = true;
            }
            UCI::cv.notify_one();
            break;
        }
    }
}

void UCI::inputUCI() {
    std::cout << "id name " << engineName << '\n';
    std::cout << "id author Vlad Ciocoiu\n";
    std::cout << "uciok\n";
}

void UCI::inputIsReady() {
    std::cout << "readyok\n";
}

// prepare for a new game by clearing hash tables and history/killer tables
void UCI::inputUCINewGame() {
    Search::clearHistory();

    delete board;
    board = new Board();

    delete transpositionTable;
    transpositionTable = new TranspositionTable();
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
    std::cout.flush();

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

        UCI::startFlag = false;

        vector<string> parsedInput = splitStr(UCI::goCommand);

        // do a perft and then continue if it is requested
        if(parsedInput[1] == "perft") {
            long long startTime = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();
            long long num = moveGenTest(stoi(parsedInput[2]), true);
            long long endTime = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();
            long long time = max(1LL, endTime-startTime);
            long long nps = 1000LL*num/time;

            std::cout << "nodes " << num << " time " << time << " nps " << nps << '\n';
            continue;
        }

        // loop through words
        for(unsigned int i = 0; i < parsedInput.size(); i++) {
            // only do a quiescence search
            if(parsedInput[i] == "quiescence") {
                int score = Search::quiescence(-1000000, 1000000);
                std::cout << score * (board->turn == Black ? -1 : 1) << '\n';
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

        if(movesToGo != -1) movesToGo += 2;
        else movesToGo = 80;

        // get current time;
        long long startTime = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();
        
        // if move time is set, we can use it all for the current move
        if(moveTime != -1) {
            time = moveTime;
            movesToGo = 1;

        // otherwise consider 
        }
        if(time != -1) {
            Search::infiniteTime = false;
            time /= movesToGo;
        }

        if(time > 1000) time -= 500;
        else if(time > 100) time -= 50;

        // set the stop time
        Search::stopTime = startTime + time + inc;

        std::pair<int, int> searchResult = Search::root();
        std::cout << "bestmove " << BoardUtils::moveToString(searchResult.first) << '\n';
        std::cout.flush();
    }
}