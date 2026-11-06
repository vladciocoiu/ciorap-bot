#include <unordered_map>
#include <string>
#include <stack>
#include <iostream>
#include <cassert>

#include "Board.h"
#include "MagicBitboardUtils.h"
#include "TranspositionTable.h"
#include "Search.h"
#include "MoveUtils.h"
#include "BoardUtils.h"
#include "Enums.h"

using namespace std;

Board::Board() : halfmoveClock(0), repetitionIndex(0), repetitionMap(new U64[2048]) {
    clear();
}

Board::~Board() {
    delete[] repetitionMap;
}


// initialize all the variables before starting the actual engine
void init() {
    // bitmasks for every bit from 0 to 63
    for(int i = 0; i < 64; i++)
        BoardUtils::bits[i] = (1LL << i);

    // initialize zobrist numbers in order to make zobrist hash keys
    TranspositionTable::generateZobristHashNumbers();

    // bitboard for checking empty squares between king and rook when castling
    BoardUtils::castleMask[0] =(BoardUtils::bits[f1] | BoardUtils::bits[g1]);
    BoardUtils::castleMask[1] = (BoardUtils::bits[b1] | BoardUtils::bits[c1] | BoardUtils::bits[d1]);
    BoardUtils::castleMask[2] = (BoardUtils::bits[f8] | BoardUtils::bits[g8]);
    BoardUtils::castleMask[3] = (BoardUtils::bits[b8] | BoardUtils::bits[c8] | BoardUtils::bits[d8]);

    // create file and rank masks
    for(int i = 0; i < 64; i++) {
        int file = (i & 7), rank = (i >> 3);

        BoardUtils::filesBB[file] |= BoardUtils::bits[i];
        BoardUtils::ranksBB[rank] |= BoardUtils::bits[i];
    }

    // create pawn attacks masks and vectors
    for(int i = 0; i < 64; i++) {
        int file = (i & 7);

        if(file > 0) {
            if(i+7 < 64) BoardUtils::whitePawnAttacksBB[i] |= BoardUtils::bits[i+7];
            if(i-9 >= 0) BoardUtils::blackPawnAttacksBB[i] |= BoardUtils::bits[i-9];
        }
        if(file < 7) {
            if(i+9 < 64) BoardUtils::whitePawnAttacksBB[i] |= BoardUtils::bits[i+9];
            if(i-7 >= 0) BoardUtils::blackPawnAttacksBB[i] |= BoardUtils::bits[i-7];
        }
    }

    // create knight attacks masks and vectors
    for(int i = 0; i < 64; i++) {
        int file = (i & 7), rank = (i >> 3);

        if(file > 1) {
            if(rank > 0) BoardUtils::knightAttacksBB[i] |= BoardUtils::bits[i-10];
            if(rank < 7) BoardUtils::knightAttacksBB[i] |= BoardUtils::bits[i+6];
        }
        if(file < 6) {
            if(rank > 0) BoardUtils::knightAttacksBB[i] |= BoardUtils::bits[i-6];
            if(rank < 7) BoardUtils::knightAttacksBB[i] |= BoardUtils::bits[i+10];
        }
        if(rank > 1) {
            if(file > 0) BoardUtils::knightAttacksBB[i] |= BoardUtils::bits[i-17];
            if(file < 7) BoardUtils::knightAttacksBB[i] |= BoardUtils::bits[i-15];
        }
        if(rank < 6) {
            if(file > 0) BoardUtils::knightAttacksBB[i] |= BoardUtils::bits[i+15];
            if(file < 7) BoardUtils::knightAttacksBB[i] |= BoardUtils::bits[i+17];
        }
    }

    // create bishop masks
    for(int i = 0; i < 64; i++) {
        for(Direction dir: {northEast, northWest, southEast, southWest}) {
            int sq = i;
            while(BoardUtils::isInBoard(sq, dir)) {
                BoardUtils::bishopMasks[i] |= BoardUtils::bits[sq];
                sq += dir;
            }
        }
        BoardUtils::bishopMasks[i] ^= BoardUtils::bits[i];
    }

    // create rook masks
    for(int i = 0; i < 64; i++) {
        for(Direction dir: {east, west, north, south}) {
            int sq = i;
            while(BoardUtils::isInBoard(sq, dir)) {
                BoardUtils::rookMasks[i] |= BoardUtils::bits[sq];
                sq += dir;
            }
        }
        BoardUtils::rookMasks[i] ^= BoardUtils::bits[i];
    }

    // create king moves masks and vectors
    for(int i = 0; i < 64; i++) {
        BoardUtils::kingAttacksBB[i] = (BoardUtils::eastOne(BoardUtils::bits[i]) | BoardUtils::westOne(BoardUtils::bits[i]));
        U64 king = (BoardUtils::bits[i] | BoardUtils::kingAttacksBB[i]);
        BoardUtils::kingAttacksBB[i] |= (BoardUtils::northOne(king) | BoardUtils::southOne(king));
    }

    // squares near king are squares that a king can move to and the squares in front of his 'forward' moves
    for(int i = 0; i < 64; i++) {
        BoardUtils::squaresNearWhiteKing[i] = BoardUtils::squaresNearBlackKing[i] = (BoardUtils::kingAttacksBB[i] | BoardUtils::bits[i]);
        if(i+south >= 0) BoardUtils::squaresNearBlackKing[i] |= BoardUtils::kingAttacksBB[i+south];
        if(i+north < 64) BoardUtils::squaresNearWhiteKing[i] |= BoardUtils::kingAttacksBB[i+north];
    }

    // create light and dark squares masks
    for(int i = 0; i < 64; i++) {
        if(i%2) BoardUtils::lightSquaresBB |= BoardUtils::bits[i];
        else BoardUtils::darkSquaresBB |= BoardUtils::bits[i];
    }

    MagicBitboardUtils::initMagics();

    Search::clearHistory();
    if(board == nullptr) board = new Board();
    if(transpositionTable == nullptr) transpositionTable = new TranspositionTable();
}

void Board::clear() {
    for(int i = 0; i < 64; i++) this->squares[i] = Empty;
    whiteKingSquare = blackKingSquare = 0;
    halfmoveClock = 0;
    repetitionIndex = 0;

    this->turn = White;
    this->castleRights = 0;
    this->ep = -1;
    this->hashKey = 0;

    // clear bitboards
    this->whitePiecesBB = this->blackPiecesBB = 0;
    this->pawnsBB = this->knightsBB = this->bishopsBB = this->rooksBB = this->queensBB = 0;

    // clear stacks
    while(!castleStk.empty()) castleStk.pop();
    while(!epStk.empty()) epStk.pop();
    while(!halfmoveStk.empty()) halfmoveStk.pop();
    while(!moveStk.empty()) moveStk.pop();
}

