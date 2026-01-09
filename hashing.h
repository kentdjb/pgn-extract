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

        /* Define a type to hold hash values of interest.
         * This is used both to aid in duplicate detection
         * and in finding positional variations.
         */
#ifndef HASHING_H
#define HASHING_H

typedef struct HashLog {
    /* Store both the final position hash value and
     * the cumulative hash value for a game.
     */
    HashCode final_hash_value;
    HashCode cumulative_hash_value;
    /* Record the file list index for the file this game was first found in. */
    unsigned file_number;
    struct HashLog *next;
} HashLog;

/*
 * A structure for counting the number of times a position arises
 * in a game.
 */
typedef struct PositionCount {
    HashCode hash_value;
    Colour to_move;
    unsigned short castling_rights;
    Rank ep_rank;
    Col ep_col;
    unsigned count;
    struct PositionCount *next;
} PositionCount;


Boolean check_duplicate_setup(const Game *game_details);
Boolean check_for_only_repetition(PositionCount *position_counts);
void clear_duplicate_hash_table(void);
PositionCount *copy_position_count_list(PositionCount *original);
void free_position_count_list(PositionCount *position_counts);
void init_duplicate_hash_table(void);
PositionCount *new_position_count_list(const Board *board);
const char *previous_occurance(Game game_details, unsigned plycount);
unsigned update_position_counts(PositionCount *position_counts, const Board *board);

#endif	// HASHING_H

