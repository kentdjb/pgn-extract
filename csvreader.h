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
#ifndef CSVREADER_H
#define CSVREADER_H

/* Store CSV-style data. Each row is assumed to have the same
 * number of columns. Column and row counts are included in the
 * data.
 */

typedef struct {
    char ***rows;
    int *col_counts;
    size_t row_count;
} CSVData;

CSVData *read_csv(const char *filename);
void free_csv(CSVData *data);
#endif //CSVREADER_H
