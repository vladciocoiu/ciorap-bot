#include <unordered_map>
#include <string>
#include <stack>
#include <iostream>
#include <assert.h>

#include "Board.h"
#include "MagicBitboards.h"
#include "TranspositionTable.h"
#include "Search.h"
#include "Moves.h"

using namespace std;

unordered_map<U64, int> repetitionMap;

U64 bits[64];
U64 filesBB[8], ranksBB[8], knightAttacksBB[64], kingAttacksBB[64], whitePawnAttacksBB[64], blackPawnAttacksBB[64];
U64 squaresNearWhiteKing[64], squaresNearBlackKing[64];
U64 lightSquaresBB, darkSquaresBB;
U64 castleMask[4];
U64 bishopMasks[64], rookMasks[64];

int castleStartSq[4] = {e1,e1,e8,e8};
int castleEndSq[4] = {g1,c1,g8,c8};

stack<int> castleStk, epStk, moveStk;

void Board::clear() {
    for(int i = 0; i < 64; i++) this->squares[i] = Empty;
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
}

// returns the algebraic notation for a move
string moveToString(int move) {
    int from = getFromSq(move);
    int to = getToSq(move);
    int prom = getPromotionPiece(move);

    if(move == NO_MOVE) return "0000";

    string s;
    s += (from%8)+'a';
    s += (from/8)+'1';
    s += (to%8)+'a';
    s += (to/8)+'1';

    if(prom == Knight) s += 'n';
    if(prom == Bishop) s += 'b';
    if(prom == Rook) s += 'r';
    if(prom == Queen) s += 'q';

    return s;
}

// returns the name of the square in algebraic notation
string square(int x) {
    assert(x >= 0 && x < 64);

    string s;
    s += ('a'+x%8);
    s += ('1'+x/8);
    return s;
}

// functions that return the board shifted in a direction
U64 eastOne(U64 bb) {
    return ((bb << 1) & (~filesBB[0]));
}

U64 westOne(U64 bb) {
    return ((bb >> 1) & (~filesBB[7]));
}

U64 northOne(U64 bb) {
    return (bb << 8);
}
U64 southOne(U64 bb) {
    return (bb >> 8);
}

// returns true if we can go in a specific direction from a square and not go outside the board
bool isInBoard(int sq, int dir) {
    int file = (sq & 7);
    int rank = (sq >> 3);

    if(dir == north) return rank < 7;
    if(dir == south) return rank > 0;
    if(dir == east) return file < 7;
    if(dir == west) return file > 0;

    if(dir == northEast) return rank < 7 && file < 7;
    if(dir == southEast) return rank > 0 && file < 7;
    if(dir == northWest) return rank < 7 && file > 0;
    if(dir == southWest) return rank > 0 && file > 0;

    return false;
}

