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

#ifndef GRAMMAR_H
#define GRAMMAR_H

int yyparse(SourceFileType file_type);
void free_string_list(StringList *list);
void init_game_header(void);
void increase_game_header_tags_length(unsigned new_length);
void report_details(FILE *outfp);
void append_comments_to_move(Move *move,CommentList *Comment);
/* The following function is used for linking list items together. */
StringList *save_string_list_item(StringList *list,const char *str);
void free_comment_list(CommentList *comment_list);

#endif	// GRAMMAR_H

