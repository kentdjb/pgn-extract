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

/* Classifications for the arguments allowed in an argsfile. */
#ifndef ARGSFILE_H
#define ARGSFILE_H

typedef enum {
    SEVEN_TAG_ROSTER_ARGUMENT = '7',
    GAMES_PER_FILE_ARGUMENT = '#',
    ALTERNATIVE_HELP_ARGUMENT = '?',
    LONG_FORM_ARGUMENT = '-',
    APPEND_TO_OUTPUT_FILE_ARGUMENT = 'a',
    MOVE_BOUNDS_ARGUMENT = 'b',
    CHECK_FILE_ARGUMENT = 'c',
    DUPLICATES_FILE_ARGUMENT = 'd',
    USE_ECO_FILE_ARGUMENT = 'e',
    FILE_OF_FILES_ARGUMENT = 'f',
    HELP_ARGUMENT = 'h',
    WRITE_TO_LOG_FILE_ARGUMENT = 'l',
    NON_MATCHING_GAMES_ARGUMENT = 'n',
    WRITE_TO_OUTPUT_FILE_ARGUMENT = 'o',
    PLY_BOUNDS_ARGUMENT = 'p',
    CHECK_ONLY_ARGUMENT = 'r',
    KEEP_SILENT_ARGUMENT = 's',
    TAGS_ARGUMENT = 't',
    MOVES_ARGUMENT = 'v',
    LINE_WIDTH_ARGUMENT = 'w',
    POSITIONS_ARGUMENT = 'x',
    ENDINGS_COLOURED_ARGUMENT = 'y',
    ENDINGS_ARGUMENT = 'z',
    FILE_OF_ARGUMENTS_ARGUMENT = 'A',
    DONT_KEEP_COMMENTS_ARGUMENT = 'C',
    DONT_KEEP_DUPLICATES_ARGUMENT = 'D',
    ECO_OUTPUT_LEVEL_ARGUMENT = 'E',
    OUTPUT_FEN_STRING_ARGUMENT = 'F',
    HASHCODE_MATCH_ARGUMENT = 'H',
    APPEND_TO_LOG_FILE_ARGUMENT = 'L',
    MATCH_CHECKMATE_ARGUMENT = 'M',
    DONT_KEEP_NAGS_ARGUMENT = 'N',
    DONT_MATCH_PERMUTATIONS_ARGUMENT = 'P',
    TAG_ROSTER_ARGUMENT = 'R',
    USE_SOUNDEX_ARGUMENT = 'S',
    TAG_EXTRACTION_ARGUMENT = 'T',
    SUPPRESS_ORIGINALS_ARGUMENT = 'U',
    DONT_KEEP_VARIATIONS_ARGUMENT = 'V',
    OUTPUT_FORMAT_ARGUMENT = 'W',
    USE_VIRTUAL_HASH_TABLE_ARGUMENT = 'Z',
    NO_ARGUMENT_MATCH = '\0'        /* No argument match. */
} ArgType;

void process_argument(char arg_letter,const char *associated_value);
int process_long_form_argument(const char *argument, const char *associated_value);

#endif	// ARGSFILE_H

