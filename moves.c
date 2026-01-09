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

/***
 * These routines are concerned with gathering moves of
 * the various sorts of variations specified by the -v
 * and -x flags.  In the former case, there are also functions
 * for checking the moves of a game against the variation
 * lists that are wanted.  Checking of the variations specified
 * by the -x flag is handled elsewhere by apply_move_list().
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "bool.h"
#include "mymalloc.h"
#include "lines.h"
#include "defs.h"
#include "typedef.h"
#include "map.h"
#include "lists.h"
#include "tokens.h"
#include "taglist.h"
#include "lex.h"
#include "moves.h"
#include "apply.h"
#include "decode.h"
#include "fenmatcher.h"
#include "end.h"

/* Define a character that can be used in the variations file to
 * mean that we don't mind what move was played at this point.
 * So: 
 *        * b6
 * means look for all games in which Black playes 1...b6, regardless
 * of White's first move.
 */
#define ANY_MOVE '*'
/* Define a character that can be used in the variations file to
 * mean that we do not wish to match a particular move.
 * So:
 *        e4 c5 !Nf3
 * means look for games in which Black does not play 2. Nf3 against
 * the Sicilian defence.
 */
#define DISALLOWED_MOVE '!'

/* Hold details of a single move within a variation. */
typedef struct {
    /* Characters of the move.
     * Alternative notations for the same move are separated by
     * a non-move character, e.g.:
     *        cxd|cxd4|c5xd4
     * could all be alternatives for the same basic pawn capture.
     */
    char *move;
    /* If we are interested in matching the moves in any order,
     * then we need to record whether or not the current move has
     * been matched or not.
     */
    Boolean matched;
} variant_move;

/* Hold details of a single variation, with a pointer to
 * an alternative variation.
 */
typedef struct variation_list {
    /* The list of moves. */
    variant_move *moves;
    /* Keep a count of how many ANY_MOVE moves there are in the move
     * list for each colour.  If these are non-zero then one is used
     * when a match fails when looking for permutations.
     */
    unsigned num_white_any_moves;
    unsigned num_black_any_moves;
    /* Keep a count of how many DISALLOWED_MOVE moves there are in the move
     * list for each colour.  If these are non-zero then the game
     * must be searched for them when looking for permutations before
     * the full search is made.
     */
    unsigned num_white_disallowed_moves;
    unsigned num_black_disallowed_moves;
    /* How many half-moves in the variation? */
    unsigned length;
    struct variation_list *next;
} variation_list;

/* The head of the variations-of-interest list. */
static variation_list *games_to_keep = NULL;

static Boolean is_insufficient_material(const Board *board);
static Boolean textual_variation_match(const char *variation_move,
        const unsigned char *actual_move);

/*** Functions concerned with reading details of the variations
 *** of interest.
 ***/

/* Remove any move number prefix from str.
 * Return NULL if there is no move (only number)
 * otherwise return the head of the move portion.
 */
static char *
strip_move_number(char *str)
{
    while (isdigit((int) *str)) {
        str++;
    }
    while (*str == '.') {
        str++;
    }
    if (*str != '\0') {
        return str;
    }
    else {
        return (char *) NULL;
    }
}



/* Define values for malloc/realloc allocation. */
#define INIT_MOVE_NUMBER 10
#define MOVE_INCREMENT 5

/* Break up a single line of moves into a list of moves
 * comprising a variation.
 */