// updates the bitboards when a piece is moved
void Board::updatePieceInBB(int piece, int color, int sq) {
    if(color == White) this->whitePiecesBB ^= BoardUtils::bits[sq];
    else this->blackPiecesBB ^= BoardUtils::bits[sq];

    if(piece == Pawn) this->pawnsBB ^= BoardUtils::bits[sq];
    if(piece == Knight) this->knightsBB ^= BoardUtils::bits[sq];
    if(piece == Bishop) this->bishopsBB ^= BoardUtils::bits[sq];
    if(piece == Rook) this->rooksBB ^= BoardUtils::bits[sq];
    if(piece == Queen) this->queensBB ^= BoardUtils::bits[sq];
}

void Board::movePieceInBB(int piece, int color, int from, int to) {
    this->updatePieceInBB(piece, color, from);
    this->updatePieceInBB(piece, color, to);
}

string Board::getFenFromCurrPos() {
    unordered_map<int, int> pieceSymbols = {{Pawn, 'p'}, {Knight, 'n'},
    {Bishop, 'b'}, {Rook, 'r'}, {Queen, 'q'}, {King, 'k'}};

    string fen;

    int emptySquares = 0;
    // loop through squares
    for(int rank = 7; rank >= 0; rank--)
        for(int file = 0; file < 8; file++) {
            int color = (this->squares[rank*8 + file] & (Black | White));
            int piece = (this->squares[rank*8 + file] ^ color);

            if(this->squares[rank*8 + file] == Empty) {
                emptySquares++;
            } else {
                if(emptySquares) {
                    fen += (emptySquares + '0');
                    emptySquares = 0;
                }
                fen += (color == White ? toupper(pieceSymbols[piece]) : pieceSymbols[piece]);
            }

            if(file == 7) {
                if(emptySquares) fen += (emptySquares+'0');
                emptySquares = 0;

                if(rank > 0) fen += '/';
            }
        }

    // add turn
    fen += (this->turn == White ? " w " : " b ");

    // add castle rights
    string castles;
    if(this->castleRights & BoardUtils::bits[0]) castles += 'K';
    if(this->castleRights & BoardUtils::bits[1]) castles += 'Q';
    if(this->castleRights & BoardUtils::bits[2]) castles += 'k';
    if(this->castleRights & BoardUtils::bits[3]) castles += 'q';
    if(castles.length() == 0) castles += '-';

    fen += castles;
    fen += ' ';

    // add en passant square
    if(this->ep == -1) fen += '-';
    else fen += BoardUtils::square(this->ep);

    //TODO: add halfmove clock and full moves
    fen += " 0 1";

    return fen;
}

// loads the squares array and the bitboards according to the fen position
void Board::loadFenPos(string input) {
    clear();

    // parse the fen string
    string pieces;
    unsigned int i = 0;
    for(; input[i] != ' ' && i < input.length(); i++)
        pieces += input[i];
    i++;

    char turn = input[i];
    i += 2;

    string castles;
    for(; input[i] != ' ' && i < input.length(); i++)
        castles += input[i];
    i++;
            
    string epTargetSq;
    for(; input[i] != ' ' && i < input.length(); i++) 
        epTargetSq += input[i];
    if(i < input.length()) i++;

    string halfmoveStr;
    for(; i < input.length() && input[i] != ' '; i++)
        halfmoveStr += input[i];
    if(!halfmoveStr.empty() && isdigit(halfmoveStr[0]))
        this->halfmoveClock = stoi(halfmoveStr);

    unordered_map<char, int> pieceSymbols = {{'p', Pawn}, {'n', Knight},
    {'b', Bishop}, {'r', Rook}, {'q', Queen}, {'k', King}};

    // loop through squares
    int file = 0, rank = 7;
    for(char p: pieces) {
        if(p == '/') {
            rank--;
            file = 0;
        } else {
            // if it is a digit skip the squares
            if(p-'0' >= 1 && p-'0' <= 8) {
                for(char i = 0; i < (p-'0'); i++) {
                    this->squares[rank*8 + file] = 0;
                    file++;
                }
            } else {
                int color = ((p >= 'A' && p <= 'Z') ? White : Black);
                int type = pieceSymbols[tolower(p)];

                this->updatePieceInBB(type, color, rank*8 + file);
                if(type == King) {
                    if(color == White) this->whiteKingSquare = rank*8 + file;
                    if(color == Black) this->blackKingSquare = rank*8 + file;
                }

                this->squares[rank*8 + file] = (color | type);

                file++;
            }
        }
    }

    // get turn
    this->turn = (turn == 'w' ? White : Black);

    // get castling rights
    unordered_map<char, U64> castleSymbols = {{'K', BoardUtils::bits[0]}, {'Q', BoardUtils::bits[1]}, {'k', BoardUtils::bits[2]}, {'q', BoardUtils::bits[3]}, {'-', 0}};
    this->castleRights = 0;

    for(char c: castles)
        this->castleRights |= castleSymbols[c];

    // get en passant target square
    this->ep = (epTargetSq == "-" ? -1 : (epTargetSq[0]-'a' + 8*(epTargetSq[1]-'1')));

    // initialize hash key
    this->hashKey = getZobristHashFromCurrPos();
}

