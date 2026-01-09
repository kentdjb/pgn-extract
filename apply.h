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

#ifndef APPLY_H
#define APPLY_H

void add_fen_castling(Game *game_details, Board *board);
Boolean apply_move_list(Game *game_details,unsigned *plycount, unsigned max_depth, Boolean check_for_a_match);
Boolean apply_move(Move *move_details, Board *board);
Board *apply_eco_move_list(Game *game_details,unsigned *number_of_half_moves);
void build_basic_EPD_string(const Board *board,char *fen);
char coloured_piece_to_SAN_letter(Piece coloured_piece);
Piece convert_FEN_char_to_piece(char c);
CommentList *create_match_comment(const Board *board);
Boolean ep_is_redundant(const Board *board);
void free_board(Board *board);
char *get_FEN_string(const Board *board);
Board *new_fen_board(const char *fen);
Board *new_game_board(const char *fen);
const char *piece_str(Piece piece);
Board *rewrite_game(Game *game_details);
char SAN_piece_letter(Piece piece);
Boolean save_polyglot_hashcode(const char *value);
/* letters should contain a string of the form: "PNBRQK" */
void set_output_piece_characters(const char *letters);
void store_hash_value(Move *move_details,const char *fen);

#endif	// APPLY_H