// initialize all the variables before starting the actual engine
void init() {
    board.clear();
    // bitmasks for every bit from 0 to 63
    for(int i = 0; i < 64; i++)
        bits[i] = (1LL << i);

    // bitboard for checking empty squares between king and rook when castling
    castleMask[0] = (bits[f1] | bits[g1]);
    castleMask[1] = (bits[b1] | bits[c1] | bits[d1]);
    castleMask[2] = (bits[f8] | bits[g8]);
    castleMask[3] = (bits[b8] | bits[c8] | bits[d8]);

    // initialize zobrist numbers in order to make zobrist hash keys
    generateZobristHashNumbers();

    // create file and rank masks
    for(int i = 0; i < 64; i++) {
        int file = (i & 7), rank = (i >> 3);

        filesBB[file] |= bits[i];
        ranksBB[rank] |= bits[i];
    }

    // create pawn attacks masks and vectors
    for(int i = 0; i < 64; i++) {
        int file = (i & 7), rank = (i >> 3);

        if(file > 0) {
            if(i+7 < 64) whitePawnAttacksBB[i] |= bits[i+7];
            if(i-9 >= 0) blackPawnAttacksBB[i] |= bits[i-9];
        }
        if(file < 7) {
            if(i+9 < 64) whitePawnAttacksBB[i] |= bits[i+9];
            if(i-7 >= 0) blackPawnAttacksBB[i] |= bits[i-7];
        }
    }

    // create knight attacks masks and vectors
    for(int i = 0; i < 64; i++) {
        int file = (i & 7), rank = (i >> 3);

        if(file > 1) {
            if(rank > 0) knightAttacksBB[i] |= bits[i-10];
            if(rank < 7) knightAttacksBB[i] |= bits[i+6];
        }
        if(file < 6) {
            if(rank > 0) knightAttacksBB[i] |= bits[i-6];
            if(rank < 7) knightAttacksBB[i] |= bits[i+10];
        }
        if(rank > 1) {
            if(file > 0) knightAttacksBB[i] |= bits[i-17];
            if(file < 7) knightAttacksBB[i] |= bits[i-15];
        }
        if(rank < 6) {
            if(file > 0) knightAttacksBB[i] |= bits[i+15];
            if(file < 7) knightAttacksBB[i] |= bits[i+17];
        }
    }

    // create bishop masks
    for(int i = 0; i < 64; i++) {
        for(auto dir: {northEast, northWest, southEast, southWest}) {
            int sq = i;
            while(isInBoard(sq, dir)) {
                bishopMasks[i] |= bits[sq];
                sq += dir;
            }
        }
        bishopMasks[i] ^= bits[i];
    }

    // create rook masks
    for(int i = 0; i < 64; i++) {
        for(auto dir: {east, west, north, south}) {
            int sq = i;
            while(isInBoard(sq, dir)) {
                rookMasks[i] |= bits[sq];
                sq += dir;
            }
        }
        rookMasks[i] ^= bits[i];
    }

    // create king moves masks and vectors
    for(int i = 0; i < 64; i++) {
        kingAttacksBB[i] = (eastOne(bits[i]) | westOne(bits[i]));
        U64 king = (bits[i] | kingAttacksBB[i]);
        kingAttacksBB[i] |= (northOne(king) | southOne(king));
    }

    // squares near king are squares that a king can move to and the squares in front of his 'forward' moves
    for(int i = 0; i < 64; i++) {
        squaresNearWhiteKing[i] = squaresNearBlackKing[i] = (kingAttacksBB[i] | bits[i]);
        if(i+south >= 0) squaresNearBlackKing[i] |= kingAttacksBB[i+south];
        if(i+north < 64) squaresNearWhiteKing[i] |= kingAttacksBB[i+north];
    }

    // create light and dark squares masks
    for(int i = 0; i < 64; i++) {
        if(i%2) lightSquaresBB |= bits[i];
        else darkSquaresBB |= bits[i];
    }

    initMagics();

    clearTT();
    clearHistory();
}

// returns the direction of the move if any, or 0 otherwise
int direction(int from, int to) {
    int fromRank = (from >> 3), fromFile = (from & 7);
    int toRank = (to >> 3), toFile = (to & 7);

    if(fromRank == toRank)
        return (to > from ? east : west);

    if(fromFile == toFile)
        return (to > from ? north : south);

    if(fromRank-toRank == fromFile-toFile)
        return (to > from ? northEast : southWest);

    if(fromRank-toRank == toFile-fromFile)
        return (to > from ? northWest : southEast);

    return 0;
}

// piece attack patterns
U64 pawnAttacks (U64 pawns, int color) {
    if(color == White) {
        U64 north = northOne(pawns);
        return (eastOne(north) | westOne(north));
    }
    U64 south = southOne(pawns);
    return (eastOne(south) | westOne(south));
}

U64 knightAttacks(U64 knights) {
    U64 east, west, attacks;

    east = eastOne(knights);
    west = westOne(knights);
    attacks = (northOne(northOne(east | west)) | southOne(southOne(east | west)));

    east = eastOne(east);
    west = westOne(west);

    attacks |= (northOne(east | west) | southOne(east | west));

    return attacks;
}