// pseudo legal moves are moves that can be made but might leave the king in check
short Board::generatePseudoLegalMoves() {
    short numberOfMoves = 0;

    int color = this->turn;

    U64 ourPiecesBB = (color == White ? this->whitePiecesBB : this->blackPiecesBB);
    U64 opponentPiecesBB = (color == White ? this->blackPiecesBB : this->whitePiecesBB);
    U64 allPiecesBB = (this->whitePiecesBB | this->blackPiecesBB);

    // -----pawns-----
    U64 ourPawnsBB = (ourPiecesBB & this->pawnsBB);
    int pawnDir = (color == White ? north : south);
    int pawnStartRank = (color == White ? 1 : 6);
    int pawnPromRank = (color == White ? 7 : 0);

    while(ourPawnsBB) {
        int sq = MagicBitboardUtils::bitscanForward(ourPawnsBB);
        bool isPromoting = (((sq+pawnDir) >> 3) == pawnPromRank);

        // normal moves and promotions
        if((allPiecesBB & BoardUtils::bits[sq+pawnDir]) == 0) {
            assert(this->squares[sq] == (Pawn | color));

            if((sq >> 3) == pawnStartRank && (allPiecesBB & BoardUtils::bits[sq+2*pawnDir]) == 0)
                pseudoLegalMoves[numberOfMoves++] = MoveUtils::getMove(sq, sq+2*pawnDir, color, Pawn, 0, 0, 0, 0);

            if(isPromoting) {
                for(int piece: {Knight, Bishop, Rook, Queen})
                    pseudoLegalMoves[numberOfMoves++] = MoveUtils::getMove(sq, sq+pawnDir, color, Pawn, 0, piece, 0, 0);
            } else pseudoLegalMoves[numberOfMoves++] = MoveUtils::getMove(sq, sq+pawnDir, color, Pawn, 0, 0, 0, 0);

        }

        // captures and capture-promotions
        U64 pawnCapturesBB = (color == White ? BoardUtils::whitePawnAttacksBB[sq] : BoardUtils::blackPawnAttacksBB[sq]);
        pawnCapturesBB &= opponentPiecesBB;
        while(pawnCapturesBB) {
            assert(this->squares[sq] == (Pawn | color));

            int to = MagicBitboardUtils::bitscanForward(pawnCapturesBB);
            if(isPromoting) {
                for(int piece: {Knight, Bishop, Rook, Queen})
                    pseudoLegalMoves[numberOfMoves++] = MoveUtils::getMove(sq, to, color, Pawn, (this->squares[to] & (~8)), piece, 0, 0);
            } else pseudoLegalMoves[numberOfMoves++] = MoveUtils::getMove(sq, to, color, Pawn, (this->squares[to] & (~8)), 0, 0, 0);

            pawnCapturesBB &= (pawnCapturesBB-1);

        }

        ourPawnsBB &= (ourPawnsBB-1);
    }

    //-----knights-----
    U64 ourKnightsBB = (this->knightsBB & ourPiecesBB);

    while(ourKnightsBB) {
        int sq = MagicBitboardUtils::bitscanForward(ourKnightsBB);

        U64 knightMoves = (BoardUtils::knightAttacksBB[sq] & ~ourPiecesBB);
        while(knightMoves) {
            assert(this->squares[sq] == (Knight | color));

            int to = MagicBitboardUtils::bitscanForward(knightMoves);
            pseudoLegalMoves[numberOfMoves++] = MoveUtils::getMove(sq, to, color, Knight, (this->squares[to] & (~8)), 0, 0, 0);
            knightMoves &= (knightMoves-1);
        }

        ourKnightsBB &= (ourKnightsBB-1);
    }

    //-----king-----
    int kingSquare = (color == White ? this->whiteKingSquare : this->blackKingSquare);
    U64 ourKingMoves = (BoardUtils::kingAttacksBB[kingSquare] & ~ourPiecesBB);

    while(ourKingMoves) {
        assert(this->squares[kingSquare] == (King | color));

        int to = MagicBitboardUtils::bitscanForward(ourKingMoves);
        pseudoLegalMoves[numberOfMoves++] = MoveUtils::getMove(kingSquare, to, color, King, (this->squares[to] & (~8)), 0, 0, 0);
        ourKingMoves &= (ourKingMoves-1);
    }

    //-----sliding pieces-----
    U64 rooksQueens = (ourPiecesBB & (this->rooksBB | this->queensBB));
    while(rooksQueens) {
        int sq = MagicBitboardUtils::bitscanForward(rooksQueens);

        U64 rookMoves = (MagicBitboardUtils::magicRookAttacks(allPiecesBB, sq) & ~ourPiecesBB);
        while(rookMoves) {
            assert(this->squares[sq] == (Rook | color) || this->squares[sq] == (Queen | color));

            int to = MagicBitboardUtils::bitscanForward(rookMoves);
            pseudoLegalMoves[numberOfMoves++] = MoveUtils::getMove(sq, to, color, (this->squares[sq] & (~8)), (this->squares[to] & (~8)), 0, 0, 0);
            rookMoves &= (rookMoves-1);
        }

        rooksQueens &= (rooksQueens-1);
    }

    U64 bishopsQueens = (ourPiecesBB & (this->bishopsBB | this->queensBB));
    while(bishopsQueens) {
        int sq = MagicBitboardUtils::bitscanForward(bishopsQueens);

        U64 bishopMoves = (MagicBitboardUtils::magicBishopAttacks(allPiecesBB, sq) & ~ourPiecesBB);
        while(bishopMoves) {
             assert(this->squares[sq] == (Bishop | color) || this->squares[sq] == (Queen | color));

            int to = MagicBitboardUtils::bitscanForward(bishopMoves);
            pseudoLegalMoves[numberOfMoves++] = MoveUtils::getMove(sq, to, color, (this->squares[sq] & (~8)), (this->squares[to] & (~8)), 0, 0, 0);
            bishopMoves &= (bishopMoves-1);
        }

        bishopsQueens &= (bishopsQueens-1);
    }

    // -----castles-----
    int allowedCastles = (color == White ? 3 : 12);
    for(int i = 0; i < 4; i++) {
        if((this->castleRights & BoardUtils::bits[i]) && ((allPiecesBB & BoardUtils::castleMask[i]) == 0) && (allowedCastles & BoardUtils::bits[i])) {
            assert(this->squares[BoardUtils::castleStartSq[i]] == (King | color));

            pseudoLegalMoves[numberOfMoves++] = MoveUtils::getMove(BoardUtils::castleStartSq[i], BoardUtils::castleEndSq[i], color, King, 0, 0, 1, 0);
        }
    }


    // -----en passant-----
    if(ep != -1) {
        ourPawnsBB = (this->pawnsBB & ourPiecesBB);
        U64 epBB = ((color == White ? BoardUtils::blackPawnAttacksBB[ep] : BoardUtils::whitePawnAttacksBB[ep]) & ourPawnsBB);
        while(epBB) {
            int sq = MagicBitboardUtils::bitscanForward(epBB);

            assert(this->squares[sq] == (Pawn | color));

            pseudoLegalMoves[numberOfMoves++] = MoveUtils::getMove(sq, ep, color, Pawn, Pawn, 0, 0, 1);
            epBB &= (epBB-1);
        }
    }
    return numberOfMoves;
}

