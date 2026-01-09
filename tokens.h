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

        /* Define a type for lexical classification of tokens.
         * Not all of these values are returned to the parser.
         */
#ifndef TOKENS_H
#define TOKENS_H

typedef enum {
    /* The first section of tokens contains those that are
     * returned to the parser as complete token identifications.
     */
    EOF_TOKEN, TAG, STRING, COMMENT, NAG,
    CHECK_SYMBOL, MOVE_NUMBER, RAV_START, RAV_END,
    MOVE, TERMINATING_RESULT,
    /* The remaining tokens are those that are used to
     * perform the identification.  They are not handled by
     * the parser.
     */
    WHITESPACE, TAG_START, TAG_END, DOUBLE_QUOTE,
    COMMENT_START, COMMENT_END, ANNOTATE,
    DOT, PERCENT, SEMICOLON, ESCAPE, ALPHA, DIGIT,
    STAR, DASH, SLASH, EOS, OPERATOR, NO_TOKEN, ERROR_TOKEN
} TokenType;

typedef union {
    /* This string is used to retain tag and result information. */
    char *token_string;
    /* Move information. */
    Move *move_details;
    unsigned move_number;
    StringList *nags;
    Variation *variation_details;
    CommentList *comment;
    /* An index into the Game_Header.Tags array for tag strings. */
    unsigned tag_index;
} YYSTYPE;

extern YYSTYPE yylval;

#endif	// TOKENS_H

