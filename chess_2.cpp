// vim: spell :
#include <cstddef>
#include <cstring>
#include <iostream>
#include <ranges>

#include "chess_2.h"
#include "stpwatch.h"
#include "strparse.h"

extern StopWatch gTimer[10];

static constexpr inline char ChessFileToChar(unsigned file) { return file + 'a'; }
static constexpr inline char ChessRankToChar(unsigned rank) { return rank + '1'; }
static constexpr inline int  ChessCharToFile(char file) { return file - 'a'; }
static constexpr inline int  ChessCharToRank(char rank) { return rank - '1'; }

// in case you already know all of the chessMoves to save processing time (very
constexpr inline char ChessSquare::pieceToChar() {
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
        case empty: return ' ';
        default: assert(0); return 'x'; // not Reached
    }
}

//-----------------------------------------------------------------------------
static constexpr inline ChessSquare LetterToSquare(char SAN_FEN_Letter) {
    switch (SAN_FEN_Letter) {
        case 'P': return ChessSquare::whitePawn;
        case 'p': return ChessSquare::blackPawn;
        case 'R': return ChessSquare::whiteRook;
        case 'r': return ChessSquare::blackRook;
        case 'N': return ChessSquare::whiteKnight;
        case 'n': return ChessSquare::blackKnight;
        case 'B': return ChessSquare::whiteBishop;
        case 'b': return ChessSquare::blackBishop;
        case 'Q': return ChessSquare::whiteQueen;
        case 'q': return ChessSquare::blackQueen;
        case 'K': return ChessSquare::whiteKing;
        case 'k': return ChessSquare::blackKing;

        default: return ChessSquare::empty;
    }
}

// FEN notation of initial position
// position : to move : castling : (50*2) - plies till draw : current move
// number "RNBQKBNR/PPPPPPPP/8/8/8/8/pppppppp/rnbqkbnr w KQkq - 0 1"

Board::Board()
    : toMove_(endOfGame)
    , status_(notInCheck)
    , castle_(noCastle)
    , enPassant_(allCaptures)
    , pliesSince_(0)
    , moveNumber_(1) {
    processFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

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
                                fBoard.at(i).at(j++) = ChessSquare(ChessSquare::empty);
                        else
                            fBoard.at(i).at(j++) = LetterToSquare(ch);
                    }
                    --i;
                }

                break;
            case 2: // to move
                if (token == "w" || token == "W")
                    toMove_ = white;
                else if (token == "b" || token == "B")
                    toMove_ = black;
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
    int i;
    for (i = ranks() - 1; i >= 0; --i) {
        std::cout << "\n" << i + 1 << " ";
        for (int j = 0; j < files(); ++j) {
            std::cout << fBoard[i][j].pieceToChar();
        }
    }
    std::cout << "\n  abcdefgh \n";
    std::cout << "\nTo Move:\t " << toMove_;
    std::cout << "\nCastle:\t " << castle_;
    std::cout << "\nStatus:\t " << status_;
    std::cout << "\nEnPassant:\t " << enPassant_;
    std::cout << "\nMove: " << moveNumber_;
    std::cout << "\nPlies Since:\t " << pliesSince_;
}

// true on success
//??!! No checking is done?
bool Board::move(ChessMove const& move) {
    // can't move an empty square or capture your own piece
    // illegal en-passant
    // general legality -- check etc.. ?? How much checking should be done here?

    if (fBoard[move.rf()][move.ff()].isEmpty() ||
        IsSameColor(fBoard[move.rf()][move.ff()], fBoard[move.rt()][move.ft()]) ||
        (move.ff() == move.ft() && move.rf() == move.rt()) ||
        (move.isEnPassant() && enPassant_ != allCaptures && enPassant_ != move.ft()) ||
        toMove_ == endOfGame) {
        return false;
    }

    fBoard[move.rt()][move.ft()] = fBoard[move.rf()][move.ff()];
    fBoard[move.rf()][move.ff()] = ChessSquare(ChessSquare::empty);

    switch (move.type()) {
        case ChessMove::whiteEnPassant:
            fBoard[move.rt() - 1][move.ft()] = ChessSquare(ChessSquare::empty);
            break;
        case ChessMove::blackEnPassant:
            fBoard[move.rt() + 1][move.ft()] = ChessSquare(ChessSquare::empty);
            break;
        case ChessMove::whiteCastleKS:
            fBoard[0][gFiles - 1]    = ChessSquare(ChessSquare::empty);
            fBoard[0][move.ft() - 1] = ChessSquare(ChessSquare::whiteRook);
            break;
        case ChessMove::whiteCastleQS:
            fBoard[0][0]             = ChessSquare(ChessSquare::empty);
            fBoard[0][move.ft() + 1] = ChessSquare(ChessSquare::whiteRook);
            break;
        case ChessMove::blackCastleKS:
            fBoard[gRanks - 1][gFiles - 1]    = ChessSquare(ChessSquare::empty);
            fBoard[gRanks - 1][move.ft() - 1] = ChessSquare(ChessSquare::blackRook);
            break;
        case ChessMove::blackCastleQS:
            fBoard[gRanks - 1][0]             = ChessSquare(ChessSquare::empty);
            fBoard[gRanks - 1][move.ft() + 1] = ChessSquare(ChessSquare::blackRook);
            break;
        case ChessMove::promoQueen:
            assert(move.rt() == gRanks - 1 || move.rt() == 0);
            fBoard[move.rt()][move.ft()] = fBoard[move.rt()][move.ft()].isWhite()
                ? ChessSquare(ChessSquare::whiteQueen)
                : ChessSquare(ChessSquare::blackQueen);
            break;
        case ChessMove::promoKnight:
            fBoard[move.rt()][move.ft()] = fBoard[move.rt()][move.ft()].isWhite()
                ? ChessSquare(ChessSquare::whiteKnight)
                : ChessSquare(ChessSquare::blackKnight);
            break;
        case ChessMove::promoRook:
            fBoard[move.rt()][move.ft()] = fBoard[move.rt()][move.ft()].isWhite()
                ? ChessSquare(ChessSquare::whiteRook)
                : ChessSquare(ChessSquare::blackRook);
            break;
        case ChessMove::promoBishop:
            fBoard[move.rt()][move.ft()] = fBoard[move.rt()][move.ft()].isWhite()
                ? ChessSquare(ChessSquare::whiteBishop)
                : ChessSquare(ChessSquare::blackBishop);
            break;
#if ALLOW_KING_PROMOTION
        case ChessMove::promoKing:
            fBoard[move.rt()][move.ft()] = fBoard[move.rt()][move.ft()].isWhite()
                ? ChessSquare(ChessSquare::whiteKing)
                : ChessSquare(ChessSquare::blackKing);
            break;
#endif
        default: break; // do Nothing
    }

    return true;
}

