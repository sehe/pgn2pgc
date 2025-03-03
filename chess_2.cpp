// vim: spell :
#include <cstddef>
#include <cstring>
#include <iostream>
#include <ranges>

#include "chess_2.h"
#include "stpwatch.h"

namespace pgn2pgc::Chess {
    using enum Occupant;

    static void RemoveWhiteSpace(std::string& s) {
        if (auto b = s.find_first_not_of(" \t\n\f\r"), e = s.find_last_not_of(" \t\n\f\r");
                b == std::string::npos) {
            s.clear();
        } else {
            s = s.substr(b, e - b + 1);
        }
    }

    static void RemoveChars(std::string& s, std::string_view remove) {
        for (size_t p = 0; p < s.size(); ++p) {
            if (remove.contains(s[p])) {
                s.erase(p, 1);
                --p;
            }
        }
    }

    static constexpr inline char ChessFileToChar(unsigned file) { return file + 'a'; }
    static constexpr inline char ChessRankToChar(unsigned rank) { return rank + '1'; }
    static constexpr inline int  ChessCharToFile(char file) { return file - 'a'; }
    static constexpr inline int  ChessCharToRank(char rank) { return rank - '1'; }
    static std::ostream&         operator<<(std::ostream& os, RankFile const& f) {
        return os << ChessFileToChar(f.file) << ChessRankToChar(f.rank);
    }

    // case insensitive
    inline static constexpr int NumCharsInStr(std::string_view str, char c) {
        int count = 0;
        for (auto ch : str)
            if (toupper(ch) == toupper(c))
                ++count;
        return count;
    }

    constexpr inline char Square::pieceToChar() const {
        switch (occ_) {
            case whitePawn: return 'P';
            case blackPawn: return 'p';
            case whiteKnight: return 'N';
            case blackKnight: return 'n';
            case whiteBishop: return 'B';
            case blackBishop: return 'b';
            case whiteRook: return 'R';
            case blackRook: return 'r';
            case whiteQueen: return 'Q';
            case blackQueen: return 'q';
            case whiteKing: return 'K';
            case blackKing: return 'k';
            case noPiece: return ' ';
            default: assert(0); return 'x'; // not Reached
        }
    }

    //-----------------------------------------------------------------------------
    static constexpr inline Square LetterToSquare(char SAN_FEN_Letter) {
        switch (SAN_FEN_Letter) {
            case 'P': return whitePawn;
            case 'p': return blackPawn;
            case 'R': return whiteRook;
            case 'r': return blackRook;
            case 'N': return whiteKnight;
            case 'n': return blackKnight;
            case 'B': return whiteBishop;
            case 'b': return blackBishop;
            case 'Q': return whiteQueen;
            case 'q': return blackQueen;
            case 'K': return whiteKing;
            case 'k': return blackKing;

            default: return noPiece;
        }
    }

    // FEN notation of initial position
    // position : to move : castling : (50*2) - plies till draw : current move
    // number "RNBQKBNR/PPPPPPPP/8/8/8/8/pppppppp/rnbqkbnr w KQkq - 0 1"