// updates the bitboards when a piece is moved
void Board::updatePieceInBB(int piece, int color, int sq) {
    if(color == White) this->whitePiecesBB ^= bits[sq];
    else this->blackPiecesBB ^= bits[sq];

    if(piece == Pawn) this->pawnsBB ^= bits[sq];
    if(piece == Knight) this->knightsBB ^= bits[sq];
    if(piece == Bishop) this->bishopsBB ^= bits[sq];
    if(piece == Rook) this->rooksBB ^= bits[sq];
    if(piece == Queen) this->queensBB ^= bits[sq];
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
    if(this->castleRights & bits[0]) castles += 'K';
    if(this->castleRights & bits[1]) castles += 'Q';
    if(this->castleRights & bits[2]) castles += 'k';
    if(this->castleRights & bits[3]) castles += 'q';
    if(castles.length() == 0) castles += '-';

    fen += castles;
    fen += ' ';

    // add en passant square
    if(this->ep == -1) fen += '-';
    else fen += square(this->ep);

    //TODO: add halfmove clock and full moves
    fen += " 0 1";

    return fen;
}

// loads the squares array and the bitboards according to the fen position
void Board::loadFenPos(string input) {
    board.clear();

    // parse the fen string
    string pieces;
    int i = 0;
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

                U64 currBit = bits[rank*8 + file];

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
    unordered_map<char, U64> castleSymbols = {{'K', bits[0]}, {'Q', bits[1]}, {'k', bits[2]}, {'q', bits[3]}, {'-', 0}};
    this->castleRights = 0;

    for(char c: castles)
        this->castleRights |= castleSymbols[c];

    // get en passant target square
    this->ep = (epTargetSq == "-" ? -1 : (epTargetSq[0]-'a' + 8*(epTargetSq[1]-'1')));

    // initialize hash key
    this->hashKey = getZobristHashFromCurrPos();
}

