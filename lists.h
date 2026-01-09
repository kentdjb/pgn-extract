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

        /* Define values for the amount of space to initially malloc
         * and incrementally realloc in a list.
         */
#ifndef LISTS_H
#define LISTS_H

#define INIT_LIST_SPACE 10
#define MORE_LIST_SPACE 5

        /* Tags to be sought may have an operator to specify the
         * relationship between value in the tag list and that in
         * the game. For instance, in order to find games before 1962
         * use Date < "1962". The < turns into a LESS_THAN operator.
         * Potentially any tag may have an operator, but not all make
         * sense in all circumstances.
         */
typedef enum {
    NONE,
    LESS_THAN, GREATER_THAN, EQUAL_TO, NOT_EQUAL_TO,
    LESS_THAN_OR_EQUAL_TO, GREATER_THAN_OR_EQUAL_TO,
    REGEX
} TagOperator;

void add_tag_to_negative_list(int tag, const char *tagstr, TagOperator operator);
void add_tag_to_positive_list(int tag, const char *tagstr, TagOperator operator);
Boolean check_setup_tag(char *Details[]);
Boolean check_ECO_tag(char *Details[], Boolean positive_match);
Boolean check_tag_details_not_ECO(char *Details[],int num_details, Boolean positive_match);
void extract_tag_argument(const char *argstr, Boolean positive_match);
void init_tag_lists(void);

#endif	// LISTS_H

