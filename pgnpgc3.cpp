///////////////////////////////////////////////////////////////////////////////
//
// *** note on standard iostream error reporting: ***
//
//	bad() returns hardfail and badbit
//	fail() returns failbit, hardfail, and badbit
//	eof() returns eofbit
//	good() returns nonzero if no state bits are set (!fail() && !eof() )
//	operator void*() returns !fail() (used like if(some_stream) no_errors)
//	operator!() returns fail() (used like if(!some_stream) errors
//
///////////////////////////////////////////////////////////////////////////////

#include "stpwatch.h" // profiling
StopWatch gTimer[10];

#include <assert.h>
#include <cstring>
#include <ctype.h>
#include <filesystem>
#include <limits.h> // UCHAR_MAX
#include <sstream>
namespace fs = std::filesystem;

// .pgn to .pgc
#include "chess_2.h"
#include "strparse.h"
#include "joshdefs.h"
#include "pqueue_2.h"
#include "list5.h"

using pgcByteT = int8_t;  // one byte (two's complement)
using pgcWordT = int16_t; // two bytes (two's complement)
using pgcDoubleWordT = int32_t; // four bytes (two's complement)

// must be converted to little-endian
// Endian conversion code idea taken from "Obfuscated C and Other Mysteries" by Don Libes, 1993 pp. 121-3
//??! This code still needs to be tested on big or big-little endian machines
class ToLittleEndian
{
private:
	void (*fSwapFunc)(pgcWordT, char[]);
	static void byteSwap(pgcWordT w, char swaped[]) {
			assert(swaped);
			char* source = (char*)&w;
			swaped[0] = source[1];
			swaped[1] = source[0];
		}
	static void noSwap(pgcWordT w, char c[]) {
			assert(c);
			char* source = (char*)&w;
			c[0] = source[0];
			c[1] = source[1];
		}

public:
  ToLittleEndian() { // what endian is the machine we're using?
    const pgcWordT testValue = 0x0100;
    char *cp = (char *)&testValue;
    if (*cp == 0x01)
      fSwapFunc = &ToLittleEndian::byteSwap;
    else
      fSwapFunc = &ToLittleEndian::noSwap;
  }
  void operator()(pgcWordT w, char c[]) { fSwapFunc(w, c); }
};

ToLittleEndian gToLittleEndian;

//!!? possible expansion (not covered in PGN standard document):
//	support for comments
//	special markers for supplemantary tags, instead of kMarkerTagPair
// additional markers for strings that now only can have 255 length or only 2 byte length to have choice (like short and long move sequence)
// change result tag to one byte // remove length info as well
// remove date length info (it's always the same) and change to byte sequence (e.g. int-2 year int-2 month int-1 day)

#include <cstring>
#include "list5.h"

static const pgcByteT kMarkerBeginGameReduced = 0x01;
static const pgcByteT kMarkerTagPair = 0x02;
static const pgcByteT kMarkerShortMoveSequence = 0x03;
static const pgcByteT kMarkerLongMoveSequence = 0x04;
static const pgcByteT kMarkerGameDataBegin = 0x05;
static const pgcByteT kMarkerGameDataEnd = 0x06;
static const pgcByteT kMarkerSimpleNAG = 0x07;
static const pgcByteT kMarkerRAVBegin = 0x08;
static const pgcByteT kMarkerRAVEnd = 0x09;
static const pgcByteT kMarkerEscape = 0x0a;

const char* SkipWhite(const char* c)
{
	assert(c);
	while(isspace(*c) && *c != '\0')
		++c;
	return c;
}
void SkipWhite(const char** c)
{
	assert(c);
	*c = SkipWhite(*c);
}

//	skips over all chars till it finds any chars in target, then moves just past target
// "xxxx[xxx"
//       ^
const char* SkipTo(const char* c, const char target[])
{
	assert(c);
	assert(target);

	while(strcspn(c, target) && *c != '\0')
		++c;

	if(*c != '\0')
		++c;

	return c;
}

void SkipTo(const char** c, const char target[])
{
	assert(c);
	assert(target);
	*c = SkipTo(*c, target);
}

// skips over all chars in unwanted
const char* SkipChar(const char* c, const char unwanted[])
{
	assert(c);
	assert(unwanted);
	while(!strcspn(c, unwanted) && *c != '\0')
		++c;

	return c;
}

