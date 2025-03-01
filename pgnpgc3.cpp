#include "stpwatch.h" // profiling
StopWatch gTimer[10];

#include <assert.h>
#include <cstring>
#include <ctype.h>
#include <filesystem>
#include <sstream>
namespace fs = std::filesystem;

// .pgn to .pgc
#include "chess_2.h"
#include "list5.h"

using pgcByteT       = int8_t;  // one byte (two's complement)
using pgcWordT       = int16_t; // two bytes (two's complement)
using pgcDoubleWordT = int32_t; // four bytes (two's complement)

// from https://stackoverflow.com/a/8197886/85371
#include <bit>
#include <climits>
template <std::integral T> constexpr bool is_little_endian() {
    for (unsigned bit = 0; bit != sizeof(T) * CHAR_BIT; ++bit) {
        unsigned char data[sizeof(T)] = {};
        // In little-endian, bit i of the raw bytes ...
        data[bit / CHAR_BIT] = 1 << (bit % CHAR_BIT);
        // ... Corresponds to bit i of the value.
        if (std::bit_cast<T>(data) != T(1) << bit)
            return false;
    }
    return true;
}

static inline void gToLittleEndian(pgcWordT w, char c[]) {
    assert(c);

    if constexpr (char* source = (char*)&w; is_little_endian<pgcWordT>()) {
        c[0] = source[0];
        c[1] = source[1];
    } else {
        c[0] = source[1];
        c[1] = source[0];
    }
}

//!!? Possible expansion (not covered in PGN standard document):
//  support for comments
//  special markers for supplementary tags, instead of kMarkerTagPair
// additional markers for strings that now only can have 255 length or only 2
// byte length to have choice (like short and long move sequence) change result
// tag to one byte // remove length info as well remove date length info (it's
// always the same) and change to byte sequence (e.g. int-2 year int-2 month
// int-1 day)

#include "list5.h"
#include <cstring>
#include <iostream>

[[maybe_unused]] //
static pgcByteT const kMarkerBeginGameReduced  = 0x01;
static pgcByteT const kMarkerTagPair           = 0x02;
static pgcByteT const kMarkerShortMoveSequence = 0x03;
static pgcByteT const kMarkerLongMoveSequence  = 0x04;
static pgcByteT const kMarkerGameDataBegin     = 0x05;
static pgcByteT const kMarkerGameDataEnd       = 0x06;
static pgcByteT const kMarkerSimpleNAG         = 0x07;
static pgcByteT const kMarkerRAVBegin          = 0x08;
static pgcByteT const kMarkerRAVEnd            = 0x09;
static pgcByteT const kMarkerEscape            = 0x0a;

void SkipWhite(char const*& c) {
    assert(c);
    while (isspace(*c) && *c != '\0')
        ++c;
}

// skips over all chars till it finds any chars in target, then moves just past target
//
// "xxxx[xxx"
//       ^
void SkipTo(char const*& c, char const target[]) {
    assert(c);
    assert(target);

    while (auto n = strcspn(c, target))
        c += n;

    if (*c != '\0')
        ++c;
}

struct PGNTag {
    std::string name, value;
};

// adds all of the tags to the list, and returns where the tags ended.
// tag names are converted to upper case
char const* ParsePGNTags(char const* pgn, List<PGNTag>* tags) {
    assert(pgn);
    assert(tags);

    char const kPGNTagBegin[]      = "[";
    char const kPGNTagEnd[]        = "]";
    char const kPGNTagValueBegin[] = "\"";
    char const kPGNTagValueEnd     = '\"';

    bool parsingTags = true;
    while (parsingTags) {
        SkipTo(pgn, kPGNTagBegin);
        SkipWhite(pgn);

        PGNTag tag;
        while (!isspace(*pgn) && *pgn != kPGNTagValueBegin[0] && *pgn != '\0') {
            tag.name += toupper(*pgn);
            ++pgn;
        }

        SkipTo(pgn, kPGNTagValueBegin);
        while (*pgn != kPGNTagValueEnd && *pgn != '\0') {
            tag.value += *pgn;
            ++pgn;
        }
        if (*pgn != '\0')
            tags->add(tag);

        SkipTo(pgn, kPGNTagEnd);
        SkipWhite(pgn);

        if (*pgn != kPGNTagBegin[0] || *pgn == '\0')
            parsingTags = false;
    }

    return pgn;
}

