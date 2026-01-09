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
#include <string.h>
#include <ctype.h>
#include "bool.h"
#include "mymalloc.h"
#include "defs.h"
#include "typedef.h"
#include "map.h"
#include "tokens.h"
#include "taglist.h"
#include "lex.h"
#include "eco.h"
#include "apply.h"

/* Place a limit on how distant a position may be from the ECO line
 * it purports to match. This is to try to stop collisions way past
 * where the line could still be active.
 */
#define ECO_HALF_MOVE_LIMIT 6
/* Keep track of the longest ECO line, in half moves, plus
 * ECO_HALF_MOVE_LIMIT.
 * If a line exceeds this length, don't bother attempting
 * a match.
 */
static unsigned maximum_half_moves = ECO_HALF_MOVE_LIMIT;

/* Define a table to hold hash values of the ECO positions.
 * This is used to enable duplicate detection.
 */
#define ECO_TABLE_SIZE 4096
static EcoLog **EcoTable;

#if INCLUDE_UNUSED_FUNCTIONS

static void
dumpEcoTable(void)
{
    unsigned ix;
    for (ix = 0; ix < ECO_TABLE_SIZE; ix++) {
        if (EcoTable[ix] != NULL) {
            EcoLog *entry = NULL;
            for (entry = EcoTable[ix]; entry != NULL; entry = entry->next) {
                fprintf(stderr, "%s %lu %lu ", entry->ECO_tag,
                        entry->required_hash_value,
                        entry->cumulative_hash_value);
            }
            fprintf(stderr, "\n");
        }
    }
}

/* Return at how many points this match works.
 * 
 * This is a heuristic attempt to permit later
 * longer matches to be chosen in preference to
 * earlier shorter matches, while avoiding the
 * greater probability of false matches when there
 * are a lot of ECO lines and we are further into
 * a game.
 */
static int
eco_match_level(EcoLog *entry, HashCode current_hash_value,
        HashCode cumulative_hash_value, unsigned half_moves_played)
{
    int level = 0;
    if (entry != NULL) {
        if (entry->required_hash_value == current_hash_value) {
            level++;
            if (entry->cumulative_hash_value == cumulative_hash_value) {
                level++;
                if (entry->half_moves == half_moves_played) {
                    level++;
                }
            }
        }
    }
    return level;
}

/* Quality values for aspects of an ECO match.
 * Currently unused.
 */
static int ECO_REQUIRED_HASH_VALUE = 1;
static int ECO_HALF_MOVE_VALUE = 1;
static int ECO_CUMULATIVE_HASH_VALUE = 0;

/* Rate the quality of the given match.
 * Currently unused.
 */
static int eco_match_quality(EcoLog* entry,
        HashCode current_hash_value,
        HashCode cumulative_hash_value,
        int half_moves_played)
{
    int quality = 0;
    if (entry->required_hash_value == current_hash_value) {
        quality += ECO_REQUIRED_HASH_VALUE;
        if (abs(half_moves_played - entry->half_moves) <= ECO_HALF_MOVE_LIMIT) {
            quality += ECO_HALF_MOVE_VALUE;
        }
        if (entry->cumulative_hash_value == cumulative_hash_value) {
            quality += ECO_CUMULATIVE_HASH_VALUE;
        }
    }
    return quality;
}
#endif

void initEcoTable(void)
{
    /* Avoid multiple calls. */
    if (EcoTable == NULL) {
        int i;
        EcoTable = (EcoLog **) malloc_or_die(ECO_TABLE_SIZE * sizeof (EcoLog *));

        for (i = 0; i < ECO_TABLE_SIZE; i++) {
            EcoTable[i] = NULL;
        }
    }
}

/* Enter the ECO details of game into EcoTable.
 */
