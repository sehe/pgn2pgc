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
                                    fBoard.at(i).at(j++) = noPiece;
                            else
                                fBoard.at(i).at(j++) = LetterToSquare(ch);
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
                        enPassant_ = noCaptures;
                    } else {
                        if (!isalpha(token.front()))
                            return false;

                        enPassant_ = tolower(token.front()) - 'a';
                        assert(enPassant_ > 0);

                        if (enPassant_ >= gFiles)
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
                std::cout << fBoard[i][j].pieceToChar();
            }
        }
        std::cout << "\n  abcdefgh \n";
        std::cout << "\nTo Move:\t " << std::to_underlying(toMove_);
        std::cout << "\nCastle:\t " << castle_;
        std::cout << "\nStatus:\t " << std::to_underlying(status_);
        std::cout << "\nEnPassant:\t " << enPassant_;
        std::cout << "\nMove: " << moveNumber_;
        std::cout << "\nPlies Since:\t " << pliesSince_;
    }

    // true on success
    //??!! No checking is done?
    bool Board::applyMove(ChessMove const& move) {
        // can't move an empty square or capture your own piece
        // illegal en-passant
        // general legality -- check etc.. ?? How much checking should be done here?

        if (fBoard[move.r_from()][move.f_from()].isEmpty() ||
            IsSameColor(fBoard[move.r_from()][move.f_from()], fBoard[move.r_to()][move.f_to()]) ||
            (move.f_from() == move.f_to() && move.r_from() == move.r_to()) ||
            (move.isEnPassant() && enPassant_ != allCaptures && enPassant_ != move.f_to()) ||
            toMove_ == ToMove::endOfGame) {
            return false;
        }

        fBoard[move.r_to()][move.f_to()]     = fBoard[move.r_from()][move.f_from()];
        fBoard[move.r_from()][move.f_from()] = noPiece;

        switch (move.type()) {
            case ChessMove::whiteEnPassant: fBoard[move.r_to() - 1][move.f_to()] = noPiece; break;
            case ChessMove::blackEnPassant: fBoard[move.r_to() + 1][move.f_to()] = noPiece; break;
            case ChessMove::whiteCastleKS:
                fBoard[0][gFiles - 1]      = noPiece;
                fBoard[0][move.f_to() - 1] = whiteRook;
                break;
            case ChessMove::whiteCastleQS:
                fBoard[0][0]               = noPiece;
                fBoard[0][move.f_to() + 1] = whiteRook;
                break;
            case ChessMove::blackCastleKS:
                fBoard[gRanks - 1][gFiles - 1]      = noPiece;
                fBoard[gRanks - 1][move.f_to() - 1] = blackRook;
                break;
            case ChessMove::blackCastleQS:
                fBoard[gRanks - 1][0]               = noPiece;
                fBoard[gRanks - 1][move.f_to() + 1] = blackRook;
                break;
            case ChessMove::promoQueen:
                assert(move.rt() == gRanks - 1 || move.rt() == 0);
                fBoard[move.r_to()][move.f_to()] =
                    fBoard[move.r_to()][move.f_to()].isWhite() ? whiteQueen : blackQueen;
                break;
            case ChessMove::promoKnight:
                fBoard[move.r_to()][move.f_to()] =
                    fBoard[move.r_to()][move.f_to()].isWhite() ? whiteKnight : blackKnight;
                break;
            case ChessMove::promoRook:
                fBoard[move.r_to()][move.f_to()] =
                    fBoard[move.r_to()][move.f_to()].isWhite() ? whiteRook : blackRook;
                break;
            case ChessMove::promoBishop:
                fBoard[move.r_to()][move.f_to()] =
                    fBoard[move.r_to()][move.f_to()].isWhite() ? whiteBishop : blackBishop;
                break;
#if ALLOW_KING_PROMOTION
            case ChessMove::promoKing:
                fBoard[move.rt()][move.ft()] =
                    fBoard[move.rt()][move.ft()].isWhite() ? ChessSquare::whiteKing : ChessSquare::blackKing;
                break;
#endif
            default: break; // do Nothing
        }

        return true;
    }

    // all legal moves are added to the list, including castling
    inline void Board::genLegalMoves(MoveList& moves) const {
        GenPseudoLegalMoves(moves); // FIXME
        int i;

        // castling moves
        if (isWhiteToMove() && ((getCastle() & Board::whiteKS) || (getCastle() & Board::whiteQS))) {
            // search for king on first rank
            for (i = 0; i < files(); ++i) {
                if (fBoard[0][i] == whiteKing) {
                    if (getCastle() & Board::whiteKS) {
                        bool pieceInWay = false;
                        for (int j = i + 1; j < files() - 1; ++j)
                            if (!fBoard[0][j].isEmpty())
                                pieceInWay = true;

                        if (!pieceInWay && fBoard[0][files() - 1] == whiteRook && i + 2 < files()) {
                            if (!IsInCheck() && !WillBeInCheck(ChessMove(0, i, 0, i + 1)))
                                moves.add(ChessMove(0, i, 0, i + 2, ChessMove::whiteCastleKS));
                        }
                    }
                    if (getCastle() & Board::whiteQS) {
                        bool pieceInWay = false;
                        for (int j = i - 1; j > 1; --j)
                            if (!fBoard[0][j].isEmpty())
                                pieceInWay = true;

                        if (!pieceInWay && fBoard[0][0] == whiteRook && i - 2 > 0) {
                            if (!IsInCheck() &&
                                !WillBeInCheck(ChessMove(0, i, 0, i - 1))) // cannot castle through
                                                                           // check, or when in check
                                moves.add(ChessMove(0, i, 0, i - 2, ChessMove::whiteCastleQS));
                        }
                    }
                }
                //!!? Break is possible here, unless two kings on rank and one can castle
                //!(some form of wild?)
            }

        } else if (isBlackToMove() && ((getCastle() & Board::blackKS) || (getCastle() & Board::blackQS))) {
            // search for king on back rank
            for (i = 0; i < files(); ++i) {
                if (fBoard[ranks() - 1][i] == blackKing) {
                    if (getCastle() & Board::blackKS) {
                        bool pieceInWay = false;
                        for (int j = i + 1; j < files() - 1; ++j)
                            if (!fBoard[ranks() - 1][j].isEmpty())
                                pieceInWay = true;

                        if (!pieceInWay && fBoard[ranks() - 1][files() - 1] == blackRook && i + 2 < files()) {
                            if (!IsInCheck() &&
                                !WillBeInCheck(ChessMove(ranks() - 1, i, ranks() - 1, i + 1))) {
                                moves.add(
                                    ChessMove(ranks() - 1, i, ranks() - 1, i + 2, ChessMove::blackCastleKS));
                            }
                        }
                    }
                    if (getCastle() & Board::blackQS) {
                        bool pieceInWay = false;
                        for (int j = i - 1; j > 1; --j)
                            if (!fBoard[ranks() - 1][j].isEmpty())
                                pieceInWay = true;

                        if (!pieceInWay && fBoard[ranks() - 1][0] == blackRook && i - 2 > 0) {
                            if (!IsInCheck() && !WillBeInCheck(ChessMove(ranks() - 1, i, ranks() - 1, i - 1)))
                                moves.add(
                                    ChessMove(ranks() - 1, i, ranks() - 1, i - 2, ChessMove::blackCastleQS));
                        }
                    }
                }
            }
        }

        i = 0;
        while (i < ssize(moves)) {
            if (WillBeInCheck(moves[i]))
                moves.remove(i);
            else
                ++i;
        };
    }

    bool Board::processMove(ChessMove const& m) {
        MoveList legal;
        genLegalMoves(legal);
        return processMove(m, legal);
    }

    bool Board::processMove(ChessMove const& m, MoveList& legal) {
        int enPassant =
            fBoard[m.r_from()][m.f_from()].isPawn() && abs(m.r_to() - m.r_from()) > 1 ? m.f_to() : noCaptures;
        unsigned plysSince = !fBoard[m.r_from()][m.f_from()].isPawn() && fBoard[m.r_to()][m.f_to()].isEmpty()
            ? pliesSince_ + 1
            : 0;

        // eliminate castling move's
        int castle = castle_;
        switch (fBoard[m.r_from()][m.f_from()].contents()) {
            case whiteKing: castle &= ~(whiteKS | whiteQS); break;
            case blackKing: castle &= ~(blackKS | blackQS); break;
            case whiteRook:
                if (m.f_from() < files() / 2)
                    castle &= ~whiteQS;
                else
                    castle &= ~whiteKS;
                break;
            case blackRook:
                if (m.f_from() < files() / 2)
                    castle &= ~blackQS;
                else
                    castle &= ~blackKS;
                break;
            default: break; // do Nothing
        }

        // isLegal()
        if (toMove_ != ToMove::endOfGame && applyMove(m)) {
            status_     = CheckStatus(legal);
            enPassant_  = enPassant;
            pliesSince_ = plysSince;
            castle_     = castle;

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

    std::string Board::toSAN(ChessMove const& move, MoveList const& legal) const {
        std::ostringstream o;

        bool conflict = false, fileConflict = false,
             rankConflict = false; // use the file in case of a conflict, and rank if no file
                                   // conflict, and both if necessary (e.g. there are three
                                   // pieces accessing the same square)
        switch (fBoard[move.r_from()][move.f_from()].contents()) {
            case whitePawn:
            case blackPawn:

                o << ChessFileToChar(move.f_from());
                if (move.f_from() != move.f_to()) {
                    o << 'x' << ChessFileToChar(move.f_to())
                      << ChessRankToChar(move.r_to()); // Capture; use style "exd5"
                } else {
                    o << ChessRankToChar(move.r_to()); // Non-capture; use style "e5"
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
                if (move.r_from() == move.r_to() &&
                            move.r_from() == (fBoard[move.r_from()][move.f_from()].isWhite())
                        ? 0
                        : ranks() - 1) {
                    if ((signed)move.f_from() - (signed)move.f_to() < -1) {
                        o << "O-O";
                        break;
                    } else if ((signed)move.f_from() - (signed)move.f_to() > 1) {
                        o << "O-O-O";
                        break;
                    }
                }

                [[fallthrough]];
            default:
                o << (char)toupper(fBoard[move.r_from()][move.f_from()].pieceToChar());

                for (int i = 0; i < ssize(legal); ++i) {
                    if (legal[i] != move && // not the same move
                        legal[i].r_to() == move.r_to() &&
                        legal[i].f_to() == move.f_to() && // the same 'to' square
                        fBoard[move.r_from()][move.f_from()].contents() ==
                            fBoard[legal[i].r_from()][legal[i].f_from()].contents()) // same type of piece
                    {
                        conflict = true;
                        if (move.r_from() == legal[i].r_from())
                            rankConflict = true;
                        else if (move.f_from() == legal[i].f_from())
                            fileConflict = true;
                    }
                }
                // resolve if the piece is on same file of rank (if there are three same
                // pieces then use file and rank)
                if (conflict && !rankConflict && !fileConflict) {
                    o << ChessFileToChar(move.f_from());
                } else if (conflict && !rankConflict) {
                    o << ChessRankToChar(move.r_from());
                } else if (fileConflict && rankConflict) {
                    o << ChessFileToChar(move.f_from()) << ChessRankToChar(move.r_from());
                } else if (rankConflict) {
                    o << ChessFileToChar(move.f_from());
                }

                // determine if it's a capture
                if (!fBoard[move.r_to()][move.f_to()].isEmpty()) {
                    o << 'x';
                }

                // destination square
                o << ChessFileToChar(move.f_to()) << ChessRankToChar(move.r_to());

                // Check or Checkmate
                //??! Removed for speed, and compatibility with Board::genLegalMoveSet()
        }
        return o.str();
    }

    // throws EmptyMove, IllegalMove, InvalidSAN
    ChessMove Board::parseSAN(std::string_view constSAN, MoveList const& legal) const {
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
                    for (size_t i = 0; i < legal.size(); ++i)
                        if (legal[i].type() == ChessMove::whiteCastleQS ||
                            legal[i].type() == ChessMove::blackCastleQS) {
                            assert((isWhiteToMove() && legal[i].type() == ChessMove::whiteCastleQS) ||
                                   (isBlackToMove() && legal[i].type() == ChessMove::blackCastleQS));
                            return legal[i];
                        }
                    throw IllegalMove{constSAN};
                } else {
                    for (size_t i = 0; i < legal.size(); ++i)
                        if (legal[i].type() == ChessMove::whiteCastleKS ||
                            legal[i].type() == ChessMove::blackCastleKS) {
                            assert((isWhiteToMove() && legal[i].type() == ChessMove::whiteCastleKS) ||
                                   (isBlackToMove() && legal[i].type() == ChessMove::blackCastleKS));
                            return legal[i];
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
                for (size_t i = 0; i < legal.size(); ++i) {
                    if (legal[i].f_from() == ChessCharToFile(san[0]) &&
                        (move.isPromo() ? move.type() == legal[i].type() : true) &&
                        fBoard[legal[i].r_from()][legal[i].f_from()] == piece) {
                        return legal[i];
                    }
                }
            } else if (san.length() == 2 && isdigit(san[1])) // f4
            {
                for (size_t i = 0; i < legal.size(); ++i)
                    if (legal[i].f_from() == ChessCharToFile(san[0]) &&
                        legal[i].r_to() == ChessCharToRank(san[1]) &&
                        (move.isPromo() ? move.type() == legal[i].type() : true) &&
                        fBoard[legal[i].r_from()][legal[i].f_from()] == piece) {
                        return legal[i];
                    }

            } else if (san.length() == 2) // ef
            {
                assert(!isdigit(san[1]));
                for (size_t i = 0; i < legal.size(); ++i)
                    if (legal[i].f_from() == ChessCharToFile(san[0]) &&
                        legal[i].f_to() == ChessCharToFile(san[1]) &&
                        (move.isPromo() ? move.type() == legal[i].type() : true) &&
                        fBoard[legal[i].r_from()][legal[i].f_from()] == piece) {
                        return legal[i];
                    }
            } else if (san.length() == 3) // ef4
            {
                for (size_t i = 0; i < legal.size(); ++i)
                    if (legal[i].f_from() == ChessCharToFile(san[0]) &&
                        legal[i].f_to() == ChessCharToFile(san[1]) &&
                        legal[i].r_to() == ChessCharToRank(san[2]) &&
                        (move.isPromo() ? move.type() == legal[i].type() : true) &&
                        fBoard[legal[i].r_from()][legal[i].f_from()] == piece) {
                        return legal[i];
                    }
            } else if (san.length() == 4) // e3f4
            {
                for (size_t i = 0; i < legal.size(); ++i)
                    if (legal[i].f_from() == ChessCharToFile(san[0]) &&
                        legal[i].r_from() == ChessCharToRank(san[1]) &&
                        legal[i].f_to() == ChessCharToFile(san[2]) &&
                        legal[i].r_to() == ChessCharToRank(san[3]) &&
                        (move.isPromo() ? move.type() == legal[i].type() : true) &&
                        fBoard[legal[i].r_from()][legal[i].f_from()] == piece) {
                        return legal[i];
                    }
            }
        } else {
            size_t const sanLength = san.length();
            if (sanLength < 3)
                throw InvalidSAN{constSAN};

            for (size_t i = 0; i < legal.size(); ++i)
                if (legal[i].f_to() == ChessCharToFile(san[sanLength - 2]) &&
                    legal[i].r_to() == ChessCharToRank(san[sanLength - 1]) &&
                    fBoard[legal[i].r_from()][legal[i].f_from()] == piece) {
                    switch (sanLength) {
                        case 3: return legal[i]; // Nf3
                        case 4:
                            if (isdigit(san[1]) && legal[i].r_from() == ChessCharToRank(san[1])) // N1f3
                            {
                                return legal[i];
                            } else if (legal[i].f_from() == ChessCharToFile(san[1])) // Ngf3
                            {
                                return legal[i];
                            }
                            break;
                        case 5:
                            if (legal[i].f_from() == ChessCharToFile(san[1]) &&
                                legal[i].r_from() == ChessCharToFile(san[2])) // Ng1f3
                            {
                                return legal[i];
                            }
                            break;
                        default: throw InvalidSAN{constSAN};
                    }
                }
        }

        throw IllegalMove{constSAN};
    }

    void Board::genLegalMoveSet(MoveList& legal, SANQueue& allSAN) {
        // should combine genLegalMoves and toSAN efficiently
        genLegalMoves(legal, allSAN);
        // disambiguateAllMoves
        disambiguateMoves(allSAN);
    }

    inline void Board::addMove(ChessMove mv, MoveList& moves, SANQueue& allSAN) {
#if !ALLOW_KING_PROMOTION
        if (mv.type() == ChessMove::promoKing)
            return;
#endif
        moves.add(mv);
        allSAN.add({mv, ambiguousSAN(mv)});
    }

    void Board::genLegalMoves(MoveList& legal, SANQueue& allSAN) {
        genPseudoLegalMoves(legal, allSAN);
        int i;

        // castling moves
        if (status_ == GameStatus::notInCheck) {
            if (isWhiteToMove() && ((getCastle() & whiteKS) || (getCastle() & whiteQS))) {
                // search for king on first rank
                for (i = 0; i < files(); ++i) {
                    if (fBoard[0][i] == whiteKing) {
                        if (getCastle() & whiteKS) {
                            bool pieceInWay = false;
                            for (int j = i + 1; j < files() - 1; ++j)
                                if (!fBoard[0][j].isEmpty())
                                    pieceInWay = true;

                            if (!pieceInWay && fBoard[0][files() - 1] == whiteRook && i + 2 < files()) {
                                if (!WillBeInCheck({0, i, 0, i + 1}))
                                    addMove({0, i, 0, i + 2, ChessMove::whiteCastleKS}, legal, allSAN);
                            }
                        }
                        if (getCastle() & whiteQS) {
                            bool pieceInWay = false;
                            for (int j = i - 1; j > 1; --j)
                                if (!fBoard[0][j].isEmpty())
                                    pieceInWay = true;

                            if (!pieceInWay && fBoard[0][0] == whiteRook && i - 2 > 0) {
                                if (!WillBeInCheck({0, i, 0, i - 1})) // cannot castle through
                                                                      // check, or when in check
                                    addMove({0, i, 0, i - 2, ChessMove::whiteCastleQS}, legal, allSAN);
                            }
                        }
                        break; //!!? Break is possible here, unless two kings on rank and one
                               //! Can castle (some form of wild?)
                    }
                }

            } else if (isBlackToMove() && ((getCastle() & blackKS) || (getCastle() & blackQS))) {
                // search for king on back rank
                for (i = 0; i < files(); ++i) {
                    if (fBoard[ranks() - 1][i] == blackKing) {
                        if (getCastle() & blackKS) {
                            bool pieceInWay = false;
                            for (int j = i + 1; j < files() - 1; ++j)
                                if (!fBoard[ranks() - 1][j].isEmpty())
                                    pieceInWay = true;

                            if (!pieceInWay && fBoard[ranks() - 1][files() - 1] == blackRook &&
                                i + 2 < files()) {
                                if (!WillBeInCheck({ranks() - 1, i, ranks() - 1, i + 1})) {
                                    addMove({ranks() - 1, i, ranks() - 1, i + 2, ChessMove::blackCastleKS},
                                            legal, allSAN);
                                }
                            }
                        }
                        if (getCastle() & blackQS) {
                            bool pieceInWay = false;
                            for (int j = i - 1; j > 1; --j)
                                if (!fBoard[ranks() - 1][j].isEmpty())
                                    pieceInWay = true;

                            if (!pieceInWay && fBoard[ranks() - 1][0] == blackRook && i - 2 > 0) {
                                if (!WillBeInCheck({ranks() - 1, i, ranks() - 1, i - 1}))
                                    addMove({ranks() - 1, i, ranks() - 1, i - 2, ChessMove::blackCastleQS},
                                            legal, allSAN);
                            }
                        }
                        break;
                    }
                }
            }
        }

        TIMED(removeIllegalMoves(legal, allSAN));
    }

    void Board::removeIllegalMoves(MoveList& legal, SANQueue& allSAN) {
        bool     isFound = false;
        Occupant target  = toMove() == ToMove::white ? whiteKing : blackKing;

        struct {
            unsigned r, f;
        } targetSquare;

        for (int r = 0; r < ranks() && !isFound; ++r)
            for (int f = 0; f < files() && !isFound; ++f) {
                if (fBoard[r][f] == target) {
                    targetSquare.r = r;
                    targetSquare.f = f;
                    isFound        = true;
                }
            }

        if (!isFound) {
            return; // there is no king on the board
        }

        Board b = *this;
        for (unsigned i = 0; i < legal.size(); b = *this) {
            ChessMove m = legal[i];
            b.applyMove(m);
            b.switchMove();

            if (b.fBoard[m.r_to()][m.f_to()] == whiteKing || b.fBoard[m.r_to()][m.f_to()] == blackKing) {
                if (b.canCaptureSquare(m.r_to(), m.f_to())) {
                    legal.remove(i);
                    for (unsigned j = 0; j < allSAN.size(); ++j)
                        if (allSAN[j].move() == m) {
                            allSAN.remove(j);
                            break;
                        }
                } else
                    ++i;
            } else {
                if (b.canCaptureSquare(targetSquare.r, targetSquare.f)) {
                    legal.remove(i);
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
    std::string Board::ambiguousSAN(ChessMove const& m) {
        std::ostringstream o;

        switch (fBoard[m.r_from()][m.f_from()].contents()) {
            case whitePawn:
            case blackPawn:
                o << ChessFileToChar(m.f_from());

                if (m.f_from() != m.f_to()) {
                    o << 'x' << ChessFileToChar(m.f_to())
                      << ChessRankToChar(m.r_to()); // Capture; use style "exd5"
                } else {
                    o << ChessRankToChar(m.r_to()); // Non-capture; use style "e5"
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
                if (m.r_from() == m.r_to() && m.r_from() == (fBoard[m.r_from()][m.f_from()].isWhite())
                        ? 0
                        : ranks() - 1) {
                    if ((signed)m.f_from() - (signed)m.f_to() < -1) {
                        o << "O-O";
                        break;
                    } else if ((signed)m.f_from() - (signed)m.f_to() > 1) {
                        o << "O-O-O";
                        break;
                    }
                }

                [[fallthrough]];
            default:
                o << (char)toupper(fBoard[m.r_from()][m.f_from()].pieceToChar());

                // determine if it's a capture
                if (!fBoard[m.r_to()][m.f_to()].isEmpty())
                    o << 'x';

                // destination square
                o << ChessFileToChar(m.f_to()) << ChessRankToChar(m.r_to());
        }
        return std::move(o).str();
    }

    void Board::genPseudoLegalMoves(MoveList& moves, SANQueue& allSAN) {
        if (toMove() == ToMove::endOfGame)
            return;

        for (int rf = 0; rf < ranks(); ++rf)
            for (int ff = 0; ff < files(); ++ff) {
                if ((fBoard[rf][ff].isBlack() && toMove() == ToMove::white) ||
                    (fBoard[rf][ff].isWhite() && toMove() == ToMove::black)) {
                    continue;
                }
                switch (fBoard[rf][ff].contents()) {
                    case whitePawn:
                        if (rf < 7 && fBoard[rf + 1][ff].isEmpty()) {
                            if (rf == ranks() - 2) // promotion
                            {
                                addMove({rf, ff, rf + 1, ff, ChessMove::promoQueen}, moves, allSAN);
                                addMove({rf, ff, rf + 1, ff, ChessMove::promoBishop}, moves, allSAN);
                                addMove({rf, ff, rf + 1, ff, ChessMove::promoKnight}, moves, allSAN);
                                addMove({rf, ff, rf + 1, ff, ChessMove::promoRook}, moves, allSAN);
                                addMove({rf, ff, rf + 1, ff, ChessMove::promoKing}, moves, allSAN);
                            } else {
                                addMove({rf, ff, rf + 1, ff, ChessMove::normal}, moves, allSAN);
                            }
                        }
                        if (rf == 1 && fBoard[2][ff].isEmpty() && fBoard[3][ff].isEmpty()) {
                            addMove({rf, ff, 3, ff, ChessMove::normal}, moves, allSAN);
                        }
                        for (int s = -1; s <= 1; s += 2) {
                            if (rf < 7 && ff + s >= 0 && ff + s <= 7 && fBoard[rf + 1][ff + s].isBlack()) {
                                if (rf == ranks() - 2) // promotion
                                {
                                    addMove({rf, ff, rf + 1, ff + s, ChessMove::promoQueen}, moves, allSAN);
                                    addMove({rf, ff, rf + 1, ff + s, ChessMove::promoKnight}, moves, allSAN);
                                    addMove({rf, ff, rf + 1, ff + s, ChessMove::promoBishop}, moves, allSAN);
                                    addMove({rf, ff, rf + 1, ff + s, ChessMove::promoRook}, moves, allSAN);
                                    addMove({rf, ff, rf + 1, ff + s, ChessMove::promoKing}, moves, allSAN);
                                } else {
                                    addMove({rf, ff, rf + 1, ff + s, ChessMove::normal}, moves, allSAN);
                                }
                            }
                            if (rf == ranks() - 4) {
                                if (ff + s >= 0 && ff + s <= 7 &&
                                    (enPassant() == ff + s || enPassant() == Board::allCaptures) &&
                                    fBoard[ranks() - 4][ff + s].contents() == blackPawn &&
                                    fBoard[ranks() - 3][ff + s].isEmpty()) {

                                    addMove({rf, ff, ranks() - 3, ff + s, ChessMove::whiteEnPassant}, moves,
                                            allSAN);
                                }
                            }
                        }
                        break;

                    case blackPawn:
                        if (rf > 0 && fBoard[rf - 1][ff].isEmpty()) {
                            if (rf == 1) // promotion
                            {
                                addMove({rf, ff, rf - 1, ff, ChessMove::promoQueen}, moves, allSAN);
                                addMove({rf, ff, rf - 1, ff, ChessMove::promoKnight}, moves, allSAN);
                                addMove({rf, ff, rf - 1, ff, ChessMove::promoBishop}, moves, allSAN);
                                addMove({rf, ff, rf - 1, ff, ChessMove::promoRook}, moves, allSAN);
                                addMove({rf, ff, rf - 1, ff, ChessMove::promoKing}, moves, allSAN);
                            } else {
                                addMove({rf, ff, rf - 1, ff, ChessMove::normal}, moves, allSAN);
                            }
                        }
                        if (rf == ranks() - 2 && fBoard[ranks() - 3][ff].isEmpty() &&
                            fBoard[ranks() - 4][ff].isEmpty()) {
                            addMove({rf, ff, ranks() - 4, ff, ChessMove::normal}, moves, allSAN);
                        }
                        for (int s = -1; s <= 1; s += 2) {
                            if (rf > 0 && ff + s >= 0 && ff + s <= 7 && fBoard[rf - 1][ff + s].isWhite()) {
                                if (rf == 1) // promotion
                                {
                                    addMove({rf, ff, rf - 1, ff + s, ChessMove::promoQueen}, moves, allSAN);
                                    addMove({rf, ff, rf - 1, ff + s, ChessMove::promoKnight}, moves, allSAN);
                                    addMove({rf, ff, rf - 1, ff + s, ChessMove::promoBishop}, moves, allSAN);
                                    addMove({rf, ff, rf - 1, ff + s, ChessMove::promoRook}, moves, allSAN);
                                    addMove({rf, ff, rf - 1, ff + s, ChessMove::promoKing}, moves, allSAN);
                                } else {
                                    addMove({rf, ff, rf - 1, ff + s, ChessMove::normal}, moves, allSAN);
                                }
                            }
                            if (rf == 3) {
                                if (ff + s >= 0 && ff + s <= 7 &&
                                    (enPassant() == ff + s || enPassant() == Board::allCaptures) &&
                                    fBoard[3][ff + s].contents() == whitePawn &&
                                    fBoard[2][ff + s].isEmpty()) {
                                    addMove({rf, ff, 2, ff + s, ChessMove::blackEnPassant}, moves, allSAN);
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

                                    if (IsSameColor(fBoard[rf][ff], fBoard[rt][ft]))
                                        continue;

                                    addMove({rf, ff, rt, ft, ChessMove::normal}, moves, allSAN);
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
                                    if (IsSameColor(fBoard[rf][ff], fBoard[rt][ft]))
                                        break;

                                    addMove({rf, ff, rt, ft, ChessMove::normal}, moves, allSAN);

                                    if (!fBoard[rt][ft].isEmpty())
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
                                    if (IsSameColor(fBoard[rf][ff], fBoard[rt][ft]))
                                        break;

                                    addMove({rf, ff, rt, ft, ChessMove::normal}, moves, allSAN);

                                    if (!fBoard[rt][ft].isEmpty())
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
                                    if (IsSameColor(fBoard[rf][ff], fBoard[rt][ft]))
                                        break;

                                    addMove({rf, ff, rt, ft, ChessMove::normal}, moves, allSAN);

                                    if (!fBoard[rt][ft].isEmpty())
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
                                if (IsSameColor(fBoard[rf][ff], fBoard[rt][ft]))
                                    continue;

                                addMove({rf, ff, rt, ft, ChessMove::normal}, moves, allSAN);
                            }
                        break;

                    case noPiece: break;

                    default: assert(0); // not Reached
                }
            }
    }

    // all moves that capture the square, square can be same color as person to move
    bool Board::canCaptureSquare(int targetRank, int targetFile) const {
        assert(targetRank < ranks());
        assert(targetFile < files());

        if (toMove() == ToMove::endOfGame)
            return false;

        for (int rf = 0; rf < ranks(); ++rf)
            for (int ff = 0; ff < files(); ++ff) {
                if ((fBoard[rf][ff].isBlack() && toMove() == ToMove::white) ||
                    (fBoard[rf][ff].isWhite() && toMove() == ToMove::black)) {
                    continue;
                }
                switch (fBoard[rf][ff].contents()) {

                    case whitePawn:

                        if (rf + 1 == targetRank && (ff - 1 == targetFile || ff + 1 == targetFile))
                            return true;
                        break;

                    case blackPawn:

                        if (rf - 1 == targetRank && (ff - 1 == targetFile || ff + 1 == targetFile))
                            return true;
                        break;

                    case whiteKnight:
                    case blackKnight:
                        for (int i = -1; i <= 1; i += 2)
                            for (int j = -1; j <= 1; j += 2)
                                for (int s = 1; s <= 2; s++) {
                                    int rt = rf + i * s;
                                    int ft = ff + j * (3 - s);
                                    if (rt != targetRank || ft != targetFile)
                                        continue;

                                    if (rt == targetRank && ft == targetFile)
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

                                    if (rt == targetRank && ft == targetFile)
                                        return true;

                                    if (!fBoard[rt][ft].isEmpty())
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

                                    if (rt == targetRank && ft == targetFile)
                                        return true;

                                    if (!fBoard[rt][ft].isEmpty())
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

                                    if (rt == targetRank && ft == targetFile)
                                        return true;

                                    if (!fBoard[rt][ft].isEmpty())
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
                                if (rt != targetRank || ft != targetFile)
                                    continue;

                                if (rt == targetRank && ft == targetFile)
                                    return true;
                            }
                        break;

                    case noPiece: break;

                    default: assert(0); // not Reached
                }
            }

        return false;
    }

    void Board::disambiguate(ChessMove const& move, std::string& san, SANQueue& all) {
        if (san.length()) {
            bool conflict = false, rankConflict = false, fileConflict = false;
            for (int i = 0; i < ssize(all); ++i) {
                auto& altMv = all[i].move();
                if (altMv.r_to() == move.r_to() && altMv.f_to() == move.f_to() && // the same 'to' square
                    altMv != move &&                                              // not the same move
                    fBoard[move.r_from()][move.f_from()] ==
                        fBoard[altMv.r_from()][altMv.f_from()]) // same type of piece
                {
                    conflict = true;
                    if (move.r_from() == altMv.r_from())
                        rankConflict = true;
                    else if (move.f_from() == altMv.f_from())
                        fileConflict = true;
                    if (rankConflict && fileConflict)
                        break; // if you have two conflicts, no need to continue
                }
            }
            // resolve if the piece is on same file of rank (if there are three same
            // pieces then use file and rank)
            if (conflict && !rankConflict && !fileConflict) {
                san.insert(1, 1, ChessFileToChar(move.f_from()));
            } else if (conflict && !rankConflict) {
                san.insert(1, 1, ChessRankToChar(move.r_from()));
            } else if (fileConflict && rankConflict) {
                san.insert(1, {ChessFileToChar(move.f_from()), ChessRankToChar(move.r_from())});
            } else if (rankConflict) {
                san.insert(1, 1, ChessFileToChar(move.f_from()));
            }
        }
    }

    void Board::disambiguateMoves(SANQueue& allSAN) {
        bool ambiguity = false;
        int  i;
        for (i = 0; i < ssize(allSAN) - 1; ++i) {
            if (allSAN[i] == allSAN[i + 1]) {
                disambiguate(allSAN[i].move(), allSAN[i].SAN(), allSAN);
                ambiguity = true;
            } else if (ambiguity) {
                disambiguate(allSAN[i].move(), allSAN[i].SAN(), allSAN);
                ambiguity = false;
            }
        }
        if (ambiguity) // for the last element
        {
            disambiguate(allSAN[i].move(), allSAN[i].SAN(), allSAN);
        }
    }

    //-----------------------------------------------------------------------------
    // GenLegalMoves -- generates all possible legal moves adding them to the list
    //
    // pseudoLegal: not castling, and not worrying about being left in check
    //
    void Board::GenPseudoLegalMoves(MoveList& moves) const {
        if (toMove() == Board::ToMove::endOfGame)
            return;

        for (int rf = 0; rf < ranks(); ++rf)
            for (int ff = 0; ff < files(); ++ff) {
                if ((fBoard[rf][ff].isBlack() && toMove() == ToMove::white) ||
                    (fBoard[rf][ff].isWhite() && toMove() == ToMove::black)) {
                    continue;
                }
                switch (fBoard[rf][ff].contents()) {

                    case whitePawn:
                        if (rf < 7 && fBoard[rf + 1][ff].isEmpty()) {
                            if (rf == ranks() - 2) // promotion
                            {
                                moves.add(ChessMove(rf, ff, rf + 1, ff, ChessMove::promoQueen));
                                moves.add(ChessMove(rf, ff, rf + 1, ff, ChessMove::promoKnight));
                                moves.add(ChessMove(rf, ff, rf + 1, ff, ChessMove::promoBishop));
                                moves.add(ChessMove(rf, ff, rf + 1, ff, ChessMove::promoRook));
                                moves.add(ChessMove(rf, ff, rf + 1, ff, ChessMove::promoKing));
                            } else {
                                moves.add(ChessMove(rf, ff, rf + 1, ff));
                            }
                        }
                        if (rf == 1 && fBoard[2][ff].isEmpty() && fBoard[3][ff].isEmpty()) {
                            moves.add(ChessMove(rf, ff, 3, ff));
                        }
                        for (int s = -1; s <= 1; s += 2) {
                            if (rf < 7 && ff + s >= 0 && ff + s <= 7 && fBoard[rf + 1][ff + s].isBlack()) {
                                if (rf == ranks() - 2) // promotion
                                {
                                    moves.add(ChessMove(rf, ff, rf + 1, ff + s, ChessMove::promoQueen));
                                    moves.add(ChessMove(rf, ff, rf + 1, ff + s, ChessMove::promoKnight));
                                    moves.add(ChessMove(rf, ff, rf + 1, ff + s, ChessMove::promoBishop));
                                    moves.add(ChessMove(rf, ff, rf + 1, ff + s, ChessMove::promoRook));
                                    moves.add(ChessMove(rf, ff, rf + 1, ff + s, ChessMove::promoKing));
                                    // wild variants
                                } else {
                                    moves.add(ChessMove(rf, ff, rf + 1, ff + s));
                                }
                            }
                            if (rf == ranks() - 4) {
                                if (ff + s >= 0 && ff + s <= 7 &&
                                    (enPassant() == ff + s || enPassant() == Board::allCaptures) &&
                                    fBoard[ranks() - 4][ff + s].contents() == blackPawn &&
                                    fBoard[ranks() - 3][ff + s].isEmpty()) {
                                    moves.add(
                                        ChessMove(rf, ff, ranks() - 3, ff + s, ChessMove::whiteEnPassant));
                                }
                            }
                        }
                        break;

                    case blackPawn:
                        if (rf > 0 && fBoard[rf - 1][ff].isEmpty()) {
                            if (rf == 1) // promotion
                            {
                                moves.add(ChessMove(rf, ff, rf - 1, ff, ChessMove::promoQueen));
                                moves.add(ChessMove(rf, ff, rf - 1, ff, ChessMove::promoKnight));
                                moves.add(ChessMove(rf, ff, rf - 1, ff, ChessMove::promoBishop));
                                moves.add(ChessMove(rf, ff, rf - 1, ff, ChessMove::promoRook));
                                moves.add(ChessMove(rf, ff, rf - 1, ff, ChessMove::promoKing));
                            } else {
                                moves.add(ChessMove(rf, ff, rf - 1, ff));
                            }
                        }
                        if (rf == ranks() - 2 && fBoard[ranks() - 3][ff].isEmpty() &&
                            fBoard[ranks() - 4][ff].isEmpty()) {
                            moves.add(ChessMove(rf, ff, ranks() - 4, ff));
                        }
                        for (int s = -1; s <= 1; s += 2) {
                            if (rf > 0 && ff + s >= 0 && ff + s <= 7 && fBoard[rf - 1][ff + s].isWhite()) {
                                if (rf == 1) // promotion
                                {
                                    moves.add(ChessMove(rf, ff, rf - 1, ff + s, ChessMove::promoQueen));
                                    moves.add(ChessMove(rf, ff, rf - 1, ff + s, ChessMove::promoKnight));
                                    moves.add(ChessMove(rf, ff, rf - 1, ff + s, ChessMove::promoBishop));
                                    moves.add(ChessMove(rf, ff, rf - 1, ff + s, ChessMove::promoRook));
                                    moves.add(ChessMove(rf, ff, rf - 1, ff + s, ChessMove::promoKing));
                                } else {
                                    moves.add(ChessMove(rf, ff, rf - 1, ff + s));
                                }
                            }
                            if (rf == 3) {
                                if (ff + s >= 0 && ff + s <= 7 &&
                                    (enPassant() == ff + s || enPassant() == Board::allCaptures) &&
                                    fBoard[3][ff + s].contents() == whitePawn &&
                                    fBoard[2][ff + s].isEmpty()) {
                                    moves.add(ChessMove(rf, ff, 2, ff + s, ChessMove::blackEnPassant));
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

                                    if (IsSameColor(fBoard[rf][ff], fBoard[rt][ft]))
                                        continue;

                                    moves.add(ChessMove(rf, ff, rt, ft));
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
                                    if (IsSameColor(fBoard[rf][ff], fBoard[rt][ft]))
                                        break;

                                    moves.add(ChessMove(rf, ff, rt, ft));

                                    if (!fBoard[rt][ft].isEmpty())
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
                                    if (IsSameColor(fBoard[rf][ff], fBoard[rt][ft]))
                                        break;

                                    moves.add(ChessMove(rf, ff, rt, ft));

                                    if (!fBoard[rt][ft].isEmpty())
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
                                    if (IsSameColor(fBoard[rf][ff], fBoard[rt][ft]))
                                        break;

                                    moves.add(ChessMove(rf, ff, rt, ft));

                                    if (!fBoard[rt][ft].isEmpty())
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
                                if (IsSameColor(fBoard[rf][ff], fBoard[rt][ft]))
                                    continue;

                                moves.add(ChessMove(rf, ff, rt, ft));
                            }
                        break;

                    case noPiece: break;

                    default: assert(0); // not Reached
                }
            }
    }

    // returns if the person to move is in check
    bool Board::IsInCheck() const {
        // scan a1,a2,...,h8 only test first king found
        bool     isFound = false;
        Occupant target  = toMove() == ToMove::white ? whiteKing : blackKing;

        struct {
            unsigned r, f;
        } targetSquare;

        for (int r = 0; r < ranks() && !isFound; ++r)
            for (int f = 0; f < files() && !isFound; ++f) {
                if (fBoard[r][f].contents() == target) {
                    targetSquare.r = r;
                    targetSquare.f = f;
                    isFound        = true;
                }
            }

        if (!isFound) {
            return false; // there is no king on the board
        }

        Board b = *this;
        b.switchMove();
        return b.canCaptureSquare(targetSquare.r, targetSquare.f);
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

    GameStatus Board::CheckStatus(MoveList& legal) const {
        if (legal.size())
            return IsInCheck() ? GameStatus::inCheck : GameStatus::notInCheck;
        else
            return IsInCheck() ? GameStatus::inCheckmate : GameStatus::inStalemate;
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
        pgn2pgc::Chess::SANQueue set;
        pgn2pgc::Chess::MoveList moves;

        b.display();
        b.genLegalMoveSet(moves, set);
        std::cout << "\n";

        for (auto& mv : set)
            std::cout << mv.SAN() << "\t";

        std::cout << "\nEnter Move (or end): ";
        std::cin.getline(buf, sizeof(buf) / sizeof(buf[0]));
        std::cout << "buffer: '" << buf << "' ";

        if (!strcmp(buf, "end"))
            break;

        auto        move = b.parseSAN(buf, moves); // TODO handle MoveError
        std::string SAN  = b.toSAN(move, moves);

        b.processMove(move);

        std::cout << "\n Moved: '" << SAN.c_str() << "' ";
    }
}

#endif