// returns the element, or -1 if it didn't find it
int FindElement(std::string const& target, SANQueue& source) {
    for (unsigned i = 0; i < source.size(); ++i)
        if (source[i].SAN() == target)
            return i;

    return -1;
}

//??! Illegal moves will mess up the .pgc, changing will be non-trivial (e.g.
// illegal move just before RAVBegin)
//    solution was to not record games with illegal moves
// recursive
//!?? Game termination must appear after all comments and escape sequences --
//! PGN standard is not clear in this regard
//!?? A RAV can have a format 1. e4 e5 (1...d5)(1...Nf6) even though the
//! Standard only specifies 1. e4 e5 (1...d5 (1...Nf6))
// this performs a lot of clean-up, e.g. move numbers are ignored
enum E_gameTermination {
    none, // still need to processing game
    parsingError,
    illegalMove,
    RAVUnderflow,
    unknown,
    whiteWin,
    blackWin,
    draw
}; //?!! Use later to determine if original STR Result is correct

E_gameTermination ProcessMoveSequence(Board& game, char const*& pgn, std::ostream& pgc) {
    static Board gPreviousGamePos; // used in case their is something other than a
                                   // move sequence before a RAV
    static int gRAVLevels;         // used to finish putting RAVEnd markers, and to detect
                                   // RAV underflow

    enum E_reasonToEndSequence { RAVBegin, RAVEnd, NAG, escape, other } reasonToBreak = other;
    E_gameTermination gameResult                                                      = none;

    pgcByteT    NAGVal;
    std::string escapeToken;

    auto const moves = [&] {
        std::vector<std::string> moves;
        std::string              token;
        for (bool processMoveSequence = true; processMoveSequence; token.clear()) // move sequence
        {
            SkipWhite(pgn);
            if (*pgn == '[') {
                gameResult = unknown;
                // processMoveSequence = false; // redundant here
                break;
            }

            auto const token = [&] {
                std::string token;
                // get the next token
                while (!isspace(*pgn) && *pgn != '\0') {
                    if (token.length() && *pgn == ')')
                        break;

                    if ((*pgn == '!') || (*pgn == '?')) // NAG
                    {
                        if (token.length()) {
                            break;
                        } else {
                            while (!isspace(*pgn) && *pgn != '\0' && (*pgn == '!' || *pgn == '?')) {
                                token += *pgn;
                                ++pgn;
                            }
                            break;
                        }
                    }
                    token += *pgn;
                    //                cout << "\nCurToken: " << token;
                    ++pgn;
                    assert(token.length());
                    if (token == "(" || token == ")" || token[token.length() - 1] == '.')
                        break;
                }
                return token;
            }();

            if (token.empty()) // no more tokens to process
            {
                processMoveSequence = false;
            } else if (isdigit(token[0])) // move# or end-of-game (1-0 1-0 1/2-1/2)
            {
                if (token == "1-0") {
                    gameResult = whiteWin; // there can only be one game termination per
                                           // game, cannot be in a RAV
                    processMoveSequence = false;
                } else if (token == "0-1") {
                    gameResult          = blackWin;
                    processMoveSequence = false;
                } else if (token == "1/2-1/2" || token == "1/2") {
                    gameResult          = draw;
                    processMoveSequence = false;
                }
            } else if (token == "*") {
                gameResult          = unknown;
                processMoveSequence = false;
            } else if (isalpha(token[0])) // SAN move
            {
                moves.push_back(std::move(token));
            } else if (token == "!") // NAVVal values from pgn standard sec. 10
            {
                reasonToBreak       = NAG;
                NAGVal              = 1;
                processMoveSequence = false;
            } else if (token == "?") {
                reasonToBreak       = NAG;
                NAGVal              = 2;
                processMoveSequence = false;
            } else if (token == "!!") {
                reasonToBreak       = NAG;
                NAGVal              = 3;
                processMoveSequence = false;
            } else if (token == "??") {
                reasonToBreak       = NAG;
                NAGVal              = 4;
                processMoveSequence = false;
            } else if (token == "!?") {
                reasonToBreak       = NAG;
                NAGVal              = 5;
                processMoveSequence = false;
            } else if (token == "?!") {
                reasonToBreak       = NAG;
                NAGVal              = 6;
                processMoveSequence = false;
            } else if (token[0] == '{') // multi-line comment
            {
                // .pgc doesn't allow for comments yet..
                SkipTo(pgn, "}");
            } else if (token[0] == ';') // single-line comment
            {
                SkipTo(pgn, "\n");
            } else if (token[0] == '(') // RAV
            {
                ++gRAVLevels;
                reasonToBreak       = RAVBegin;
                processMoveSequence = false;
            } else if (token[0] == ')') // RAV
            {
                --gRAVLevels;
                reasonToBreak       = RAVEnd;
                processMoveSequence = false;
            } else if (token[0] == '$') // NAG
            {
                reasonToBreak       = NAG;
                NAGVal              = atoi(&token.c_str()[1]);
                processMoveSequence = false;
            } else if (token[0] == '.') // black to move (...)
            {
                // redundant
            } else if (token[0] == '[') // begin another game (without end-of-game
                                        // marker) // taken care of up top
            {
                assert(0);
            } else if (token[0] == '%') // escape sequence
            {
                escapeToken = &token.c_str()[1]; //??! An escape sequence can have null, but
                                                 // this function uses char*, so it can't
                while (*pgn != '\n' && *pgn != '\0') {
                    escapeToken += *pgn;
                    pgn++;
                }
                reasonToBreak       = escape;
                processMoveSequence = false;
            } else // error, everything in a valid PGN game should be taken care of
            {
                throw parsingError;
            }
        }
        return moves;
    }(); // FIXME handle exception

    // Process Moves
    //!!? Only need to indicate zero moves if the game is empty and not using
    //! Begin and end game data markers (i.e. using kMarkerBeginGameReduced)
    if (moves.size()) {
        if (moves.size() <= UCHAR_MAX)
            pgc << kMarkerShortMoveSequence << (pgcByteT)moves.size();
        else {
            char moveSize[2];
            gToLittleEndian(moves.size(), moveSize);
            pgc << kMarkerLongMoveSequence << moveSize[0] << moveSize[1];
        }

        gTimer[4].start();
        for (size_t i = 0; auto& mv : moves) {
            SANQueue        SANMoves;
            List<ChessMove> allMoves;
            gTimer[2].start();
            game.genLegalMoveSet(allMoves, SANMoves); // 57%
            gTimer[2].stop();

            gTimer[3].start();
            ChessMove cm;
            if (!game.algebraicToMove(cm, mv, allMoves)) {
                std::cout << "\nIllegal move: " << mv;
                std::cout << "\n";
                game.display();
                return illegalMove; // move is not legal
            }
            std::string san;

            game.moveToAlgebraic(san, cm, allMoves);
            gTimer[3].stop();

            assert(FindElement(san, SANMoves) != -1);

            pgc << (pgcByteT)FindElement(san, SANMoves);

            if (i == (moves.size() - 1)) {
                if (reasonToBreak == RAVBegin) {
                    pgc << kMarkerRAVBegin;
                    Board temp = game;
                    gameResult = ProcessMoveSequence(temp, pgn, pgc);
                } else {
                    // their can't be two RAV's at the same level for the same
                    // move, instead use 1. (1. (1.)) 1... not 1. (1.)(1.) 1...
                    // (pgn formal syntax)
                    gPreviousGamePos = game;
                }
            }

            gTimer[7].start();
            game.processMove(cm, allMoves);
            gTimer[7].stop();

            ++i;
        }
        gTimer[4].stop();

    } else if (reasonToBreak == RAVBegin) // e.g. in case their is a NAG in before the RAVBegin
    {
        pgc << kMarkerRAVBegin;
        gameResult = ProcessMoveSequence(gPreviousGamePos, pgn, pgc);
    }

    switch (reasonToBreak) {
        case RAVEnd: pgc << kMarkerRAVEnd; break;
        case NAG:
            if (moves.size())
                pgc << kMarkerSimpleNAG << NAGVal;
            break; // you can only have a NAG if you have a move, and only one NAG per
                   // move (formal pgn syntax)
        case escape:
            char escapeLength[2];
            gToLittleEndian(escapeToken.length(), escapeLength);
            pgc << kMarkerEscape << escapeLength[0] << escapeLength[1] << escapeToken;
            break;
        case RAVBegin: break;
        default: break;
    }

    if (gRAVLevels < 0)
        return RAVUnderflow;

    if (gameResult != none && gRAVLevels)
        while (gRAVLevels) {
            pgc << kMarkerRAVEnd;
            --gRAVLevels;
        }

    return gameResult;
}