void SkipChar(const char** c, char unwanted[])
{
	assert(c);
	assert(unwanted);
	*c = SkipChar(*c, unwanted);
}

struct PGNTag {
		string name, value;
		};

// adds all of the tags to the list, and returns where the tags ended.
// tag names are converted to upper case
const char* ParsePGNTags(const char* pgn, List<PGNTag>* tags)
{
	assert(pgn);
	assert(tags);

	const char kPGNTagBegin[] = "[";
	const char kPGNTagEnd[] = "]";
	const char kPGNTagValueBegin[] = "\"";
	const char kPGNTagValueEnd = '\"';

	bool parsingTags = true;
	while(parsingTags)
	{
		SkipTo(&pgn, kPGNTagBegin);
		SkipWhite(&pgn);

		PGNTag tag;
		while(!isspace(*pgn) && *pgn != kPGNTagValueBegin[0] && *pgn != '\0')
		{
			tag.name.append((char)toupper(*pgn), 0, 1);
			++pgn;
		}

		SkipTo(&pgn, kPGNTagValueBegin);
		while(*pgn != kPGNTagValueEnd && *pgn != '\0')
		{
			tag.value.append(*pgn, 0, 1);
			++pgn;
		}
		if(*pgn != '\0')
			tags->add(tag);

		SkipTo(&pgn, kPGNTagEnd);
		SkipWhite(&pgn);

		if(*pgn != kPGNTagBegin[0] || *pgn == '\0')
			parsingTags = false;
	}

	return pgn;
}

// returns the element, or -1 if it didn't find it
int FindElement(const string& target, PriorityQueue<ChessMoveSAN>& source)
{
	for(int i = 0; i < source.size(); ++i)
		if(source[i].san() == target)
			return i;

	return -1;
}



//??! illegal moves will mess up the .pgc, changing will be non-trivial (e.g. illegal move just before RAVBegin)
//    solution was to not record games with illegal moves
// recusive
//!?? game termination must appear after all comments and escape sequences -- pgn standard is not clear in this regard
//!?? a RAV can have a format 1. e4 e5 (1...d5)(1...Nf6) even though the standard only specifies 1. e4 e5 (1...d5 (1...Nf6))
// this performs a lot of clean-up, e.g. move numbers are ignored
enum E_gameTermination {none, // still need to processing game
					parsingError, illegalMove, RAVUnderflow, unknown, whiteWin, blackWin, draw}; //?!! use later to determine if original STR Result is correct