short Board::generatePseudoLegalMovesQS() {
    short numberOfMoves = 0;

    int color = this->turn;

    U64 ourPiecesBB = (color == White ? this->whitePiecesBB : this->blackPiecesBB);
    U64 opponentPiecesBB = (color == White ? this->blackPiecesBB : this->whitePiecesBB);
    U64 allPiecesBB = (this->whitePiecesBB | this->blackPiecesBB);

    // -----pawns-----
    U64 ourPawnsBB = (ourPiecesBB & this->pawnsBB);
    int pawnDir = (color == White ? north : south);
    int pawnPromRank = (color == White ? 7 : 0);

    while(ourPawnsBB) {
        int sq = MagicBitboardUtils::bitscanForward(ourPawnsBB);
        bool isPromoting = (((sq+pawnDir) >> 3) == pawnPromRank);

        // normal promotions
        if((allPiecesBB & BoardUtils::bits[sq+pawnDir]) == 0) {
            assert(this->squares[sq] == (Pawn | color));

            if(isPromoting) {
                for(int piece: {Knight, Bishop, Rook, Queen})
                    pseudoLegalMoves[numberOfMoves++] = MoveUtils::getMove(sq, sq+pawnDir, color, Pawn, 0, piece, 0, 0);
            }

        }

        // captures and capture-promotions
        U64 pawnCapturesBB = (color == White ? BoardUtils::whitePawnAttacksBB[sq] : BoardUtils::blackPawnAttacksBB[sq]);
        pawnCapturesBB &= opponentPiecesBB;
        while(pawnCapturesBB) {
            assert(this->squares[sq] == (Pawn | color));

            int to = MagicBitboardUtils::bitscanForward(pawnCapturesBB);
            if(isPromoting) {
                for(int piece: {Knight, Bishop, Rook, Queen})
                    pseudoLegalMoves[numberOfMoves++] = MoveUtils::getMove(sq, to, color, Pawn, (this->squares[to] & (~8)), piece, 0, 0);
            } else pseudoLegalMoves[numberOfMoves++] = MoveUtils::getMove(sq, to, color, Pawn, (this->squares[to] & (~8)), 0, 0, 0);

            pawnCapturesBB &= (pawnCapturesBB-1);

        }

        ourPawnsBB &= (ourPawnsBB-1);
    }

    //-----knights-----
    U64 ourKnightsBB = (this->knightsBB & ourPiecesBB);

    while(ourKnightsBB) {
        int sq = MagicBitboardUtils::bitscanForward(ourKnightsBB);

        U64 knightMoves = (BoardUtils::knightAttacksBB[sq] & opponentPiecesBB);
        while(knightMoves) {
            assert(this->squares[sq] == (Knight | color));

            int to = MagicBitboardUtils::bitscanForward(knightMoves);
            pseudoLegalMoves[numberOfMoves++] = MoveUtils::getMove(sq, to, color, Knight, (this->squares[to] & (~8)), 0, 0, 0);
            knightMoves &= (knightMoves-1);
        }

        ourKnightsBB &= (ourKnightsBB-1);
    }

    //-----king-----
    int kingSquare = (color == White ? this->whiteKingSquare : this->blackKingSquare);
    U64 ourKingMoves = (BoardUtils::kingAttacksBB[kingSquare] & opponentPiecesBB);

    while(ourKingMoves) {
        assert(this->squares[kingSquare] == (King | color));

        int to = MagicBitboardUtils::bitscanForward(ourKingMoves);
        pseudoLegalMoves[numberOfMoves++] = MoveUtils::getMove(kingSquare, to, color, King, (this->squares[to] & (~8)), 0, 0, 0);
        ourKingMoves &= (ourKingMoves-1);
    }

    //-----sliding pieces-----
    U64 rooksQueens = (ourPiecesBB & (this->rooksBB | this->queensBB));
    while(rooksQueens) {
        int sq = MagicBitboardUtils::bitscanForward(rooksQueens);

        U64 rookMoves = (MagicBitboardUtils::magicRookAttacks(allPiecesBB, sq) & opponentPiecesBB);
        while(rookMoves) {
            assert(this->squares[sq] == (Rook | color) || this->squares[sq] == (Queen | color));

            int to = MagicBitboardUtils::bitscanForward(rookMoves);
            pseudoLegalMoves[numberOfMoves++] = MoveUtils::getMove(sq, to, color, (this->squares[sq] & (~8)), (this->squares[to] & (~8)), 0, 0, 0);
            rookMoves &= (rookMoves-1);
        }

        rooksQueens &= (rooksQueens-1);
    }

    U64 bishopsQueens = (ourPiecesBB & (this->bishopsBB | this->queensBB));
    while(bishopsQueens) {
        int sq = MagicBitboardUtils::bitscanForward(bishopsQueens);

        U64 bishopMoves = (MagicBitboardUtils::magicBishopAttacks(allPiecesBB, sq) & opponentPiecesBB);
        while(bishopMoves) {
             assert(this->squares[sq] == (Bishop | color) || this->squares[sq] == (Queen | color));

            int to = MagicBitboardUtils::bitscanForward(bishopMoves);
            pseudoLegalMoves[numberOfMoves++] = MoveUtils::getMove(sq, to, color, (this->squares[sq] & (~8)), (this->squares[to] & (~8)), 0, 0, 0);
            bishopMoves &= (bishopMoves-1);
        }

        bishopsQueens &= (bishopsQueens-1);
    }

    // -----en passant-----
    if(ep != -1) {
        ourPawnsBB = (this->pawnsBB & ourPiecesBB);
        U64 epBB = ((color == White ? BoardUtils::blackPawnAttacksBB[ep] : BoardUtils::whitePawnAttacksBB[ep]) & ourPawnsBB);
        while(epBB) {
            int sq = MagicBitboardUtils::bitscanForward(epBB);

            assert(this->squares[sq] == (Pawn | color));

            pseudoLegalMoves[numberOfMoves++] = MoveUtils::getMove(sq, ep, color, Pawn, Pawn, 0, 0, 1);
            epBB &= (epBB-1);
        }
    }
    return numberOfMoves;
}

// returns true if the square sq is attacked by enemy pieces
bool Board::isAttacked(int sq) {
    int color = this->turn;
    int otherColor = (color ^ (Black | White));
    int otherKingSquare = (otherColor == White ? this->whiteKingSquare : this->blackKingSquare);

    U64 opponentPiecesBB = (color == White ? this->blackPiecesBB : this->whitePiecesBB);
    U64 allPiecesBB = (this->whitePiecesBB | this->blackPiecesBB);
    U64 bishopsQueens = (opponentPiecesBB & (this->bishopsBB | this->queensBB));
    U64 rooksQueens = (opponentPiecesBB & (this->rooksBB | this->queensBB));

    // king attacks
    if(BoardUtils::kingAttacksBB[otherKingSquare] & BoardUtils::bits[sq])
        return true;

    // knight attacks
    if(BoardUtils::knightAttacksBB[sq] & opponentPiecesBB & this->knightsBB)
        return true;

    // pawn attacks
    if(BoardUtils::bits[sq] & BoardUtils::pawnAttacks((opponentPiecesBB & this->pawnsBB), otherColor))
        return true;

    // sliding piece attacks
    if(bishopsQueens & MagicBitboardUtils::magicBishopAttacks(allPiecesBB, sq))
        return true;

    if(rooksQueens & MagicBitboardUtils::magicRookAttacks(allPiecesBB, sq))
        return true;

    return false;
}

// returns true if the current player is in check
bool Board::isInCheck() {
    int color = this->turn;
    int kingSquare = (color == White ? this->whiteKingSquare : this->blackKingSquare);
    return this->isAttacked(kingSquare);
}

// returns the bitboard that contains all the attackers on the square sq
U64 Board::attacksTo(int sq) {
    int color = (this->turn ^ (Black | White));

    U64 ourPiecesBB = (color == White ? this->whitePiecesBB : this->blackPiecesBB);
    U64 allPiecesBB = (this->whitePiecesBB | this->blackPiecesBB);
    U64 pawnAtt = (color == Black ? BoardUtils::whitePawnAttacksBB[sq] : BoardUtils::blackPawnAttacksBB[sq]);
    U64 rooksQueens = (ourPiecesBB & (this->rooksBB | this->queensBB));
    U64 bishopsQueens = (ourPiecesBB & (this->bishopsBB | this->queensBB));
    int kingSquare = (color == White ? this->whiteKingSquare : this->blackKingSquare);

    U64 res = 0;
    res |= (BoardUtils::knightAttacksBB[sq] & ourPiecesBB & this->knightsBB);
    res |= (pawnAtt & this->pawnsBB & ourPiecesBB);
    res |= (BoardUtils::kingAttacksBB[sq] & BoardUtils::bits[kingSquare]);
    res |= (MagicBitboardUtils::magicBishopAttacks(allPiecesBB, sq) & bishopsQueens);
    res |= (MagicBitboardUtils::magicRookAttacks(allPiecesBB, sq) & rooksQueens);

    return res;
}