// pseudo legal moves are moves that can be made but might leave the king in check
static int pseudoLegalMoves[512];
short Board::generatePseudoLegalMoves() {
    short numberOfMoves = 0;

    int color = this->turn;
    int otherColor = (color ^ 8);

    U64 ourPiecesBB = (color == White ? this->whitePiecesBB : this->blackPiecesBB);
    U64 opponentPiecesBB = (color == White ? this->blackPiecesBB : this->whitePiecesBB);
    U64 allPiecesBB = (this->whitePiecesBB | this->blackPiecesBB);

    // -----pawns-----
    U64 ourPawnsBB = (ourPiecesBB & this->pawnsBB);
    int pawnDir = (color == White ? north : south);
    int pawnStartRank = (color == White ? 1 : 6);
    int pawnPromRank = (color == White ? 7 : 0);

    while(ourPawnsBB) {
        int sq = bitscanForward(ourPawnsBB);
        bool isPromoting = (((sq+pawnDir) >> 3) == pawnPromRank);

        // normal moves and promotions
        if((allPiecesBB & bits[sq+pawnDir]) == 0) {
            assert(this->squares[sq] == (Pawn | color));

            if((sq >> 3) == pawnStartRank && (allPiecesBB & bits[sq+2*pawnDir]) == 0)
                pseudoLegalMoves[numberOfMoves++] = getMove(sq, sq+2*pawnDir, color, Pawn, 0, 0, 0, 0);

            if(isPromoting) {
                for(int piece: {Knight, Bishop, Rook, Queen})
                    pseudoLegalMoves[numberOfMoves++] = getMove(sq, sq+pawnDir, color, Pawn, 0, piece, 0, 0);
            } else pseudoLegalMoves[numberOfMoves++] = getMove(sq, sq+pawnDir, color, Pawn, 0, 0, 0, 0);

        }

        // captures and capture-promotions
        U64 pawnCapturesBB = (color == White ? whitePawnAttacksBB[sq] : blackPawnAttacksBB[sq]);
        pawnCapturesBB &= opponentPiecesBB;
        while(pawnCapturesBB) {
            assert(this->squares[sq] == (Pawn | color));

            int to = bitscanForward(pawnCapturesBB);
            if(isPromoting) {
                for(int piece: {Knight, Bishop, Rook, Queen})
                    pseudoLegalMoves[numberOfMoves++] = getMove(sq, to, color, Pawn, (this->squares[to] & (~8)), piece, 0, 0);
            } else pseudoLegalMoves[numberOfMoves++] = getMove(sq, to, color, Pawn, (this->squares[to] & (~8)), 0, 0, 0);

            pawnCapturesBB &= (pawnCapturesBB-1);

        }

        ourPawnsBB &= (ourPawnsBB-1);
    }

    //-----knights-----
    U64 ourKnightsBB = (this->knightsBB & ourPiecesBB);

    while(ourKnightsBB) {
        int sq = bitscanForward(ourKnightsBB);

        U64 knightMoves = (knightAttacksBB[sq] & ~ourPiecesBB);
        while(knightMoves) {
            if(this->squares[sq] != (Knight | color)) {
                cout <<  this->getFenFromCurrPos() << ' ' << sq << ' ' << this->squares[sq] << '\n'; 
            }
            assert(this->squares[sq] == (Knight | color));

            int to = bitscanForward(knightMoves);
            pseudoLegalMoves[numberOfMoves++] = getMove(sq, to, color, Knight, (this->squares[to] & (~8)), 0, 0, 0);
            knightMoves &= (knightMoves-1);
        }

        ourKnightsBB &= (ourKnightsBB-1);
    }

    //-----king-----
    int kingSquare = (color == White ? this->whiteKingSquare : this->blackKingSquare);
    U64 ourKingMoves = (kingAttacksBB[kingSquare] & ~ourPiecesBB);

    while(ourKingMoves) {
        assert(this->squares[kingSquare] == (King | color));

        int to = bitscanForward(ourKingMoves);
        pseudoLegalMoves[numberOfMoves++] = getMove(kingSquare, to, color, King, (this->squares[to] & (~8)), 0, 0, 0);
        ourKingMoves &= (ourKingMoves-1);
    }

    //-----sliding pieces-----
    U64 rooksQueens = (ourPiecesBB & (this->rooksBB | this->queensBB));
    while(rooksQueens) {
        int sq = bitscanForward(rooksQueens);

        U64 rookMoves = (magicRookAttacks(allPiecesBB, sq) & ~ourPiecesBB);
        while(rookMoves) {
            assert(this->squares[sq] == (Rook | color) || this->squares[sq] == (Queen | color));

            int to = bitscanForward(rookMoves);
            pseudoLegalMoves[numberOfMoves++] = getMove(sq, to, color, (this->squares[sq] & (~8)), (this->squares[to] & (~8)), 0, 0, 0);
            rookMoves &= (rookMoves-1);
        }

        rooksQueens &= (rooksQueens-1);
    }

    U64 bishopsQueens = (ourPiecesBB & (this->bishopsBB | this->queensBB));
    while(bishopsQueens) {
        int sq = bitscanForward(bishopsQueens);

        U64 bishopMoves = (magicBishopAttacks(allPiecesBB, sq) & ~ourPiecesBB);
        while(bishopMoves) {
             assert(this->squares[sq] == (Bishop | color) || this->squares[sq] == (Queen | color));

            int to = bitscanForward(bishopMoves);
            pseudoLegalMoves[numberOfMoves++] = getMove(sq, to, color, (this->squares[sq] & (~8)), (this->squares[to] & (~8)), 0, 0, 0);
            bishopMoves &= (bishopMoves-1);
        }

        bishopsQueens &= (bishopsQueens-1);
    }

    // -----castles-----
    int allowedCastles = (color == White ? 3 : 12);
    for(int i = 0; i < 4; i++) {
        if((this->castleRights & bits[i]) && ((allPiecesBB & castleMask[i]) == 0) && (allowedCastles & bits[i])) {
            assert(this->squares[castleStartSq[i]] == (King | color));

            pseudoLegalMoves[numberOfMoves++] = getMove(castleStartSq[i], castleEndSq[i], color, King, 0, 0, 1, 0);
        }
    }


    // -----en passant-----
    if(ep != -1) {
        ourPawnsBB = (this->pawnsBB & ourPiecesBB);
        U64 epBB = ((color == White ? blackPawnAttacksBB[ep] : whitePawnAttacksBB[ep]) & ourPawnsBB);
        while(epBB) {
            int sq = bitscanForward(epBB);

            assert(this->squares[sq] == (Pawn | color));

            pseudoLegalMoves[numberOfMoves++] = getMove(sq, ep, color, Pawn, Pawn, 0, 0, 1);
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
    if(kingAttacksBB[otherKingSquare] & bits[sq])
        return true;

    // knight attacks
    if(knightAttacksBB[sq] & opponentPiecesBB & this->knightsBB)
        return true;

    // pawn attacks
    if(bits[sq] & pawnAttacks((opponentPiecesBB & this->pawnsBB), otherColor))
        return true;

    // sliding piece attacks
    if(bishopsQueens & magicBishopAttacks(allPiecesBB, sq))
        return true;

    if(rooksQueens & magicRookAttacks(allPiecesBB, sq))
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
    U64 pawnAtt = (color == Black ? whitePawnAttacksBB[sq] : blackPawnAttacksBB[sq]);
    U64 rooksQueens = (ourPiecesBB & (this->rooksBB | this->queensBB));
    U64 bishopsQueens = (ourPiecesBB & (this->bishopsBB | this->queensBB));
    int kingSquare = (color == White ? this->whiteKingSquare : this->blackKingSquare);

    U64 res = 0;
    res |= (knightAttacksBB[sq] & ourPiecesBB & this->knightsBB);
    res |= (pawnAtt & this->pawnsBB & ourPiecesBB);
    res |= (kingAttacksBB[sq] & bits[kingSquare]);
    res |= (magicBishopAttacks(allPiecesBB, sq) & bishopsQueens);
    res |= (magicRookAttacks(allPiecesBB, sq) & rooksQueens);

    return res;
}

// takes the pseudo legal moves and checks if they don't leave the king in check
int Board::generateLegalMoves(int *moves) {
    int color = this->turn;
    int otherColor = (color ^ 8);
    int kingSquare = (color == White ? this->whiteKingSquare : this->blackKingSquare);
    int otherKingSquare = (otherColor == White ? this->whiteKingSquare : this->blackKingSquare);

    U64 allPiecesBB = (this->whitePiecesBB | this->blackPiecesBB);
    U64 opponentPiecesBB = (color == White ? this->blackPiecesBB : this->whitePiecesBB);
    U64 ourPiecesBB = (color == White ? this->whitePiecesBB : this->blackPiecesBB);

    short pseudoNum = this->generatePseudoLegalMoves();
    unsigned int num = 0;

    // remove the king so we can correctly find all squares attacked by sliding pieces, where the king can't go
    if(color == White) this->whitePiecesBB ^= bits[kingSquare];
    else this->blackPiecesBB ^= bits[kingSquare];

    U64 checkingPiecesBB = this->attacksTo(kingSquare);
    int checkingPiecesCnt = popcount(checkingPiecesBB);
    int checkingPieceIndex = bitscanForward(checkingPiecesBB);

    // check ray is the path between the king and the sliding piece checking it
    U64 checkRayBB = 0;
    if(checkingPiecesCnt == 1) {
        checkRayBB |= checkingPiecesBB;
        int piece = (this->squares[checkingPieceIndex]^otherColor);
        int dir = direction(checkingPieceIndex, kingSquare);

        // check direction is a straight line
        if(abs(dir) == north || abs(dir) == east)
            checkRayBB |= (magicRookAttacks(allPiecesBB, kingSquare) & magicRookAttacks(allPiecesBB, checkingPieceIndex));

        // check direction is a diagonal
        else checkRayBB |= (magicBishopAttacks(allPiecesBB, kingSquare) & magicBishopAttacks(allPiecesBB, checkingPieceIndex));
    }

    // finding the absolute pins
    U64 pinnedBB = 0;

    U64 attacks = magicRookAttacks(allPiecesBB, kingSquare); // attacks on the rook directions from the king square to our pieces
    U64 blockers = (ourPiecesBB & attacks); // potentially pinned pieces
    U64 oppRooksQueens = ((this->rooksBB | this->queensBB) & opponentPiecesBB);
    U64 pinners = ((attacks ^ magicRookAttacks((allPiecesBB ^ blockers), kingSquare)) & oppRooksQueens); // get pinners by computing attacks on the board without the blockers
    while(pinners) {
        int sq = bitscanForward(pinners);

        // get pinned pieces by &-ing attacks from the rook square with attacks from the king square, and then with our own pieces
        pinnedBB |= (magicRookAttacks(allPiecesBB, sq) & magicRookAttacks(allPiecesBB, kingSquare) & ourPiecesBB);
        pinners &= (pinners-1); // remove bit
    }

    attacks = magicBishopAttacks(allPiecesBB, kingSquare);
    blockers = (ourPiecesBB & attacks);
    U64 oppBishopsQueens = ((this->bishopsBB | this->queensBB) & opponentPiecesBB);
    pinners = ((attacks ^ magicBishopAttacks((allPiecesBB ^ blockers), kingSquare)) & oppBishopsQueens);
    while(pinners) {
        int sq = bitscanForward(pinners);
        pinnedBB |= (magicBishopAttacks(allPiecesBB, sq) & magicBishopAttacks(allPiecesBB, kingSquare) & ourPiecesBB);
        pinners &= (pinners-1);
    }

    for(int idx = 0; idx < pseudoNum; idx++) {
        int from = getFromSq(pseudoLegalMoves[idx]);
        int to = getToSq(pseudoLegalMoves[idx]);

        // we can always move the king to a safe square
        if(from == kingSquare) {
            if(this->isAttacked(to) == false) moves[num++] = pseudoLegalMoves[idx];
            continue;
        }

        // single check, can capture the attacker or intercept the check only if the moving piece is not pinned
        if(checkingPiecesCnt == 1 && ((pinnedBB & bits[from]) == 0)) {

            // capturing the checking pawn by en passant (special case)
            if(isEP(pseudoLegalMoves[idx]) && checkingPieceIndex == to + (color == White ? south: north))
                moves[num++] = pseudoLegalMoves[idx];

            // check ray includes interception or capturing the attacker
            else if(checkRayBB & bits[to]) {
                moves[num++] = pseudoLegalMoves[idx];
            }
        }

        // no checks, every piece can move if it is not pinned or it moves in the direction of the pin
        if(checkingPiecesCnt == 0) {
            // not pinned
            if((pinnedBB & bits[from]) == 0)
                moves[num++] = pseudoLegalMoves[idx];

            // pinned, can only move in the pin direction
            else if(abs(direction(from, to)) == abs(direction(from, kingSquare)))
                moves[num++] = pseudoLegalMoves[idx];
        }
    }

    // en passant are the last added pseudo legal moves
    int epMoves[2];
    int numEp = 0;
    while(num && isEP(moves[num-1])) {
        epMoves[numEp++] = moves[--num];
    }

    assert(numEp <= 2);

    // before ep there are the castle moves so we do the same
    int castleMoves[2];
    int numCastles = 0;
    while(num && isCastle(moves[num-1])) {
        castleMoves[numCastles++] = moves[--num];
    }
    assert(numCastles <= 2);

    // manual check for the legality of castle moves
    for(int idx = 0; idx < numCastles; idx++) {
        int from = getFromSq(castleMoves[idx]);
        int to = getToSq(castleMoves[idx]);

        int first = min(from, to);
        int second = max(from, to);

        bool ok = true;

        for(int i = first; i <= second; i++)
            if(this->isAttacked(i))
                ok = false;

        if(ok) moves[num++] = castleMoves[idx];
    }

    // ep horizontal pin
    for(int idx = 0; idx < numEp; idx++) {
        int from = getFromSq(epMoves[idx]);
        int to = getToSq(epMoves[idx]);

        int epRank = (from >> 3);
        int otherPawnSquare = (epRank << 3) | (to & 7);
        U64 rooksQueens = (opponentPiecesBB & (this->rooksBB | this->queensBB) & ranksBB[epRank]);

        // remove the 2 pawns and compute attacks from rooks/queens on king square
        U64 removeWhite = bits[from], 
            removeBlack = bits[otherPawnSquare];
        if(color == Black) swap(removeWhite, removeBlack);

        this->whitePiecesBB ^= removeWhite;
        this->blackPiecesBB ^= removeBlack;

        U64 attacks = this->attacksTo(kingSquare);
        if((rooksQueens & attacks) == 0) moves[num++] = epMoves[idx];

        this->whitePiecesBB ^= removeWhite;
        this->blackPiecesBB ^= removeBlack;
    }

    // put the king back
    if(color == White) this->whitePiecesBB ^= bits[kingSquare];
    else this->blackPiecesBB ^= bits[kingSquare];

    return num;
}

// make a move, updating the squares and bitboards
void Board::makeMove(int move) {
    this->updateHashKey(move);

    if(move == NO_MOVE) { // null move
        epStk.push(this->ep);
        this->ep = -1;
        this->turn ^= 8;

        return;
    }
    repetitionMap[this->hashKey]++;

    // push the current castle and ep info in order to retrieve it when we unmake the move
    castleStk.push(this->castleRights);
    epStk.push(this->ep);

    moveStk.push(move);

    // get move info
    int from = getFromSq(move);
    int to = getToSq(move);

    int color = getColor(move);
    int piece = getPiece(move);
    int otherColor = (color ^ 8);
    int otherPiece = getCapturedPiece(move);
    int promotionPiece = getPromotionPiece(move);

    bool isMoveEP = isEP(move);
    bool isMoveCapture = isCapture(move);
    bool isMoveCastle = isCastle(move);

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
    if((this->castleRights & bits[1]) && (from == a1 || to == a1))
        this->castleRights ^= bits[1];
    if((this->castleRights & bits[0]) && (from == h1 || to == h1))
        this->castleRights ^= bits[0];
    if((this->castleRights & bits[3]) && (from == a8 || to == a8))
        this->castleRights ^= bits[3];
    if((this->castleRights & bits[2]) && (from == h8 || to == h8))
        this->castleRights ^= bits[2];

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

    if(move == NO_MOVE) { // null move
        this->ep = epStk.top();
        epStk.pop();
        this->turn ^= 8;

        this->updateHashKey(move);

        return;
    }
    repetitionMap[this->hashKey]--;

    // get move info
    int from = getFromSq(move);
    int to = getToSq(move);

    int color = getColor(move);
    int piece = getPiece(move);
    int otherColor = (color ^ 8);
    int otherPiece = getCapturedPiece(move);
    int promotionPiece = getPromotionPiece(move);

    bool isMoveEP = isEP(move);
    bool isMoveCapture = isCapture(move);
    bool isMoveCastle = isCastle(move);

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

// draw by insufficient material or repetition
bool Board::isDraw() {
    if(repetitionMap[this->hashKey] > 1) return true; // repetition

    if(this->queensBB | this->rooksBB | this->pawnsBB) return false;

    if((this->knightsBB | this->bishopsBB) == 0) return true; // king vs king

    int whiteBishops = popcount(this->bishopsBB | this->whitePiecesBB);
    int blackBishops = popcount(this->bishopsBB | this->blackPiecesBB);
    int whiteKnights = popcount(this->knightsBB | this->whitePiecesBB);
    int blackKnights = popcount(this->knightsBB | this->blackPiecesBB);

    if(whiteKnights + blackKnights + whiteBishops + blackBishops == 1) return true; // king and minor piece vs king

    if(whiteKnights + blackKnights == 0 && whiteBishops == 1 && blackBishops == 1) {
        int lightSquareBishops = popcount(lightSquaresBB & this->bishopsBB);
        if(lightSquareBishops == 0 || lightSquareBishops == 2) return true; // king and bishop vs king and bishop with same color bishops
    }

    return false;
}

Board board;