inline void Board::genLegalMoves(MoveList& moves) { GenLegalMoves(*this, moves); }

bool Board::processMove(ChessMove const& m) {
    MoveList allMoves;
    genLegalMoves(allMoves);
    return processMove(m, allMoves);
}

bool Board::processMove(ChessMove const& m, MoveList& allMoves) {
    int      enPassant = fBoard[m.rf()][m.ff()].isPawn() && abs(m.rt() - m.rf()) > 1 ? m.ft() : noCaptures;
    unsigned plysSince =
        !fBoard[m.rf()][m.ff()].isPawn() && fBoard[m.rt()][m.ft()].isEmpty() ? pliesSince_ + 1 : 0;

    // eliminate castling move's
    int castle = castle_;
    switch (fBoard[m.rf()][m.ff()].contents()) {
        case ChessSquare::whiteKing: castle &= ~(whiteKS | whiteQS); break;
        case ChessSquare::blackKing: castle &= ~(blackKS | blackQS); break;
        case ChessSquare::whiteRook:
            if (m.ff() < files() / 2)
                castle &= ~whiteQS;
            else
                castle &= ~whiteKS;
            break;
        case ChessSquare::blackRook:
            if (m.ff() < files() / 2)
                castle &= ~blackQS;
            else
                castle &= ~blackKS;
            break;
        default: break; // do Nothing
    }

    // isLegal()
    if (toMove_ != endOfGame && move(m)) {
        status_     = CheckStatus(*this, allMoves);
        enPassant_  = enPassant;
        pliesSince_ = plysSince;
        castle_     = castle;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wenum-compare"      // FIXME
        if (status_ == noStatus || status_ == check) // otherwise end of game
#pragma GCC diagnostic pop
        {
            switchMove();
            if (toMove_ == white) {
                ++moveNumber_;
            }
        } else {
            toMove_ = endOfGame;
        }
        return true;
    } else {
        return false;
    }
}

// inline
void Board::moveToAlgebraic(std::string& SAN, ChessMove const& move, MoveList const& allMoves) {
    MoveToAlgebraic(move, *this, allMoves, SAN);
}

//??! Change all these functions to const
// inline
bool Board::algebraicToMove(ChessMove& move, std::string_view SAN, MoveList const& allMoves) {
    return AlgebraicToMove(SAN, *this, allMoves, move);
}

void Board::genLegalMoveSet(MoveList& allMoves, SANQueue& allSAN) {
    // should combine genLegalMoves and moveToAlgebraic efficiently
    genLegalMoves(allMoves, allSAN);
    // disambiguateAllMoves
    disambiguateMoves(allSAN);
}

inline void Board::addMove(ChessMove mv, MoveList& moves, SANQueue& allSAN) {
#if !ALLOW_KING_PROMOTION
    if (mv.type() == ChessMove::promoKing)
        return;
#endif
    std::string sanValue;
    moves.add(mv);
    moveToAlgebraicAmbiguity(sanValue, mv);
    allSAN.add(ChessMoveSAN(mv, sanValue));
}

void Board::genLegalMoves(MoveList& allMoves, SANQueue& allSAN) {
    genPseudoLegalMoves(allMoves, allSAN);
    int i;

    // castling moves
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wenum-compare" // FIXME
    if (status_ == noStatus) {
#pragma GCC diagnostic pop
        if (isWhiteToMove() && ((getCastle() & whiteKS) || (getCastle() & whiteQS))) {
            // search for king on first rank
            for (i = 0; i < files(); ++i) {
                if (fBoard[0][i] == ChessSquare::whiteKing) {
                    if (getCastle() & whiteKS) {
                        bool pieceInWay = false;
                        for (int j = i + 1; j < files() - 1; ++j)
                            if (!fBoard[0][j].isEmpty())
                                pieceInWay = true;

                        if (!pieceInWay && fBoard[0][files() - 1] == ChessSquare::whiteRook &&
                            i + 2 < files()) {
                            if (!WillBeInCheck(*this, ChessMove(0, i, 0, i + 1)))
                                addMove({0, i, 0, i + 2, ChessMove::whiteCastleKS}, allMoves, allSAN);
                        }
                    }
                    if (getCastle() & whiteQS) {
                        bool pieceInWay = false;
                        for (int j = i - 1; j > 1; --j)
                            if (!fBoard[0][j].isEmpty())
                                pieceInWay = true;

                        if (!pieceInWay && fBoard[0][0] == ChessSquare::whiteRook && i - 2 > 0) {
                            if (!WillBeInCheck(*this, ChessMove(0, i, 0, i - 1))) // cannot castle through
                                                                                  // check, or when in check
                                addMove({0, i, 0, i - 2, ChessMove::whiteCastleQS}, allMoves, allSAN);
                        }
                    }
                    break; //!!? Break is possible here, unless two kings on rank and one
                           //! Can castle (some form of wild?)
                }
            }

        } else if (isBlackToMove() && ((getCastle() & blackKS) || (getCastle() & blackQS))) {
            // search for king on back rank
            for (i = 0; i < files(); ++i) {
                if (fBoard[ranks() - 1][i] == ChessSquare::blackKing) {
                    if (getCastle() & blackKS) {
                        bool pieceInWay = false;
                        for (int j = i + 1; j < files() - 1; ++j)
                            if (!fBoard[ranks() - 1][j].isEmpty())
                                pieceInWay = true;

                        if (!pieceInWay && fBoard[ranks() - 1][files() - 1] == ChessSquare::blackRook &&
                            i + 2 < files()) {
                            if (!WillBeInCheck(*this, ChessMove(ranks() - 1, i, ranks() - 1, i + 1))) {
                                addMove({ranks() - 1, i, ranks() - 1, i + 2, ChessMove::blackCastleKS},
                                        allMoves, allSAN);
                            }
                        }
                    }
                    if (getCastle() & blackQS) {
                        bool pieceInWay = false;
                        for (int j = i - 1; j > 1; --j)
                            if (!fBoard[ranks() - 1][j].isEmpty())
                                pieceInWay = true;

                        if (!pieceInWay && fBoard[ranks() - 1][0] == ChessSquare::blackRook && i - 2 > 0) {
                            if (!WillBeInCheck(*this, ChessMove(ranks() - 1, i, ranks() - 1, i - 1)))
                                addMove({ranks() - 1, i, ranks() - 1, i - 2, ChessMove::blackCastleQS},
                                        allMoves, allSAN);
                        }
                    }
                    break;
                }
            }
        }
    }

    gTimer[9].start(); // 55% (11s)
    removeIllegalMoves(allMoves, allSAN);
    gTimer[9].stop();
}

