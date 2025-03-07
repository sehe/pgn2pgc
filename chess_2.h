#pragma once
///////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////
#include <array>
#include <cassert>
#include <stdexcept>
#include <string>
#include <vector>

//!!? Rank and file mean row (y) and column (x) in chess

#ifndef ALLOW_KING_PROMOTION
    #define ALLOW_KING_PROMOTION false
#endif

namespace pgn2pgc::Chess {
    static constexpr int gRanks = 8, gFiles = 8;

    enum class Occupant {
        noPiece,
        whiteBishop,
        whiteKing,
        whiteKnight,
        whitePawn,
        whiteQueen,
        whiteRook,
        blackBishop,
        blackKing,
        blackKnight,
        blackPawn,
        blackQueen,
        blackRook,
    };

    struct Square {
      public:
        constexpr Square(Occupant contents = Occupant::noPiece) : occ_(contents) {}

        constexpr bool isWhite() const {
            using enum Occupant;
            return occ_ == whitePawn || occ_ == whiteKnight || occ_ == whiteBishop || occ_ == whiteRook ||
                occ_ == whiteQueen || occ_ == whiteKing;
        }
        constexpr bool isBlack() const {
            using enum Occupant;
            return occ_ == blackPawn || occ_ == blackKnight || occ_ == blackBishop || occ_ == blackRook ||
                occ_ == blackQueen || occ_ == blackKing;
        }
        constexpr bool isEmpty() const { return occ_ == Occupant::noPiece; }
        constexpr bool isPawn() const { return occ_ == Occupant::blackPawn || occ_ == Occupant::whitePawn; }
        constexpr Occupant contents() const { return occ_; }

        constexpr auto operator<=>(Square const& rhs) const = default;

        constexpr char pieceToChar() const;

      private:
        Occupant occ_;
    };

    inline bool IsSameColor(Square a, Square b) {
        return (a.isWhite() && b.isWhite()) || (a.isBlack() && b.isBlack());
    }

    struct RankFile {
        int            rank = 0, file = 0;
        constexpr auto operator<=>(RankFile const& rhs) const = default;

        constexpr RankFile operator+(RankFile const& rhs) const { return {rank + rhs.rank, file + rhs.file}; }
        constexpr RankFile operator-(RankFile const& rhs) const { return {rank - rhs.rank, file - rhs.file}; }

        friend std::ostream& operator<<(std::ostream& os, RankFile const& f);
    };

    struct ChessMove {
      public:
        enum Type {
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

        constexpr ChessMove(Occupant actor = Occupant::noPiece, int y1 = 0, int x1 = 0, int y2 = 0,
                            int x2 = 0, enum Type type = normal)
            : actor_(actor)
            , capture_(type == whiteEnPassant || type == blackEnPassant)
            , from_{y1, x1}
            , to_{y2, x2}
            , type_(type) {}

        constexpr auto operator<=>(ChessMove const& rhs) const = default;
        constexpr bool isPromo() const {
            return type_ == promoKnight || type_ == promoBishop || type_ == promoRook ||
                type_ == promoQueen || type_ == promoKing;
        }
        constexpr bool     isEnPassant() const { return type_ == whiteEnPassant || type_ == blackEnPassant; }
        constexpr Occupant actor() const { return actor_; }
        constexpr RankFile from() const { return from_; }
        constexpr RankFile to() const { return to_; }
        constexpr Type const& type() const { return type_; }
        constexpr Type&       type() { return type_; }
        constexpr bool const& isCapture() const { return capture_; }
        constexpr bool&       isCapture() { return capture_; }

        constexpr std::string ambiguousSAN() const;

      private:
        // row from, file from, row to, file to //!?? Keep signed int so that can
        // use in expressions that depend on signed values
        Occupant actor_   = Occupant::noPiece;
        bool     capture_ = false;
        RankFile from_, to_;
        Type     type_ = normal;
    };

    class ChessMoveSAN {
      public:
        ChessMoveSAN(ChessMove mv = {}, std::string SAN = {}) : move_(std::move(mv)), SAN_(std::move(SAN)) {}
        ChessMove const&   move() const { return move_; }
        std::string const& SAN() const { return SAN_; }
        std::string&       SAN() { return SAN_; }

        bool operator<(ChessMoveSAN const& b) const { return SAN_ < b.SAN_; }

        ChessMove   move_{};
        std::string SAN_{};
    };

    struct MoveList : std::vector<ChessMove> {
        void add(ChessMove v) { push_back(std::move(v)); }
        void remove(size_t index) { erase(begin() + index); }
        void makeEmpty() { clear(); }
    };

    struct OrderedMoveList {
        MoveList list;
        std::vector<ChessMoveSAN> bysan;

        void disambiguate();
    };

    // char        ChessFileToChar(unsigned file); // 0 = a, 1 = b...
    // char        ChessRankToChar(unsigned rank); // 0 = '1', 1 = '1'...
    // int         ChessCharToFile(char file);     // 'a' = 0
    // int         ChessCharToRank(char rank);     // '1' = 0
    // ChessSquare LetterToSquare(char SAN_FEN_Letter); // 'R' == rook 'K' == king..