static variation_list *
compose_variation(char *line)
{
    variation_list *variation;
    variant_move *move_list;
    /* Keep track of the number of moves extracted from line. */
    unsigned num_moves = 0;
    unsigned max_moves = 0;
    char *move;

    variation = (variation_list *) malloc_or_die(sizeof (variation_list));
    /* We don't yet know how many ANY_MOVEs or DISALLOWED_MOVES there
     * will be in this variation.
     */
    variation->num_white_any_moves = 0;
    variation->num_black_any_moves = 0;
    variation->num_white_disallowed_moves = 0;
    variation->num_black_disallowed_moves = 0;
    /* Allocate an initial number of pointers for the moves of the variation. */
    move_list = (variant_move *) malloc_or_die(INIT_MOVE_NUMBER *
            sizeof (*move_list));
    max_moves = INIT_MOVE_NUMBER;
    /* Find the first move. */
    move = strtok(line, " ");
    while (move != NULL) {
        if ((move = strip_move_number(move)) == NULL) {
            /* Only a move number. */
        }
        else {
            /* See if we need some more space. */
            if (num_moves == max_moves) {
                move_list = (variant_move *) realloc_or_die((void *) move_list,
                        (max_moves + MOVE_INCREMENT) * sizeof (*move_list));
                if (move_list == NULL) {
                    /* Catastrophic failure. */
                    free((void *) variation);
                    return NULL;
                }
                else {
                    max_moves += MOVE_INCREMENT;
                }
            }
            /* Keep the move and initialise the matched field for
             * when we start matching games.
             */
            move_list[num_moves].move = move;
            move_list[num_moves].matched = FALSE;
            /* Keep track of moves that will match anything. */
            if (*move == ANY_MOVE) {
                /* Odd numbered half-moves in the variant list are Black. */
                if (num_moves & 0x01) {
                    variation->num_black_any_moves++;
                }
                else {
                    variation->num_white_any_moves++;
                }
                /* Beware of the potential for false matches. */
                if (strlen(move) > 1) {
                    fprintf(GlobalState.logfile,
                            "Warning: %c in %s should not be followed by additional move text.\n",
                            *move, move);
                    fprintf(GlobalState.logfile, "It could give false matches.\n");
                }
            }
            else if (*move == DISALLOWED_MOVE) {
                /* Odd numbered half-moves in the variant list are Black. */
                if (num_moves & 0x01) {
                    variation->num_black_disallowed_moves++;
                }
                else {
                    variation->num_white_disallowed_moves++;
                }
            }
            else {
                /* Unadorned move. */
            }
            num_moves++;
        }
        move = strtok((char *) NULL, " ");
    }
    variation->moves = move_list;
    variation->length = num_moves;
    variation->next = NULL;
    return variation;
}

/* Read each line of input and decompose it into a variation
 * to be placed in the games_to_keep list.
 */
void
add_textual_variations_from_file(FILE *fpin)
{
    char *line;

    while ((line = read_line(fpin)) != NULL) {
        add_textual_variation_from_line(line);
    }
}

/* Add the text of the given line to the list of games_to_keep.
 */
void
add_textual_variation_from_line(char *line)
{
    if (non_blank_line(line)) {
        variation_list *next_variation = compose_variation(line);
        if (next_variation != NULL) {
            next_variation->next = games_to_keep;
            games_to_keep = next_variation;
        }
    }
}

/*** Functions concerned with reading details of the positional
 *** variations of interest.
 ***/

/* Break up a single line of moves into a list of moves
 * comprising a positional variation.
 * In doing so, set GlobalState.depth_of_positional_search
 * if this variation is longer than the default.
 */
static Move *
compose_positional_variation(char *line)
{
    char *move;
    /* Build a linked list of the moves of the variation. */
    Move *head = NULL, *tail = NULL;
    Boolean ok = TRUE;
    /* Keep track of the ply depth. */
    unsigned depth = 0;

    move = strtok(line, " ");
    while (ok && (move != NULL) && (*move != '*')) {
        if ((move = strip_move_number(move)) == NULL) {
            /* Only a move number. */
        }
        else {
            Move *next = decode_move((unsigned char *) move);

            if (next == NULL) {
                fprintf(GlobalState.logfile, "Failed to identify %s\n", move);
                ok = FALSE;
            }
            else {
                /* Chain it on to the list. */
                if (tail == NULL) {
                    head = next;
                    tail = next;
                }
                else {
                    tail->next = next;
                    tail = next;
                }
                next->next = NULL;
            }
            depth++;
        }
        /* Pick up the next likely move. */
        move = strtok(NULL, " ");
    }
    if (ok) {
        /* Determine whether the depth of this variation exceeds
         * the current default.
         * Depth is counted in ply.
         * Add some extras, in order to catch transpositions.
         */
        depth += 8;
        if (depth > GlobalState.depth_of_positional_search) {
            GlobalState.depth_of_positional_search = depth;
        }
    }
    else {
        if (head != NULL) {
            free_move_list(head);
        }
        head = NULL;
    }
    return head;
}

