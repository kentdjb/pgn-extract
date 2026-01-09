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

        /* Define a type to make it easy to return where we are
         * in the input line.  This type is returned from those functions
         * that may modify the line/linep pair in next_token().
         */
#ifndef LEX_H
#define LEX_H

typedef struct{
    /* Start of the malloc'ed line. */
    char *line;
    /* The next char to access in line, if line is not NULL. */
    unsigned char *linep;
    /* What token resulted. */
    TokenType token;
} LinePair;

/* Define a type to hold a list of input files. */
typedef struct {
    /* The list should have a (char *)NULL on the end. */
    const char **files;
    SourceFileType *file_type;
    /* Keep track of the number of files in the list. */
    unsigned num_files;
    /* Keep track of how many the list may hold. */
    unsigned max_files;
} FILE_LIST;

#if 1
#define RUSSIAN_KNIGHT_OR_KING (0x008a)
#define RUSSIAN_KING_SECOND_LETTER (0x00e0)
#define RUSSIAN_QUEEN (0x0094)
#define RUSSIAN_ROOK (0x008b)
#define RUSSIAN_BISHOP (0x0091)
#define RUSSIAN_PAWN (0x00af)
#else
#define RUSSIAN_KNIGHT_OR_KING (0x00eb)
#define RUSSIAN_KING_SECOND_LETTER (0x00d2)
#define RUSSIAN_QUEEN (0x00e6)
#define RUSSIAN_ROOK (0x00ec)
#define RUSSIAN_BISHOP (0x00f3)
#endif
/* Define a macro for checking the value of a char against
 * one of the Russian piece letters.
 */
#define RUSSIAN_PIECE_CHECK(c) ((c) & 0x00ff)

/* Shared prototypes for the non-static functions in the
 * lexical analyser.
 */
void add_filename_to_source_list(const char *filename,SourceFileType file_type);
void add_filename_list_from_file(FILE *fp,SourceFileType file_type);
unsigned current_file_number(void);
void free_move_list(Move *move_list);
LinePair gather_tag(char *line, unsigned char *linep);
LinePair gather_string(char *line, unsigned char *linep);
void init_lex_tables(void);
const char *input_file_name(unsigned file_number);
unsigned long get_line_number(void);
Boolean is_character_class(unsigned char ch, TokenType character_class);
Boolean is_suppressed_tag(TagName tag);
char *next_input_line(FILE *fp);
TokenType next_token(void);
Boolean open_eco_file(const char *eco_file);
Boolean open_first_file(void);
void print_error_context(FILE *fp);
char *read_line(FILE *fpin);
void reset_line_number(void);
void restart_lex_for_new_game(void);
void save_assessment(const char *assess);
TokenType skip_to_next_game(TokenType token);
void suppress_tag(const char *tag_string);
const char *tag_header_string(TagName tag);
void yyerror(const char *s);
int yywrap(void);

#endif	// LEX_H