E_gameTermination ProcessMoveSequence(Board& game, const char*& pgn, std::ostream& pgc)
{
	static Board gPreviousGamePos; // used in case their is something other than a move sequence before a RAV
	static int gRAVLevels;					// used to finish putting RAVEnd markers, and to detect RAV underflow

	enum E_reasonToEndSequence {RAVBegin, RAVEnd, NAG, escape, other} reasonToBreak = other;
	E_gameTermination gameResult = none;

	List<string> moves;
	bool processMoveSequence = true;
	pgcByteT NAGVal;
	string escapeToken;
	while(processMoveSequence) // move sequence
	{
		string token;
		SkipWhite(&pgn);
		if(*pgn == '[')
		{
			gameResult = unknown;
			//processMoveSequence = false; // redundant here
			break;
		}
		// get the next token
		while(!isspace(*pgn) && *pgn != '\0')
		{
				if(token.length() && *pgn == ')')
					break;

				if(*pgn == '!' || *pgn == '?') // NAG
					if(token.length())
					{
						break;
					} else
					{
						while(!isspace(*pgn) && *pgn != '\0' && (*pgn == '!' || *pgn == '?'))
						{
							token.append(*pgn, 0, 1);
							++pgn;
						}
						break;
					}
				token.append(*pgn, 0, 1);
//				cout << "\ncurToken: " << token;
				++pgn;
				assert(token.length());
				if(token == "(" || token == ")" || token.get_at(token.length() - 1) == '.')
					break;
		}

		if(token.is_null()) // no more tokens to process
		{
			processMoveSequence = false;
		} else if(isdigit(token.get_at(0))) // move# or end-of-game (1-0 1-0 1/2-1/2)
		{
			if(token == "1-0")
			{
				gameResult = whiteWin; // there can only be one game termination per game, cannot be in a RAV
				processMoveSequence = false;
			} else if(token == "0-1")
			{
					gameResult = blackWin;
					processMoveSequence = false;
			} else if(token == "1/2-1/2" || token == "1/2")
			{
				gameResult = draw;
				processMoveSequence = false;
			}
		} else if(token == "*")
		{
			gameResult = unknown;
			processMoveSequence = false;
		} else if(isalpha(token.get_at(0))) // SAN move
		{
			moves.add(token);
		} else if(token == "!") // NAVVal values from pgn standard sec. 10
		{
			reasonToBreak = NAG;
			NAGVal = 1;
			processMoveSequence = false;
		} else if(token == "?")
		{
			reasonToBreak = NAG;
			NAGVal = 2;
			processMoveSequence = false;
		} else if(token == "!!")
		{
			reasonToBreak = NAG;
			NAGVal = 3;
			processMoveSequence = false;
		} else if(token == "??")
		{
			reasonToBreak = NAG;
			NAGVal = 4;
			processMoveSequence = false;
		} else if(token == "!?")
		{
			reasonToBreak = NAG;
			NAGVal = 5;
			processMoveSequence = false;
		} else if(token == "?!")
		{
			reasonToBreak = NAG;
			NAGVal = 6;
			processMoveSequence = false;
		} else if(token.get_at(0) == '{') // multi-line comment
		{
			// .pgc doesn't allow for comments yet..
			SkipTo(&pgn, "}");
		} else if(token.get_at(0) == ';') // single-line comment
		{
			SkipTo(&pgn, "\n");
		} else if(token.get_at(0) == '(') // RAV
		{
			++gRAVLevels;
			reasonToBreak = RAVBegin;
			processMoveSequence = false;
		} else if(token.get_at(0) == ')') // RAV
		{
			--gRAVLevels;
			reasonToBreak = RAVEnd;
			processMoveSequence = false;
		} else if(token.get_at(0) == '$') // NAG
		{
			reasonToBreak = NAG;
			NAGVal = atoi(&token.c_str()[1]);
			processMoveSequence = false;
		} else if(token.get_at(0) == '.') // black to move (...)
		{
			// redundant
		} else if(token.get_at(0) == '[') // begin another game (without end-of-game marker) // taken care of up top
		{
			assert(0);
		} else if(token.get_at(0) == '%') // escape sequence
		{
			escapeToken = string(&token.c_str()[1]); //??! an escape sequence can have null, but this function uses char*, so it can't
			while(*pgn != '\n' && *pgn != '\0')
			{
				escapeToken.append(*pgn, 0, 1);
				pgn++;
			}
			reasonToBreak = escape;
			processMoveSequence = false;
		} else // error, everything in a valid PGN game should be taken care of
		{
			return parsingError;
		}

	}

	// Process Moves
	//!!? only need to indicate zero moves if the game is empty and not using begin and end game data markers (i.e. using kMarkerBeginGameReduced)
	if(moves.size())
	{
		if(moves.size() <= UCHAR_MAX)
			pgc << kMarkerShortMoveSequence << (pgcByteT)moves.size();
		else
		{
			char moveSize[2];
			gToLittleEndian(moves.size(), moveSize);
			pgc << kMarkerLongMoveSequence << moveSize[0] << moveSize[1];
		}

		gTimer[4].start();
		for(size_t i = 0; i < moves.size(); ++i)
		{
			PriorityQueue<ChessMoveSAN> SANMoves;
			List<ChessMove> allMoves;
			gTimer[2].start();
			game.genLegalMoveSet(&allMoves, &SANMoves); // 57%
			gTimer[2].stop();

/*/ debugBegin
//
		SANMoves.gotoFirst();
		string debug;
		int debugI = 0;
		cout << "\n";
		while(SANMoves.next(&debug))
			cout << debugI++ << " " << debug << "\t";
		SANMoves.gotoFirst();
//
*/// debugEnd
			gTimer[3].start();
			ChessMove cm;
			if(!game.algebraicToMove(&cm, moves[i].c_str(), allMoves))
			{
                std::cout << "\nillegal move: " << moves[i].c_str();
                std::cout << "\n";
				game.display();
				return illegalMove; // move is not legal
			}
			string san;

			game.moveToAlgebraic(&san, cm, allMoves);
			gTimer[3].stop();

			assert(FindElement(san, SANMoves) != -1);
/*			if(FindElement(san, SANMoves) == -1)
			{
				game.display();
				SANMoves.gotoFirst();
				ChessMoveSAN debug;
				int debugI = 0;
                std::cout << "\n";
				while(SANMoves.next(&debug))
                std::cout << debugI++ << " " << debug.san() << "\t";
				SANMoves.gotoFirst();

				TRACE(san);
			}
*/
//			std::cout << "\nMove: " << san << "\n";
//			game.display();

			pgc << (pgcByteT)FindElement(san, SANMoves);

			if(i == moves.size() - 1)
				if(reasonToBreak == RAVBegin)
				{
					pgc << kMarkerRAVBegin;
                    Board temp = game;
					gameResult = ProcessMoveSequence(temp, pgn, pgc);
				} else // SEHE FIXME hanging else
				{
					gPreviousGamePos = game; // their can't be two rav's at the same level for the same move, instead use 1. (1. (1.)) 1... not 1. (1.)(1.) 1... (pgn formal syntax)
				}

			gTimer[7].start();
			game.processMove(cm, allMoves);
			gTimer[7].stop();
		}
		gTimer[4].stop();

	} else if(reasonToBreak == RAVBegin) // e.g. in case their is a NAG in before the RAVBegin
	{
		pgc << kMarkerRAVBegin;
		gameResult = ProcessMoveSequence(gPreviousGamePos, pgn, pgc);
	}


	switch(reasonToBreak)
	{
		case RAVEnd: pgc << kMarkerRAVEnd; break;
		case NAG: if(moves.size()) pgc << kMarkerSimpleNAG << NAGVal; break; // you can only have a NAG if you have a move, and only one NAG per move (formal pgn syntax)
		case escape:
			char escapeLength[2];
			gToLittleEndian(escapeToken.length(), escapeLength);
			pgc << kMarkerEscape << escapeLength[0] << escapeLength[1] << escapeToken;
			break;
		case RAVBegin: break;
		default: break;
	}

	if(gRAVLevels < 0)
		return RAVUnderflow;

	if(gameResult != none && gRAVLevels)
		while(gRAVLevels)
		{
			pgc << kMarkerRAVEnd;
			--gRAVLevels;
		}

	return gameResult;
}