/* Read each line of input and decompose it into a positional variation
 * to be placed in the list of required hash values.
 */
void
add_positional_variations_from_file(FILE *fpin)
{
    char *line;

    while ((line = read_line(fpin)) != NULL) {
        add_positional_variation_from_line(line);
    }
}

void
add_positional_variation_from_line(char *line)
{
    if (non_blank_line(line)) {
        Move *next_variation = compose_positional_variation(line);
        if (next_variation != NULL) {
            /* We need a NULL fen string, because this is from
             * the initial position.
             */
            store_hash_value(next_variation, (const char *) NULL);
            free_move_list(next_variation);
            /* We need to know globally that positional variations
             * are of interest.
             */
            GlobalState.positional_variations = TRUE;
        }
    }
}

/* Treat fen_string as being a position to be matched.
 */
void
add_fen_positional_match(const char *fen_string)
{
    store_hash_value((Move *) NULL, fen_string);
    GlobalState.positional_variations = TRUE;
}

/* Treat fen_pattern as being a position to be matched.
 */
void
add_fen_pattern_match(const char *fen_pattern, Boolean add_reverse, const char *label)
{
    add_fen_pattern(fen_pattern, add_reverse, label);
    GlobalState.positional_variations = TRUE;
}

/* Roughly define a move character for the purposes of textual
 * matching.
 */
static Boolean
move_char(char c)
{
    return (Boolean) isalpha((int) c) || isdigit((int) c) || (c == '-');
}

/* Return TRUE if there is a match for actual_move in variation_move.
 * A match means that the string in actual_move is found surrounded
 * by non-move characters in variation_move. For instance,
 *    variation_move == "Nc6|Nf3|f3" would match
 *    actual_move == "f3" but not actual_move == "c6".
 */
static Boolean
textual_variation_match(const char *variation_move, const unsigned char *actual_move)
{
    const char *match_point;
    Boolean found = FALSE;

    for (match_point = variation_move; !found && (match_point != NULL);) {
        /* Try for a match from where we are. */
        match_point = strstr(match_point, (const char *) actual_move);
        if (match_point != NULL) {
            /* A possible match. Make sure that the match point
             * is surrounded by non-move characters so as to be sure
             * that we haven't picked up part way through a variation string.
             * Assume success.
             */
            found = TRUE;
            if (match_point != variation_move) {
                if (move_char(match_point[-1])) {
                    found = FALSE;
                }
            }
            if (move_char(match_point[strlen((const char *) actual_move)])) {
                found = FALSE;
            }
            if (!found) {
                /* Move on the match point and try again. */
                while (move_char(*match_point)) {
                    match_point++;
                }
            }
        }
    }
    return found;

}

/*** Functions concerned with matching the moves of the current game
 *** against the variations of interest.
 ***/

/* Do the moves of the current game match the given variation?
 * Go for a straight 1-1 match in the ordering, without considering
 * permutations.
 * Return TRUE if so, FALSE otherwise.
 */