// convert game from .pgn format to .pgc format
// returns true if game is valid and succeeded, false otherwise
// sets endOfGame to the place in pgn where the game stopped being processed
E_gameTermination PgnToPgc(char const* pgn, char const*& endOfGame, std::ostream& pgc) {
    assert(pgn);
    List<PGNTag> tags;
    endOfGame = pgn;
    pgn       = ParsePGNTags(pgn, &tags);

    Board game;

    if (!tags.size()) {
        endOfGame = pgn;
        return parsingError;
    }

    pgc << kMarkerGameDataBegin;
    // output the tags in the right order
    char const* sevenTagRoster[] = {
        "EVENT", "SITE", "DATE", "ROUND", "WHITE", "BLACK", "RESULT",
    };
    int i, j;
    for (i = 0; i < int(sizeof(sevenTagRoster) / sizeof(sevenTagRoster[0])); ++i) {
        bool foundTag = false;
        for (j = 0; j < int(tags.size()) && !foundTag; ++j) {
            if (tags[j].name == sevenTagRoster[i]) {
                assert(tags[j].value.length() <= UCHAR_MAX); //??! Need to deal with this
                pgc << (pgcByteT)tags[j].value.length() << tags[j].value;
                tags.remove(j);
                foundTag = true;
            }
        }
        if (!foundTag) {
            switch (i) {
                case 0:
                case 1:
                case 3:
                case 4:
                case 5: pgc << (pgcByteT)1 << '?'; break;
                case 2: pgc << (pgcByteT)strlen("????.??.??") << "????.??.??"; break;
                case 6: pgc << (pgcByteT)1 << '*'; break;
                default: assert(0); // not Reached
            }
        }
    }
    // any remaining tags
    //??! Case information is lost when parsing tags
    for (i = 0; i < int(tags.size()); ++i) {
        assert(tags[i].name.length() < UCHAR_MAX); //??! Need to deal with this
        pgc << kMarkerTagPair << (pgcByteT)tags[i].name.length() << tags[i].name
            << (pgcByteT)tags[i].value.length() << tags[i].value;

        if (tags[i].name == "FEN") // process FEN
            game.processFEN(tags[i].value.c_str());
    }

    E_gameTermination processGame = none;
    // Board          previousBoard = game;     // used for RAV

    // gTimer[3].start();
    while (processGame == none && *pgn != '\0') // whole game
    {
        processGame = ProcessMoveSequence(game, pgn, pgc);
    }
    // gTimer[3].stop();
    pgc << kMarkerGameDataEnd;

    endOfGame = pgn;
    return processGame;
}

