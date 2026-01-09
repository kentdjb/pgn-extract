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

#include <stdio.h>
#include <stdlib.h>

#include "bool.h"
#include "defs.h"
#include "typedef.h"
#include "tokens.h"
#include "taglist.h"
#include "lex.h"
#include "lines.h"
#include "lists.h"
#include "moves.h"
#include "output.h"
#include "taglines.h"

/* Read the list of extraction criteria from TagFile.
 * This doesn't use the normal lexical analyser before the
 * PGN files are processed but circumvents next_token by
 * calling get_tag() and get_string.
 * This allows it to detect EOF before yywrap() is called.
 * Be careful to leave lex in the right state.
 */
void
read_tag_file(const char *TagFile, Boolean positive_match)
{
    FILE *tagfile = fopen(TagFile, "r");
    if (tagfile != NULL) {
        Boolean keep_reading = TRUE;

        while (keep_reading) {
            char *line = next_input_line(tagfile);
            if (line != NULL) {
                keep_reading = process_tag_line(TagFile, line, positive_match);
            }
            else {
                keep_reading = FALSE;
            }
        }
        (void) fclose(tagfile);
#if 0
        tagfile = NULL;
        /* Call yywrap in order to set up for the next (first) input file. */
        (void) yywrap();
#endif
    }
    else {
        fprintf(GlobalState.logfile,
                "Unable to open %s for reading.\n", TagFile);
        exit(1);
    }
}

/* Read the contents of a file that lists the
 * required output ordering for tags.
 */
void
read_tag_roster_file(const char *RosterFile)
{
    Boolean keep_reading = TRUE;
    FILE *rosterfile = must_open_file(RosterFile, "r");

    while (keep_reading) {
        char *line = next_input_line(rosterfile);
        if (line != NULL) {
            keep_reading = process_roster_line(line);
        }
        else {
            keep_reading = FALSE;
        }
    }
    (void) fclose(rosterfile);
#if 0
    rosterfile = NULL;
    /* Call yywrap in order to set up for the next (first) input file. */
    (void) yywrap();
#endif
}

/* Extract a tag/value pair from the given line.
 * Return TRUE if this was successful.
 */
Boolean
process_tag_line(const char *TagFile, char *line, Boolean positive_match)
{
    Boolean keep_reading = TRUE;
    if (non_blank_line(line)) {
        unsigned char *linep = (unsigned char *) line;
        /* We should find a tag. */
        LinePair resulting_line = gather_tag(line, linep);
        TokenType tag_token;

        /* Pick up where we are now. */
        line = resulting_line.line;
        linep = resulting_line.linep;
        tag_token = resulting_line.token;
        if (tag_token != NO_TOKEN) {
            /* Pick up which tag it was. */
            int tag_index = yylval.tag_index;
            /* Allow for an optional operator. */
            TagOperator operator = NONE;

            /* Skip whitespace. */
            while (is_character_class(*linep, WHITESPACE)) {
                linep++;
            }
            /* Allow for an optional operator. */
            if (is_character_class(*linep, OPERATOR)) {
                switch (*linep) {
                    case '<':
                        linep++;
                        if (*linep == '=') {
                            linep++;
                            operator = LESS_THAN_OR_EQUAL_TO;
                        }
                        else if (*linep == '>') {
                            linep++;
                            operator = NOT_EQUAL_TO;
                        }
                        else {
                            operator = LESS_THAN;
                        }
                        break;
                    case '>':
                        linep++;
                        if (*linep == '=') {
                            linep++;
                            operator = GREATER_THAN_OR_EQUAL_TO;
                        }
                        else {
                            operator = GREATER_THAN;
                        }
                        break;
                    case '=':
                        linep++;
                        if (*linep == '~') {
                            operator = REGEX;
                            linep++;
                        }
                        else {
                            operator = EQUAL_TO;
                        }
                        break;
                    default:
                        fprintf(GlobalState.logfile,
                                "Internal error: unknown operator in %s\n", line);
                        linep++;
                        break;
                }
                /* Skip whitespace. */
                while (is_character_class(*linep, WHITESPACE)) {
                    linep++;
                }
            }

            if (is_character_class(*linep, DOUBLE_QUOTE)) {
                /* A string, as expected. */
                linep++;
                resulting_line = gather_string(line, linep);
                line = resulting_line.line;
                linep = resulting_line.linep;
                if (tag_token == TAG) {
                    /* Treat FEN and FENPattern tags as special cases.
                     * Use the position they represent to indicate
                     * a positional match.
                     */
                    if (tag_index == FEN_TAG) {
                        add_fen_positional_match(yylval.token_string);
                        (void) free((void *) yylval.token_string);
                    }
                    else if (tag_index == PSEUDO_FEN_PATTERN_TAG ||
                            tag_index == PSEUDO_FEN_PATTERN_I_TAG) {
                        /* Skip whitespace. */
                        while (is_character_class(*linep, WHITESPACE)) {
                            linep++;
                        }
                        const char *label;
                        if(*linep != '\0') {
                            /* Treat the remainder of the line as a label. */
                            label = (const char *) linep;
                        }
                        else {
                            label = NULL;
                        }
                        /* Generate an inverted version as well if
                         * it is PSEUDO_FEN_PATTERN_I_TAG.
                         */
                        add_fen_pattern_match(yylval.token_string, 
                                tag_index == PSEUDO_FEN_PATTERN_I_TAG, label);
                        (void) free((void *) yylval.token_string);
                    }
                    else {
                        if(positive_match) {
                            add_tag_to_positive_list(tag_index, yylval.token_string, operator);
                        }
                        else {
                            add_tag_to_negative_list(tag_index, yylval.token_string, operator);
                        }
                        (void) free((void *) yylval.token_string);
                    }
                }
                else {
                    if (!GlobalState.skipping_current_game) {
                        fprintf(GlobalState.logfile,
                                "File %s: unrecognised tag name %s\n",
                                TagFile, line);
                    }
                    (void) free((void *) yylval.token_string);
                }
            }
            else {
                if (!GlobalState.skipping_current_game) {
                    fprintf(GlobalState.logfile,
                            "File %s: missing quoted tag string in %s at %s\n",
                            TagFile, line, linep);
                }
            }
        }
        else {
            /* Terminate the reading, as we have run out of tags. */
            keep_reading = FALSE;
        }
    }
    return keep_reading;
}

/* Extract a tag name from the given line.
 * Return TRUE if this was successful.
 */
Boolean
process_roster_line(char *line)
{
    Boolean keep_reading = TRUE;
    if (non_blank_line(line)) {
        unsigned char *linep = (unsigned char *) line;
        /* We should find a tag. */
        LinePair resulting_line = gather_tag(line, linep);
        TokenType tag_token;

        /* Pick up where we are now. */
        line = resulting_line.line;
        linep = resulting_line.linep;
        tag_token = resulting_line.token;
        if (tag_token != NO_TOKEN) {
            /* Pick up which tag it was. */
            int tag_index = yylval.tag_index;
            add_to_output_tag_order((TagName) tag_index);
        }
        else {
            /* Terminate the reading, as we have run out of tags. */
            keep_reading = FALSE;
        }
    }
    return keep_reading;
}
