This is the second beta release of pgn2pgc.

pgn2pgc.exe should be run in win9x at the DOS prompt.

Cleaned up the code a bit, and reworked some of the algorithms.

New code is an order of magnitude faster.  Further performance increase is possible, but not nearly to the same extent.  May be able to shave 30% off the time without much work.  After that a new design would be needed.

Fixed many bugs.  In particular:
Promotion to king is no longer allowed.
Problems in PgnToPgcDatabase() with iostreams were found and fixed.  (I think I got all of them.)

It is my hope that commercial and non-commercial pgn readers will incorporate .pgc format very soon.  It's been so long without an implementation.

Please direct any comments to Josh at joallen@trentu.ca.  I like to hear what ppl have to say about my code, whether positive or not.