void Board::removeIllegalMoves(MoveList& allMoves, SANQueue& allSAN) {
    bool                  isFound = false;
    ChessSquare::Occupant target  = toMove() == white ? ChessSquare::whiteKing : ChessSquare::blackKing;

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

    unsigned i = 0;
    while (i < allMoves.size()) {
        Board     b = *this;
        ChessMove m = allMoves[i];
        b.move(m);
        b.switchMove();

        if (b.fBoard[m.rt()][m.ft()] == ChessSquare::whiteKing ||
            b.fBoard[m.rt()][m.ft()] == ChessSquare::blackKing) {
            if (b.canCaptureSquare(m.rt(), m.ft())) {
                allMoves.remove(i);
                for (unsigned j = 0; j < allSAN.size(); ++j)
                    if (allSAN[j].move() == m) {
                        allSAN.remove(j);
                        break;
                    }
            } else
                ++i;
        } else {
            if (b.canCaptureSquare(targetSquare.r, targetSquare.f)) {
                allMoves.remove(i);
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
void Board::moveToAlgebraicAmbiguity(std::string& out, ChessMove const& m) {
    std::ostringstream o;

    switch (fBoard[m.rf()][m.ff()].contents()) {
        case ChessSquare::whitePawn:
        case ChessSquare::blackPawn:
            o << ChessFileToChar(m.ff());

            if (m.ff() != m.ft()) {
                o << 'x' << ChessFileToChar(m.ft()) << ChessRankToChar(m.rt()); // Capture; use style "exd5"
            } else {
                o << ChessRankToChar(m.rt()); // Non-capture; use style "e5"
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

        case ChessSquare::whiteKing:
        case ChessSquare::blackKing:
            // castling
            if (m.rf() == m.rt() && m.rf() == (fBoard[m.rf()][m.ff()].isWhite()) ? 0 : ranks() - 1) {
                if ((signed)m.ff() - (signed)m.ft() < -1) {
                    o << "O-O";
                    break;
                } else if ((signed)m.ff() - (signed)m.ft() > 1) {
                    o << "O-O-O";
                    break;
                }
            }

            [[fallthrough]];
        default:
            o << (char)toupper(fBoard[m.rf()][m.ff()].pieceToChar());

            // determine if it's a capture
            if (!fBoard[m.rt()][m.ft()].isEmpty())
                o << 'x';

            // destination square
            o << ChessFileToChar(m.ft()) << ChessRankToChar(m.rt());
    }
    out += o.str();
}

void Board::genPseudoLegalMoves(MoveList& moves, SANQueue& allSAN) {
    int i, j, d, s, fs, rs, rt, ft;
    if (toMove() == endOfGame)
        return;

    for (int rf = 0; rf < ranks(); ++rf)
        for (int ff = 0; ff < files(); ++ff) {
            if ((fBoard[rf][ff].isBlack() && toMove() == white) ||
                (fBoard[rf][ff].isWhite() && toMove() == black)) {
                continue;
            }
            switch (fBoard[rf][ff].contents()) {

                case ChessSquare::whitePawn:
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
                    for (s = -1; s <= 1; s += 2) {
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
                                fBoard[ranks() - 4][ff + s].contents() == ChessSquare::blackPawn &&
                                fBoard[ranks() - 3][ff + s].isEmpty()) {

                                addMove({rf, ff, ranks() - 3, ff + s, ChessMove::whiteEnPassant}, moves,
                                        allSAN);
                            }
                        }
                    }
                    break;

                case ChessSquare::blackPawn:
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
                    for (s = -1; s <= 1; s += 2) {
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
                                fBoard[3][ff + s].contents() == ChessSquare::whitePawn &&
                                fBoard[2][ff + s].isEmpty()) {
                                addMove({rf, ff, 2, ff + s, ChessMove::blackEnPassant}, moves, allSAN);
                            }
                        }
                    }
                    break;

                case ChessSquare::whiteKnight:
                case ChessSquare::blackKnight:
                    for (i = -1; i <= 1; i += 2)
                        for (j = -1; j <= 1; j += 2)
                            for (s = 1; s <= 2; s++) {
                                rt = rf + i * s;
                                ft = ff + j * (3 - s);
                                if (rt < 0 || rt > ranks() - 1 || ft < 0 || ft > files() - 1)
                                    continue;

                                if (IsSameColor(fBoard[rf][ff], fBoard[rt][ft]))
                                    continue;

                                addMove({rf, ff, rt, ft, ChessMove::normal}, moves, allSAN);
                            }
                    break;

                case ChessSquare::whiteBishop:
                case ChessSquare::blackBishop:
                    for (rs = -1; rs <= 1; rs += 2)
                        for (fs = -1; fs <= 1; fs += 2)
                            for (i = 1;; i++) {
                                rt = rf + (i * rs);
                                ft = ff + (i * fs);
                                if (rt < 0 || rt > ranks() - 1 || ft < 0 || ft > files() - 1)
                                    break;
                                if (IsSameColor(fBoard[rf][ff], fBoard[rt][ft]))
                                    break;

                                addMove({rf, ff, rt, ft, ChessMove::normal}, moves, allSAN);

                                if (!fBoard[rt][ft].isEmpty())
                                    break;
                            }
                    break;

                case ChessSquare::whiteRook:
                case ChessSquare::blackRook:
                    for (d = 0; d <= 1; d++)
                        for (s = -1; s <= 1; s += 2)
                            for (i = 1;; i++) {
                                rt = rf + (i * s) * d;
                                ft = ff + (i * s) * (1 - d);
                                if (rt < 0 || rt > ranks() - 1 || ft < 0 || ft > files() - 1)
                                    break;
                                if (IsSameColor(fBoard[rf][ff], fBoard[rt][ft]))
                                    break;

                                addMove({rf, ff, rt, ft, ChessMove::normal}, moves, allSAN);

                                if (!fBoard[rt][ft].isEmpty())
                                    break;
                            }
                    break;

                case ChessSquare::whiteQueen:
                case ChessSquare::blackQueen:
                    for (rs = -1; rs <= 1; rs++)
                        for (fs = -1; fs <= 1; fs++) {
                            if (rs == 0 && fs == 0)
                                continue;
                            for (i = 1;; i++) {
                                rt = rf + (i * rs);
                                ft = ff + (i * fs);

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

                case ChessSquare::whiteKing:
                case ChessSquare::blackKing:

                    for (i = -1; i <= 1; i++)
                        for (j = -1; j <= 1; j++) {
                            if (i == 0 && j == 0)
                                continue;
                            rt = rf + i;
                            ft = ff + j;
                            if (rt < 0 || rt > 7 || ft < 0 || ft > 7)
                                continue;
                            if (IsSameColor(fBoard[rf][ff], fBoard[rt][ft]))
                                continue;

                            addMove({rf, ff, rt, ft, ChessMove::normal}, moves, allSAN);
                        }
                    break;

                case ChessSquare::empty: break;

                default: assert(0); // not Reached
            }
        }
}

// all moves that capture the square, square can be same color as person to move
bool Board::canCaptureSquare(int targetRank, int targetFile) {
    assert(targetRank < ranks());
    assert(targetFile < files());

    int i, j, d, s, fs, rs, rt, ft;

    if (toMove() == endOfGame)
        return false;

    for (int rf = 0; rf < ranks(); ++rf)
        for (int ff = 0; ff < files(); ++ff) {
            if ((fBoard[rf][ff].isBlack() && toMove() == white) ||
                (fBoard[rf][ff].isWhite() && toMove() == black)) {
                continue;
            }
            switch (fBoard[rf][ff].contents()) {

                case ChessSquare::whitePawn:

                    if (rf + 1 == targetRank && (ff - 1 == targetFile || ff + 1 == targetFile))
                        return true;
                    break;

                case ChessSquare::blackPawn:

                    if (rf - 1 == targetRank && (ff - 1 == targetFile || ff + 1 == targetFile))
                        return true;
                    break;

                case ChessSquare::whiteKnight:
                case ChessSquare::blackKnight:
                    for (i = -1; i <= 1; i += 2)
                        for (j = -1; j <= 1; j += 2)
                            for (s = 1; s <= 2; s++) {
                                rt = rf + i * s;
                                ft = ff + j * (3 - s);
                                if (rt != targetRank || ft != targetFile)
                                    continue;

                                if (rt == targetRank && ft == targetFile)
                                    return true;
                            }
                    break;

                case ChessSquare::whiteBishop:
                case ChessSquare::blackBishop:
                    for (rs = -1; rs <= 1; rs += 2)
                        for (fs = -1; fs <= 1; fs += 2)
                            for (i = 1;; i++) {
                                rt = rf + (i * rs);
                                ft = ff + (i * fs);
                                if (rt < 0 || rt > ranks() - 1 || ft < 0 || ft > files() - 1)
                                    break;

                                if (rt == targetRank && ft == targetFile)
                                    return true;

                                if (!fBoard[rt][ft].isEmpty())
                                    break;
                            }
                    break;

                case ChessSquare::whiteRook:
                case ChessSquare::blackRook:
                    for (d = 0; d <= 1; d++)
                        for (s = -1; s <= 1; s += 2)
                            for (i = 1;; i++) {
                                rt = rf + (i * s) * d;
                                ft = ff + (i * s) * (1 - d);
                                if (rt < 0 || rt > ranks() - 1 || ft < 0 || ft > files() - 1)
                                    break;

                                if (rt == targetRank && ft == targetFile)
                                    return true;

                                if (!fBoard[rt][ft].isEmpty())
                                    break;
                            }
                    break;

                case ChessSquare::whiteQueen:
                case ChessSquare::blackQueen:
                    for (rs = -1; rs <= 1; rs++)
                        for (fs = -1; fs <= 1; fs++) {
                            if (rs == 0 && fs == 0)
                                continue;
                            for (i = 1;; i++) {
                                rt = rf + (i * rs);
                                ft = ff + (i * fs);

                                if (rt < 0 || rt > ranks() - 1 || ft < 0 || ft > files() - 1)
                                    break;

                                if (rt == targetRank && ft == targetFile)
                                    return true;

                                if (!fBoard[rt][ft].isEmpty())
                                    break;
                            }
                        }
                    break;

                case ChessSquare::whiteKing:
                case ChessSquare::blackKing:

                    for (i = -1; i <= 1; i++)
                        for (j = -1; j <= 1; j++) {
                            if (i == 0 && j == 0)
                                continue;
                            rt = rf + i;
                            ft = ff + j;
                            if (rt != targetRank || ft != targetFile)
                                continue;

                            if (rt == targetRank && ft == targetFile)
                                return true;
                        }
                    break;

                case ChessSquare::empty: break;

                default: assert(0); // not Reached
            }
        }

    return false;
}

void Board::disambiguate(ChessMove const& move, std::string& san, SANQueue& allSAN) {
    if (san.length()) {
        bool conflict = false, rankConflict = false, fileConflict = false;
        for (int i = 0; i < ssize(allSAN); ++i) {
            auto& sanMv = allSAN[i].move();
            if (sanMv.rt() == move.rt() && sanMv.ft() == move.ft() && // the same 'to' square
                sanMv != move &&                                      // not the same move
                fBoard[move.rf()][move.ff()].contents() ==
                    fBoard[sanMv.rf()][sanMv.ff()].contents() && // same type of piece
                (!rankConflict || !fileConflict)) // if you have two conflicts, no need to continue
            {
                conflict = true;
                if (move.rf() == sanMv.rf())
                    rankConflict = true;
                else if (move.ff() == sanMv.ff())
                    fileConflict = true;
            }
        }
        // resolve if the piece is on same file of rank (if there are three same
        // pieces then use file and rank)
        if (conflict && !rankConflict && !fileConflict) {
            san.insert(1, 1, ChessFileToChar(move.ff()));
        } else if (conflict && !rankConflict) {
            san.insert(1, 1, ChessRankToChar(move.rf()));
        } else if (fileConflict && rankConflict) {
            san.insert(1, {ChessFileToChar(move.ff()), ChessRankToChar(move.rf())});
        } else if (rankConflict) {
            san.insert(1, 1, ChessFileToChar(move.ff()));
        }
    }
}

void Board::disambiguateMoves(SANQueue& allSAN) {
    bool ambiguity = false;
    int  i;
    for (i = 0; i < (signed)allSAN.size() - 1; ++i) {
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
void GenPseudoLegalMoves(Board const& b, MoveList& moves) {
    if (b.toMove() == Board::endOfGame)
        return;

    for (int rf = 0; rf < b.ranks(); ++rf)
        for (int ff = 0; ff < b.files(); ++ff) {
            if ((b(rf, ff).isBlack() && b.toMove() == Board::white) ||
                (b(rf, ff).isWhite() && b.toMove() == Board::black)) {
                continue;
            }
            switch (b(rf, ff).contents()) {

                case ChessSquare::whitePawn:
                    if (rf < 7 && b(rf + 1, ff).isEmpty()) {
                        if (rf == b.ranks() - 2) // promotion
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
                    if (rf == 1 && b(2, ff).isEmpty() && b(3, ff).isEmpty()) {
                        moves.add(ChessMove(rf, ff, 3, ff));
                    }
                    for (int s = -1; s <= 1; s += 2) {
                        if (rf < 7 && ff + s >= 0 && ff + s <= 7 && b(rf + 1, ff + s).isBlack()) {
                            if (rf == b.ranks() - 2) // promotion
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
                        if (rf == b.ranks() - 4) {
                            if (ff + s >= 0 && ff + s <= 7 &&
                                (b.enPassant() == ff + s || b.enPassant() == Board::allCaptures) &&
                                b(b.ranks() - 4, ff + s).contents() == ChessSquare::blackPawn &&
                                b(b.ranks() - 3, ff + s).isEmpty()) {
                                moves.add(
                                    ChessMove(rf, ff, b.ranks() - 3, ff + s, ChessMove::whiteEnPassant));
                            }
                        }
                    }
                    break;

                case ChessSquare::blackPawn:
                    if (rf > 0 && b(rf - 1, ff).isEmpty()) {
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
                    if (rf == b.ranks() - 2 && b(b.ranks() - 3, ff).isEmpty() &&
                        b(b.ranks() - 4, ff).isEmpty()) {
                        moves.add(ChessMove(rf, ff, b.ranks() - 4, ff));
                    }
                    for (int s = -1; s <= 1; s += 2) {
                        if (rf > 0 && ff + s >= 0 && ff + s <= 7 && b(rf - 1, ff + s).isWhite()) {
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
                                (b.enPassant() == ff + s || b.enPassant() == Board::allCaptures) &&
                                b(3, ff + s).contents() == ChessSquare::whitePawn && b(2, ff + s).isEmpty()) {
                                moves.add(ChessMove(rf, ff, 2, ff + s, ChessMove::blackEnPassant));
                            }
                        }
                    }
                    break;

                case ChessSquare::whiteKnight:
                case ChessSquare::blackKnight:
                    for (int i = -1; i <= 1; i += 2)
                        for (int j = -1; j <= 1; j += 2)
                            for (int s = 1; s <= 2; s++) {
                                int rt = rf + i * s;
                                int ft = ff + j * (3 - s);
                                if (rt < 0 || rt > b.ranks() - 1 || ft < 0 || ft > b.files() - 1)
                                    continue;

                                if (IsSameColor(b(rf, ff), b(rt, ft)))
                                    continue;

                                moves.add(ChessMove(rf, ff, rt, ft));
                            }
                    break;

                case ChessSquare::whiteBishop:
                case ChessSquare::blackBishop:
                    for (int rs = -1; rs <= 1; rs += 2)
                        for (int fs = -1; fs <= 1; fs += 2)
                            for (int i = 1;; i++) {
                                int rt = rf + (i * rs);
                                int ft = ff + (i * fs);
                                if (rt < 0 || rt > b.ranks() - 1 || ft < 0 || ft > b.files() - 1)
                                    break;
                                if (IsSameColor(b(rf, ff), b(rt, ft)))
                                    break;

                                moves.add(ChessMove(rf, ff, rt, ft));

                                if (!b(rt, ft).isEmpty())
                                    break;
                            }
                    break;

                case ChessSquare::whiteRook:
                case ChessSquare::blackRook:
                    for (int d = 0; d <= 1; d++)
                        for (int s = -1; s <= 1; s += 2)
                            for (int i = 1;; i++) {
                                int rt = rf + (i * s) * d;
                                int ft = ff + (i * s) * (1 - d);
                                if (rt < 0 || rt > b.ranks() - 1 || ft < 0 || ft > b.files() - 1)
                                    break;
                                if (IsSameColor(b(rf, ff), b(rt, ft)))
                                    break;

                                moves.add(ChessMove(rf, ff, rt, ft));

                                if (!b(rt, ft).isEmpty())
                                    break;
                            }
                    break;

                case ChessSquare::whiteQueen:
                case ChessSquare::blackQueen:
                    for (int rs = -1; rs <= 1; rs++)
                        for (int fs = -1; fs <= 1; fs++) {
                            if (rs == 0 && fs == 0)
                                continue;
                            for (int i = 1;; i++) {
                                int rt = rf + (i * rs);
                                int ft = ff + (i * fs);
                                if (rt < 0 || rt > 7 || ft < 0 || ft > 7)
                                    break;
                                if (IsSameColor(b(rf, ff), b(rt, ft)))
                                    break;

                                moves.add(ChessMove(rf, ff, rt, ft));

                                if (!b(rt, ft).isEmpty())
                                    break;
                            }
                        }
                    break;

                case ChessSquare::whiteKing:
                case ChessSquare::blackKing:

                    for (int i = -1; i <= 1; i++)
                        for (int j = -1; j <= 1; j++) {
                            if (i == 0 && j == 0)
                                continue;
                            int rt = rf + i;
                            int ft = ff + j;
                            if (rt < 0 || rt > 7 || ft < 0 || ft > 7)
                                continue;
                            if (IsSameColor(b(rf, ff), b(rt, ft)))
                                continue;

                            moves.add(ChessMove(rf, ff, rt, ft));
                        }
                    break;

                case ChessSquare::empty: break;

                default: assert(0); // not Reached
            }
        }
}

// returns if the person to move is in check
bool IsInCheck(Board b) {
    // scan a1,a2,...,h8 only test first king found
    bool                  isFound = false;
    ChessSquare::Occupant target =
        b.toMove() == Board::white ? ChessSquare::whiteKing : ChessSquare::blackKing;

    struct {
        unsigned r, f;
    } targetSquare;

    for (int r = 0; r < b.ranks() && !isFound; ++r)
        for (int f = 0; f < b.files() && !isFound; ++f) {
            if (b(r, f).contents() == target) {
                targetSquare.r = r;
                targetSquare.f = f;
                isFound        = true;
            }
        }

    if (!isFound) {
        return false; // there is no king on the board
    }

    b.switchMove();
    return b.canCaptureSquare(targetSquare.r, targetSquare.f);
}

// if the move is made what will you be left in check?
bool WillBeInCheck(Board b, ChessMove const& move) {
    b.move(move);
    return IsInCheck(b);
}

// if the move is made will you be giving check?
bool WillGiveCheck(Board b, ChessMove const& move) {
    b.move(move);
    b.switchMove();
    return IsInCheck(b);
}

void GenLegalMoves(Board const& b, MoveList& moves) {
    GenPseudoLegalMoves(b, moves);
    int i;

    // castling moves
    if (b.isWhiteToMove() && ((b.getCastle() & Board::whiteKS) || (b.getCastle() & Board::whiteQS))) {
        // search for king on first rank
        for (i = 0; i < b.files(); ++i) {
            if (b(0, i) == ChessSquare::whiteKing) {
                if (b.getCastle() & Board::whiteKS) {
                    bool pieceInWay = false;
                    for (int j = i + 1; j < b.files() - 1; ++j)
                        if (!b(0, j).isEmpty())
                            pieceInWay = true;

                    if (!pieceInWay && b(0, b.files() - 1) == ChessSquare::whiteRook && i + 2 < b.files()) {
                        if (!IsInCheck(b) && !WillBeInCheck(b, ChessMove(0, i, 0, i + 1)))
                            moves.add(ChessMove(0, i, 0, i + 2, ChessMove::whiteCastleKS));
                    }
                }
                if (b.getCastle() & Board::whiteQS) {
                    bool pieceInWay = false;
                    for (int j = i - 1; j > 1; --j)
                        if (!b(0, j).isEmpty())
                            pieceInWay = true;

                    if (!pieceInWay && b(0, 0) == ChessSquare::whiteRook && i - 2 > 0) {
                        if (!IsInCheck(b) &&
                            !WillBeInCheck(b, ChessMove(0, i, 0, i - 1))) // cannot castle through
                                                                          // check, or when in check
                            moves.add(ChessMove(0, i, 0, i - 2, ChessMove::whiteCastleQS));
                    }
                }
            }
            //!!? Break is possible here, unless two kings on rank and one can castle
            //!(some form of wild?)
        }

    } else if (b.isBlackToMove() && ((b.getCastle() & Board::blackKS) || (b.getCastle() & Board::blackQS))) {
        // search for king on back rank
        for (i = 0; i < b.files(); ++i) {
            if (b(b.ranks() - 1, i) == ChessSquare::blackKing) {
                if (b.getCastle() & Board::blackKS) {
                    bool pieceInWay = false;
                    for (int j = i + 1; j < b.files() - 1; ++j)
                        if (!b(b.ranks() - 1, j).isEmpty())
                            pieceInWay = true;

                    if (!pieceInWay && b(b.ranks() - 1, b.files() - 1) == ChessSquare::blackRook &&
                        i + 2 < b.files()) {
                        if (!IsInCheck(b) &&
                            !WillBeInCheck(b, ChessMove(b.ranks() - 1, i, b.ranks() - 1, i + 1))) {
                            moves.add(
                                ChessMove(b.ranks() - 1, i, b.ranks() - 1, i + 2, ChessMove::blackCastleKS));
                        }
                    }
                }
                if (b.getCastle() & Board::blackQS) {
                    bool pieceInWay = false;
                    for (int j = i - 1; j > 1; --j)
                        if (!b(b.ranks() - 1, j).isEmpty())
                            pieceInWay = true;

                    if (!pieceInWay && b(b.ranks() - 1, 0) == ChessSquare::blackRook && i - 2 > 0) {
                        if (!IsInCheck(b) &&
                            !WillBeInCheck(b, ChessMove(b.ranks() - 1, i, b.ranks() - 1, i - 1)))
                            moves.add(
                                ChessMove(b.ranks() - 1, i, b.ranks() - 1, i - 2, ChessMove::blackCastleQS));
                    }
                }
            }
        }
    }

    i = 0;
    while (i < ssize(moves)) {
        if (WillBeInCheck(b, moves[i]))
            moves.remove(i);
        else
            ++i;
    }
}

E_gameCheckStatus CheckStatus(Board const& b, MoveList& allMoves) {
    if (allMoves.size())
        return IsInCheck(b) ? inCheck : notInCheck;
    else
        return IsInCheck(b) ? inCheckmate : inStalemate;
}

// significant in some cases)
//!?? allMoves must be the legal moves for the board, e.g. call GenLegalMoves(b,
//!&allMoves) just before this function
//??! Still need to test what happens when the allMoves is incorrect
void MoveToAlgebraic(ChessMove const& move, Board const& b, MoveList const& allMoves, std::string& out) {
    std::ostringstream o;

    bool conflict = false, fileConflict = false,
         rankConflict = false; // use the file in case of a conflict, and rank if no file
                               // conflict, and both if necessary (e.g. there are three
                               // pieces accessing the same square)
    switch (b(move.rf(), move.ff()).contents()) {
        case ChessSquare::whitePawn:
        case ChessSquare::blackPawn:

            o << ChessFileToChar(move.ff());
            if (move.ff() != move.ft()) {
                o << 'x' << ChessFileToChar(move.ft())
                  << ChessRankToChar(move.rt()); // Capture; use style "exd5"
            } else {
                o << ChessRankToChar(move.rt()); // Non-capture; use style "e5"
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

        case ChessSquare::whiteKing:
        case ChessSquare::blackKing:
            // castling
            if (move.rf() == move.rt() && move.rf() == (b(move.rf(), move.ff()).isWhite()) ? 0
                                                                                           : b.ranks() - 1) {
                if ((signed)move.ff() - (signed)move.ft() < -1) {
                    o << "O-O";
                    break;
                } else if ((signed)move.ff() - (signed)move.ft() > 1) {
                    o << "O-O-O";
                    break;
                }
            }

            [[fallthrough]];
        default:
            o << (char)toupper(b(move.rf(), move.ff()).pieceToChar());

            for (int i = 0; i < ssize(allMoves); ++i) {
                if (allMoves[i] != move &&                                            // not the same move
                    allMoves[i].rt() == move.rt() && allMoves[i].ft() == move.ft() && // the same 'to' square
                    b(move.rf(), move.ff()).contents() ==
                        b(allMoves[i].rf(), allMoves[i].ff()).contents()) // same type of piece
                {
                    conflict = true;
                    if (move.rf() == allMoves[i].rf())
                        rankConflict = true;
                    else if (move.ff() == allMoves[i].ff())
                        fileConflict = true;
                }
            }
            // resolve if the piece is on same file of rank (if there are three same
            // pieces then use file and rank)
            if (conflict && !rankConflict && !fileConflict) {
                o << ChessFileToChar(move.ff());
            } else if (conflict && !rankConflict) {
                o << ChessRankToChar(move.rf());
            } else if (fileConflict && rankConflict) {
                o << ChessFileToChar(move.ff()) << ChessRankToChar(move.rf());
            } else if (rankConflict) {
                o << ChessFileToChar(move.ff());
            }

            // determine if it's a capture
            if (!b(move.rt(), move.ft()).isEmpty()) {
                o << 'x';
            }

            // destination square
            o << ChessFileToChar(move.ft()) << ChessRankToChar(move.rt());

            // Check or Checkmate
            //??! Removed for speed, and compatibility with Board::genLegalMoveSet()
    }
    out += o.str();
}

// case insensitive
unsigned NumCharsInStr(std::string_view str, char c) {
    int count = 0;
    for (auto ch : str)
        if (toupper(ch) == toupper(c))
            ++count;
    return count;
}

//!?? If a move is ambiguous it will just pick the first move it finds.
// returns true if the moved could be parsed, false otherwise
// performs a lot of cleanup, so the move can be messy (calls
// RemoveChars("x+=#!? "))
bool AlgebraicToMove(std::string_view constSAN, Board const& b, ChessMove& move) {
    MoveList allMoves;
    GenLegalMoves(b, allMoves);

    return AlgebraicToMove(constSAN, b, allMoves, move);
}

bool AlgebraicToMove(std::string_view constSAN, Board const& b, MoveList const& allMoves, ChessMove& move) {
    std::string san(constSAN);

    RemoveWhiteSpace(san);
    // remove unnecessary chars
    RemoveChars(san, "+#x=!? ");

    if (san.empty())
        return false;

    // get piece type, look for
    ChessSquare::Occupant piece;
    switch (san[0]) {
        case 'N':
        case 'n': piece = b.isWhiteToMove() ? ChessSquare::whiteKnight : ChessSquare::blackKnight; break;
        case 'B': piece = b.isWhiteToMove() ? ChessSquare::whiteBishop : ChessSquare::blackBishop; break;
        case 'R':
        case 'r': piece = b.isWhiteToMove() ? ChessSquare::whiteRook : ChessSquare::blackRook; break;
        case 'Q':
        case 'q': piece = b.isWhiteToMove() ? ChessSquare::whiteQueen : ChessSquare::blackQueen; break;
        case 'K':
        case 'k': piece = b.isWhiteToMove() ? ChessSquare::whiteKing : ChessSquare::blackKing; break;
        case 'O':
        case 'o': // castling
            if (NumCharsInStr(san, 'O') > 2) {
                for (size_t i = 0; i < allMoves.size(); ++i)
                    if (allMoves[i].type() == ChessMove::whiteCastleQS ||
                        allMoves[i].type() == ChessMove::blackCastleQS) {
                        assert((b.isWhiteToMove() && allMoves[i].type() == ChessMove::whiteCastleQS) ||
                               (b.isBlackToMove() && allMoves[i].type() == ChessMove::blackCastleQS));
                        move = allMoves[i];
                        return true;
                    }
                return false;
            } else {
                for (size_t i = 0; i < allMoves.size(); ++i)
                    if (allMoves[i].type() == ChessMove::whiteCastleKS ||
                        allMoves[i].type() == ChessMove::blackCastleKS) {
                        assert((b.isWhiteToMove() && allMoves[i].type() == ChessMove::whiteCastleKS) ||
                               (b.isBlackToMove() && allMoves[i].type() == ChessMove::blackCastleKS));
                        move = allMoves[i];
                        return true;
                    }
                return false;
            }

        default: piece = b.isWhiteToMove() ? ChessSquare::whitePawn : ChessSquare::blackPawn; break;
    }

    move.type() = ChessMove::normal;

    if (piece == ChessSquare::whitePawn || piece == ChessSquare::blackPawn) {
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
                return false;
        }
        // possibilities: "f", "f4", "ef (e.p.)", "ef4", "exf4 (e.p.)", "e3f4
        // (e.p.)", "e3xf4" (e.p.) all x's have been removed, e.p. is taken care of
        if (san.empty())
            return false;

        if (san.length() == 1) // f
        {
            for (size_t i = 0; i < allMoves.size(); ++i) {
                if (allMoves[i].ff() == ChessCharToFile(san[0]) &&
                    (move.isPromo() ? move.type() == allMoves[i].type() : true) &&
                    b(allMoves[i].rf(), allMoves[i].ff()) == piece) {
                    move = allMoves[i];
                    return true;
                }
            }
        } else if (san.length() == 2 && isdigit(san[1])) // f4
        {
            for (size_t i = 0; i < allMoves.size(); ++i)
                if (allMoves[i].ff() == ChessCharToFile(san[0]) &&
                    allMoves[i].rt() == ChessCharToRank(san[1]) &&
                    (move.isPromo() ? move.type() == allMoves[i].type() : true) &&
                    b(allMoves[i].rf(), allMoves[i].ff()) == piece) {
                    move = allMoves[i];
                    return true;
                }

        } else if (san.length() == 2) // ef
        {
            assert(!isdigit(san[1]));
            for (size_t i = 0; i < allMoves.size(); ++i)
                if (allMoves[i].ff() == ChessCharToFile(san[0]) &&
                    allMoves[i].ft() == ChessCharToFile(san[1]) &&
                    (move.isPromo() ? move.type() == allMoves[i].type() : true) &&
                    b(allMoves[i].rf(), allMoves[i].ff()) == piece) {
                    move = allMoves[i];
                    return true;
                }
        } else if (san.length() == 3) // ef4
        {
            for (size_t i = 0; i < allMoves.size(); ++i)
                if (allMoves[i].ff() == ChessCharToFile(san[0]) &&
                    allMoves[i].ft() == ChessCharToFile(san[1]) &&
                    allMoves[i].rt() == ChessCharToRank(san[2]) &&
                    (move.isPromo() ? move.type() == allMoves[i].type() : true) &&
                    b(allMoves[i].rf(), allMoves[i].ff()) == piece) {
                    move = allMoves[i];
                    return true;
                }
        } else if (san.length() == 4) // e3f4
        {
            for (size_t i = 0; i < allMoves.size(); ++i)
                if (allMoves[i].ff() == ChessCharToFile(san[0]) &&
                    allMoves[i].rf() == ChessCharToRank(san[1]) &&
                    allMoves[i].ft() == ChessCharToFile(san[2]) &&
                    allMoves[i].rt() == ChessCharToRank(san[3]) &&
                    (move.isPromo() ? move.type() == allMoves[i].type() : true) &&
                    b(allMoves[i].rf(), allMoves[i].ff()) == piece) {
                    move = allMoves[i];
                    return true;
                }
        }
    } else {
        size_t const sanLength = san.length();
        if (sanLength < 3)
            return false;

        for (size_t i = 0; i < allMoves.size(); ++i)
            if (allMoves[i].ft() == ChessCharToFile(san[sanLength - 2]) &&
                allMoves[i].rt() == ChessCharToRank(san[sanLength - 1]) &&
                b(allMoves[i].rf(), allMoves[i].ff()) == piece) {
                switch (sanLength) {
                    case 3: move = allMoves[i]; return true; // Nf3
                    case 4:
                        if (isdigit(san[1]) && allMoves[i].rf() == ChessCharToRank(san[1])) // N1f3
                        {
                            move = allMoves[i];
                            return true;
                        } else if (allMoves[i].ff() == ChessCharToFile(san[1])) // Ngf3
                        {
                            move = allMoves[i];
                            return true;
                        }
                        break;
                    case 5:
                        if (allMoves[i].ff() == ChessCharToFile(san[1]) &&
                            allMoves[i].rf() == ChessCharToFile(san[2])) // Ng1f3
                        {
                            move = allMoves[i];
                            return true;
                        }
                        break;
                    default: return false;
                }
            }
    }

    return false; // no matching legal move was found
}

#ifdef TEST

char const gFENBeginNormalGame[] = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

    #include <iostream>

StopWatch gTimer[10];

int main() {
    // NormalChessGame g1;
    /*
    char c[] = {" exf4 "};

    std::cout << "\n" << strcspn("eggxggf4", "xft ");

    RemoveChars(c, " ");

    std::cout << "\n '" << c << "' ";
    */
    char  buf[20];
    Board b;

    while (std::cin.good()) {
        SANQueue set;
        MoveList moves;

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

        ChessMove move;
        if (AlgebraicToMove(buf, b, move)) {
            std::string SAN;
            MoveToAlgebraic(move, b, moves, SAN);

            b.processMove(move);

            std::cout << "\n Moved: '" << SAN.c_str() << "' ";
        } else {
            std::cout << "\n Move is not legal.";
        }
    }

    return EXIT_SUCCESS;
}

#endif