// takes the pseudo legal moves and checks if they don't leave the king in check
int Board::generateLegalMoves(int *moves) {
    int color = this->turn;
    int kingSquare = (color == White ? this->whiteKingSquare : this->blackKingSquare);

    U64 allPiecesBB = (this->whitePiecesBB | this->blackPiecesBB);
    U64 opponentPiecesBB = (color == White ? this->blackPiecesBB : this->whitePiecesBB);
    U64 ourPiecesBB = (color == White ? this->whitePiecesBB : this->blackPiecesBB);

    short pseudoNum = this->generatePseudoLegalMoves();
    unsigned int num = 0;

    // remove the king so we can correctly find all squares attacked by sliding pieces, where the king can't go
    if(color == White) this->whitePiecesBB ^= BoardUtils::bits[kingSquare];
    else this->blackPiecesBB ^= BoardUtils::bits[kingSquare];

    U64 checkingPiecesBB = this->attacksTo(kingSquare);
    int checkingPiecesCnt = MagicBitboardUtils::popcount(checkingPiecesBB);
    int checkingPieceIndex = MagicBitboardUtils::bitscanForward(checkingPiecesBB);

    // check ray is the path between the king and the sliding piece checking it
    U64 checkRayBB = 0;
    if(checkingPiecesCnt == 1) {
        checkRayBB |= checkingPiecesBB;
        int dir = BoardUtils::direction(checkingPieceIndex, kingSquare);

        // check direction is a straight line
        if(abs(dir) == north || abs(dir) == east)
            checkRayBB |= (MagicBitboardUtils::magicRookAttacks(allPiecesBB, kingSquare) & MagicBitboardUtils::magicRookAttacks(allPiecesBB, checkingPieceIndex));

        // check direction is a diagonal
        else checkRayBB |= (MagicBitboardUtils::magicBishopAttacks(allPiecesBB, kingSquare) & MagicBitboardUtils::magicBishopAttacks(allPiecesBB, checkingPieceIndex));
    }

    // finding the absolute pins
    U64 pinnedBB = 0;

    U64 attacks = MagicBitboardUtils::magicRookAttacks(allPiecesBB, kingSquare); // attacks on the rook directions from the king square to our pieces
    U64 blockers = (ourPiecesBB & attacks); // potentially pinned pieces
    U64 oppRooksQueens = ((this->rooksBB | this->queensBB) & opponentPiecesBB);
    U64 pinners = ((attacks ^ MagicBitboardUtils::magicRookAttacks((allPiecesBB ^ blockers), kingSquare)) & oppRooksQueens); // get pinners by computing attacks on the board without the blockers
    while(pinners) {
        int sq = MagicBitboardUtils::bitscanForward(pinners);

        // get pinned pieces by &-ing attacks from the rook square with attacks from the king square, and then with our own pieces
        pinnedBB |= (MagicBitboardUtils::magicRookAttacks(allPiecesBB, sq) & MagicBitboardUtils::magicRookAttacks(allPiecesBB, kingSquare) & ourPiecesBB);
        pinners &= (pinners-1); // remove bit
    }

    attacks = MagicBitboardUtils::magicBishopAttacks(allPiecesBB, kingSquare);
    blockers = (ourPiecesBB & attacks);
    U64 oppBishopsQueens = ((this->bishopsBB | this->queensBB) & opponentPiecesBB);
    pinners = ((attacks ^ MagicBitboardUtils::magicBishopAttacks((allPiecesBB ^ blockers), kingSquare)) & oppBishopsQueens);
    while(pinners) {
        int sq = MagicBitboardUtils::bitscanForward(pinners);
        pinnedBB |= (MagicBitboardUtils::magicBishopAttacks(allPiecesBB, sq) & MagicBitboardUtils::magicBishopAttacks(allPiecesBB, kingSquare) & ourPiecesBB);
        pinners &= (pinners-1);
    }

    for(int idx = 0; idx < pseudoNum; idx++) {
        int from = MoveUtils::getFromSq(pseudoLegalMoves[idx]);
        int to = MoveUtils::getToSq(pseudoLegalMoves[idx]);

        // we can always move the king to a safe square
        if(from == kingSquare) {
            if(this->isAttacked(to) == false) moves[num++] = pseudoLegalMoves[idx];
            continue;
        }

        // single check, can capture the attacker or intercept the check only if the moving piece is not pinned
        if(checkingPiecesCnt == 1 && ((pinnedBB & BoardUtils::bits[from]) == 0)) {

            // capturing the checking pawn by en passant (special case)
            if(MoveUtils::isEP(pseudoLegalMoves[idx]) && checkingPieceIndex == to + (color == White ? south: north))
                moves[num++] = pseudoLegalMoves[idx];

            // check ray includes interception or capturing the attacker
            else if(checkRayBB & BoardUtils::bits[to]) {
                moves[num++] = pseudoLegalMoves[idx];
            }
        }

        // no checks, every piece can move if it is not pinned or it moves in the direction of the pin
        if(checkingPiecesCnt == 0) {
            // not pinned
            if((pinnedBB & BoardUtils::bits[from]) == 0)
                moves[num++] = pseudoLegalMoves[idx];

            // pinned, can only move in the pin direction
            else if(abs(BoardUtils::direction(from, to)) == abs(BoardUtils::direction(from, kingSquare)))
                moves[num++] = pseudoLegalMoves[idx];
        }
    }

    // en passant are the last added pseudo legal moves
    int epMoves[2];
    int numEp = 0;
    while(num && MoveUtils::isEP(moves[num-1])) {
        epMoves[numEp++] = moves[--num];
    }

    assert(numEp <= 2);

    // before ep there are the castle moves so we do the same
    int castleMoves[2];
    int numCastles = 0;
    while(num && MoveUtils::isCastle(moves[num-1])) {
        castleMoves[numCastles++] = moves[--num];
    }
    assert(numCastles <= 2);

    // manual check for the legality of castle moves
    for(int idx = 0; idx < numCastles; idx++) {
        int from = MoveUtils::getFromSq(castleMoves[idx]);
        int to = MoveUtils::getToSq(castleMoves[idx]);

        int first = min(from, to);
        int second = max(from, to);

        bool ok = true;

        for(int i = first; i <= second; i++)
            if(this->isAttacked(i))
                ok = false;

        if(ok) moves[num++] = castleMoves[idx];
    }

    // ep horizontal and diagonal pin check
    for(int idx = 0; idx < numEp; idx++) {
        int from = MoveUtils::getFromSq(epMoves[idx]);
        int to = MoveUtils::getToSq(epMoves[idx]);

        int epRank = (from >> 3);
        int otherPawnSquare = (epRank << 3) | (to & 7);

        // move the capturing pawn to 'to' and remove the captured pawn to check if king is left in check
        U64 toggleWhite = (color == White ? (BoardUtils::bits[from] | BoardUtils::bits[to]) : BoardUtils::bits[otherPawnSquare]);
        U64 toggleBlack = (color == Black ? (BoardUtils::bits[from] | BoardUtils::bits[to]) : BoardUtils::bits[otherPawnSquare]);

        this->whitePiecesBB ^= toggleWhite;
        this->blackPiecesBB ^= toggleBlack;
        this->pawnsBB ^= (BoardUtils::bits[from] | BoardUtils::bits[to] | BoardUtils::bits[otherPawnSquare]);

        U64 attacks = this->attacksTo(kingSquare);
        if(attacks == 0) moves[num++] = epMoves[idx];

        this->whitePiecesBB ^= toggleWhite;
        this->blackPiecesBB ^= toggleBlack;
        this->pawnsBB ^= (BoardUtils::bits[from] | BoardUtils::bits[to] | BoardUtils::bits[otherPawnSquare]);
    }

    // put the king back
    if(color == White) this->whitePiecesBB ^= BoardUtils::bits[kingSquare];
    else this->blackPiecesBB ^= BoardUtils::bits[kingSquare];

    return num;
}