// convert game from .pgn format to .pgc format
// returns true if game is valid and succeeded, false otherwise
// sets endOfGame to the place in pgn where the game stopped being processed
E_gameTermination PgnToPgc(const char* pgn, const char** endOfGame, std::ostream& pgc)
{

	assert(pgn);
	assert(endOfGame);
	List<PGNTag> tags;
	*endOfGame = pgn;
	pgn = ParsePGNTags(pgn, &tags);

	Board game;

	if(!tags.size())
	{
		*endOfGame = pgn;
		return parsingError;
	}

	pgc << kMarkerGameDataBegin;
	// output the tags in the right order
	const char* sevenTagRoster[] = {"EVENT", "SITE", "DATE", "ROUND", "WHITE", "BLACK", "RESULT",};
	int i, j;
	for(i = 0; i < sizeof(sevenTagRoster)/sizeof(sevenTagRoster[0]); ++i)
	{
		bool foundTag = false;
		for(j = 0; j < tags.size() && !foundTag; ++j)
		{
			if(tags[j].name == sevenTagRoster[i])
			{
				assert(tags[j].value.length() <= UCHAR_MAX); //??! need to deal with this
				pgc << (pgcByteT)tags[j].value.length() << tags[j].value;
				tags.remove(j);
				foundTag = true;
			}
		}
		if(!foundTag)
		{
			switch(i)
			{
				case 0: case 1: case 3: case 4: case 5:
					pgc << (pgcByteT)1 << '?'; break;
				case 2:
					pgc << (pgcByteT)strlen("????.??.??") << "????.??.??"; break;
				case 6:
					pgc << (pgcByteT)1 << '*'; break;
				default: assert(0); // notReached
			}
		}
	}
	// any remaining tags
	//??! case information is lost when parsing tags
	for(i = 0; i < tags.size(); ++i)
	{
		assert(tags[i].name.length() < UCHAR_MAX); //??! need to deal with this
		pgc << kMarkerTagPair << (pgcByteT)tags[i].name.length() << tags[i].name
			<< (pgcByteT)tags[i].value.length() << tags[i].value;

		if(tags[i].name == "FEN") 	// process FEN
			game.processFEN(tags[i].value.c_str());
	}

	E_gameTermination processGame = none;
	Board previousBoard = game; // used for RAV
//	gTimer[3].start();
	while(processGame == none && *pgn != '\0') // whole game
	{
		processGame = ProcessMoveSequence(game, pgn, pgc);
	}
// gTimer[3].stop();
	pgc << kMarkerGameDataEnd;


	*endOfGame = pgn;
	return processGame;
}

