#include <assert.h>
#include <bit>
#include <cassert>
#include <cctype>
#include <climits>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
namespace fs = std::filesystem;

// .pgn to .pgc
#include "chess_2.h"
#include "stpwatch.h" // profiling

// from https://stackoverflow.com/a/8197886/85371
namespace {
    using pgn2pgc::support::gTimers;
    using namespace pgn2pgc;
    using Chess::Board;
    using Chess::MoveError;
    using Chess::OrderedMoveList;

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

    static inline void gToLittleEndian(int16_t w, std::array<char, 2>& c) {
        if constexpr (char* source = (char*)&w; is_little_endian<int16_t>())
            c = {source[0], source[1]};
        else
            c = {source[1], source[0]};
    }

    //!!? Possible expansion (not covered in PGN standard document):
    //  support for comments
    //  special markers for supplementary tags, instead of kMarkerTagPair
    // additional markers for strings that now only can have 255 length or only 2
    // byte length to have choice (like short and long move sequence) change result
    // tag to one byte // remove length info as well remove date length info (it's
    // always the same) and change to byte sequence (e.g. int-2 year int-2 month
    // int-1 day)

    [[maybe_unused]] //
    static int8_t const kMarkerBeginGameReduced  = 0x01;
    static int8_t const kMarkerTagPair           = 0x02;
    static int8_t const kMarkerShortMoveSequence = 0x03;
    static int8_t const kMarkerLongMoveSequence  = 0x04;
    static int8_t const kMarkerGameDataBegin     = 0x05;
    static int8_t const kMarkerGameDataEnd       = 0x06;
    static int8_t const kMarkerSimpleNAG         = 0x07;
    static int8_t const kMarkerRAVBegin          = 0x08;
    static int8_t const kMarkerRAVEnd            = 0x09;
    static int8_t const kMarkerEscape            = 0x0a;

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
    char const* ParsePGNTags(char const* pgn, std::vector<PGNTag>& tags) {
        assert(pgn);

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
                tags.push_back(std::move(tag));

            SkipTo(pgn, kPGNTagEnd);
            SkipWhite(pgn);

            if (*pgn != kPGNTagBegin[0] || *pgn == '\0')
                parsingTags = false;
        }

