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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "csvreader.h"
#include "mymalloc.h"
#include "bool.h"
#include "defs.h"
#include "typedef.h"


/* Read a tab-separated file of data. Each row is assumed to have the same
 * number of columns. Column and row counts are included in the data returned.
 */

char *read_line(FILE *fpin);

/* Split a line into fields */
static char **split_line(char *line, int *count) {
    const char *separator = "\t";
    char **fields = NULL;
    int capacity = 10;
    *count = 0;

    fields = malloc_or_die(capacity * sizeof(char*));

    char *token = strtok(line, separator);
    while (token) {
        if (*count >= capacity) {
            capacity *= 2;
            fields = realloc_or_die(fields, capacity * sizeof(char*));
        }
        fields[*count] = copy_string(token);
        (*count)++;
        token = strtok(NULL, separator);
    }
    return fields;
}


/* Read CSV file */
CSVData *read_csv(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(GlobalState.logfile, "Failed to open %s\n", filename);
        return NULL;
    }

    CSVData *data = malloc_or_die(sizeof(CSVData));
    data->rows = NULL;
    data->col_counts = NULL;
    data->row_count = 0;

    size_t capacity = 10;
    data->rows = malloc_or_die(capacity * sizeof(char **));
    data->col_counts = malloc_or_die(capacity * sizeof(int));

    char *line;
    while ((line = read_line(fp)) != NULL) {
        if (data->row_count >= capacity) {
            capacity *= 2;
            data->rows = realloc_or_die(data->rows, capacity * sizeof(char **));
            data->col_counts = realloc_or_die(data->col_counts, capacity * sizeof(int));
        }

        int col_count;
        char *line_copy = copy_string(line);
        char **fields = split_line(line_copy, &col_count);

        data->rows[data->row_count] = fields;
        data->col_counts[data->row_count] = col_count;
        data->row_count++;
        free(line_copy);
    }

    fclose(fp);
    return data;
}


/* Free CSV memory */
void free_csv(CSVData *data) {
    for (size_t i = 0; i < data->row_count; i++) {
        for (int j = 0; j < data->col_counts[i]; j++) {
            free(data->rows[i][j]);
        }
        free(data->rows[i]);
    }

    free(data->rows);
    free(data->col_counts);
    free(data);
}