void
save_eco_details(const Game *game_details, const Board *final_position, unsigned number_of_half_moves)
{
    unsigned ix = game_details->final_hash_value % ECO_TABLE_SIZE;
    EcoLog *entry = NULL;
    /* Assume that it can be saved: that there is no collision. */
    Boolean can_save = TRUE;
    /* In an effort to save string space, keep a record of the
     * last entry stored, because there is a good chance that it
     * will have the same ECO_tag and Opening_tag as the next
     * one.
     */
    static EcoLog *last_entry = NULL;

    for (entry = EcoTable[ix]; (entry != NULL) && can_save; entry = entry->next) {
        if ((entry->required_hash_value == game_details->final_hash_value) &&
                (entry->half_moves == number_of_half_moves) &&
                (entry->cumulative_hash_value == game_details->cumulative_hash_value)) {
            const char *tag = entry->ECO_tag,
                    *opening = entry->Opening_tag,
                    *variation = entry->Variation_tag;
            if (tag == NULL) {
                tag = "";
            }
            if (opening == NULL) {
                opening = "";
            }
            if (variation == NULL) {
                variation = "";
            }
            fprintf(GlobalState.logfile, "ECO hash collision of ");
            fprintf(GlobalState.logfile, "%s %s %s", tag, opening, variation);
            fprintf(GlobalState.logfile, " against ");
            tag = game_details->tags[ECO_TAG];
            opening = game_details->tags[OPENING_TAG];
            variation = game_details->tags[VARIATION_TAG];
            if (tag == NULL) {
                tag = "";
            }
            if (opening == NULL) {
                opening = "";
            }
            if (variation == NULL) {
                variation = "";
            }
            fprintf(GlobalState.logfile, "%s %s %s\n", tag, opening, variation);
            fprintf(GlobalState.logfile, "Possible duplicate move sequences.\n");

            can_save = FALSE;
        }
    }

    if (can_save) {
        /* First occurrence, so add it to the log. */
        entry = (EcoLog *) malloc_or_die(sizeof (*entry));

        entry->required_hash_value = game_details->final_hash_value;
        entry->cumulative_hash_value = game_details->cumulative_hash_value;
        /* Keep a record of the current move number as a sanity
         * check on matches.
         */
        entry->half_moves = number_of_half_moves;
        /* Check for a new greater depth. */
        if (number_of_half_moves + ECO_HALF_MOVE_LIMIT > maximum_half_moves) {
            maximum_half_moves = number_of_half_moves + ECO_HALF_MOVE_LIMIT;
        }
        if (game_details->tags[ECO_TAG] != NULL) {
            if ((last_entry != NULL) && (last_entry->ECO_tag != NULL) &&
                    (strcmp(last_entry->ECO_tag, game_details->tags[ECO_TAG]) == 0)) {
                /* Share the last entry's tag. */
                entry->ECO_tag = last_entry->ECO_tag;
            }
            else {
                entry->ECO_tag = copy_string(game_details->tags[ECO_TAG]);
            }
        }
        else {
            entry->ECO_tag = NULL;
        }
        if (game_details->tags[OPENING_TAG] != NULL) {
            if ((last_entry != NULL) && (last_entry->Opening_tag != NULL) &&
                    (strcmp(last_entry->Opening_tag,
                    game_details->tags[OPENING_TAG]) == 0)) {
                /* Share the last entry's tag. */
                entry->Opening_tag = last_entry->Opening_tag;
            }
            else {
                entry->Opening_tag = copy_string(game_details->tags[OPENING_TAG]);
            }
        }
        else {
            entry->Opening_tag = NULL;
        }
        if (game_details->tags[VARIATION_TAG] != NULL) {
            entry->Variation_tag = copy_string(game_details->tags[VARIATION_TAG]);
        }
        else {
            entry->Variation_tag = NULL;
        }
        if (game_details->tags[SUB_VARIATION_TAG] != NULL) {
            entry->Sub_Variation_tag =
                    copy_string(game_details->tags[SUB_VARIATION_TAG]);
        }
        else {
            entry->Sub_Variation_tag = NULL;
        }
        /* Link it into the head at this index. */
        entry->next = EcoTable[ix];
        EcoTable[ix] = entry;
        /* Keep this one for next time around. */
        last_entry = entry;
    }
}

/* Look in EcoTable for current_hash_value.
 * Use cumulative_hash_value to refine the match.
 * An exact match is preferable to a partial match.
 */
EcoLog *
eco_matches(const Board *board, HashCode cumulative_hash_value,
            unsigned half_moves_played)
{
    HashCode current_hash_value = board->weak_hash_value;
    EcoLog *possible = NULL;

    /* Don't bother trying if we are too far on in the game.  */
    if (half_moves_played <= maximum_half_moves) {
        /* Where to look. */
        unsigned ix = current_hash_value % ECO_TABLE_SIZE;
        EcoLog *entry;

        for (entry = EcoTable[ix]; entry != NULL; entry = entry->next) {
            if (entry->required_hash_value == current_hash_value) {
                /* See if we have a full match. */
                if (half_moves_played == entry->half_moves &&
                        entry->cumulative_hash_value == cumulative_hash_value) {
                    return entry;
                }
                else if ((half_moves_played - entry->half_moves) <=
                        ECO_HALF_MOVE_LIMIT) {
                    /* Retain this as a possible. */
                    possible = entry;
                }
                else {
                    /* Ignore it, as the lines are too distant. */
                }
            }
        }
    }
    return possible;
}

/* Depending upon the ECO_level and the eco string of the
 * current game, open the correctly named ECO file.
 */
FILE *
open_eco_output_file(EcoDivision ECO_level, const char *eco)
{ /* Allow space for the maximum number of
     * ECO digits plus a .pgn suffix.
     */
    static const char suffix[] = ".pgn";

    enum {
        MAXNAME = MAX_ECO_LEVEL + sizeof (suffix) - 1
    };
    static char filename[MAXNAME + 1];

    if ((eco == NULL) || !isalpha((int) *eco)) {
        strcpy(filename, "noeco.pgn");
    }
    else if (ECO_level == DONT_DIVIDE) {
        fprintf(GlobalState.logfile,
                "Internal error: ECO division in open_eco_output_file\n");
        strcpy(filename, "noeco");
    }
    else if (ECO_level == DONT_DIVIDE) {
        fprintf(GlobalState.logfile,
                "Internal error: ECO division in open_eco_output_file\n");
        strcpy(filename, "noeco");
    }
    else {
        strncpy(filename, eco, ECO_level);
        filename[ECO_level] = '\0';
        strcat(filename, suffix);
    }
    return must_open_file(filename, "a");
}
