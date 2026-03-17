/*
*  Program: pgn-extract: a Portable Game Notation (PGN) extractor.
 *  Copyright (C) 1994-2026 David Barnes
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 1, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  David Barnes may be contacted as D.J.Barnes@kent.ac.uk
 *  https://www.cs.kent.ac.uk/people/staff/djb/
 *
 */

#ifndef HASH_TABLE_H
#define HASH_TABLE_H


/* What sort of player information is being entered into the hash table. */
typedef enum {PLAYER_ID, PLAYER_RATING}  player_info;

const char *get_player_info(const char *player_name, player_info info_type);
Boolean read_player_info_file(const char *filename, player_info info_type);

#endif

