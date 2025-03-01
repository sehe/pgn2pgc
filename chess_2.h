#pragma once
///////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////
#include <array>
#include <string>

#include "list5.h"
#include "pqueue_2.h"
//!!? Rank and file mean row (y) and column (x) in chess

// 0 = a, 1 = b...
char ChessFileToChar(unsigned file);
// 0 = '1', 1 = '1'...
char ChessRankToChar(unsigned rank);
// 'a' = 0
int ChessCharToFile(char file);
// '1' = 0
int ChessCharToRank(char rank);

struct ChessSquare {
  public:
    enum E_contents {
        empty,
        whitePawn,
        whiteKnight,
        whiteBishop,
        whiteRook,
        whiteQueen,
        whiteKing,
        blackPawn,
        blackKnight,
        blackBishop,
        blackRook,
        blackQueen,
        blackKing,
    };
    enum E_status { none, highlight };

    ChessSquare() : fContents(empty), fStatus(none) {}
    ChessSquare(E_contents contents, E_status status = none) : fContents(contents), fStatus(status) {}

    bool isWhite() const {
        return fContents == whitePawn || fContents == whiteKnight || fContents == whiteBishop ||
            fContents == whiteRook || fContents == whiteQueen || fContents == whiteKing;
    }
    bool isBlack() const {
        return fContents == blackPawn || fContents == blackKnight || fContents == blackBishop ||
            fContents == blackRook || fContents == blackQueen || fContents == blackKing;
    }
    bool       isEmpty() const { return fContents == empty; }
    bool       isPawn() const { return fContents == blackPawn || fContents == whitePawn; }
    E_contents contents() const { return fContents; }
    E_status   status() const { return fStatus; }

    bool operator==(E_contents rhs) const { return fContents == rhs; }
    bool operator!=(E_contents rhs) const { return fContents != rhs; }

    char pieceToChar() const;

  private:
    E_contents fContents;
    E_status   fStatus;
};

inline bool IsSameColor(ChessSquare a, ChessSquare b) {
    return (a.isWhite() && b.isWhite()) || (a.isBlack() && b.isBlack());
}

struct ChessMove {
  public:
    enum E_type {
        normal,
        promoKnight,
        promoBishop,
        promoRook,
        promoQueen,
        promoKing,
        whiteEnPassant,
        blackEnPassant, // technically not necessary, but makes things easier
        whiteCastleKS,
        whiteCastleQS,
        blackCastleKS,
        blackCastleQS,
    };

    ChessMove() : fRF(0), fFF(0), fRT(0), fFT(0), fType(normal) {}
    ChessMove(int y1, int x1, int y2, int x2, enum E_type kind = normal)
        : fRF(y1), fFF(x1), fRT(y2), fFT(x2), fType(kind) {}

    auto operator<=>(ChessMove const& rhs) const = default;
    bool isPromo() const {
        return fType == promoKnight || fType == promoBishop || fType == promoRook || fType == promoQueen ||
            fType == promoKing;
    }
    bool          isEnPassant() const { return fType == whiteEnPassant || fType == blackEnPassant; }
    int           rf() const { return fRF; }
    int           ff() const { return fFF; }
    int           rt() const { return fRT; }
    int           ft() const { return fFT; }
    E_type const& type() const { return fType; }
    E_type&       type() { return fType; }

  private:
    // row from, file from, row to, file to //!?? Keep signed int so that can
    // use in expressions that depend on signed values
    int    fRF, fFF, fRT, fFT;
    E_type fType;
};

class ChessMoveSAN {
  public:
    ChessMoveSAN(ChessMove mv = {}, std::string SAN = {}) : move_(std::move(mv)), SAN_(std::move(SAN)) {}
    ChessMove const& move() const { return move_; }
    std::string&     SAN() { return SAN_; }

    bool operator<(ChessMoveSAN const& b) const { return SAN_ < b.SAN_; }
    bool operator==(ChessMoveSAN const& b) const { return SAN_ == b.SAN_; }

  private:
    ChessMove   move_{};
    std::string SAN_{};
};

// 'R' == rook 'K' == king..
ChessSquare LetterToSquare(char SAN_FEN_Letter);

enum E_gameCheckStatus {
    notInCheck,
    inCheck,
    inCheckmate,
    inStalemate,
};

using SANQueue = PriorityQueue<ChessMoveSAN>;
//-----------------------------------------------------------------------------
class Board {
  public:
    Board();

    bool processFEN(std::string_view FEN); // true if valid position, false otherwise

    enum E_toMove { white, black, endOfGame };
    enum E_status { noStatus, checkmate, check, stalemate };
    enum E_castleAvailability { noCastle = 0, whiteKS = 1, whiteQS = 2, blackKS = 4, blackQS = 8 };
    enum E_enPassant {
        allCaptures = -2,
        noCaptures  = -1
    }; // all means don't care, all captures are possible, 1 - fFile for the file
       // where it is possible

