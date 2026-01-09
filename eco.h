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

        /* Define a type to hold hash values of interest. */
#ifndef ECO_H
#define ECO_H

typedef struct EcoLog {
    HashCode required_hash_value;
    /* cumulative_hash_value is used to disambiguate clashing
     * final hash values in duplicate detection.
     */
    HashCode cumulative_hash_value;
    /* How deep the line is, from the half_moves associated with
     * the board when the line is played out.
     */
    unsigned half_moves;
    const char *ECO_tag;
    const char *Opening_tag;
    const char *Variation_tag;
    const char *Sub_Variation_tag;
    struct EcoLog *next;
} EcoLog;

EcoLog *eco_matches(const Board *board, HashCode cumulative_hash_value,
                    unsigned half_moves_played);
Boolean add_ECO(Game game_details);
FILE *open_eco_output_file(EcoDivision ECO_level,const char *eco);
void initEcoTable(void);
void save_eco_details(const Game *game_details, const Board *final_position, unsigned number_of_moves);

#endif	// ECO_H