    Board::Board() { processFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"); }

    bool Board::processFEN(std::string_view FEN) {
        auto split = [](auto sv, char delim) {
            return std::views::split(sv, delim) |
                std::views::transform([](auto&& r) { return std::string_view(r.begin(), r.end()); });
        };
        int RECORD = 0;
        for (auto token : split(FEN, ' ')) {
            switch (++RECORD) {
                case 1: // board position
                    for (int i = ranks() - 1; auto rank : split(token, '/')) {
                        for (int j = 0; auto ch : rank) {
                            if (isdigit(ch))
                                for (auto n = ch - '0'; n--;)
                                    at(i, j++) = noPiece;
                            else
                                at(i, j++) = LetterToSquare(ch);
                        }
                        --i;
                    }

                    break;
                case 2: // to move
                    if (token == "w" || token == "W")
                        toMove_ = ToMove::white;
                    else if (token == "b" || token == "B")
                        toMove_ = ToMove::black;
                    else
                        return false;
                    break;
                case 3: // castling
                    castle_ = noCastle;
                    for (auto ch : token) {
                        switch (ch) {
                            case 'K': castle_ |= whiteKS; break;
                            case 'Q': castle_ |= whiteQS; break;
                            case 'k': castle_ |= blackKS; break;
                            case 'q': castle_ |= blackQS; break;
                            case '-': castle_ = noCastle; break;

                            default: return false;
                        }
                    }
                    break;
                case 4: // en-passant
                    if (token.empty())
                        return false;

                    if (token == "-") {
                        enPassantFile_ = noCaptures;
                    } else {
                        if (!isalpha(token.front()))
                            return false;

                        enPassantFile_ = tolower(token.front()) - 'a';
                        assert(enPassantFile_ > 0);

                        if (enPassantFile_ >= gFiles)
                            return false;
                    }
                    // ignore the rank info

                    break;
                case 5: // plies since last capture or pawn move
                    if (token.empty())
                        return false; // the string is empty

                    {
                        auto n = std::stoul(std::string(token));
                        if (n > std::numeric_limits<decltype(pliesSince_)>::max())
                            return false;
                        pliesSince_ = n;
                    }
                    break;
                case 6: // current move number (starts at 1)
                    if (token.empty())
                        return false; // the string is empty
                    {
                        auto n = std::stoul(std::string(token));
                        if (n > std::numeric_limits<decltype(moveNumber_)>::max())
                            return false;
                        moveNumber_ = n;
                    }
                    break;
            }
        }
        if (RECORD != 6)
            return false;

        return true;
    }

    void Board::display() {
        for (int i = ranks() - 1; i >= 0; --i) {
            std::cout << "\n" << i + 1 << " ";
            for (int j = 0; j < files(); ++j) {
                std::cout << at(i, j).pieceToChar();
            }
        }
        std::cout << "\n  abcdefgh \n";
        std::cout << "\nTo Move:\t " << std::to_underlying(toMove_);
        std::cout << "\nCastle:\t " << castle_;
        std::cout << "\nStatus:\t " << std::to_underlying(status_);
        std::cout << "\nEnPassant:\t " << enPassantFile_;
        std::cout << "\nMove: " << moveNumber_;
        std::cout << "\nPlies Since:\t " << pliesSince_;
    }

    // true on success
    //??!! No checking is done?
    bool Board::applyMove(ChessMove const& move) {
        // can't move an empty square or capture your own piece
        // illegal en-passant
        // general legality -- check etc.. ?? How much checking should be done here?

        if (at(move.from()).isEmpty() || IsSameColor(at(move.from()), at(move.to())) ||
                (move.from() == move.to()) ||
                (move.isEnPassant() && enPassantFile_ != allCaptures && enPassantFile_ != move.to().file) ||
                toMove_ == ToMove::endOfGame) {
            return false;
        }

        at(move.to())   = at(move.from());
        at(move.from()) = noPiece;

        switch (move.type()) {
            case ChessMove::whiteEnPassant: at(move.to() - RankFile{1, 0}) = noPiece; break;
            case ChessMove::blackEnPassant: at(move.to() + RankFile{1, 0}) = noPiece; break;
            case ChessMove::whiteCastleKS:
                                            at(0, gFiles - 1)         = noPiece;
                                            at(0, move.to().file - 1) = whiteRook;
                                            break;
            case ChessMove::whiteCastleQS:
                                            at(0, 0)                  = noPiece;
                                            at(0, move.to().file + 1) = whiteRook;
                                            break;
            case ChessMove::blackCastleKS:
                                            at(gRanks - 1, gFiles - 1)         = noPiece;
                                            at(gRanks - 1, move.to().file - 1) = blackRook;
                                            break;
            case ChessMove::blackCastleQS:
                                            at(gRanks - 1, 0)                  = noPiece;
                                            at(gRanks - 1, move.to().file + 1) = blackRook;
                                            break;
            case ChessMove::promoQueen:
                                            assert(move.to().rank == gRanks - 1 || move.to().rank == 0);
                                            at(move.to()) = at(move.to()).isWhite() ? whiteQueen : blackQueen;
                                            break;
            case ChessMove::promoKnight:
                                            at(move.to()) = at(move.to()).isWhite() ? whiteKnight : blackKnight;
                                            break;
            case ChessMove::promoRook: at(move.to()) = at(move.to()).isWhite() ? whiteRook : blackRook; break;
            case ChessMove::promoBishop:
                                       at(move.to()) = at(move.to()).isWhite() ? whiteBishop : blackBishop;
                                       break;
#if ALLOW_KING_PROMOTION
            case ChessMove::promoKing: at(move.to()) = at(move.to()).isWhite() ? whiteKing : blackKing; break;
#endif
            default: break; // do Nothing
        }

        return true;
    }

    // all legal moves are added to the list, including castling
    template <typename Moves>
        requires std::is_base_of_v<MoveList, Moves> || std::is_base_of_v<OrderedMoveList, Moves>
    inline void Board::genLegalMoves(Moves& moves) const {
        genPseudoLegalMoves(moves);

        // castling moves
        if (isWhiteToMove() && (getCastle() & (whiteKS | whiteQS))) {
            // search for king on first rank
            for (int file = 0; file < files(); ++file) {
                if (at(0, file) == whiteKing) {
                    if (getCastle() & whiteKS) {
                        bool pieceInWay = false;
                        for (int j = file + 1; j < files() - 1; ++j)
                            if (!at(0, j).isEmpty())
                                pieceInWay = true;

                        if (!pieceInWay && at(0, files() - 1) == whiteRook && file + 2 < files()) {
                            if (!IsInCheck() && !WillBeInCheck({Occupant::whiteKing, 0, file, 0, file + 1}))
                                addMove({Occupant::whiteKing, 0, file, 0, file + 2, ChessMove::whiteCastleKS},
                                        moves);
                        }
                    }
                    if (getCastle() & whiteQS) {
                        bool pieceInWay = false;
                        for (int j = file - 1; j > 1; --j)
                            if (!at(0, j).isEmpty())
                                pieceInWay = true;

                        if (!pieceInWay && at(0, 0) == whiteRook && file - 2 > 0) {
                            if (!IsInCheck() &&
                                !WillBeInCheck(
                                    {Occupant::whiteKing, 0, file, 0, file - 1})) // cannot castle through
                                                                                  // check, or when in check
                                addMove({Occupant::whiteKing, 0, file, 0, file - 2, ChessMove::whiteCastleQS},
                                        moves);
                        }
                    }
                    break;
                }
            }

        } else if (isBlackToMove() && (getCastle() & (blackKS | blackQS))) {
            // search for king on back rank
            for (int file = 0; file < files(); ++file) {
                if (at(ranks() - 1, file) == blackKing) {
                    if (getCastle() & blackKS) {
                        bool pieceInWay = false;
                        for (int j = file + 1; j < files() - 1; ++j)
                            if (!at(ranks() - 1, j).isEmpty())
                                pieceInWay = true;

                        if (!pieceInWay && at(ranks() - 1, files() - 1) == blackRook && file + 2 < files()) {
                            if (!IsInCheck() &&
                                !WillBeInCheck(
                                    {Occupant::blackKing, ranks() - 1, file, ranks() - 1, file + 1})) {
                                addMove({Occupant::blackKing, ranks() - 1, file, ranks() - 1, file + 2,
                                         ChessMove::blackCastleKS},
                                        moves);
                            }
                        }
                    }
                    if (getCastle() & blackQS) {
                        bool pieceInWay = false;
                        for (int j = file - 1; j > 1; --j)
                            if (!at(ranks() - 1, j).isEmpty())
                                pieceInWay = true;

                        if (!pieceInWay && at(ranks() - 1, 0) == blackRook && file - 2 > 0) {
                            if (!IsInCheck() &&
                                !WillBeInCheck(
                                    {Occupant::blackKing, ranks() - 1, file, ranks() - 1, file - 1}))
                                addMove({Occupant::blackKing, ranks() - 1, file, ranks() - 1, file - 2,
                                         ChessMove::blackCastleQS},
                                        moves);
                        }
                    }
                    break;
                }
            }
        }

        if constexpr (std::is_same_v<Moves, MoveList>) {
            for (int file = 0; file < ssize(moves);) {
                if (WillBeInCheck(moves[file]))
                    moves.remove(file);
                else
                    ++file;
            };
        } else {
            TIMED(removeIllegalMoves(moves));
        }
    }

    bool Board::processMove(ChessMove const& m) {
        MoveList list;
        genLegalMoves(list);
        return processMove(m, list);
    }

    bool Board::processMove(ChessMove const& m, MoveList& list) {
        auto& source= at(m.from());
        auto& target= at(m.to());

        int enPassant =                                          //
            source.isPawn() && abs((m.to() - m.from()).rank) > 1 //
            ? m.to().file
            : noCaptures;
        unsigned plysSince =                     //
            !source.isPawn() && target.isEmpty() //
            ? pliesSince_ + 1
            : 0;

        // eliminate castling rights
        int castle = castle_;
        switch (source.contents()) {
            case whiteKing: castle &= ~(whiteKS | whiteQS); break;
            case blackKing: castle &= ~(blackKS | blackQS); break;
            case whiteRook:
                if (m.from().file < files() / 2)
                    castle &= ~whiteQS;
                else
                    castle &= ~whiteKS;
                break;
            case blackRook:
                if (m.from().file < files() / 2)
                    castle &= ~blackQS;
                else
                    castle &= ~blackKS;
                break;
            default: break; // do Nothing
        }

        // isLegal()
        if (toMove_ != ToMove::endOfGame && applyMove(m)) {
            status_        = CheckStatus(list);
            enPassantFile_ = enPassant;
            pliesSince_    = plysSince;
            castle_        = castle;

            if (status_ == GameStatus::notInCheck ||
                status_ == GameStatus::inCheck) { // otherwise end of game
                switchMove();
                if (toMove_ == ToMove::white) {
                    ++moveNumber_;
                }
            } else {
                toMove_ = ToMove::endOfGame;
            }
            return true;
        } else {
            return false;
        }
    }

    std::string Board::toSAN(ChessMove const& move, MoveList const& list) const {
        std::ostringstream o;

        bool conflict = false, fileConflict = false,
             rankConflict = false; // use the file in case of a conflict, and rank if no file
                                   // conflict, and both if necessary (e.g. there are three
                                   // pieces accessing the same square)
        switch (at(move.from()).contents()) {
            case whitePawn:
            case blackPawn:

                o << ChessFileToChar(move.from().file);
                if (move.from().file != move.to().file) {
                    o << 'x' << move.to(); // Capture; use style "exd5"
                } else {
                    o << ChessRankToChar(move.to().rank); // Non-capture; use style "e5"
                }
                switch (move.type()) {
                    case ChessMove::promoBishop: o << "=B"; break;
                    case ChessMove::promoKnight: o << "=N"; break;
                    case ChessMove::promoRook: o << "=R"; break;
                    case ChessMove::promoQueen: o << "=Q"; break;
                    case ChessMove::promoKing: o << "=K"; break;
                    default: // do nothing
                        break;
                }
                break;

            case whiteKing:
            case blackKing:
                // castling
                if (move.from().rank == move.to().rank &&
                    (move.from().rank == (at(move.from()).isWhite() ? 0 : ranks() - 1))) {
                    if (move.from().file - move.to().file < -1) {
                        o << "O-O";
                        break;
                    } else if (move.from().file - move.to().file > 1) {
                        o << "O-O-O";
                        break;
                    }
                }

                [[fallthrough]];
            default:
                o << (char)toupper(at(move.from()).pieceToChar());

                for (ChessMove const& alt : list) {
                    if (alt != move && // not the same move
                        alt.to().rank == move.to().rank &&
                        alt.to().file == move.to().file &&                       // the same 'to' square
                        at(move.from()).contents() == at(alt.from()).contents()) // same type of piece
                    {
                        conflict = true;
                        if (move.from().rank == alt.from().rank)
                            rankConflict = true;
                        else if (move.from().file == alt.from().file)
                            fileConflict = true;
                    }
                }
                // resolve if the piece is on same file of rank (if there are three same
                // pieces then use file and rank)
                if (conflict && !rankConflict && !fileConflict) {
                    o << ChessFileToChar(move.from().file);
                } else if (conflict && !rankConflict) {
                    o << ChessRankToChar(move.from().rank);
                } else if (fileConflict && rankConflict) {
                    o << ChessFileToChar(move.from().file) << ChessRankToChar(move.from().rank);
                } else if (rankConflict) {
                    o << ChessFileToChar(move.from().file);
                }

                // determine if it's a capture
                if (!at(move.to()).isEmpty()) {
                    o << 'x';
                }

                // destination square
                o << ChessFileToChar(move.to().file) << ChessRankToChar(move.to().rank);

                // Check or Checkmate
                //??! Removed for speed, and compatibility with Board::genLegalMoveSet()
        }
        return o.str();
    }

    // throws EmptyMove, IllegalMove, InvalidSAN
    ChessMove Board::parseSAN(std::string_view constSAN, MoveList const& list) const {
        ChessMove   move;
        std::string san(constSAN);

        RemoveWhiteSpace(san);
        // remove unnecessary chars
        RemoveChars(san, "+#x=!? ");

        if (san.empty())
            throw EmptyMove{};

        // get piece type, look for
        Occupant piece;
        switch (san[0]) {
            case 'N':
            case 'n': piece = isWhiteToMove() ? whiteKnight : blackKnight; break;
            case 'B': piece = isWhiteToMove() ? whiteBishop : blackBishop; break;
            case 'R':
            case 'r': piece = isWhiteToMove() ? whiteRook : blackRook; break;
            case 'Q':
            case 'q': piece = isWhiteToMove() ? whiteQueen : blackQueen; break;
            case 'K':
            case 'k': piece = isWhiteToMove() ? whiteKing : blackKing; break;
            case 'O':
            case 'o': // castling
                if (NumCharsInStr(san, 'O') > 2) {
                    for (auto& match : list)
                        if (match.type() == ChessMove::whiteCastleQS ||
                            match.type() == ChessMove::blackCastleQS) {
                            assert((isWhiteToMove() && match.type() == ChessMove::whiteCastleQS) ||
                                   (isBlackToMove() && match.type() == ChessMove::blackCastleQS));
                            return match;
                        }
                    throw IllegalMove{constSAN};
                } else {
                    for (auto& match : list)
                        if (match.type() == ChessMove::whiteCastleKS ||
                            match.type() == ChessMove::blackCastleKS) {
                            assert((isWhiteToMove() && match.type() == ChessMove::whiteCastleKS) ||
                                   (isBlackToMove() && match.type() == ChessMove::blackCastleKS));
                            return match;
                        }
                    throw IllegalMove{constSAN};
                }

            default: piece = isWhiteToMove() ? whitePawn : blackPawn; break;
        }

        move.type() = ChessMove::normal;

        if (piece == whitePawn || piece == blackPawn) {
            // is it a promotion?
            if (IsPromoChar(san.back())) //!!? Lower case b is not counted as Bishop
            {
                switch (toupper(san.back())) {
                    case 'N': move.type() = ChessMove::promoKnight; break;
                    case 'B': move.type() = ChessMove::promoBishop; break;
                    case 'R': move.type() = ChessMove::promoRook; break;
                    case 'Q': move.type() = ChessMove::promoQueen; break;
#ifdef ALLOW_KING_PROMOTION
                    case 'K': move.type() = ChessMove::promoKing; break;
#endif
                    default: assert(0); // not Reached
                }
                san.pop_back();
                if (san.empty())
                    throw IllegalMove{constSAN};
            }
            // possibilities: "f", "f4", "ef (e.p.)", "ef4", "exf4 (e.p.)", "e3f4
            // (e.p.)", "e3xf4" (e.p.) all x's have been removed, e.p. is taken care of
            if (san.empty())
                throw IllegalMove{constSAN};

            if (san.length() == 1) // f
            {
                for (auto& match : list)
                    if (match.from().file == ChessCharToFile(san[0]) &&
                        (move.isPromo() ? move.type() == match.type() : true) && at(match.from()) == piece) {
                        return match;
                    }
            } else if (san.length() == 2 && isdigit(san[1])) // f4
            {
                for (auto& match : list)
                    if (match.from().file == ChessCharToFile(san[0]) &&
                        match.to().rank == ChessCharToRank(san[1]) &&
                        (move.isPromo() ? move.type() == match.type() : true) && at(match.from()) == piece) {
                        return match;
                    }

            } else if (san.length() == 2) // ef
            {
                assert(!isdigit(san[1]));
                for (auto& match : list)
                    if (match.from().file == ChessCharToFile(san[0]) &&
                        match.to().file == ChessCharToFile(san[1]) &&
                        (move.isPromo() ? move.type() == match.type() : true) && at(match.from()) == piece) {
                        return match;
                    }
            } else if (san.length() == 3) // ef4
            {
                for (auto& match : list)
                    if (match.from().file == ChessCharToFile(san[0]) &&
                        match.to().file == ChessCharToFile(san[1]) &&
                        match.to().rank == ChessCharToRank(san[2]) &&
                        (move.isPromo() ? move.type() == match.type() : true) && at(match.from()) == piece) {
                        return match;
                    }
            } else if (san.length() == 4) // e3f4
            {
                for (auto& match : list)
                    if (match.from().file == ChessCharToFile(san[0]) &&
                        match.from().rank == ChessCharToRank(san[1]) &&
                        match.to().file == ChessCharToFile(san[2]) &&
                        match.to().rank == ChessCharToRank(san[3]) &&
                        (move.isPromo() ? move.type() == match.type() : true) && at(match.from()) == piece) {
                        return match;
                    }
            }
        } else {
            size_t const sanLength = san.length();
            if (sanLength < 3)
                throw InvalidSAN{constSAN};

            for (auto& match : list)
                if (match.to().file == ChessCharToFile(san[sanLength - 2]) &&
                    match.to().rank == ChessCharToRank(san[sanLength - 1]) && at(match.from()) == piece) {
                    switch (sanLength) {
                        case 3: return match; // Nf3
                        case 4:
                            if (isdigit(san[1]) && match.from().rank == ChessCharToRank(san[1])) // N1f3
                            {
                                return match;
                            } else if (match.from().file == ChessCharToFile(san[1])) // Ngf3
                            {
                                return match;
                            }
                            break;
                        case 5:
                            if (match.from().file == ChessCharToFile(san[1]) &&
                                match.from().rank == ChessCharToRank(san[2])) // Ng1f3
                            {
                                return match;
                            }
                            break;
                        default: throw InvalidSAN{constSAN};
                    }
                }
        }

        throw IllegalMove{constSAN};
    }

    void Board::genLegalMoveSet(OrderedMoveList& moves) {
        // should combine genLegalMoves and toSAN efficiently
        genLegalMoves(moves);
        // disambiguateAllMoves
        moves.disambiguateMoves();
    }

    inline void Board::addMove(ChessMove mv, MoveList& moves) const {
#if !ALLOW_KING_PROMOTION
        if (mv.type() == ChessMove::promoKing)
            return;
#endif
        moves.add(mv);
    }

    inline void Board::addMove(ChessMove mv, OrderedMoveList& moves) const {
#if !ALLOW_KING_PROMOTION
        if (mv.type() == ChessMove::promoKing)
            return;
#endif
        moves.list.add(mv);
        moves.bysan.add({mv, ambiguousSAN(mv)});
    }

    void Board::removeIllegalMoves(OrderedMoveList& moves) const {
        auto& [list, allSAN] = moves;
        bool     isFound     = false;
        Occupant activeKing  = toMove() == ToMove::white ? whiteKing : blackKing;

        RankFile targetSquare;

        for (int r = 0; r < ranks() && !isFound; ++r)
            for (int f = 0; f < files() && !isFound; ++f) {
                if (at(r, f) == activeKing) {
                    targetSquare = {r, f};
                    isFound      = true;
                }
            }

        if (!isFound) {
            return; // there is no king on the board
        }

        Board b = *this;
        for (unsigned i = 0; i < list.size(); b = *this) {
            ChessMove m = list[i];
            b.applyMove(m);
            b.switchMove();

            if (b.at(m.to()) == whiteKing || b.at(m.to()) == blackKing) {
                if (b.canCaptureSquare(m.to())) {
                    list.remove(i);
                    for (unsigned j = 0; j < allSAN.size(); ++j)
                        if (allSAN[j].move() == m) {
                            allSAN.remove(j);
                            break;
                        }
                } else
                    ++i;
            } else {
                if (b.canCaptureSquare(targetSquare)) {
                    list.remove(i);
                    for (unsigned j = 0; j < allSAN.size(); ++j)
                        if (allSAN[j].move() == m) {
                            allSAN.remove(j);
                            break;
                        }
                } else
                    ++i;
            }
        }
    }

    // doesn't worry about any ambiguities, nor does it indicate check
    // or checkmate status (which don't alter sort order anyway)
    std::string Board::ambiguousSAN(ChessMove const& m) const {
        std::ostringstream o;

        switch (at(m.from()).contents()) {
            case whitePawn:
            case blackPawn:
                o << ChessFileToChar(m.from().file);

                if (m.from().file != m.to().file) {
                    o << 'x' << ChessFileToChar(m.to().file)
                      << ChessRankToChar(m.to().rank); // Capture; use style "exd5"
                } else {
                    o << ChessRankToChar(m.to().rank); // Non-capture; use style "e5"
                }
                switch (m.type()) {
                    case ChessMove::promoBishop: o << "=B"; break;
                    case ChessMove::promoKnight: o << "=N"; break;
                    case ChessMove::promoRook: o << "=R"; break;
                    case ChessMove::promoQueen: o << "=Q"; break;
#if ALLOW_KING_PROMOTION
                    case ChessMove::promoKing: o << "=K"; break;
#endif
                    default: break; // do Nothing
                }
                break;

            case whiteKing:
            case blackKing:
                // castling
                if (m.from().rank == m.to().rank &&
                    (m.from().rank == (at(m.from()).isWhite() ? 0 : ranks() - 1))) {
                    if ((signed)m.from().file - (signed)m.to().file < -1) {
                        o << "O-O";
                        break;
                    } else if ((signed)m.from().file - (signed)m.to().file > 1) {
                        o << "O-O-O";
                        break;
                    }
                }

                [[fallthrough]];
            default:
                o << (char)toupper(at(m.from()).pieceToChar());

                // determine if it's a capture
                if (!at(m.to()).isEmpty())
                    o << 'x';

                // destination square
                o << ChessFileToChar(m.to().file) << ChessRankToChar(m.to().rank);
        }
        return std::move(o).str();
    }

    //-----------------------------------------------------------------------------
    // GenLegalMoves -- generates all possible legal moves adding them to the list
    //
    // pseudoLegal: not castling, and not worrying about being left in check
    //
    template <typename Moves>
        requires std::is_base_of_v<MoveList, Moves> || std::is_base_of_v<OrderedMoveList, Moves>
    void Board::genPseudoLegalMoves(Moves& moves) const {
        if (toMove() == ToMove::endOfGame)
            return;

        for (int rf = 0; rf < ranks(); ++rf)
            for (int ff = 0; ff < files(); ++ff) {
                auto& source = at(rf, ff);
                if ((source.isBlack() && toMove() == ToMove::white) ||
                    (source.isWhite() && toMove() == ToMove::black)) {
                    continue;
                }
                switch (auto actor = source.contents()) {
                    case whitePawn:
                        if (rf < 7 && at(rf + 1, ff).isEmpty()) {
                            if (rf == ranks() - 2) // promotion
                            {
                                addMove({actor, rf, ff, rf + 1, ff, ChessMove::promoQueen}, moves);
                                addMove({actor, rf, ff, rf + 1, ff, ChessMove::promoBishop}, moves);
                                addMove({actor, rf, ff, rf + 1, ff, ChessMove::promoKnight}, moves);
                                addMove({actor, rf, ff, rf + 1, ff, ChessMove::promoRook}, moves);
                                addMove({actor, rf, ff, rf + 1, ff, ChessMove::promoKing}, moves);
                            } else {
                                addMove({actor, rf, ff, rf + 1, ff}, moves);
                            }
                        }
                        if (rf == 1 && at(2, ff).isEmpty() && at(3, ff).isEmpty()) {
                            addMove({actor, rf, ff, 3, ff}, moves);
                        }
                        for (int s = -1; s <= 1; s += 2) {
                            if (rf < 7 && ff + s >= 0 && ff + s <= 7 && at(rf + 1, ff + s).isBlack()) {
                                if (rf == ranks() - 2) // promotion
                                {
                                    addMove({actor, rf, ff, rf + 1, ff + s, ChessMove::promoQueen}, moves);
                                    addMove({actor, rf, ff, rf + 1, ff + s, ChessMove::promoKnight}, moves);
                                    addMove({actor, rf, ff, rf + 1, ff + s, ChessMove::promoBishop}, moves);
                                    addMove({actor, rf, ff, rf + 1, ff + s, ChessMove::promoRook}, moves);
                                    addMove({actor, rf, ff, rf + 1, ff + s, ChessMove::promoKing}, moves);
                                } else {
                                    addMove({actor, rf, ff, rf + 1, ff + s}, moves);
                                }
                            }
                            if (rf == ranks() - 4) {
                                if (ff + s >= 0 && ff + s <= 7 &&
                                    (enPassant() == ff + s || enPassant() == Board::allCaptures) &&
                                    at(ranks() - 4, ff + s).contents() == blackPawn &&
                                    at(ranks() - 3, ff + s).isEmpty()) {

                                    addMove({actor, rf, ff, ranks() - 3, ff + s, ChessMove::whiteEnPassant},
                                            moves);
                                }
                            }
                        }
                        break;

                    case blackPawn:
                        if (rf > 0 && at(rf - 1, ff).isEmpty()) {
                            if (rf == 1) // promotion
                            {
                                addMove({actor, rf, ff, rf - 1, ff, ChessMove::promoQueen}, moves);
                                addMove({actor, rf, ff, rf - 1, ff, ChessMove::promoKnight}, moves);
                                addMove({actor, rf, ff, rf - 1, ff, ChessMove::promoBishop}, moves);
                                addMove({actor, rf, ff, rf - 1, ff, ChessMove::promoRook}, moves);
                                addMove({actor, rf, ff, rf - 1, ff, ChessMove::promoKing}, moves);
                            } else {
                                addMove({actor, rf, ff, rf - 1, ff}, moves);
                            }
                        }
                        if (rf == ranks() - 2 && at(ranks() - 3, ff).isEmpty() &&
                            at(ranks() - 4, ff).isEmpty()) {
                            addMove({actor, rf, ff, ranks() - 4, ff}, moves);
                        }
                        for (int s = -1; s <= 1; s += 2) {
                            if (rf > 0 && ff + s >= 0 && ff + s <= 7 && at(rf - 1, ff + s).isWhite()) {
                                if (rf == 1) // promotion
                                {
                                    addMove({actor, rf, ff, rf - 1, ff + s, ChessMove::promoQueen}, moves);
                                    addMove({actor, rf, ff, rf - 1, ff + s, ChessMove::promoKnight}, moves);
                                    addMove({actor, rf, ff, rf - 1, ff + s, ChessMove::promoBishop}, moves);
                                    addMove({actor, rf, ff, rf - 1, ff + s, ChessMove::promoRook}, moves);
                                    addMove({actor, rf, ff, rf - 1, ff + s, ChessMove::promoKing}, moves);
                                } else {
                                    addMove({actor, rf, ff, rf - 1, ff + s}, moves);
                                }
                            }
                            if (rf == 3) {
                                if (ff + s >= 0 && ff + s <= 7 &&
                                    (enPassant() == ff + s || enPassant() == Board::allCaptures) &&
                                    at(3, ff + s).contents() == whitePawn && at(2, ff + s).isEmpty()) {
                                    addMove({actor, rf, ff, 2, ff + s, ChessMove::blackEnPassant}, moves);
                                }
                            }
                        }
                        break;

                    case whiteKnight:
                    case blackKnight:
                        for (int i = -1; i <= 1; i += 2)
                            for (int j = -1; j <= 1; j += 2)
                                for (int s = 1; s <= 2; s++) {
                                    int rt = rf + i * s;
                                    int ft = ff + j * (3 - s);
                                    if (rt < 0 || rt > ranks() - 1 || ft < 0 || ft > files() - 1)
                                        continue;

                                    if (IsSameColor(at(rf, ff), at(rt, ft)))
                                        continue;

                                    addMove({actor, rf, ff, rt, ft}, moves);
                                }
                        break;

                    case whiteBishop:
                    case blackBishop:
                        for (int rs = -1; rs <= 1; rs += 2)
                            for (int fs = -1; fs <= 1; fs += 2)
                                for (int i = 1;; i++) {
                                    int rt = rf + (i * rs);
                                    int ft = ff + (i * fs);
                                    if (rt < 0 || rt > ranks() - 1 || ft < 0 || ft > files() - 1)
                                        break;
                                    if (IsSameColor(at(rf, ff), at(rt, ft)))
                                        break;

                                    addMove({actor, rf, ff, rt, ft}, moves);

                                    if (!at(rt, ft).isEmpty())
                                        break;
                                }
                        break;

                    case whiteRook:
                    case blackRook:
                        for (int d = 0; d <= 1; d++)
                            for (int s = -1; s <= 1; s += 2)
                                for (int i = 1;; i++) {
                                    int rt = rf + (i * s) * d;
                                    int ft = ff + (i * s) * (1 - d);
                                    if (rt < 0 || rt > ranks() - 1 || ft < 0 || ft > files() - 1)
                                        break;
                                    if (IsSameColor(at(rf, ff), at(rt, ft)))
                                        break;

                                    addMove({actor, rf, ff, rt, ft}, moves);

                                    if (!at(rt, ft).isEmpty())
                                        break;
                                }
                        break;

                    case whiteQueen:
                    case blackQueen:
                        for (int rs = -1; rs <= 1; rs++)
                            for (int fs = -1; fs <= 1; fs++) {
                                if (rs == 0 && fs == 0)
                                    continue;
                                for (int i = 1;; i++) {
                                    int rt = rf + (i * rs);
                                    int ft = ff + (i * fs);

                                    if (rt < 0 || rt > 7 || ft < 0 || ft > 7)
                                        break;
                                    if (IsSameColor(at(rf, ff), at(rt, ft)))
                                        break;

                                    addMove({actor, rf, ff, rt, ft}, moves);

                                    if (!at(rt, ft).isEmpty())
                                        break;
                                }
                            }
                        break;

                    case whiteKing:
                    case blackKing:
                        for (int i = -1; i <= 1; i++)
                            for (int j = -1; j <= 1; j++) {
                                if (i == 0 && j == 0)
                                    continue;
                                int rt = rf + i;
                                int ft = ff + j;
                                if (rt < 0 || rt > 7 || ft < 0 || ft > 7)
                                    continue;
                                if (IsSameColor(at(rf, ff), at(rt, ft)))
                                    continue;

                                addMove({actor, rf, ff, rt, ft}, moves);
                            }
                        break;

                    case noPiece: break;

                    default: assert(0); // not Reached
                }
            }
    }

    // all moves that capture the square, square can be same color as person to move
    bool Board::canCaptureSquare(RankFile target) const {
        assert(target.rank < ranks());
        assert(target.file < files());

        if (toMove() == ToMove::endOfGame)
            return false;

        for (int rf = 0; rf < ranks(); ++rf)
            for (int ff = 0; ff < files(); ++ff) {
                if ((at(rf, ff).isBlack() && toMove() == ToMove::white) ||
                    (at(rf, ff).isWhite() && toMove() == ToMove::black)) {
                    continue;
                }
                switch (at(rf, ff).contents()) {

                    case whitePawn:

                        if (rf + 1 == target.rank && (ff - 1 == target.file || ff + 1 == target.file))
                            return true;
                        break;

                    case blackPawn:

                        if (rf - 1 == target.rank && (ff - 1 == target.file || ff + 1 == target.file))
                            return true;
                        break;

                    case whiteKnight:
                    case blackKnight:
                        for (int i = -1; i <= 1; i += 2)
                            for (int j = -1; j <= 1; j += 2)
                                for (int s = 1; s <= 2; s++) {
                                    int rt = rf + i * s;
                                    int ft = ff + j * (3 - s);
                                    if (rt != target.rank || ft != target.file)
                                        continue;

                                    if (rt == target.rank && ft == target.file)
                                        return true;
                                }
                        break;

                    case whiteBishop:
                    case blackBishop:
                        for (int rs = -1; rs <= 1; rs += 2)
                            for (int fs = -1; fs <= 1; fs += 2)
                                for (int i = 1;; i++) {
                                    int rt = rf + (i * rs);
                                    int ft = ff + (i * fs);
                                    if (rt < 0 || rt > ranks() - 1 || ft < 0 || ft > files() - 1)
                                        break;

                                    if (rt == target.rank && ft == target.file)
                                        return true;

                                    if (!at(rt, ft).isEmpty())
                                        break;
                                }
                        break;

                    case whiteRook:
                    case blackRook:
                        for (int d = 0; d <= 1; d++)
                            for (int s = -1; s <= 1; s += 2)
                                for (int i = 1;; i++) {
                                    int rt = rf + (i * s) * d;
                                    int ft = ff + (i * s) * (1 - d);
                                    if (rt < 0 || rt > ranks() - 1 || ft < 0 || ft > files() - 1)
                                        break;

                                    if (rt == target.rank && ft == target.file)
                                        return true;

                                    if (!at(rt, ft).isEmpty())
                                        break;
                                }
                        break;

                    case whiteQueen:
                    case blackQueen:
                        for (int rs = -1; rs <= 1; rs++)
                            for (int fs = -1; fs <= 1; fs++) {
                                if (rs == 0 && fs == 0)
                                    continue;
                                for (int i = 1;; i++) {
                                    int rt = rf + (i * rs);
                                    int ft = ff + (i * fs);

                                    if (rt < 0 || rt > ranks() - 1 || ft < 0 || ft > files() - 1)
                                        break;

                                    if (rt == target.rank && ft == target.file)
                                        return true;

                                    if (!at(rt, ft).isEmpty())
                                        break;
                                }
                            }
                        break;

                    case whiteKing:
                    case blackKing:

                        for (int i = -1; i <= 1; i++)
                            for (int j = -1; j <= 1; j++) {
                                if (i == 0 && j == 0)
                                    continue;
                                int rt = rf + i;
                                int ft = ff + j;
                                if (rt != target.rank || ft != target.file)
                                    continue;

                                if (rt == target.rank && ft == target.file)
                                    return true;
                            }
                        break;

                    case noPiece: break;

                    default: assert(0); // not Reached
                }
            }

        return false;
    }

    void OrderedMoveList::disambiguate(ChessMove const& move, std::string& san) {
        if (san.length()) {
            bool conflict = false, rankConflict = false, fileConflict = false;
            for (int i = 0; i < ssize(bysan); ++i) {
                auto& altMv = bysan[i].move();
                if (altMv.to() == move.to() &&     // the same 'to' square
                    altMv != move &&               // not the same move
                    move.actor() == altMv.actor()) // same type of piece
                {
                    conflict = true;
                    if (move.from().rank == altMv.from().rank)
                        rankConflict = true;
                    else if (move.from().file == altMv.from().file)
                        fileConflict = true;
                    if (rankConflict && fileConflict)
                        break; // if you have two conflicts, no need to continue
                }
            }
            // resolve if the piece is on same file of rank (if there are three same
            // pieces then use file and rank)
            if (conflict && !rankConflict && !fileConflict) {
                san.insert(1, 1, ChessFileToChar(move.from().file));
            } else if (conflict && !rankConflict) {
                san.insert(1, 1, ChessRankToChar(move.from().rank));
            } else if (fileConflict && rankConflict) {
                san.insert(1, {ChessFileToChar(move.from().file), ChessRankToChar(move.from().rank)});
            } else if (rankConflict) {
                san.insert(1, 1, ChessFileToChar(move.from().file));
            }
        }
    }

    void OrderedMoveList::disambiguateMoves() {
        auto& [list, allSAN] = *this;
        bool ambiguity       = false;
        int  i;
        for (i = 0; i < ssize(allSAN) - 1; ++i) {
            if (allSAN[i] == allSAN[i + 1]) {
                disambiguate(allSAN[i].move(), allSAN[i].SAN());
                ambiguity = true;
            } else if (ambiguity) {
                disambiguate(allSAN[i].move(), allSAN[i].SAN());
                ambiguity = false;
            }
        }
        if (ambiguity) { // for the last element
            disambiguate(allSAN[i].move(), allSAN[i].SAN());
        }
    }

    // returns if the person to move is in check
    bool Board::IsInCheck() const {
        // scan a1,a2,...,h8 only test first king found
        bool     isFound = false;
        Occupant target  = toMove() == ToMove::white ? whiteKing : blackKing;

        RankFile targetSquare;

        for (int r = 0; r < ranks() && !isFound; ++r)
            for (int f = 0; f < files() && !isFound; ++f) {
                if (at(r, f).contents() == target) {
                    targetSquare = {r, f};
                    isFound        = true;
                }
            }

        if (!isFound) {
            return false; // there is no king on the board
        }

        Board b = *this;
        b.switchMove();
        return b.canCaptureSquare(targetSquare);
    }

    // if the move is made what will you be left in check?
    bool Board::WillBeInCheck(ChessMove const& move) const {
        Board b = *this;
        b.applyMove(move);
        return b.IsInCheck();
    }

    // if the move is made will you be giving check?
    bool Board::WillGiveCheck(ChessMove const& move) const {
        Board b = *this;
        b.applyMove(move);
        b.switchMove();
        return b.IsInCheck();
    }

    GameStatus Board::CheckStatus(MoveList const& list) const {
        if (list.empty())
            return IsInCheck() ? GameStatus::inCheckmate : GameStatus::inStalemate;
        else
            return IsInCheck() ? GameStatus::inCheck : GameStatus::notInCheck;
    }
} // namespace pgn2pgc::Chess

#ifdef TEST

    #include <iostream>

int main() {
    // NormalChessGame g1;
    /*
    char c[] = {" exf4 "};

    std::cout << "\n" << strcspn("eggxggf4", "xft ");

    RemoveChars(c, " ");

    std::cout << "\n '" << c << "' ";
    */
    char                  buf[20];
    pgn2pgc::Chess::Board b;

    while (std::cin.good()) {

        b.display();
        pgn2pgc::Chess::OrderedMoveList moves;
        b.genLegalMoveSet(moves);
        std::cout << "\n";

        for (auto& mv : moves.bysan)
            std::cout << mv.SAN() << "\t";

        std::cout << "\nEnter Move (or end): ";
        std::cin.getline(buf, sizeof(buf) / sizeof(buf[0]));
        std::cout << "buffer: '" << buf << "' ";

        if (!strcmp(buf, "end"))
            break;

        auto        move = b.parseSAN(buf, moves.list); // TODO handle MoveError
        std::string SAN  = b.toSAN(move, moves.list);

        b.processMove(move, moves.list);

        std::cout << "\n Moved: '" << SAN.c_str() << "' ";
    }
}

#endif