// returns the number of games processed successfully
int PgnToPgcDataBase(std::istream &pgn, std::ostream &pgc) {
  const size_t kLargestGame =
      0x4000; // too large will impede performance due to memmove
  char *const gameBuffer = NewArray(char, kLargestGame + 1);
  CHECK_POINTER(gameBuffer);
  AdoptArray<char> gameAdopter(gameBuffer);
  char *gameBufferCurrent = gameBuffer;

  unsigned gamesProcessed = 0;

  std::cout << "\n"; // USER UPDATE

  auto oldPGNFlags = pgn.flags(); //!!? ios::skipws was set and causing problems
  pgn >> std::noskipws;
  unsigned totalGames = 0;
  while (!pgn.bad() && pgc.good()) {
    std::cout << '.'; // USER UPDATE
    //?!! should work, but it'll be interesting to know what happens when
    //endOfGame == gameBuffer and pgnGame has a buffer size of 0

    std::ostringstream pgnGame;
    std::ostringstream pgcGame;

    if (!pgn.eof()) //!!? clearing eofbit and then reading from file will set
                    //!badbit (illegal operation), but need to clear failbit
                    //!because it fails when pgnGame reaches eof()
      pgn.clear();

    pgn >> pgnGame.rdbuf();

    /*
                    if(pgn.bad())
                            TRACE(pgn.bad());

                    TRACE(pgn);
                    TRACE(!pgn);
                    TRACE(pgn.good());
                    TRACE(pgn.eof());
                    TRACE(pgn.fail());
                    TRACE(pgn.bad());
                    TRACE((int)*gameBuffer);
    */
    assert(pgnGame.tellp() <= kLargestGame - (gameBufferCurrent - gameBuffer));
    TRACE(pgnGame.tellp());
    gameBufferCurrent[pgnGame.tellp()] = '\0';

    const char *endOfGame = 0;
    //		gTimer[2].start();
    TRACE(++totalGames);
    E_gameTermination result = PgnToPgc(gameBuffer, &endOfGame, pgcGame);
    //		gTimer[2].stop();

    switch (result) {
    case illegalMove:
      std::cout << "\n Illegal move.";
      break;
    case RAVUnderflow:
      std::cout << "\n RAV underflow.";
      break;
    case parsingError:
      std::cout << "\n Parsing error (may be end-of-file).";
      break; // return gamesProcessed; //??! needs fixing .eof()
    default:
      pgc << pgcGame.rdbuf();
      ++gamesProcessed;
      break;
    }
    assert(endOfGame && endOfGame >= gameBuffer);
    memmove(gameBuffer, endOfGame, kLargestGame - (endOfGame - gameBuffer));
    gameBufferCurrent = gameBuffer + kLargestGame - (endOfGame - gameBuffer);

    if (!pgnGame || !pgcGame || !pgn.good() && *gameBuffer == '\0') {
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
#include <cctype> // isalnum()
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip> // stream manipulators (e.g. setw() )
#include <iostream>
#include <limits> // INT_MAX

// non-standard (may not be portable to some operating systems)

// the different kinds of file operations that can cause an error
enum FileOperationErrorT
	{
		E_openForInput,
		E_openForOutput,
		E_output,
		E_input,
		E_nameReserved,
		E_sameFile,
	};

// report a file error to the standard error stream
void ReportFileError(FileOperationErrorT operation, fs::path const &name);
// returns if the file name is reserved (e.g. "PRN" is reserved for the printer)
bool IsFileNameReserved(fs::path const &fileName);

int main(int argc, char *argv[]) {
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
    std::cout << "\nUsage: PGN2PGC [source_file [report_file]]\n";
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
  // use ios::binary so that we don't limit the programs possible uses
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

    // if (!findfirst(outputFileName.str(), &matchingFileInfo, 0)) {
    if (exists(outputFileName)) {
      // ask the user if the're sure they want to overwrite the file
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
  bool inputOutputSameFile =
      inputFileName.lexically_normal() == outputFileName.lexically_normal();
  inputOutputSameFile = true;
  outputFileName = fs::temp_directory_path() / "t_wcXXXXXX";

  // open the ouput stream
  // use ios::binary so that we don't limit the programs possible uses
  std::ofstream outputStream(outputFileName,
                             std::ios::trunc | std::ios::binary);

  // was the file opened successfully?
  if (!outputStream) {
    ReportFileError(E_openForOutput, outputFileName);
    return EXIT_FAILURE;
  }

  // Let user know that what we are about to do
  std::cout << "\nConverting the PGN file " << inputFileName
            << "\n to PGC format and sending the output to file '"
            << outputFileName << "'";

  //	gTimer[1].start();
  unsigned gameProcessed = PgnToPgcDataBase(inputStream, outputStream);
  //	gTimer[1].stop();

  std::cout << "\n\nThere " << (gameProcessed == 1 ? "was" : "were") << " "
            << gameProcessed << " game" << (gameProcessed == 1 ? "" : "s")
            << " processed.";

  if (!outputStream.good()) {
    ReportFileError(E_output, inputFileName);
    return EXIT_FAILURE;
  }

  if (inputOutputSameFile) {
    inputStream.close();
    outputStream.close();

    // delete oldfile
    int systemCallSuccess = remove(inputFileName);
    if (systemCallSuccess) // non-zero on failure
    {
      // print the appropiate message
      perror("\nUnable to delete old input file");
      return EXIT_FAILURE;
    }

    // rename temp file to old file
    std::error_code ec;
    fs::rename(outputFileName, inputFileName, ec);

    if (ec) // non-zero on failure
    {
      // print the appropiate message
      std::cerr << "Unable to rename the temporary file " + ec.message()
                << std::endl;
      return EXIT_FAILURE;
    }
  }

  // If we get to here than their were no file errors
  std::cout << "\n\nOperation was successful." << std::endl;

  gTimer[0].stop();

  for (int debugI = 0; debugI < sizeof(gTimer) / sizeof(gTimer[0]); ++debugI) {
    TRACE(debugI);
    TRACE(gTimer[debugI].time());
  }

  return EXIT_SUCCESS;
}

/// reports a file error to the standard error stream
/// @param operation the type of file operation that failed
/// @param name the name of the file
/// @return void
void ReportFileError(FileOperationErrorT operation, fs::path const& name) {
  std::cerr << std::endl;

  switch (operation) {
  case E_openForInput:
    std::cerr << "Error: Unable to open file " << name << " for input";
    break;
  case E_openForOutput:
    std::cerr << "Error: Unable to open file " << name << " for output";
    break;
  case E_output:
    std::cerr << "Error trying to output to file " << name;
    break;
  case E_input:
    std::cerr << "Error trying to input from file " << name;
    break;
  case E_nameReserved:
    std::cerr << "Error: file name " << name
              << " is system reserved. Please use another.";
    break;
  case E_sameFile:
    std::cerr << "Cannot use file " << name << " for both input and output.";
    break;

  default:
    assert(0); // NotReached
  }

  std::cerr << std::endl;
}

/// checks if the file name is reserved by the system
/// @param fileName the name of the file to check
/// @return true if the file name is reserved, false otherwise
bool IsFileNameReserved(fs::path const &fileName) {
  std::string const name = fileName.filename();

  // stricmp returns 0 if the strings are equal (case-insensitive)
  for (auto reservedName : {"PRN", "LPT1", "LPT2"})
    if (::strcasecmp(reservedName, name.c_str()) == 0)
      return true;

  return false;
}

#endif
