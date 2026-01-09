/*
 *  This file is part of pgn-extract: a Portable Game Notation (PGN) extractor.
 *  Copyright (C) 1994-2026 David J. Barnes
 *
 *  pgn-extract is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  pgn-extract is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with pgn-extract. If not, see <http://www.gnu.org/licenses/>.
 *
 *  David J. Barnes may be contacted as d.j.barnes@kent.ac.uk
 *  https://www.cs.kent.ac.uk/people/staff/djb/
 */

/* These colour values are used as modifiers of the Piece values to
 * produce pieces of the appropriate colours.
 * A coloured piece is formed by shifting the piece value and setting the
 * bottom bit to either 0 (BLACK) or 1 (WHITE).
 */
#ifndef DEFS_H
#define DEFS_H
#include <stdint.h>

typedef enum { BLACK, WHITE } Colour;
typedef enum {
    OFF, EMPTY,
    /* The order of these is important and used in several places.
     * In particular, several for-loops iterate from PAWN to KING.
     */
    PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING,
    /* Must be last. */
    NUM_PIECE_VALUES
} Piece;
/* Different classes of move determined by the lexical analyser. */
typedef enum { PAWN_MOVE, PAWN_MOVE_WITH_PROMOTION, ENPASSANT_PAWN_MOVE,
               PIECE_MOVE, KINGSIDE_CASTLE, QUEENSIDE_CASTLE,
	       NULL_MOVE,
               UNKNOWN_MOVE
             } MoveClass;

/* Whether who is to move matters for positional matches. */
typedef enum { WHITE_TO_MOVE, BLACK_TO_MOVE, EITHER_TO_MOVE } WhoseMove;

/* Types for algebraic rank and column. */
typedef char Rank;
typedef char Col;

/* Define the base characters for ranks and columns. */
#define RANKBASE '1'
#define COLBASE 'a'
#define FIRSTRANK (RANKBASE)
#define LASTRANK (RANKBASE+BOARDSIZE-1)
#define FIRSTCOL (COLBASE)
#define LASTCOL (COLBASE+BOARDSIZE-1)

/* Convert the given rank to the correct index into a board. */
#define RankConvert(rank) ((FIRSTRANK <= (rank)) && ((rank) <= LASTRANK)?\
                                        ((rank)-RANKBASE+HEDGE):0)
/* Convert the given column to the correct index into a board. */
#define ColConvert(col) ((FIRSTCOL <= (col)) && ((col) <= LASTCOL)?\
                                        ((col)-COLBASE+HEDGE):0)

/* Convert a board index back to Rank or Col form. */
#define ToRank(r) ((r)+RANKBASE-HEDGE)
#define ToCol(c) ((c)+COLBASE-HEDGE)
#define COLOUR_OFFSET(colour) (((colour) == WHITE)? 1 : -1)

#define BOARDSIZE 8
/* Define the size of a hedge around the board.
 * This should have a size of 2 to make calculation of Knight moves easier.
 */
#define HEDGE 2

/* Define a type for position hashing.
 * The original type for this is unsigned long.
 * @@@ At some point, it would be worth moving this to uint64_t to be
 * consistent with the polyglot/zobrist hashing function.
 */
typedef uint64_t HashCode;

typedef struct {
    Piece board[HEDGE+BOARDSIZE+HEDGE][HEDGE+BOARDSIZE+HEDGE];
    /* Who has the next move. */
    Colour to_move;
    /* The current move number. */
    unsigned move_number;
    /* Rook starting columns for the 4 castling options.
     * This accommodates Chess960.
     */
    Col WKingCastle, WQueenCastle;
    Col BKingCastle, BQueenCastle;
    /* Keep track of where the two kings are, to make check-detection
     * simple.
     */
    Col WKingCol; Rank WKingRank;
    Col BKingCol; Rank BKingRank;
    /* Is EnPassant capture possible?  If so then ep_rank and ep_col have
     * the square on which this can be made.
     */
    Boolean EnPassant;
    Rank ep_rank;
    Col ep_col;
    /* NB: @@@
     * This value is based on a relatively weak hashing approach
     * that really needs updating to properly use the Zobrist hash.
     */
    HashCode weak_hash_value;
    /* Provision for storing a Zobrist hash value. However,
     * this is only set if GlobalState.add_hashcode_comments.
     * At some point, it should supersede the weak_hash_value.
     */
    uint64_t zobrist;
    /* The half-move clock since the last pawn move or capture. */
    unsigned halfmove_clock;
} Board;

/* Define a type that can be used to create a list of possible source
 * squares for a move.
 */
typedef struct move_pair {
    Col from_col;
    Rank from_rank;
    Col to_col;
    Rank to_rank;
    struct move_pair *next;
} MovePair;
    
/* Conversion macros. */
#define PIECE_SHIFT 3
#define MAKE_COLOURED_PIECE(colour,piece) ((Piece) (((piece) << PIECE_SHIFT) | (colour)))
#define W(piece) MAKE_COLOURED_PIECE(WHITE,piece)
#define B(piece) MAKE_COLOURED_PIECE(BLACK,piece)
/* Conversion macro, from one colour to another. */
#define OPPOSITE_COLOUR(colour) (!(colour))
#define EXTRACT_COLOUR(coloured_piece) ((coloured_piece) & 0x01)
#define EXTRACT_PIECE(coloured_piece) ((coloured_piece) >> PIECE_SHIFT)

/* The string for internally representing the non-standard PGN
 * notation for null moves.
 */
#define NULL_MOVE_STRING ("--")


#endif	// DEFS_H