int Board::generateLegalMovesQS(int *moves) {
    int color = this->turn;
    int kingSquare = (color == White ? this->whiteKingSquare : this->blackKingSquare);

    U64 allPiecesBB = (this->whitePiecesBB | this->blackPiecesBB);
    U64 opponentPiecesBB = (color == White ? this->blackPiecesBB : this->whitePiecesBB);
    U64 ourPiecesBB = (color == White ? this->whitePiecesBB : this->blackPiecesBB);

    short pseudoNum = this->generatePseudoLegalMovesQS();
    unsigned int num = 0;

    // remove the king so we can correctly find all squares attacked by sliding pieces, where the king can't go
    if(color == White) this->whitePiecesBB ^= BoardUtils::bits[kingSquare];
    else this->blackPiecesBB ^= BoardUtils::bits[kingSquare];

    U64 checkingPiecesBB = this->attacksTo(kingSquare);
    int checkingPiecesCnt = MagicBitboardUtils::popcount(checkingPiecesBB);
    int checkingPieceIndex = MagicBitboardUtils::bitscanForward(checkingPiecesBB);

    // check ray is the path between the king and the sliding piece checking it
    U64 checkRayBB = 0;
    if(checkingPiecesCnt == 1) {
        checkRayBB |= checkingPiecesBB;
        int dir = BoardUtils::direction(checkingPieceIndex, kingSquare);

        // check direction is a straight line
        if(abs(dir) == north || abs(dir) == east)
            checkRayBB |= (MagicBitboardUtils::magicRookAttacks(allPiecesBB, kingSquare) & MagicBitboardUtils::magicRookAttacks(allPiecesBB, checkingPieceIndex));

        // check direction is a diagonal
        else checkRayBB |= (MagicBitboardUtils::magicBishopAttacks(allPiecesBB, kingSquare) & MagicBitboardUtils::magicBishopAttacks(allPiecesBB, checkingPieceIndex));
    }

    // finding the absolute pins
    U64 pinnedBB = 0;

    U64 attacks = MagicBitboardUtils::magicRookAttacks(allPiecesBB, kingSquare); // attacks on the rook directions from the king square to our pieces
    U64 blockers = (ourPiecesBB & attacks); // potentially pinned pieces
    U64 oppRooksQueens = ((this->rooksBB | this->queensBB) & opponentPiecesBB);
    U64 pinners = ((attacks ^ MagicBitboardUtils::magicRookAttacks((allPiecesBB ^ blockers), kingSquare)) & oppRooksQueens); // get pinners by computing attacks on the board without the blockers
    while(pinners) {
        int sq = MagicBitboardUtils::bitscanForward(pinners);

        // get pinned pieces by &-ing attacks from the rook square with attacks from the king square, and then with our own pieces
        pinnedBB |= (MagicBitboardUtils::magicRookAttacks(allPiecesBB, sq) & MagicBitboardUtils::magicRookAttacks(allPiecesBB, kingSquare) & ourPiecesBB);
        pinners &= (pinners-1); // remove bit
    }

    attacks = MagicBitboardUtils::magicBishopAttacks(allPiecesBB, kingSquare);
    blockers = (ourPiecesBB & attacks);
    U64 oppBishopsQueens = ((this->bishopsBB | this->queensBB) & opponentPiecesBB);
    pinners = ((attacks ^ MagicBitboardUtils::magicBishopAttacks((allPiecesBB ^ blockers), kingSquare)) & oppBishopsQueens);
    while(pinners) {
        int sq = MagicBitboardUtils::bitscanForward(pinners);
        pinnedBB |= (MagicBitboardUtils::magicBishopAttacks(allPiecesBB, sq) & MagicBitboardUtils::magicBishopAttacks(allPiecesBB, kingSquare) & ourPiecesBB);
        pinners &= (pinners-1);
    }

    for(int idx = 0; idx < pseudoNum; idx++) {
        int from = MoveUtils::getFromSq(pseudoLegalMoves[idx]);
        int to = MoveUtils::getToSq(pseudoLegalMoves[idx]);

        // we can always move the king to a safe square
        if(from == kingSquare) {
            if(this->isAttacked(to) == false) moves[num++] = pseudoLegalMoves[idx];
            continue;
        }

        // single check, can capture the attacker or intercept the check only if the moving piece is not pinned
        if(checkingPiecesCnt == 1 && ((pinnedBB & BoardUtils::bits[from]) == 0)) {

            // capturing the checking pawn by en passant (special case)
            if(MoveUtils::isEP(pseudoLegalMoves[idx]) && checkingPieceIndex == to + (color == White ? south: north))
                moves[num++] = pseudoLegalMoves[idx];

            // check ray includes interception or capturing the attacker
            else if(checkRayBB & BoardUtils::bits[to]) {
                moves[num++] = pseudoLegalMoves[idx];
            }
        }

        // no checks, every piece can move if it is not pinned or it moves in the direction of the pin
        if(checkingPiecesCnt == 0) {
            // not pinned
            if((pinnedBB & BoardUtils::bits[from]) == 0)
                moves[num++] = pseudoLegalMoves[idx];

            // pinned, can only move in the pin direction
            else if(abs(BoardUtils::direction(from, to)) == abs(BoardUtils::direction(from, kingSquare)))
                moves[num++] = pseudoLegalMoves[idx];
        }
    }

    // en passant are the last added pseudo legal moves
    int epMoves[2];
    int numEp = 0;
    while(num && MoveUtils::isEP(moves[num-1])) {
        epMoves[numEp++] = moves[--num];
    }

    assert(numEp <= 2);

    // ep horizontal and diagonal pin check
    for(int idx = 0; idx < numEp; idx++) {
        int from = MoveUtils::getFromSq(epMoves[idx]);
        int to = MoveUtils::getToSq(epMoves[idx]);

        int epRank = (from >> 3);
        int otherPawnSquare = (epRank << 3) | (to & 7);

        // move the capturing pawn to 'to' and remove the captured pawn to check if king is left in check
        U64 toggleWhite = (color == White ? (BoardUtils::bits[from] | BoardUtils::bits[to]) : BoardUtils::bits[otherPawnSquare]);
        U64 toggleBlack = (color == Black ? (BoardUtils::bits[from] | BoardUtils::bits[to]) : BoardUtils::bits[otherPawnSquare]);

        this->whitePiecesBB ^= toggleWhite;
        this->blackPiecesBB ^= toggleBlack;
        this->pawnsBB ^= (BoardUtils::bits[from] | BoardUtils::bits[to] | BoardUtils::bits[otherPawnSquare]);

        U64 attacks = this->attacksTo(kingSquare);
        if(attacks == 0) moves[num++] = epMoves[idx];

        this->whitePiecesBB ^= toggleWhite;
        this->blackPiecesBB ^= toggleBlack;
        this->pawnsBB ^= (BoardUtils::bits[from] | BoardUtils::bits[to] | BoardUtils::bits[otherPawnSquare]);
    }

    // put the king back
    if(color == White) this->whitePiecesBB ^= BoardUtils::bits[kingSquare];
    else this->blackPiecesBB ^= BoardUtils::bits[kingSquare];

    return num;
}

