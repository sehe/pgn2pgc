#pragma once
///////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////
#include <cstddef>
#include <cstring>

#include "joshdefs.h"
#include "list5.h"
#include "pqueue_2.h"
//!!? rank and file mean row (y) and column (x) in chess

// 0 = a, 1 = b...
char ChessFileToChar(unsigned file);
// 0 = '1', 1 = '1'...
char ChessRankToChar(unsigned rank);
// 'a' = 0
unsigned ChessCharToFile(char file);
// '1' = 0
unsigned ChessCharToRank(char rank);

struct ChessSquare
{
public:
	enum E_contents {  empty, whitePawn, whiteKnight, whiteBishop, whiteRook, whiteQueen,
					whiteKing, blackPawn, blackKnight, blackBishop, blackRook,
					blackQueen, blackKing, };
	enum E_status { none, highlight };

	ChessSquare() : fContents(empty), fStatus(none) {}
	ChessSquare(E_contents contents, E_status status = none) : fContents(contents), fStatus(status) {}

	bool isWhite() const {return fContents == whitePawn || fContents == whiteKnight || fContents == whiteBishop || fContents == whiteRook || fContents == whiteQueen || fContents == whiteKing;}
	bool isBlack() const {return fContents == blackPawn || fContents == blackKnight || fContents == blackBishop || fContents == blackRook || fContents == blackQueen || fContents == blackKing;}
	bool isEmpty() const {return fContents == empty;}
	bool isPawn() const {return fContents == blackPawn || fContents == whitePawn;}
	E_contents contents() const {return fContents;}
	E_status status() const {return fStatus;}

	bool operator==(E_contents rhs) const { return fContents == rhs; }
	bool operator!=(E_contents rhs) const { return fContents != rhs; }

	char pieceToChar() const;

private:
	E_contents fContents;
	E_status fStatus;
};

inline
bool IsSameColor(ChessSquare a, ChessSquare b)
{
	return a.isWhite() && b.isWhite() || a.isBlack() && b.isBlack();
}

struct ChessMove
{
public:
	enum E_type { normal,
		promoKnight, promoBishop, promoRook, promoQueen, promoKing,
		whiteEnPassant, blackEnPassant, // technically not necessary, but makes things easier
		whiteCastleKS, whiteCastleQS, blackCastleKS, blackCastleQS,
		};

	ChessMove() : fRF(0), fFF(0), fRT(0), fFT(0), fType(normal) {}
	ChessMove(int y1, int x1, int y2, int x2, enum E_type kind = normal) : fRF(y1), fFF(x1), fRT(y2), fFT(x2), fType(kind) {}

	bool operator==(const ChessMove& rhs) const {
			return rhs.fRF == fRF && rhs.fFF == fFF && rhs.fRT == fRT && rhs.fFT == fFT && rhs.fType == fType; }
	bool operator!=(const ChessMove& rhs) const { return !this->operator==(rhs); }
	bool isPromo() const {return fType == promoKnight || fType == promoBishop || fType == promoRook || fType == promoQueen || fType == promoKing; }
	bool isEnPassant() const {return fType == whiteEnPassant || fType == blackEnPassant;}
	int rf() const {return fRF;}
	int ff() const {return fFF;}
	int rt() const {return fRT;}
	int ft() const {return fFT;}
	E_type const& type() const {return fType;}
	E_type & type() {return fType;}

private:

    int fRF, fFF, fRT, fFT; // row from, file from, row to, file to //!?? keep
                            // signed int so that can use in expressions that
                            // depend on signed values
	E_type fType;
};

class ChessMoveSAN: public ChessMove {
public:
	ChessMoveSAN() : ChessMove() {}
	ChessMoveSAN(int y1, int x1, int y2, int x2, const string& SAN, enum E_type kind = normal) : ChessMove(y1, x1, y2, x2, kind), fSAN(SAN) {}
	ChessMove move() {return ChessMove(rf(), ff(), rt(), ft(), type());}
	string& san() {return fSAN;}
	bool operator<(const ChessMoveSAN& b) const {return fSAN < b.fSAN;}
	bool operator>(const ChessMoveSAN& b) const {return fSAN > b.fSAN;}
	bool operator==(const ChessMoveSAN& b) const {return fSAN == b.fSAN;}
   bool operator!=(const ChessMoveSAN& b) const {return fSAN != b.fSAN;}
private:
	string fSAN;
};


// 'R' == rook 'K' == king..
ChessSquare LetterToSquare(char SAN_FEN_Letter);

enum E_gameCheckStatus {
	notInCheck,
	inCheck,
	inCheckmate,
	inStalemate,
	};


//-----------------------------------------------------------------------------
class Board
{
public:

	Board(); //!!? removed ability to determine board size

	Board(const Board&);
	Board& operator=(const Board&);

	bool processFEN(const string& FENPosition); // true if valid position, false otherwise