static Boolean
straight_match(Move *current_game_head, variation_list variation)
{
    variant_move *moves_of_the_variation;
    /* Which is the next move that we wish to match. */
    Move *next_move;
    unsigned move_index = 0;
    /* Assume that it matches. */
    Boolean matches = TRUE;

    /* Access the head of the current game. */
    next_move = current_game_head;
    /* Go for a straight move-by-move match in the order in which
     * the variation is listed.
     * Point at the head of the moves list.
     */
    moves_of_the_variation = variation.moves;
    move_index = 0;
    while (matches && (next_move != NULL) && (move_index < variation.length)) {
        Boolean this_move_matches;

        if (*(moves_of_the_variation[move_index].move) == ANY_MOVE) {
            /* Still matching as we don't care what the actual move is. */
        }
        else {
            this_move_matches =
                    textual_variation_match(moves_of_the_variation[move_index].move,
                    next_move->move);
            if (this_move_matches) {
                /* We found a match, check that it isn't disallowed. */
                if (*moves_of_the_variation[move_index].move == DISALLOWED_MOVE) {
                    /* This move is disallowed and implies failure. */
                    matches = FALSE;
                }
            }
            else {
                if (*moves_of_the_variation[move_index].move != DISALLOWED_MOVE) {
                    /* No match found for this move. */
                    matches = FALSE;
                }
                else {
                    /* This is ok, because we didn't want a match. */
                }
            }

        }
        /* If we are still matching, go on to the next move. */
        if (matches) {
            move_index++;
            next_move = next_move->next;
        }
    }
    /* The game could be shorter than the variation, so don't rely
     * on the fact that matches is still true.
     */
    matches = (move_index == variation.length);
    return matches;
}

/* Do the moves of the current game match the given variation?
 * Try all possible orderings for the moves, within the
 * constraint of proper WHITE/BLACK moves.
 * The parameter variation is passed as a copy because we modify
 * the num_ fields within it.
 * Note that there is a possibility of a false match in this
 * function if a variant move is specified in a form such as:
 *                *|c4
 * This is because the num_ field is set from this on the basis of the
 * ANY_MOVE character at the start.  However, this could also match a
 * normal move with its c4 component.  If it is used for the latter
 * purpose then it should not count as an any_ move.  There is a warning
 * issued about this when variations are read in.
 * Return TRUE if we match, FALSE otherwise.
 *
 * The DISALLOWED_MOVE presents some problems with permutation matches
 * because an ANY_MOVE could match an otherwise disallowed move. The
 * approach that has been taken is to cause matching of a single disallowed
 * move to result in complete failure of the current match.
 */