// returns the number of games processed successfully
int PgnToPgcDataBase(std::istream& pgn, std::ostream& pgc) {
    size_t constexpr kLargestGame = 0x4000; // too large will impede performance due to memmove
    thread_local std::array<char, kLargestGame + 1> gameStorage;

    char* const gameBuffer        = gameStorage.data();
    char*       gameBufferCurrent = gameBuffer;

    unsigned gamesProcessed = 0;

    std::cout << "\n"; // USER UPDATE

    auto oldPGNFlags = pgn.flags();
    pgn >> std::noskipws;
    unsigned totalGames = 0;
    while (!pgn.bad() && pgc.good()) {
        std::cout << '.' << std::flush; // USER UPDATE

        std::ostringstream pgcGame(std::ios::binary);

        if (!pgn.eof()) // Clearing eofbit and then reading from file will set
                        // badbit (illegal operation), but need to clear
                        // failbit because it fails when pgn reaches eof()
            pgn.clear();

        pgn.read(gameBufferCurrent, kLargestGame - (gameBufferCurrent - gameBuffer));
        std::streamsize received = pgn.gcount();

        gameBufferCurrent[received] = '\0';

        char const* endOfGame = 0;
        //      gTimer[2].start();
        ++totalGames;
        E_gameTermination result = PgnToPgc(gameBuffer, endOfGame, pgcGame);
        //      gTimer[2].stop();

        switch (result) {
            case illegalMove: std::cout << "\n Illegal move."; break;
            case RAVUnderflow: std::cout << "\n RAV underflow."; break;
            case parsingError:
                std::cout << "\n Parsing error (may be end-of-file).";
                break; // return gamesProcessed; //??! Needs fixing .eof()
            default:
                pgc << pgcGame.str();
                ++gamesProcessed;
                break;
        }
        assert(endOfGame && endOfGame >= gameBuffer);
        memmove(gameBuffer, endOfGame, kLargestGame - (endOfGame - gameBuffer));
        gameBufferCurrent = gameBuffer + kLargestGame - (endOfGame - gameBuffer);

        if (!pgcGame || (!pgn.good() && gameBuffer[0] == '\0')) {
            break;
        }
    }
    assert(!pgn.bad());
    assert(pgc.good());

    pgn.flags(oldPGNFlags);
    return gamesProcessed;
}

