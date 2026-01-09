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

#ifndef END_H
#define END_H

/* Define a type to represent classes of occurrance. */
typedef enum {
    EXACTLY, NUM_OR_MORE, NUM_OR_LESS,
    SAME_AS_OPPONENT, NOT_SAME_AS_OPPONENT,
    LESS_THAN_OPPONENT, MORE_THAN_OPPONENT,
    LESS_EQ_THAN_OPPONENT, MORE_EQ_THAN_OPPONENT
} Occurs;

/* Define a structure to hold details on the occurrances of
 * each of the pieces.
 */
typedef struct material_details {
    /* Whether the pieces are to be tried against
     * both colours.
     */
    Boolean both_colours;
    /* The number of each set of pieces. */
    int num_pieces[2][NUM_PIECE_VALUES];
    Occurs occurs[2][NUM_PIECE_VALUES];
    /* Numbers of general minor pieces. */
    int num_minor_pieces[2];
    Occurs minor_occurs[2];
    /* How long a given relationship must last to be recognised.
     * This value is in half moves.
     */
    unsigned move_depth;
    /* How long a match relationship has been matched.
     * This is always reset to zero on failure and incremented on
     * success. A full match is only returned when match_depth == move_depth.
     */
    unsigned match_depth[2];
    struct material_details *next;
} Material_details;

/* Character to separate a pattern from material constraints.
 * NB: This is used to add a material constraint to a FEN pattern.
 */
#define MATERIAL_CONSTRAINT ':'

Boolean build_endings(const char *infile, Boolean both_colours);
Boolean check_for_material_match(Game *game);
Boolean check_for_piece_count_match(Game *game);
Boolean constraint_material_match(Material_details *details_to_find, const Board *board);
void extract_pieces_from_board(int num_pieces[2][NUM_PIECE_VALUES], const Board *board);
Boolean insufficient_material(const Board *board);
Material_details *process_material_description(const char *line, Boolean both_colours, Boolean pattern_constraint);

#endif	// END_H