        return pgn;
    }

    // returns the element, or -1 if it didn't find it
    int FindElement(std::string const& target, OrderedMoveList& source) {
        for (unsigned i = 0; i < source.bysan.size(); ++i)
            if (source.bysan[i].SAN() == target)
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

    E_gameTermination ProcessMoveSequence(Board& game, char const*& pgn, std::ostream& pgc) try {
        static Board gPreviousGamePos; // used in case their is something other than a
                                       // move sequence before a RAV
        static int gRAVLevels;         // used to finish putting RAVEnd markers, and to detect
                                       // RAV underflow

        enum E_reasonToEndSequence { RAVBegin, RAVEnd, NAG, escape, other } reasonToBreak = other;
        E_gameTermination gameResult                                                      = none;

        int8_t      NAGVal = {};
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
                    NAGVal              = atoi(token.c_str() + 1);
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
        }();

        // Process Moves
        //!!? Only need to indicate zero moves if the game is empty and not using
        //! Begin and end game data markers (i.e. using kMarkerBeginGameReduced)
        if (moves.size()) {
            if (moves.size() <= UCHAR_MAX)
                pgc << kMarkerShortMoveSequence << (int8_t)moves.size();
            else {
                std::array<char, 2> moveSize;
                gToLittleEndian(moves.size(), moveSize);
                pgc << kMarkerLongMoveSequence << moveSize[0] << moveSize[1];
            }

            for (size_t i = 0; auto& mv : moves) {
                OrderedMoveList legal = game.genLegalMoveSet();

                auto [cm, san] = gTimers["resolveSAN & toSAN"].timed([&] {
                    auto cm = game.resolveSAN(mv, legal.list);
                    return std::tuple(cm, game.toSAN(cm, legal.list));
                });

                assert(FindElement(san, legal) != -1);

                pgc << (int8_t)FindElement(san, legal);

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

                TIMED(game.processMove(cm));

                ++i;
            }

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
                std::array<char, 2> escapeLength;
                gToLittleEndian(escapeToken.length(), escapeLength);
                pgc << kMarkerEscape << escapeLength[0] << escapeLength[1] << escapeToken;
                break;
            case RAVBegin: break;
            default: break;
        }

        if (gRAVLevels < 0)
            throw RAVUnderflow;

        if (gameResult != none && gRAVLevels)
            while (gRAVLevels) {
                pgc << kMarkerRAVEnd;
                --gRAVLevels;
            }

        return gameResult;
    } catch (MoveError const& me) {
        std::cout << "\nIllegal move: " << me.what() << std::endl;
        game.display();
        return illegalMove; // move is not legal
    } catch (E_gameTermination e) {
        return e;
    }

    // convert game from .pgn format to .pgc format
    // returns true if game is valid and succeeded, false otherwise
    // sets endOfGame to the place in pgn where the game stopped being processed
    E_gameTermination PgnToPgc(char const* pgn, char const*& endOfGame, std::ostream& pgc) {
        assert(pgn);
        std::vector<PGNTag> tags;
        endOfGame = pgn;
        pgn       = ParsePGNTags(pgn, tags);

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
        for (int i = 0; i < int(sizeof(sevenTagRoster) / sizeof(sevenTagRoster[0])); ++i) {
            bool foundTag = false;
            for (int j = 0; j < int(tags.size()); ++j) {
                if (tags[j].name == sevenTagRoster[i]) {
                    assert(tags[j].value.length() <= UCHAR_MAX); //??! Need to deal with this
                    pgc << (int8_t)tags[j].value.length() << tags[j].value;
                    tags.erase(tags.begin() + j);
                    foundTag = true;
                    break;
                }
            }
            if (!foundTag) {
                switch (i) {
                    case 0:
                    case 1:
                    case 3:
                    case 4:
                    case 5: pgc << (int8_t)1 << '?'; break;
                    case 2: pgc << (int8_t)10 << "????.??.??"; break;
                    case 6: pgc << (int8_t)1 << '*'; break;
                    default: assert(0); // not Reached
                }
            }
        }
        // any remaining tags
        //??! Case information is lost when parsing tags
        for (int i = 0; i < int(tags.size()); ++i) {
            assert(tags[i].name.length() < UCHAR_MAX); //??! Need to deal with this
            pgc << kMarkerTagPair << (int8_t)tags[i].name.length() << tags[i].name
                << (int8_t)tags[i].value.length() << tags[i].value;

            if (tags[i].name == "FEN") // process FEN
                game.processFEN(tags[i].value.c_str());
        }

        E_gameTermination processGame = none;
        // Board          previousBoard = game;     // used for RAV

        while (processGame == none && *pgn != '\0') // whole game
        {
            processGame = ProcessMoveSequence(game, pgn, pgc);
        }
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

            char const*       endOfGame = 0;
            E_gameTermination result    = PgnToPgc(gameBuffer, endOfGame, pgcGame);

            switch (result) {
                case illegalMove: std::cout << "\n Illegal move."; break;
                case RAVUnderflow: std::cout << "\n RAV underflow."; break;
                case parsingError:
                    std::cout << "\n Parsing error (may be end-of-file).";
                    break;
                    // return gamesProcessed; //??! Needs fixing .eof()
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
} // namespace

int main(int argc, char* argv[]) {
    // INITIALIZE
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
        return 2;
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

        return 2;
    }

    // open the input stream
    std::ifstream inputStream(inputFileName, std::ios::binary);

    // was the file opened successfully?
    if (!inputStream) {
        ReportFileError(E_openForInput, inputFileName);
        return 2;
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

        return 2;
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
        return 2;
    }

    // Let user know that what we are about to do
    std::cout << "\nConverting the PGN file " << inputFileName
              << "\n to PGC format and sending the output to file " << outputFileName << "";

    unsigned gameProcessed = TIMED(PgnToPgcDataBase(inputStream, outputStream));

    std::cout << "\n\nThere " << (gameProcessed == 1 ? "was" : "were") << " " << gameProcessed << " game"
              << (gameProcessed == 1 ? "" : "s") << " processed.";

    if (!outputStream.good()) {
        ReportFileError(E_output, outputFileName);
        return 2;
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
            return 2;
        }

        // rename temp file to old file
        fs::rename(outputFileName, inputFileName, ec);

        if (ec) // non-zero on failure
        {
            // print the appropriate message
            std::cerr << "Unable to rename the temporary file " + ec.message() << std::endl;
            return 2;
        }
    }

    // If we get to here than their were no file errors
    std::cout << "\n\nOperation was successful." << std::endl;
}