// make a move, updating the squares and bitboards
void Board::makeMove(int move) {
    repetitionMap[repetitionIndex++] = hashKey;
    halfmoveStk.push(this->halfmoveClock);

    this->updateHashKey(move);

    if(move == MoveUtils::NO_MOVE) { // null move
        epStk.push(this->ep);
        this->ep = -1;
        this->halfmoveClock++;
        this->turn ^= 8;

        return;
    }

    // push the current castle and ep info in order to retrieve it when we unmake the move
    castleStk.push(this->castleRights);
    epStk.push(this->ep);

    moveStk.push(move);

    // get move info
    int from = MoveUtils::getFromSq(move);
    int to = MoveUtils::getToSq(move);

    int color = MoveUtils::getColor(move);
    int piece = MoveUtils::getPiece(move);
    int otherColor = (color ^ 8);
    int otherPiece = MoveUtils::getCapturedPiece(move);
    int promotionPiece = MoveUtils::getPromotionPiece(move);

    bool isMoveEP = MoveUtils::isEP(move);
    bool isMoveCapture = MoveUtils::isCapture(move);
    bool isMoveCastle = MoveUtils::isCastle(move);

    if(piece == Pawn || isMoveCapture) this->halfmoveClock = 0;
    else this->halfmoveClock++;

    // update bitboards
    this->updatePieceInBB(piece, color, from);
    if(!isMoveEP && isMoveCapture) this->updatePieceInBB(otherPiece, otherColor, to);
    if(!promotionPiece) this->updatePieceInBB(piece, color, to);

    this->squares[to] = this->squares[from];
    this->squares[from] = Empty;

    // promote pawn
    if(promotionPiece) {
        this->updatePieceInBB(promotionPiece, color, to);
        this->squares[to] = (promotionPiece | color);
    }

    if(piece == King) {
        // bit mask for removing castle rights
        int mask = (color == White ? 12 : 3);
        this->castleRights &= mask;

        if(color == White) this->whiteKingSquare = to;
        else this->blackKingSquare = to;
    }

    // remove the respective castling right if a rook moves or gets captured
    if((this->castleRights & BoardUtils::bits[1]) && (from == a1 || to == a1))
        this->castleRights ^= BoardUtils::bits[1];
    if((this->castleRights & BoardUtils::bits[0]) && (from == h1 || to == h1))
        this->castleRights ^= BoardUtils::bits[0];
    if((this->castleRights & BoardUtils::bits[3]) && (from == a8 || to == a8))
        this->castleRights ^= BoardUtils::bits[3];
    if((this->castleRights & BoardUtils::bits[2]) && (from == h8 || to == h8))
        this->castleRights ^= BoardUtils::bits[2];

    // move the rook if castle
    if(isMoveCastle) {
        int rank = (to >> 3), file = (to & 7);
        int rookStartSquare = (rank << 3) + (file == 6 ? 7 : 0);
        int rookEndSquare = (rank << 3) + (file == 6 ? 5 : 3);

        this->movePieceInBB(Rook, color, rookStartSquare, rookEndSquare);
        swap(this->squares[rookStartSquare], this->squares[rookEndSquare]);
    }

    // remove the captured pawn if en passant
    if(isMoveEP) {
        int capturedPawnSquare = to+(color == White ? south : north);

        this->updatePieceInBB(Pawn, otherColor, capturedPawnSquare);
        this->squares[capturedPawnSquare] = Empty;
    }

    this->ep = -1;
    if(piece == Pawn && abs(from-to) == 16)
        this->ep = to+(color == White ? -8 : 8);

    // switch turn
    this->turn ^= (Black | White);
}

// basically the inverse of makeMove but we take the previous en passant square and castling rights from the stacks
void Board::unmakeMove(int move) {
    assert(epStk.top() >= -1 && epStk.top() < 64);

    repetitionIndex--;
    this->halfmoveClock = halfmoveStk.top();
    halfmoveStk.pop();

    if(move == MoveUtils::NO_MOVE) { // null move
        this->ep = epStk.top();
        epStk.pop();
        this->turn ^= 8;

        this->updateHashKey(move);

        return;
    }

    // get move info
    int from = MoveUtils::getFromSq(move);
    int to = MoveUtils::getToSq(move);

    int color = MoveUtils::getColor(move);
    int piece = MoveUtils::getPiece(move);
    int otherColor = (color ^ 8);
    int otherPiece = MoveUtils::getCapturedPiece(move);
    int promotionPiece = MoveUtils::getPromotionPiece(move);

    bool isMoveEP = MoveUtils::isEP(move);
    bool isMoveCapture = MoveUtils::isCapture(move);
    bool isMoveCastle = MoveUtils::isCastle(move);

    // retrieve previous castle and ep info
    this->ep = epStk.top();
    this->castleRights = castleStk.top();
    epStk.pop();
    castleStk.pop();
    
    moveStk.pop();

    if(piece == King) {
        if(color == White) this->whiteKingSquare = from;
        if(color == Black) this->blackKingSquare = from;
    }

    this->updatePieceInBB((this->squares[to] ^ color), color, to);

    if(promotionPiece) this->squares[to] = (Pawn | color);

    this->updatePieceInBB(piece, color, from);
    if(isMoveCapture && !isMoveEP) this->updatePieceInBB(otherPiece, otherColor, to);

    this->squares[from] = this->squares[to];
    if(isMoveCapture) this->squares[to] = (otherPiece | otherColor);
    else this->squares[to] = Empty;

    if(isMoveCastle) {
        int rank = (to >> 3), file = (to & 7);
        int rookStartSquare = (rank << 3) + (file == 6 ? 7 : 0);
        int rookEndSquare = (rank << 3) + (file == 6 ? 5 : 3);

        this->movePieceInBB(Rook, color, rookEndSquare, rookStartSquare);
        swap(this->squares[rookStartSquare], this->squares[rookEndSquare]);
    }

    if(isMoveEP) {
        int capturedPawnSquare = to+(color == White ? south : north);

        this->updatePieceInBB(Pawn, otherColor, capturedPawnSquare);
        this->squares[capturedPawnSquare] = (otherColor | Pawn);
        this->squares[to] = Empty;
    }

    this->turn ^= (Black | White);

    this->updateHashKey(move);
}