    E_toMove toMove() const { return toMove_; }

    // returns false if move is not legal
    void switchMove() {
        if (toMove_ == black)
            toMove_ = white;
        else if (toMove_ == white)
            toMove_ = black;
    } // private
    bool move(ChessMove const&);          // private, need to remove calls to external functions
    bool processMove(ChessMove const& m); // make move and change status of game,
                                          // colorToMove etc
    bool processMove(ChessMove const& m, List<ChessMove>&);
    bool isWhiteToMove() const { return toMove_ == white; }
    bool isBlackToMove() const { return toMove_ == black; }

    unsigned getCastle() const { return castle_; }

    E_gameCheckStatus Status() const { return status_; }

    int enPassant() const { return enPassant_; }

    size_t ranks() const { return gRanks; }
    size_t files() const { return gFiles; }

    void genLegalMoves(List<ChessMove>&);
    // adds the moves to the list and the SAN representations to the queue
    void genLegalMoveSet(List<ChessMove>&, SANQueue&);

    // no ambiguities, move is not checked for legality
    void moveToAlgebraic(std::string&, ChessMove const&);
    void moveToAlgebraic(std::string&, ChessMove const&, List<ChessMove>&);

    bool algebraicToMove(ChessMove&, std::string_view);
    bool algebraicToMove(ChessMove&, std::string_view, List<ChessMove>&);

    // cleans up move
    bool algebraicToSAN(std::string&, std::string const&, List<ChessMove>&, SANQueue&);

    bool canCaptureSquare(size_t r, size_t f);

    bool     inCheck();
    E_status checkStatus();
    bool     willGiveCheck(ChessMove const&);
    E_status willBeInCheck(ChessMove const&);

    ChessSquare operator()(size_t rank, size_t file) const {
        assert(rank < gRanks && file < gFiles);
        return fBoard[rank][file];
    }

    void display();

  private:
    void disambiguateMoves(SANQueue&);
    void disambiguate(ChessMove const&, std::string&, SANQueue&);
    void genPseudoLegalMoves(List<ChessMove>&);
    void genPseudoLegalMoves(List<ChessMove>&, SANQueue&);
    void genLegalMoves(List<ChessMove>&, SANQueue&);
    void addMove(int rf, int ff, int rt, int ft, ChessMove::E_type type, List<ChessMove>& moves,
                 SANQueue& allSAN);
    void moveToAlgebraicAmbiguity(std::string&, ChessMove const&);
    void removeIllegalMoves(List<ChessMove>&, SANQueue&);

    static constexpr size_t gRanks = 8, gFiles = 8;
    using Rank   = std::array<ChessSquare, gFiles>;
    using Fields = std::array<Rank, gRanks>;
    Fields fBoard{};

    E_toMove          toMove_     = endOfGame;
    E_gameCheckStatus status_     = notInCheck;
    unsigned          castle_     = noCastle;
    int               enPassant_  = allCaptures; // noCaptures, allCaptures, 0 = a file, 1 = b...
    unsigned          pliesSince_ = 0;
    unsigned          moveNumber_ = 1;
};

//-----------------------------------------------------------------------------
// pseudoLegal: not castling, and not worrying about being left in check
void GenPseudoLegalMoves(Board const& b, List<ChessMove>& moves);

// returns if the person to move is in check
bool IsInCheck(Board b);

// if the move is made what will you be left in check?
bool WillBeInCheck(Board b, ChessMove const& move);

// if the move is made will you be giving check?
bool WillGiveCheck(Board b, ChessMove const& move);

// all legal moves are added to the list, including castleling
void GenLegalMoves(Board const& b, List<ChessMove>& moves);

// what is the status of the position on the board?
E_gameCheckStatus CheckStatus(Board const& b, List<ChessMove>&);

// appends the move to 'out'
// The move has not yet been made on the board
void MoveToAlgebraic(ChessMove const& move, Board const& b, std::string& out);

// in case you already know all of the chessMoves to save processing time (very
// significant in some cases)
//!?? allMoves must be the legal moves for the board, e.g. call GenLegalMoves(b,
//!&allMoves) just before this function
void MoveToAlgebraic(ChessMove const& move, Board const& b, List<ChessMove>& allMoves, std::string& out);

// return false if 'b', as this can mean a file
inline bool IsPromoChar(char c) {
    return toupper(c) == 'Q' || toupper(c) == 'N' || c == 'B' || toupper(c) == 'R' || toupper(c) == 'K';
}

// converts the SAN string to a ChessMove
bool AlgebraicToMove(std::string_view constSAN, Board const& b, ChessMove& move);

bool AlgebraicToMove(std::string_view constSAN, Board const& b, List<ChessMove>& allMoves, ChessMove& move);