	enum E_toMove {white, black, endOfGame};
	enum E_status {noStatus, checkmate, check, stalemate};
	enum E_castleAvailability {noCastle = 0, whiteKS = 1, whiteQS = 2, blackKS = 4, blackQS = 8};
	enum E_enPassant {allCaptures = -2, noCaptures = -1}; // all means don't care, all captures are possible, 1 - fFile for the file where it is possible

	E_toMove toMove() const {return fToMove;}

	// returns false if move is not legal
	void switchMove() {if(fToMove == black) fToMove = white; else if (fToMove == white) fToMove = black; } // private
	bool move(const ChessMove&); // private, need to remove calls to external functions
	bool processMove(const ChessMove& m); // make move and change status of game, colorToMove etc
   bool processMove(const ChessMove& m, List<ChessMove>&);
	bool isWhiteToMove() const {return fToMove == white;}
	bool isBlackToMove() const {return fToMove == black;}


	unsigned getCastle() const {return fCastle;}

	E_gameCheckStatus Status() const {return fStatus;}

	int enPassant() const {return fEnPassant;}

	size_t ranks() const {return gRanks;}
	size_t files() const {return gFiles;}

	void genLegalMoves(List<ChessMove>*);
	void genLegalMoveSet(List<ChessMove>*, PriorityQueue<ChessMoveSAN>*); // adds the moves to the list and the SAN representations to the queue

	void moveToAlgebraic(string*, const ChessMove&); // no abiguities, move is not checked for legality
	void moveToAlgebraic(string*, const ChessMove&, List<ChessMove>&);

	bool algebraicToMove(ChessMove*, const char[]);
	bool algebraicToMove(ChessMove*, const char[], List<ChessMove>&);

	bool algebraicToSAN(string*, const string&, List<ChessMove>&, PriorityQueue<ChessMoveSAN>&); // cleans up move

   bool canCaptureSquare(size_t r, size_t f);

	bool inCheck();
	E_status checkStatus();
	bool willGiveCheck(const ChessMove&);
	E_status willBeInCheck(const ChessMove&);

	ChessSquare operator()(size_t rank, size_t file) const {assert(rank < gRanks && file < gFiles); return fBoard[rank][file];}

	void display();

	~Board();

private:

	void disambiguateMoves(PriorityQueue<ChessMoveSAN>&);
	void disambiguate(const ChessMove&, string*, PriorityQueue<ChessMoveSAN>&);
	void genPseudoLegalMoves(List<ChessMove>*);
	void genPseudoLegalMoves(List<ChessMove>*, PriorityQueue<ChessMoveSAN>*);
	void genLegalMoves(List<ChessMove>*, PriorityQueue<ChessMoveSAN>*);
	void addMove(int rf, int ff, int rt, int ft, ChessMove::E_type type, List<ChessMove>* moves, PriorityQueue<ChessMoveSAN>* allSAN);
	void moveToAlgebraicAmbiguity(string*, const ChessMove&);
	void removeIllegalMoves(List<ChessMove>*, PriorityQueue<ChessMoveSAN>*);


	ChessSquare** fBoard;
	static const size_t gRanks, gFiles; // size of board, ranks and files

	E_toMove fToMove;
	E_gameCheckStatus fStatus;
	unsigned fCastle;
	int fEnPassant; // noCaptures, allCaptures, 0 = a file, 1 = b...
	unsigned fPlysSince;
	unsigned fMoveNumber;
};

//-----------------------------------------------------------------------------
// pseudoLegal: not castling, and not worrying about being left in check
void GenPseudoLegalMoves(const Board& b, List<ChessMove>* moves);

// returns if the person to move is in check
bool IsInCheck(Board b);

// if the move is made what will you be left in check?
bool WillBeInCheck(Board b, const ChessMove& move);

// if the move is made will you be giving check?
bool WillGiveCheck(Board b, const ChessMove& move);

// all legal moves are added to the list, including castleling
void GenLegalMoves(const Board& b, List<ChessMove>* moves);

// what is the status of the position on the board?
E_gameCheckStatus CheckStatus(const Board& b, List<ChessMove>&);

// appends the move to 'out'
// The move has not yet been made on the board
void MoveToAlgebraic(const ChessMove& move, const Board& b, string* out);

// in case you already know all of the chessMoves to save processing time (very significant in some cases)
//!?? allMoves must be the legal moves for the board, e.g. call GenLegalMoves(b, &allMoves) just before this function
void MoveToAlgebraic(const ChessMove& move, const Board& b, List<ChessMove>& allMoves, string* out);

// return false if 'b', as this can mean a file
inline
bool IsPromoChar(char c)
{
	return toupper(c) == 'Q' || toupper(c) == 'N' || c == 'B' || toupper(c) == 'R' || toupper(c) == 'K';
}

// case insensitive
unsigned NumCharsInStr(const char* str, int c);

// converts the SAN string to a ChessMove
bool AlgebraicToMove(const char constSAN[], const Board& b, ChessMove* move);

bool AlgebraicToMove(const char constSAN[], const Board& b, List<ChessMove>& allMoves, ChessMove* move);