// Test Test Test Test Test Test Test Test Test Test Test Test Test Test Test
#define TEST
#ifdef TEST
    #undef TEST

    #include <cassert>
    #include <cctype>
    #include <cstddef>
    #include <cstdio>
    #include <cstdlib>
    #include <cstring>
    #include <fstream>
    #include <iostream>

// non-standard (may not be portable to some operating systems)

// the different kinds of file operations that can cause an error
enum FileOperationErrorT {
    E_openForInput,
    E_openForOutput,
    E_output,
    E_input,
    E_nameReserved,
    E_sameFile,
};

// report a file error to the standard error stream
void ReportFileError(FileOperationErrorT operation, fs::path const& name);
// returns if the file name is reserved (e.g. "PRN" is reserved for the printer)
bool IsFileNameReserved(fs::path const& fileName);

int main(int argc, char* argv[]) {
    // INITIALIZE
    gTimer[0].start();

    fs::path inputFileName, outputFileName;

    // If either inputFileName or outputFile name called "PRN", "LPT1", or
    // "LPT2" the program will give unexpected results.  These are reserved names
    // for the printer.

    // argc, number of elements in argv[]
    // argv[0], undefined (not used)
    // argv[1], input filename (optional)
    // argv[2], ouput filename (optional)
    assert(argc);

    if (argc > 3) {
        std::cout << "\nUsage: pgn2pgc [source_file [report_file]]\n";
        return EXIT_FAILURE;
    }

    // get the name of the input file
    if (argc >= 2) {
        inputFileName = argv[1];
    } else {
        // prompt user for file name
        std::cout << "\nWhat is the name of the PGN file to "
                     "be converted? ";
        if (std::string temp; getline(std::cin, temp))
            inputFileName = temp;
    }

    // If either inputFileName or outputFile name are called "PRN", "LPT1", or
    // "LPT2" the program will give unexpected results.  These are reserved names
    // for the printer.

    if (IsFileNameReserved(inputFileName)) {
        ReportFileError(E_nameReserved, inputFileName);

        return EXIT_FAILURE;
    }

    // open the input stream
    std::ifstream inputStream(inputFileName, std::ios::binary);

    // was the file opened successfully?
    if (!inputStream) {
        ReportFileError(E_openForInput, inputFileName);
        return EXIT_FAILURE;
    }

    if (argc >= 3) {
        outputFileName = argv[2];
    }

    // get the file name for output from user if the file already exists ask the
    // user for confirmation that they want to overwrite it.  If they do not, ask
    // for a new file name.
    bool confirmFile = true;

    do // use a do instead of a while to keep the loop entry condition logical
    {
        // get the name of the ouput file
        if (argc < 3 || !confirmFile) {
            // prompt user for file name
            std::cout << "\nWhat is the name of the PGC file to be created? ";
            if (std::string temp; getline(std::cin, temp))
                outputFileName = temp;
        }

        if (exists(outputFileName)) {
            // ask the user if they're sure they want to overwrite the file
            std::cout << "\nFile " << outputFileName
                      << " already exists, do you want to overwrite it? (y/n) ";

            char response; // we only want to use the first character

            // only read in the first character and then ignore the rest until EOL
            std::cin >> response;
            std::cin.ignore(INT_MAX, '\n');

            confirmFile = (tolower(response) == 'y');
        }
    } while (!confirmFile);

    if (IsFileNameReserved(outputFileName)) {
        ReportFileError(E_nameReserved, outputFileName);

        return EXIT_FAILURE;
    }

    // if the input file is the same as the output file, use a temporary file
    // and then delete the old file and rename the temporary file.
    bool inputOutputSameFile = inputFileName.lexically_normal() == outputFileName.lexically_normal();
    if (inputOutputSameFile)
        outputFileName = fs::temp_directory_path() / "t_wcXXXXXX";

    // open the ouput stream
    std::ofstream outputStream(outputFileName, std::ios::trunc | std::ios::binary);

    // was the file opened successfully?
    if (!outputStream) {
        ReportFileError(E_openForOutput, outputFileName);
        return EXIT_FAILURE;
    }

    // Let user know that what we are about to do
    std::cout << "\nConverting the PGN file " << inputFileName
              << "\n to PGC format and sending the output to file " << outputFileName << "";

    //    gTimer[1].start();
    unsigned gameProcessed = PgnToPgcDataBase(inputStream, outputStream);
    //    gTimer[1].stop();

    std::cout << "\n\nThere " << (gameProcessed == 1 ? "was" : "were") << " " << gameProcessed << " game"
              << (gameProcessed == 1 ? "" : "s") << " processed.";

    if (!outputStream.good()) {
        ReportFileError(E_output, outputFileName);
        return EXIT_FAILURE;
    }

    if (inputOutputSameFile) {
        inputStream.close();
        outputStream.close();

        // delete old file
        std::error_code ec;
        remove(inputFileName, ec);
        if (ec) {
            // print the appropriate message
            std::cerr << "Unable to delete old input file " + ec.message() << std::endl;
            return EXIT_FAILURE;
        }

        // rename temp file to old file
        fs::rename(outputFileName, inputFileName, ec);

        if (ec) // non-zero on failure
        {
            // print the appropriate message
            std::cerr << "Unable to rename the temporary file " + ec.message() << std::endl;
            return EXIT_FAILURE;
        }
    }

    // If we get to here than their were no file errors
    std::cout << "\n\nOperation was successful." << std::endl;

    gTimer[0].stop();

    // SEHE TODO
    // for (int debugI = 0; debugI < sizeof(gTimer) / sizeof(gTimer[0]); ++debugI)
    // { gTimer[debugI].time();
    //}

    return EXIT_SUCCESS;
}

/// reports a file error to the standard error stream
/// @param operation the type of file operation that failed
/// @param name the name of the file
/// @return void
void ReportFileError(FileOperationErrorT operation, fs::path const& name) {
    std::cerr << std::endl;

    switch (operation) {
        case E_openForInput: std::cerr << "Error: Unable to open file " << name << " for input"; break;
        case E_openForOutput: std::cerr << "Error: Unable to open file " << name << " for output"; break;
        case E_output: std::cerr << "Error trying to output to file " << name; break;
        case E_input: std::cerr << "Error trying to input from file " << name; break;
        case E_nameReserved:
            std::cerr << "Error: file name " << name << " is system reserved. Please use another.";
            break;
        case E_sameFile: std::cerr << "Cannot use file " << name << " for both input and output."; break;

        default: assert(0); // Not reached
    }

    std::cerr << std::endl;
}

/// checks if the file name is reserved by the system
/// @param fileName the name of the file to check
/// @return true if the file name is reserved, false otherwise
bool IsFileNameReserved(fs::path const& fileName) {
    std::string const name = fileName.filename();

    for (auto reservedName : {"PRN", "LPT1", "LPT2"})
        if (::strcasecmp(reservedName, name.c_str()) == 0)
            return true;

    return false;
}

#endif