static Boolean
permutation_match(Move *current_game_head, variation_list variation)
{
    variant_move *moves_of_the_variation;
    /* Which is the next move that we wish to match? */
    Move *next_move;
    unsigned variant_index = 0;
    /* Assume that it matches. */
    Boolean matches = TRUE;
    /* How many moves have we matched?
     * When this reaches variation.length we have a full match.
     */
    unsigned matched_moves = 0;
    Boolean white_to_move = TRUE;

    moves_of_the_variation = variation.moves;
    /* Clear all of the matched fields of the variation. */
    for (variant_index = 0; variant_index < variation.length; variant_index++) {
        moves_of_the_variation[variant_index].matched = FALSE;
    }
    /* Access the head of the current game. */
    next_move = current_game_head;

    /*** Stage One.
     * The first task is to ensure that there are no DISALLOWED_MOVEs in
     * the current game.
     */
    if ((variation.num_white_disallowed_moves > 0) ||
            (variation.num_black_disallowed_moves > 0)) {
        unsigned tested_moves = 0;
        Boolean disallowed_move_found = FALSE;

        /* Keep going as long as we still have not found a diallowed move,
         * we haven't matched the whole variation, and we haven't reached the end of
         * the game.
         */
        while ((!disallowed_move_found) && (tested_moves < variation.length) &&
                (next_move != NULL)) {
            /* We want to see if next_move is a disallowed move of the variation. */
            if (white_to_move) {
                /* White; start with the first move. */
                variant_index = 0;
            }
            else {
                /* For a Black move, start at the second half-move in the list, if any. */
                variant_index = 1;
            }
            /* Try each move of the variation in turn, until a match is found. */
            while ((!disallowed_move_found) && (variant_index < variation.length)) {
                if ((*moves_of_the_variation[variant_index].move ==
                        DISALLOWED_MOVE) &&
                        (textual_variation_match(
                        moves_of_the_variation[variant_index].move, next_move->move))) {
                    /* Found one. */
                    disallowed_move_found = TRUE;
                }
                if (!disallowed_move_found) {
                    /* Move on to the next available move -- 2 half moves along. */
                    variant_index += 2;
                }
            }
            if (!disallowed_move_found) {
                /* Ok so far, so move on. */
                tested_moves++;
                white_to_move = !white_to_move;
                next_move = next_move->next;
            }
        }
        if (disallowed_move_found) {
            /* This rules out the whole match. */
            matches = FALSE;
        }
        else {
            /* In effect, each DISALLOWED_MOVE now becomes an ANY_MOVE. */
            for (variant_index = 0; variant_index < variation.length; variant_index++) {
                if (*moves_of_the_variation[variant_index].move == DISALLOWED_MOVE) {
                    moves_of_the_variation[variant_index].matched = TRUE;
                    if ((variant_index & 1) == 0) {
                        variation.num_white_any_moves++;
                    }
                    else {
                        variation.num_black_any_moves++;
                    }
                }
            }
        }
    }

    /*** Stage Two.
     * Having eliminated moves which have been disallowed, try permutations
     * of the variation against the moves of the current game.
     */
    /* Access the head of the current game. */
    next_move = current_game_head;
    white_to_move = TRUE;
    /* Keep going as long as we still have matches, we haven't
     * matched the whole variation, and we haven't reached the end of
     * the game.
     */
    while (matches && (matched_moves < variation.length) && (next_move != NULL)) {
        /* Assume failure. */
        matches = FALSE;
        /* We want to find next_move in an unmatched move of the variation. */
        if (white_to_move) {
            /* Start with the first move. */
            variant_index = 0;
        }
        else {
            /* For a Black move, start at the second half-move in the list, if any. */
            variant_index = 1;
        }
        /* Try each move of the variation in turn, until a match is found. */
        while ((!matches) && (variant_index < variation.length)) {
            if (moves_of_the_variation[variant_index].matched) {
                /* We can't try this. */
            }
            else {
                Boolean this_move_matches = textual_variation_match(
                        moves_of_the_variation[variant_index].move,
                        next_move->move);
                if (this_move_matches) {
                    /* Found it. */
                    moves_of_the_variation[variant_index].matched = TRUE;
                    matches = TRUE;
                }
            }
            if (!matches) {
                /* Move on to the next available move -- 2 half moves along. */
                variant_index += 2;
            }
        }
        /* See if we made a straight match. */
        if (!matches) {
            /* See if we have some ANY_MOVEs available. */
            if (white_to_move && (variation.num_white_any_moves > 0)) {
                matches = TRUE;
                variation.num_white_any_moves--;
            }
            else if (!white_to_move && (variation.num_black_any_moves > 0)) {
                matches = TRUE;
                variation.num_black_any_moves--;
            }
            else {
                /* No slack. */
            }
        }
        /* We have tried everything, did we succeed? */
        if (matches) {
            /* Yes, so move on. */
            matched_moves++;
            next_move = next_move->next;
            white_to_move = !white_to_move;
        }
    }
    if (matches) {
        /* Ensure that we completed the variation. */
        matches = matched_moves == (variation.length);
    }
    return matches;
}

/* Determine whether or not the current game is wanted.
 * It will be if we are either not looking for checkmate-only
 * games, or if we are and the games does end in checkmate.
 */
