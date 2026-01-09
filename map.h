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

#ifndef MAP_H
#define MAP_H

void init_hashtab(void);
Boolean determine_move_details(Colour colour,Move *move_details, Board *board);
HashCode hash_lookup(Col col, Rank rank, Piece piece, Colour colour);
void make_move(MoveClass class, Col from_col, Rank from_rank, Col to_col, Rank to_rank,
                Piece piece, Colour colour,Board *board);
CheckStatus king_is_in_check(const Board *board,Colour king_colour);
MovePair *find_pawn_moves(Col from_col, Rank from_rank, Col to_col,Rank to_rank,
                Colour colour, const Board *board);
MovePair *find_knight_moves(Col to_col,Rank to_rank, Colour colour, const Board *board);
MovePair *find_bishop_moves(Col to_col,Rank to_rank, Colour colour, const Board *board);
MovePair *find_rook_moves(Col to_col,Rank to_rank, Colour colour, const Board *board);
MovePair *find_queen_moves(Col to_col,Rank to_rank, Colour colour, const Board *board);
MovePair *find_king_moves(Col to_col,Rank to_rank, Colour colour, const Board *board);
MovePair *exclude_checks(Piece piece, Colour colour,MovePair *possibles,
                                const Board *board);
void free_move_pair_list(MovePair *move_list);
Boolean king_is_in_checkmate(Colour colour,Board *board);
Col find_castling_king_col(Colour colour, const Board *board);
Col find_castling_rook_col(Colour colour, const Board *board, MoveClass castling);
MovePair *find_all_moves(const Board *board, Colour colour);
Boolean at_least_one_move(const Board *board, Colour colour);

#endif	// MAP_H