    struct MoveError : std::runtime_error {
        MoveError(std::string_view msg) : std::runtime_error(std::string(msg)) {}
    };
    struct EmptyMove : MoveError {
        EmptyMove() : MoveError("Empty move") {}
    };
    struct InvalidSAN : MoveError {
        InvalidSAN(std::string_view input) : MoveError("Invalid SAN: " + std::string(input)) {}
    };
    struct IllegalMove : MoveError {
        IllegalMove(std::string_view input) : MoveError("Illegal move: " + std::string(input)) {}
    };

    enum class GameStatus { notInCheck, inCheck, inCheckmate, inStalemate };

    //-----------------------------------------------------------------------------
    class Board {
      public:
        enum class ToMove { white, black, endOfGame };
        enum Castlings { noCastle = 0, whiteKS = 1, whiteQS = 2, blackKS = 4, blackQS = 8 };
        enum EnPassant { allCaptures = -2, noCaptures = -1 };
        // all means don't care, all captures are possible, 1 - fFile for the file wehere ep is possible

        Board();

        bool processFEN(std::string_view FEN); // true if valid position, false otherwise

        // adds the moves to the list and the SAN representations to the queue
        OrderedMoveList genLegalMoveSet();

        // throws EmptyMove, IllegalMove, InvalidSAN
        ChessMove resolveSAN(std::string_view san, MoveList const& legal) const;

        // no ambiguities, move is not checked for legality
        std::string toSAN(ChessMove const&, MoveList const&) const;

        bool processMove(ChessMove const& m);

        void display() const;

        GameStatus Status() const { return status_; }

      private:
        ToMove toMove() const { return toMove_; }

        void switchMove() {
            if (toMove_ == ToMove::black)
                toMove_ = ToMove::white;
            else if (toMove_ == ToMove::white)
                toMove_ = ToMove::black;
        } // private

        // returns false if move is not legal
        bool applyMove(ChessMove const&); // private, need to remove calls to external functions
        bool isWhiteToMove() const { return toMove_ == ToMove::white; }
        bool isBlackToMove() const { return toMove_ == ToMove::black; }

        unsigned getCastle() const { return castle_; }

        int enPassant() const { return enPassantFile_; }

        static constexpr int ranks() { return gRanks; }
        static constexpr int files() { return gFiles; }

        bool canCaptureSquare(RankFile) const;

#ifdef NDEBUG
        Square const& at(int rank, int file) const { return fBoard[rank][file]; }
        Square const& at(RankFile rf) const { return fBoard[rf.rank][rf.file]; }
        Square&       at(int rank, int file) { return fBoard[rank][file]; }
        Square&       at(RankFile rf) { return fBoard[rf.rank][rf.file]; }
#else
        Square const& at(int rank, int file) const { return fBoard.at(rank).at(file); }
        Square const& at(RankFile rf) const { return fBoard.at(rf.rank).at(rf.file); }
        Square&       at(int rank, int file) { return fBoard.at(rank).at(file); }
        Square&       at(RankFile rf) { return fBoard.at(rf.rank).at(rf.file); }
#endif

      private:
        void addMove(ChessMove, MoveList& moves) const;
        void addMove(ChessMove, OrderedMoveList& moves) const;

        void removeIllegalMoves(OrderedMoveList&) const;

        static constexpr int gRanks = 8, gFiles = 8;
        using Rank   = std::array<Square, gFiles>;
        using Fields = std::array<Rank, gRanks>;
        Fields fBoard{};

        ToMove     toMove_        = ToMove::endOfGame;
        GameStatus status_        = GameStatus::notInCheck;
        unsigned   castle_        = noCastle;
        int        enPassantFile_ = allCaptures; // noCaptures, allCaptures, 0 = a file, 1 = b...
        unsigned   pliesSince_    = 0;
        unsigned   moveNumber_    = 1;

        //-----------------------------------------------------------------------------
        // all legal moves are added to the list, including castling
        template <typename Moves>
            requires std::is_base_of_v<MoveList, Moves> || std::is_base_of_v<OrderedMoveList, Moves>
        inline Moves genLegalMoves() const;

        // pseudoLegal: not castling, and not worrying about being left in check
        template <typename Moves>
            requires std::is_base_of_v<MoveList, Moves> || std::is_base_of_v<OrderedMoveList, Moves>
        Moves genPseudoLegalMoves() const;

        // returns if the person to move is in check
        bool IsInCheck() const;

        // if the move is made what will you be left in check?
        bool WillBeInCheck(ChessMove const& move) const;

        // if the move is made will you be giving check?
        bool WillGiveCheck(ChessMove const& move) const;

        // what is the status of the position on the board?
        GameStatus CheckStatus(MoveList const&) const;
    };

    // return false if 'b', as this can mean a file
    inline bool IsPromoChar(char c) {
        return toupper(c) == 'Q' || toupper(c) == 'N' || c == 'B' || toupper(c) == 'R'
#if ALLOW_KING_PROMOTION
            || toupper(c) == 'K'
#endif
            ;
    }
} // namespace pgn2pgc::Chess