Boolean
check_for_only_checkmate(const Game *game_details)
{
    if (GlobalState.match_only_checkmate) {
        const Move *moves = game_details->moves;
        /* Check that the final move is checkmate. */
        while (moves != NULL && moves->check_status != CHECKMATE) {
            moves = moves->next;
        }
        if (moves == NULL) {
            return FALSE;
        }
        else {
            return TRUE;
        }
    }
    else {
        /* No restriction to a checkmate game. */
        return TRUE;
    }
}

/* Determine whether or not the current game is wanted.
 * It will be if we are either not looking for stalemate-only
 * games, or if we are and the games does end in stalemate.
 */
Boolean
check_for_only_stalemate(const Board *board, const Move *moves)
{
    if (GlobalState.match_only_stalemate) {
        return is_stalemate(board, moves);
    }
    else {
        /* No restriction to a stalemate game. */
        return TRUE;
    }
}

/* Determine whether or not the current game is wanted.
 * It will be if we are either not looking for insufficient-material-only
 * games, or if we are and the games does end with insufficient material.
 */
Boolean
check_for_only_insufficient_material(const Board *board)
{
    if (GlobalState.match_only_insufficient_material) {
        return is_insufficient_material(board);
    }
    else {
        /* No restriction to a stalemate game. */
        return TRUE;
    }
}

/*
 * Determine whether the final position on the given board
 * is stalemate or not.
 */
Boolean
is_stalemate(const Board *board, const Move *moves)
{
    if (moves != NULL) {
        /* Check that the final move is not check or checkmate. */
        const Move *move = moves;
        while (move->next != NULL) {
            move = move->next;
        }
        if (move->check_status != NOCHECK) {
            /* Cannot be stalemate. */
            return FALSE;
        }
    }
    return !at_least_one_move(board, board->to_move);
}

/*
 * Determine whether the final position on the given board
 * is stalemate or not.
 */
static Boolean
is_insufficient_material(const Board *board)
{
    return insufficient_material(board);
}

/* Determine whether or not the current game is wanted.
 * It will be if it matches one of the current variations
 * and its tag details match those that we are interested in.
 */
Boolean
check_textual_variations(const Game *game_details)
{
    Boolean wanted = FALSE;
    variation_list *variation;

    if (games_to_keep != NULL) {
        unsigned game_length = 0;
        if(GlobalState.variation_match_anywhere) {
            /* Allow for the starting point of a match to be anywhere
             * within the game.
             */
            for(Move *move = game_details->moves; move != NULL; move = move->next) {
                game_length++;
            }
            Move *game_position = game_details->moves;
            unsigned moves_left = game_length;
            Boolean variation_left = TRUE;
            while(! wanted && game_position != NULL && variation_left) {
                variation_left = FALSE;
                for(variation = games_to_keep; (variation != NULL) && !wanted;
                        variation = variation->next) {
                    if(variation->length <= moves_left) {
                        variation_left = TRUE;
                        if (GlobalState.match_permutations) {
                            wanted = permutation_match(game_position, *variation);
                        }
                        else {
                            wanted = straight_match(game_position, *variation);
                        }
                    }
                }
                if(! wanted) {
                    game_position = game_position->next;
                    moves_left--;
                }
            }
        }
        else {
            /* Only match from the start of the game. */
            for (variation = games_to_keep; (variation != NULL) && !wanted;
                    variation = variation->next) {
                if (GlobalState.match_permutations) {
                    wanted = permutation_match(game_details->moves, *variation);
                }
                else {
                    wanted = straight_match(game_details->moves, *variation);
                }
            }
        }
    }
    else {
        /* There are no variations, assume that selection is
         * based on other criteria.
         */
        wanted = TRUE;
    }
    return wanted;
}

/* Determine whether the number of ply in this game
 * is within the bounds of what we want.
 */
Boolean
check_move_bounds(unsigned plycount)
{

    if (GlobalState.check_move_bounds) {
        return (GlobalState.lower_move_bound <= plycount) &&
                (plycount <= GlobalState.upper_move_bound);
    }
    else {
        // No restriction.
        return TRUE;
    }
}