bool Board::checkRepetition() {
    int minIdx = max(0, repetitionIndex - halfmoveClock);
    for(int i = repetitionIndex-2; i >= minIdx; i -= 2) {
        if(repetitionMap[i] == hashKey) return true;
    }

    return false;
}

// draw by insufficient material, fifty-move rule, or repetition
bool Board::isDraw() {
    if(this->halfmoveClock >= 100) return true;
    if(checkRepetition()) return true;

    if(this->queensBB | this->rooksBB | this->pawnsBB) return false;

    if((this->knightsBB | this->bishopsBB) == 0) return true; // king vs king

    int whiteBishops = MagicBitboardUtils::popcount(this->bishopsBB & this->whitePiecesBB);
    int blackBishops = MagicBitboardUtils::popcount(this->bishopsBB & this->blackPiecesBB);
    int whiteKnights = MagicBitboardUtils::popcount(this->knightsBB & this->whitePiecesBB);
    int blackKnights = MagicBitboardUtils::popcount(this->knightsBB & this->blackPiecesBB);

    if(whiteKnights + blackKnights + whiteBishops + blackBishops == 1) return true; // king and minor piece vs king

    if(whiteKnights + blackKnights == 0 && whiteBishops == 1 && blackBishops == 1) {
        int lightSquareBishops = MagicBitboardUtils::popcount(BoardUtils::lightSquaresBB & this->bishopsBB);
        if(lightSquareBishops == 0 || lightSquareBishops == 2) return true; // king and bishop vs king and bishop with same color bishops
    }

    return false;
}


// xor zobrist numbers corresponding to all features of the current position
U64 Board::getZobristHashFromCurrPos() {
    U64 key = 0;
    for(int i = 0; i < 64; i++) {
        if(squares[i] == Empty) continue;
        int color = (squares[i] & (Black | White));
        int piece = (squares[i] ^ color);

        key ^= TranspositionTable::pieceZobristNumbers[piece][(int)(color == White)][i];
    }

    key ^= TranspositionTable::castleZobristNumbers[castleRights];
    if(ep != -1) key ^= TranspositionTable::epZobristNumbers[ep % 8];
    if(turn == Black) key ^= TranspositionTable::blackTurnZobristNumber;

    return key;
}

// update the hash key after making a move
void Board::updateHashKey(int move) {
    if(move == MoveUtils::NO_MOVE) { // null move
        if(this->ep != -1) this->hashKey ^= TranspositionTable::epZobristNumbers[this->ep % 8];
        this->hashKey ^= TranspositionTable::blackTurnZobristNumber;
        return;
    }

    // get move info
    int from = MoveUtils::getFromSq(move);
    int to = MoveUtils::getToSq(move);

    int color = MoveUtils::getColor(move);
    int piece = MoveUtils::getPiece(move);
    int otherColor = (color ^ 8);
    int otherPiece = MoveUtils::getCapturedPiece(move);

    bool isMoveEP = MoveUtils::isEP(move);
    bool isMoveCapture = MoveUtils::isCapture(move);
    bool isMoveCastle = MoveUtils::isCastle(move);
    int promotionPiece = MoveUtils::getPromotionPiece(move);

    int capturedPieceSquare = (isMoveEP ? (to + (color == White ? south : north)) : to);

    // update pieces
    this->hashKey ^= TranspositionTable::pieceZobristNumbers[piece][(int)(color == White)][from];
    if(isMoveCapture) this->hashKey ^= TranspositionTable::pieceZobristNumbers[otherPiece][(int)(otherColor == White)][capturedPieceSquare];

    if(!promotionPiece) this->hashKey ^= TranspositionTable::pieceZobristNumbers[piece][(int)(color == White)][to];
    else this->hashKey ^= TranspositionTable::pieceZobristNumbers[promotionPiece][(int)(color == White)][to];

    // castle stuff
    int newCastleRights = this->castleRights;
    if(piece == King) {
        int mask = (color == White ? 12 : 3);
        newCastleRights &= mask;
    }
    if((newCastleRights & BoardUtils::bits[1]) && (from == a1 || to == a1))
        newCastleRights ^= BoardUtils::bits[1];
    if((newCastleRights & BoardUtils::bits[0]) && (from == h1 || to == h1))
        newCastleRights ^= BoardUtils::bits[0];
    if((newCastleRights & BoardUtils::bits[3]) && (from == a8 || to == a8))
        newCastleRights ^= BoardUtils::bits[3];
    if((newCastleRights & BoardUtils::bits[2]) && (from == h8 || to == h8))
        newCastleRights ^= BoardUtils::bits[2];

    this->hashKey ^= TranspositionTable::castleZobristNumbers[this->castleRights];
    this->hashKey ^= TranspositionTable::castleZobristNumbers[newCastleRights];

    if(isMoveCastle) {
        int Rank = (to >> 3), File = (to & 7);
        int rookStartSquare = (Rank << 3) + (File == 6 ? 7 : 0);
        int rookEndSquare = (Rank << 3) + (File == 6 ? 5 : 3);

        this->hashKey ^= TranspositionTable::pieceZobristNumbers[Rook][(int)(color == White)][rookStartSquare];
        this->hashKey ^= TranspositionTable::pieceZobristNumbers[Rook][(int)(color == White)][rookEndSquare];
    }

    // update ep square
    int nextEp = -1;
    if(piece == Pawn && abs(from-to) == 16) {
        nextEp = to + (color == White ? south : north);
    }

    if(this->ep != -1) this->hashKey ^= TranspositionTable::epZobristNumbers[this->ep % 8];
    if(nextEp != -1) this->hashKey ^= TranspositionTable::epZobristNumbers[nextEp % 8];

    // switch turn
    this->hashKey ^= TranspositionTable::blackTurnZobristNumber;
}

Board *board = nullptr;
