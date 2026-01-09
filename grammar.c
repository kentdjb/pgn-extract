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
#include <regex.h>
#include "bool.h"
#include "mymalloc.h"
#include "defs.h"
#include "typedef.h"
#include "tokens.h"
#include "taglist.h"
#include "lex.h"
#include "moves.h"
#include "map.h"
#include "lists.h"
#include "apply.h"
#include "output.h"
#include "eco.h"
#include "end.h"
#include "grammar.h"
#include "hashing.h"

static TokenType current_symbol = NO_TOKEN;

/* Keep track of which RAV level we are at.
 * This is used to check whether a TERMINATING_RESULT is the final one
 * and whether NULL_MOVEs are allowed.
 */
static unsigned RAV_level = 0;

/* How often to report processing rate. */
static unsigned PROGRESS_RATE = 1000;

/* Retain details of the header of a game.
 * This comprises the Tags and any comment prefixing the
 * moves of the game.
 */
static struct {
    /* The tag values. */
    char **Tags;
    unsigned header_tags_length;
    CommentList *prefix_comment;
} GameHeader;

static void parse_opt_game_list(SourceFileType file_type);
static Boolean parse_game(Move **returned_move_list, unsigned long *start_line, unsigned long *end_line);
Boolean parse_opt_tag_list(void);
Boolean parse_tag(void);
static Move *parse_move_list(void);
static Move *parse_move_and_variants(void);
static Move *parse_move(void);
static Move *parse_move_unit(void);
static CommentList *parse_opt_comment_list(void);
Boolean parse_opt_move_number(void);
static void parse_opt_NAG_list(Move *move_details);
static Variation *parse_opt_variant_list(void);
static Variation *parse_variant(void);
static char *parse_result(void);

static void setup_for_new_game(void);
static CommentList *append_comment(CommentList *item, CommentList *list);
static void check_result(char **Tags, const char *terminating_result);
static Boolean check_for_comments(const Game *game);
static Boolean check_for_odds(const Game *game);
static Boolean chess960_setup(Board *board);
static void deal_with_ECO_line(Move *move_list);
static void deal_with_game(Move *move_list, unsigned long start_line, unsigned long end_line);
static Boolean finished_processing(void);
static void free_tags(void);
static CommentList *merge_comment_lists(CommentList *prefix, CommentList *suffix);
static void output_game(Game *game,FILE *outputfile);
static void split_variants(Game *game, FILE *outputfile, unsigned depth);

/* Initialise the game header structure to contain
 * space for the default number of tags.
 * The space will have to be increased if new tags are
 * identified in the program source.
 */
void
init_game_header(void)
{
    unsigned i;
    GameHeader.header_tags_length = ORIGINAL_NUMBER_OF_TAGS;
    GameHeader.Tags = (char **) malloc_or_die(GameHeader.header_tags_length *
            sizeof (*GameHeader.Tags));
    for (i = 0; i < GameHeader.header_tags_length; i++) {
        GameHeader.Tags[i] = (char *) NULL;
    }
    GameHeader.prefix_comment = (CommentList *) NULL;
}

void
increase_game_header_tags_length(unsigned new_length)
{
    unsigned i;

    if (new_length <= GameHeader.header_tags_length) {
        fprintf(GlobalState.logfile,
                "Internal error: inappropriate length %d ", new_length);
        fprintf(GlobalState.logfile,
                " passed to increase_game_header_tags().\n");
        exit(1);
    }
    GameHeader.Tags = (char **) realloc_or_die((void *) GameHeader.Tags,
            new_length * sizeof (*GameHeader.Tags));
    for (i = GameHeader.header_tags_length; i < new_length; i++) {
        GameHeader.Tags[i] = NULL;
    }
    GameHeader.header_tags_length = new_length;
}

/* Try to open the given file. Error and exit on failure. */
FILE *
must_open_file(const char *filename, const char *mode)
{
    FILE *fp;

    fp = fopen(filename, mode);
    if (fp == NULL) {
        fprintf(GlobalState.logfile, "Unable to open the file: \"%s\"\n",
                filename);
        exit(1);
    }
    return fp;
}

/* Print out on outfp the current details and
 * terminate with a newline.
 */
void
report_details(FILE *outfp)
{
    if (GameHeader.Tags[WHITE_TAG] != NULL) {
        fprintf(outfp, "%s - ", GameHeader.Tags[WHITE_TAG]);
    }
    if (GameHeader.Tags[BLACK_TAG] != NULL) {
        fprintf(outfp, "%s ", GameHeader.Tags[BLACK_TAG]);
    }

    if (GameHeader.Tags[EVENT_TAG] != NULL) {
        fprintf(outfp, "%s ", GameHeader.Tags[EVENT_TAG]);
    }
    if (GameHeader.Tags[SITE_TAG] != NULL) {
        fprintf(outfp, "%s ", GameHeader.Tags[SITE_TAG]);
    }

    if (GameHeader.Tags[DATE_TAG] != NULL) {
        fprintf(outfp, "%s ", GameHeader.Tags[DATE_TAG]);
    }
    putc('\n', outfp);
    fflush(outfp);
}

/* Check that terminating_result is consistent with
 * Tags[RESULT_TAG]. 
 * If the latter is missing, fill it in from terminating_result.
 * If Tags[RESULT_TAG] is the short form "1/2" then replace it
 * with the long form.
 */
static void
check_result(char **Tags, const char *terminating_result)
{
    char *result_tag = Tags[RESULT_TAG];
    
    if(result_tag != NULL && strcmp(result_tag, "1/2") == 0) {
        /* Inappropriate short form. */
        (void) free(result_tag);
        result_tag = Tags[RESULT_TAG] = copy_string("1/2-1/2");
    }

    if (terminating_result != NULL) {
        if ((result_tag == NULL) || (*result_tag == '\0') ||
                (strcmp(result_tag, "?") == 0)) {
            /* Use a copy of terminating result. */
            result_tag = copy_string(terminating_result);
            Tags[RESULT_TAG] = result_tag;
        }
        else {
            /* Consistency checks done later. */
        }
    }
}

/* Return TRUE if there is at least one comment in the move list,
 * or any of its variations.
 */
static Boolean
comments_in_move_list(const Move *move_list)
{
    Boolean comment_found = FALSE;
    const Move *move = move_list;
    while(! comment_found && move != NULL) {
        if(move->comment_list != NULL) {
            comment_found = TRUE;
        }
        else {
            if(move->Variants != NULL) {
                const Variation *v = move->Variants;
                while(! comment_found && v != NULL) {
                    comment_found = comments_in_move_list(v->moves);
                    v = v->next;
                }
                if(! comment_found) {
                    move = move->next;
                }
            }
            else {
                move = move->next;
            }
        }
    }
    return comment_found;
}

/* Check whether only games with comments are to be retained. */
static Boolean
check_for_comments(const Game *game)
{
    if(GlobalState.keep_only_commented_games) {
        if(game->prefix_comment != NULL) {
            return TRUE;
        }
        else {
            return comments_in_move_list(game->moves);
        }
    }
    else {
        return TRUE;
    }
}

/* Check whether the game appears to have been played at odds.
 * The assumption is that all pieces are on the first two ranks
 * and one side has different pieces from the other.
 */
static Boolean
check_for_odds(const Game *game)
{
    if(GlobalState.find_odds_games) {
        if(game->tags[FEN_TAG] != NULL) {
            regex_t regex;

            /* Make sure the middle 8 ranks are empty. */
            const char *empty_ranks = "^[^/]*/[^/]*/8/8/8/8/";
            if(regcomp(&regex, empty_ranks, 0)  == 0) {
                if(regexec(&regex, game->tags[FEN_TAG], 0, NULL, 0) != 0) {
                    return FALSE;
                }
            }
            else {
                return FALSE;
            }
            regfree(&regex);
            Boolean odds = FALSE;
            /* Compare the piece numbers and types for look for a mismatch. */
            Board *board = new_fen_board(game->tags[FEN_TAG]);
            if(board->move_number == 1 && board->to_move == WHITE) {
                /* Find the number of pieces for each player. */
                int num_pieces[2][NUM_PIECE_VALUES] = {
                    /* Dummies for OFF and EMPTY at the start. */
                    /*     P  N  B  R  Q  K */
                    {0, 0, 8, 2, 2, 2, 1, 1},
                    {0, 0, 8, 2, 2, 2, 1, 1}
                };
                extract_pieces_from_board(num_pieces, board);
                for(int p = PAWN; p <= QUEEN && !odds; p++) {
                    if(num_pieces[0][p] != num_pieces[1][p]) {
                        odds = TRUE;
                    }
                }
            }
            free_board(board);
            return odds;
        }
        else {
            return FALSE;
        }
    }
    else {
        return TRUE;
    }
}

/* Select which file to write to based upon the game state.
 * This will depend upon:
 *        Whether the number of games per file is limited.
 *        Whether ECO_level > DONT_DIVIDE.
 */
/* A length for filenames. */
#define FILENAME_LENGTH (250)

static FILE *
select_output_file(StateInfo *GameState, const char *eco)
{
    if (GameState->games_per_file > 0) {
        if (GameState->games_per_file == 1 ||
                (GameState->num_games_matched % GameState->games_per_file) == 1) {
            /* Time to open the next one. */
            char filename[FILENAME_LENGTH];

            if (GameState->outputfile != NULL) {
                if (GlobalState.json_format && GameState->num_games_matched != 1) {
                    /* Terminate the output of the previous file. */
                    fputs("\n]\n", GlobalState.outputfile);
                }
                (void) fclose(GameState->outputfile);
            }
            sprintf(filename, "%u%s",
                    GameState->next_file_number,
                    output_file_suffix(GameState->output_format));
            GameState->outputfile = must_open_file(filename, "w");
            GameState->next_file_number++;
            if (GlobalState.json_format) {
                fputs("[\n", GlobalState.outputfile);
            }
        }
    }
    else {
        if (GameState->ECO_level > DONT_DIVIDE) {
            /* Open a file of the appropriate name. */
            if (GameState->outputfile != NULL) {
                /* @@@ In practice, this might need refinement.
                 * Repeated opening and closing may prove inefficient.
                 */
                (void) fclose(GameState->outputfile);
                GameState->outputfile = open_eco_output_file(
                        GameState->ECO_level,
                        eco);
            }
        }
        else if (GlobalState.json_format && GameState->num_games_matched == 1) {
            fputs("[\n", GlobalState.outputfile);
        }
        else {
        }
    }
    return GameState->outputfile;
}

/*
 * Conditions for finishing processing, other than all the input
 * having been processed.
 */
static Boolean finished_processing(void)
{
    return (GlobalState.matching_game_numbers != NULL &&
            GlobalState.next_game_number_to_output == NULL) ||
            (GlobalState.maximum_matches > 0 && 
            GlobalState.num_games_matched == GlobalState.maximum_matches) ||
            GlobalState.num_games_processed >= GlobalState.game_limit;
}

/*
 * Is the given game number within the range to be matched?
 */
static Boolean in_game_number_range(unsigned long number,
        game_number *range)
{
    return range != NULL && range->min <= number && number <= range->max;
}

static void
parse_opt_game_list(SourceFileType file_type)
{
    Move *move_list = NULL;
    unsigned long start_line, end_line;

    while (parse_game(&move_list, &start_line, &end_line) && !finished_processing()) {
        if (file_type == NORMALFILE) {
            deal_with_game(move_list, start_line, end_line);
        }
        else if (file_type == CHECKFILE) {
            deal_with_game(move_list, start_line, end_line);
        }
        else if (file_type == ECOFILE) {
            if (move_list != NULL) {
                deal_with_ECO_line(move_list);
            }
            else {
                fprintf(GlobalState.logfile, "ECO line with zero moves.\n");
                report_details(GlobalState.logfile);
            }
        }
        else {
            /* Unknown type. */
            free_tags();
            free_move_list(move_list);
        }
        move_list = NULL;
        setup_for_new_game();
    }
    if(move_list != NULL) {
        free_move_list(move_list);
    }
}

/* Parse a game and return a pointer to any valid list of moves
 * in returned_move_list.
 */
static Boolean
parse_game(Move **returned_move_list, unsigned long *start_line, unsigned long *end_line)
{ /* Boolean something_found = FALSE; */
    CommentList *prefix_comment;
    Move *move_list = NULL;
    char *result;
    /* There shouldn't be a hanging comment before the result,
     * but there sometimes is.
     */
    CommentList *hanging_comment;

    /* Assume that we won't return anything. */
    *returned_move_list = NULL;
    /* Skip over any junk between games. */
    current_symbol = skip_to_next_game(current_symbol);
    prefix_comment = parse_opt_comment_list();
    if (prefix_comment != NULL) {
        /* Free this here, as it is hard to
         * know whether it belongs to the game or the file.
         * It is better to put game comments after the tags.
         */
        /* something_found = TRUE; */
        free_comment_list(prefix_comment);
        prefix_comment = NULL;
    }
    *start_line = get_line_number();
    if (parse_opt_tag_list()) {
        /* something_found = TRUE; */
    }

    /* Some games have an initial NAG as a print-board indication.
     * This is not legal PGN.
     * Silently delete it/them.
     */
    while (current_symbol == NAG) {
        current_symbol = next_token();
    }

    /* @@@ Beware of comments and/or tags without moves. */
    move_list = parse_move_list();

    /* @@@ Look for a comment with no move text before the result. */
    hanging_comment = parse_opt_comment_list();
    /* Append this to the final move, if there is one. */

    /* Look for a result, even if there were no moves. */
    result = parse_result();
    *end_line = get_line_number();
    if (move_list != NULL) {
        /* Find the last move. */
        Move *last_move = move_list;

        while (last_move->next != NULL) {
            last_move = last_move->next;
        }
        if (hanging_comment != NULL) {
            last_move->comment_list = append_comment(hanging_comment, last_move->comment_list);
        }
        if (result != NULL) {
            /* Append it to the last move. */
            last_move->terminating_result = result;
            check_result(GameHeader.Tags, result);
            *returned_move_list = move_list;
        }
        else {
            fprintf(GlobalState.logfile, "Missing result.\n");
            report_details(GlobalState.logfile);
        }
        /* something_found = TRUE; */
    }
    else {
        /* @@@ Nothing to attach the comment to. */
        (void) free((void *) hanging_comment);
        hanging_comment = NULL;
        /*
         * Workaround for games with zero moves.
         * Check the result for consistency with the tags, but then
         * there is no move to attach it to.
         * When outputting a game, the missing result in this case
         * will have to be supplied from the tags.
         */
        check_result(GameHeader.Tags, result);
        if (result != NULL) {
            (void) free((void *) result);
        }
        *returned_move_list = NULL;
    }
    return current_symbol != EOF_TOKEN;
}

Boolean
parse_opt_tag_list(void)
{
    Boolean something_found = FALSE;
    CommentList *prefix_comment;

    while (parse_tag()) {
        something_found = TRUE;
    }
    prefix_comment = parse_opt_comment_list();
    if (prefix_comment != NULL) {
        GameHeader.prefix_comment = prefix_comment;
        something_found = TRUE;
    }
    return something_found;
}

/* Return TRUE if it looks like board contains an initial
 * Chess 960 setup position. FALSE otherwise.
 * Assessment requires that:
 *     + The move number be 1.
 *     + All castling rights are intact.
 *     + The two home ranks are full.
 *     + Identical pieces are opposite each other on the back rank.
 *     + At least one piece is out of its standard position.
 */
static Boolean
chess960_setup(Board *board)
{
    if(board->move_number == 1 &&
       board->WKingRank == '1' &&
       board->BKingRank == '8' &&
       board->WKingCol == board->BKingCol &&
       (board->WKingCastle != '\0' &&
        board->WQueenCastle != '\0' &&
        board->BKingCastle != '\0' &&
        board->BQueenCastle != '\0')) {
        /* Check for a full set of pawns. */
        Boolean probable = TRUE;
        int white_r = RankConvert('2');
        int black_r = RankConvert('7');
        Piece white_pawn = MAKE_COLOURED_PIECE(WHITE, PAWN);
        Piece black_pawn = MAKE_COLOURED_PIECE(BLACK, PAWN);
        for(int c = ColConvert('a'); c <= ColConvert('h') && probable; c++) {
            probable = board->board[white_r][c] == white_pawn &&
                       board->board[black_r][c] == black_pawn;
            /* Make sure the back rank is full and identical pieces
             * are opposite each other.
             */
            if(probable) {
                probable =
                    board->board[white_r-1][c] != EMPTY &&
                    EXTRACT_PIECE(board->board[white_r-1][c]) ==
                        EXTRACT_PIECE(board->board[black_r+1][c]);
            }
        }
        if(probable) {
            /* Check for at least one piece type being out of position. */
            /* Only need to check one colour because we already know the
             * pieces are identically paired.
             */
            white_r = RankConvert('1');
            probable =
                board->board[white_r][ColConvert('a')] != W(ROOK) ||
                board->board[white_r][ColConvert('b')] != W(KNIGHT) ||
                board->board[white_r][ColConvert('c')] != W(BISHOP) ||
                board->board[white_r][ColConvert('d')] != W(QUEEN) ||
                board->board[white_r][ColConvert('e')] != W(KING) ||
                board->board[white_r][ColConvert('f')] != W(BISHOP) ||
                board->board[white_r][ColConvert('g')] != W(KNIGHT) ||
                board->board[white_r][ColConvert('h')] != W(ROOK);
        }
        return probable;
    }
    else {
        return FALSE;
    }
}

Boolean
parse_tag(void)
{
    Boolean TagFound = TRUE;

    if (current_symbol == TAG) {
        TagName tag_index = yylval.tag_index;

        current_symbol = next_token();
        if (current_symbol == STRING) {
            char *tag_string = yylval.token_string;

            if (tag_index < GameHeader.header_tags_length) {
                GameHeader.Tags[tag_index] = tag_string;
            }
            else {
                print_error_context(GlobalState.logfile);
                fprintf(GlobalState.logfile,
                        "Internal error: Illegal tag index %d for %s\n",
                        tag_index, tag_string);
                exit(1);
            }
            current_symbol = next_token();
        }
        else {
            print_error_context(GlobalState.logfile);
            fprintf(GlobalState.logfile, "Missing tag string.\n");
        }
        if(current_symbol == TAG_END) {
            current_symbol = next_token();
        }
        else {
            print_error_context(GlobalState.logfile);
            fprintf(GlobalState.logfile, "Missing ]\n");
        }
    }
    else if (current_symbol == STRING) {
        print_error_context(GlobalState.logfile);
        fprintf(GlobalState.logfile, "Missing tag for %s.\n", yylval.token_string);
        (void) free((void *) yylval.token_string);
        current_symbol = next_token();
        if(current_symbol == TAG_END) {
            current_symbol = next_token();
        }
        else {
            /* No point reporting the error. */
        }
    }
    else {
        TagFound = FALSE;
    }
    return TagFound;
}

static Move *
parse_move_list(void)
{
    Move *head = NULL, *tail = NULL;

    head = parse_move_and_variants();
    if (head != NULL) {
        Move *next_move;

        tail = head;
        while ((next_move = parse_move_and_variants()) != NULL) {
            tail->next = next_move;
            tail = next_move;
        }
    }
    return head;
}

static Move *
parse_move_and_variants(void)
{
    Move *move_details;

    move_details = parse_move();
    if (move_details != NULL) {
        CommentList *comment;

        move_details->Variants = parse_opt_variant_list();
        comment = parse_opt_comment_list();
        if (comment != NULL) {
            move_details->comment_list = append_comment(comment, move_details->comment_list);
        }
    }
    return move_details;
}

static Move *
parse_move(void)
{
    Move *move_details = NULL;

    if (parse_opt_move_number()) {
    }
    /* @@@ Watch out for finding just the number. */
    move_details = parse_move_unit();
    if (move_details != NULL) {
        parse_opt_NAG_list(move_details);
        /* Any trailing comments will have been picked up
         * and attached to the NAGs.
         */
    }
    return move_details;
}

static Move *
parse_move_unit(void)
{
    Move *move_details = NULL;

    if (current_symbol == MOVE) {
        move_details = yylval.move_details;

        if (move_details->class == NULL_MOVE && RAV_level == 0) {
            if(!GlobalState.allow_null_moves) {
                print_error_context(GlobalState.logfile);
                fprintf(GlobalState.logfile, "Null moves (--) only allowed in variations.\n");
            }
        }

        current_symbol = next_token();
        if (current_symbol == CHECK_SYMBOL) {
            strcat((char *) move_details->move, "+");
            current_symbol = next_token();
            /* Sometimes + is followed by #, so cover this case. */
            if (current_symbol == CHECK_SYMBOL) {
                current_symbol = next_token();
            }
        }
        move_details->comment_list = parse_opt_comment_list();
    }
    return move_details;
}

static CommentList *
parse_opt_comment_list(void)
{
    CommentList *head = NULL, *tail = NULL;

    while (current_symbol == COMMENT) {
        if (head == NULL) {
            head = tail = yylval.comment;
        }
        else {
            tail->next = yylval.comment;
            tail = tail->next;
        }
        current_symbol = next_token();
    }
    return head;
}

Boolean
parse_opt_move_number(void)
{
    Boolean something_found = FALSE;

    if (current_symbol == MOVE_NUMBER) {
        current_symbol = next_token();
        something_found = TRUE;
    }
    return something_found;
}

/**
 * Parse 0 or more NAGs, optionally followed by 0 or more comments.
 * @param move_details
 */
static void
parse_opt_NAG_list(Move *move_details)
{
    while (current_symbol == NAG) {
        Nag *details = (Nag *) malloc_or_die(sizeof(*details));
        details->text = NULL;
        details->comments = NULL;
        details->next = NULL;
        do {
            details->text = save_string_list_item(details->text, yylval.token_string);
            current_symbol = next_token();
        } while(current_symbol == NAG);
        details->comments = parse_opt_comment_list();
        if(move_details->NAGs == NULL) {
            move_details->NAGs = details;
        }
        else {
            Nag *nextNAG = move_details->NAGs;
            while(nextNAG->next != NULL) {
                nextNAG = nextNAG->next;
            }
            nextNAG->next = details;
        }
    }
}

static Variation *
parse_opt_variant_list(void)
{
    Variation *head = NULL, *tail = NULL,
            *variation;

    while ((variation = parse_variant()) != NULL) {
        if (head == NULL) {
            head = tail = variation;
        }
        else {
            tail->next = variation;
            tail = variation;
        }
    }
    return head;
}

static Variation *
parse_variant(void)
{
    Variation *variation = NULL;

    if (current_symbol == RAV_START) {
        CommentList *prefix_comment;
        CommentList *suffix_comment;
        char *result = NULL;
        Move *moves;

        RAV_level++;
        variation = (Variation *) malloc_or_die(sizeof (Variation));

        current_symbol = next_token();
        prefix_comment = parse_opt_comment_list();
        moves = parse_move_list();
        if (moves == NULL) {
            print_error_context(GlobalState.logfile);
            fprintf(GlobalState.logfile, "Missing move list in variation.\n");
        }
        else if(GlobalState.lichess_comment_fix && prefix_comment != NULL) {
            /* lichess study deletes the prefix comment, so
             * move it after the first move of the variation.
             */
            moves->comment_list = merge_comment_lists(prefix_comment, moves->comment_list);
            prefix_comment = NULL;
        }
        result = parse_result();
        if ((result != NULL) && (moves != NULL)) {
            /* Find the last move, to which to append the terminating
             * result.
             */
            Move *last_move = moves;
            CommentList *trailing_comment;

            while (last_move->next != NULL) {
                last_move = last_move->next;
            }
            last_move->terminating_result = result;
            /* Accept a comment after the result, but it will
             * be printed out preceding the result.
             */
            trailing_comment = parse_opt_comment_list();
            if (trailing_comment != NULL) {
                last_move->comment_list = append_comment(trailing_comment, last_move->comment_list);
            }
        }
        else {
            /* Ok. */
        }
        if (current_symbol == RAV_END) {
            RAV_level--;
            current_symbol = next_token();
        }
        else {
            fprintf(GlobalState.logfile, "Missing ')' to close variation.\n");
            print_error_context(GlobalState.logfile);
        }
        suffix_comment = parse_opt_comment_list();
        variation->prefix_comment = prefix_comment;
        variation->suffix_comment = suffix_comment;
        variation->moves = moves;
        variation->next = NULL;
    }
    return variation;
}

static char *
parse_result(void)
{
    char *result = NULL;

    if (current_symbol == TERMINATING_RESULT) {
        result = yylval.token_string;
        if (RAV_level == 0) {
            /* In the interests of skipping any intervening material
             * between games, set the lookahead to a dummy token.
             */
            current_symbol = NO_TOKEN;
        }
        else {
            current_symbol = next_token();
        }
    }
    return result;
}

static void
setup_for_new_game(void)
{
    restart_lex_for_new_game();
    RAV_level = 0;
}

/* Discard any data held in the GameHeader.Tags structure. */
static void
free_tags(void)
{
    unsigned tag;

    for (tag = 0; tag < GameHeader.header_tags_length; tag++) {
        if (GameHeader.Tags[tag] != NULL) {
            free(GameHeader.Tags[tag]);
            GameHeader.Tags[tag] = NULL;
        }
    }
}

/* Discard data from a gathered game. */
void
free_string_list(StringList *list)
{
    StringList *next;

    while (list != NULL) {
        next = list;
        list = list->next;
        if (next->str != NULL) {
            (void) free((void *) next->str);
        }
        (void) free((void *) next);
    }
}

void
free_comment_list(CommentList *comment_list)
{
    while (comment_list != NULL) {
        CommentList *this_comment = comment_list;

        if (comment_list->comment != NULL) {
            free_string_list(comment_list->comment);
        }
        comment_list = comment_list->next;
        (void) free((void *) this_comment);
    }
}

static void
free_variation(Variation *variation)
{
    Variation *next;

    while (variation != NULL) {
        next = variation;
        variation = variation->next;
        if (next->prefix_comment != NULL) {
            free_comment_list(next->prefix_comment);
        }
        if (next->suffix_comment != NULL) {
            free_comment_list(next->suffix_comment);
        }
        if (next->moves != NULL) {
            (void) free_move_list(next->moves);
        }
        (void) free((void *) next);
    }
}

static void free_NAG_list(Nag *nag_list)
{
    while(nag_list != NULL) {
        Nag *nextNAG = nag_list->next;
        free_string_list(nag_list->text);
        free_comment_list(nag_list->comments);
        (void) free((void *) nag_list);
        nag_list = nextNAG;
    }
}

void
free_move_list(Move *move_list)
{
    Move *nextMove;

    while (move_list != NULL) {
        nextMove = move_list;
        move_list = move_list->next;
        
        free_NAG_list(nextMove->NAGs);
        free_comment_list(nextMove->comment_list);
        free_variation(nextMove->Variants);
        
        if (nextMove->epd != NULL) {
            (void) free((void *) nextMove->epd);
        }
        if(nextMove->fen_suffix != NULL) {
            (void) free((void *) nextMove->fen_suffix);
            nextMove->fen_suffix = NULL;
        }
        if (nextMove->terminating_result != NULL) {
            (void) free((void *) nextMove->terminating_result);
        }
        
        (void) free((void *) nextMove);
    }
}

/* Add str onto the tail of list and
 * return the head of the resulting list.
 */
StringList *
save_string_list_item(StringList *list, const char *str)
{
    if (str != NULL && *str != '\0') {
        StringList *new_item;

        new_item = (StringList *) malloc_or_die(sizeof (*new_item));
        new_item->str = str;
        new_item->next = NULL;
        if (list == NULL) {
            list = new_item;
        }
        else {
            StringList *tail = list;

            while (tail->next != NULL) {
                tail = tail->next;
            }
            tail->next = new_item;
        }
    }
#if 1
    /* This is almost certainly correct to avoid losing
     * two bytes with malloc'd empty strings.
     * No problems with valgrind but just being
     * cautious.
     */
    else if(str != NULL) {
        (void) free((void *) str);
    }
#endif
    return list;
}

/* Append any comments in Comment onto the end of
 * any associated with move.
 */
void
append_comments_to_move(Move *move, CommentList *comment)
{
    if (comment != NULL) {
        move->comment_list = append_comment(comment, move->comment_list);
    }
}

/* Add item to the end of list.
 * If list is empty, return item.
 */
static CommentList *append_comment(CommentList *item, CommentList *list)
{
    if (list == NULL) {
        return item;
    }
    else {
        CommentList *tail = list;

        while (tail->next != NULL) {
            tail = tail->next;
        }
        tail->next = item;
        return list;
    }
}

/* Add the suffix list (if any) to the end of the prefix list.
 */
static CommentList *merge_comment_lists(CommentList *prefix, CommentList *suffix)
{
    if(prefix == NULL) {
        return suffix;
    }
    else if(suffix != NULL) {
        CommentList *tail = prefix;
        while(tail->next != NULL) {
            tail = tail->next;
        }
        tail->next = suffix;
    }
    return prefix;
}

/* Check for consistency of any FEN-related tags. */
static Boolean consistent_FEN_tags(Game *current_game)
{
    Boolean consistent = TRUE;

    if ((current_game->tags[SETUP_TAG] != NULL) &&
            (strcmp(current_game->tags[SETUP_TAG], "1") == 0)) {
        /* There must be a FEN_TAG to go with it. */
        if (current_game->tags[FEN_TAG] == NULL) {
            consistent = FALSE;
            report_details(GlobalState.logfile);
            fprintf(GlobalState.logfile,
                    "Missing %s Tag to accompany %s Tag.\n",
                    tag_header_string(FEN_TAG),
                    tag_header_string(SETUP_TAG));
            print_error_context(GlobalState.logfile);
        }
    }
    if(current_game->tags[FEN_TAG] != NULL) {
        Board *board = new_fen_board(current_game->tags[FEN_TAG]);
        if(board != NULL) {
            /* There must be a SETUP_TAG to go with it. */
            if(current_game->tags[SETUP_TAG] == NULL) {
                // This is such a common problem that it makes
                // more sense just to silently correct it.
#if 0
                report_details(GlobalState.logfile);
                fprintf(GlobalState.logfile,
                        "Missing %s Tag to accompany %s Tag.\n",
                        tag_header_string(SETUP_TAG),
                        tag_header_string(FEN_TAG));
                print_error_context(GlobalState.logfile);
#endif
                /* Fix the inconsistency. */
                current_game->tags[SETUP_TAG] = copy_string("1");
            }

            Boolean chess960 = chess960_setup(board);
            if(current_game->tags[VARIANT_TAG] == NULL) {
                /* See if there should be a Variant tag. */
                /* Look for an initial position found in Chess 960. */
                if(chess960) {
		    const char *missing_value = "chess 960";
                    report_details(GlobalState.logfile);
                    fprintf(GlobalState.logfile,
                            "Missing %s Tag for non-standard setup; adding %s.\n",
                            tag_header_string(VARIANT_TAG),
			    missing_value);
                    /* Fix the inconsistency. */
                    current_game->tags[VARIANT_TAG] = copy_string(missing_value);
                }
                else if(GlobalState.add_fen_castling) {
                    /* If add_fen_castling is TRUE and castling permissions are absent
                     * then liberally assume them based on the King and Rook positions.
                     */
                    if(!board->WKingCastle && !board->WQueenCastle &&
                            !board->BKingCastle && !board->BQueenCastle) {
                        add_fen_castling(current_game, board);
                    }
                }
            }
            else if(chess960) {
                /* @@@ Should really make sure the Variant tag is appropriate. */
            }

            free_board(board);
        }
        else {
            consistent = FALSE;
        }
    }
    return consistent;
}

static void
deal_with_game(Move *move_list, unsigned long start_line, unsigned long end_line)
{
    Game current_game;
    /* We need a dummy argument for apply_move_list. */
    unsigned plycount;
    /* Whether the game matches, as long as it is not in a CHECKFILE. */
    Boolean game_matches = FALSE;
    /* Whether to output the game. */
    Boolean output_the_game = FALSE;

    if (GlobalState.current_file_type != CHECKFILE) {
        /* Update the count of how many games handled. */
        GlobalState.num_games_processed++;
    }

    /* Fill in the information currently known. */
    current_game.tags = GameHeader.Tags;
    current_game.tags_length = GameHeader.header_tags_length;
    current_game.prefix_comment = GameHeader.prefix_comment;
    current_game.moves = move_list;
    current_game.moves_checked = FALSE;
    current_game.moves_ok = FALSE;
    current_game.error_ply = 0;
    current_game.position_counts = NULL;
    current_game.start_line = start_line;
    current_game.end_line = end_line;

    /* Determine whether or not this game is wanted, on the
     * basis of the various selection criteria available.
     */

    /*
     * apply_move_list checks out the moves.
     * If it returns TRUE as a match, it will also fill in the
     *     current_game.final_hash_value and
     *     current_game.cumulative_hash_value
     * fields of current_game so that these can be used in the
     * previous_occurrence function.
     *
     * If there are any tag criteria, it will be easy to quickly
     * eliminate most games without going through the lengthy
     * process of game matching.
     *
     * If ECO adding is done, the order of checking may cause
     * a conflict here since it won't be possible to reject a game
     * based on its ECO code unless it already has one.
     * Therefore, check for the ECO tag only after everything else has
     * been checked.
     */
    if (consistent_FEN_tags(&current_game) &&
            check_tag_details_not_ECO(current_game.tags, current_game.tags_length, TRUE) &&
            check_setup_tag(current_game.tags) &&
            check_duplicate_setup(&current_game) &&

            apply_move_list(&current_game, &plycount, GlobalState.depth_of_positional_search, TRUE) &&

            check_move_bounds(plycount) &&
            check_for_odds(&current_game) &&
            check_textual_variations(&current_game) &&
            check_for_material_match(&current_game) &&
            check_for_piece_count_match(&current_game) &&
            check_for_only_checkmate(&current_game) &&
            check_for_only_repetition(current_game.position_counts) &&
            check_ECO_tag(current_game.tags, TRUE) &&
            check_for_comments(&current_game)) {
        /* If there is no original filename then the game is not a
         * duplicate.
         */
        const char *original_filename = previous_occurance(current_game, plycount);

        if ((original_filename == NULL) && GlobalState.suppress_originals) {
            /* Don't output first occurrences. */
        }
        else if ((original_filename == NULL) || !GlobalState.suppress_duplicates) {
            if (GlobalState.current_file_type == CHECKFILE) {
                /* We are only checking, so don't count this as a matched game. */
            }
            else if(GlobalState.num_games_processed >= GlobalState.first_game_number) {
                game_matches = TRUE;
                GlobalState.num_games_matched++;
                if (GlobalState.matching_game_numbers != NULL &&
                        !in_game_number_range(GlobalState.num_games_matched, GlobalState.next_game_number_to_output)) {
                    /* This is not the right matching game to be output. */
                }
                else if (GlobalState.skip_game_numbers != NULL &&
                        in_game_number_range(GlobalState.num_games_matched, GlobalState.next_game_number_to_skip)) {
                    /* Skip this matching game. */
                    if(GlobalState.num_games_matched == GlobalState.next_game_number_to_skip->max) {
                        GlobalState.next_game_number_to_skip = GlobalState.next_game_number_to_skip->next;
                    }
                }
                else if (GlobalState.check_only) {
                    /* We are only checking. */
                    if (GlobalState.verbosity > 1) {
                        /* Report progress on logfile. */
                        report_details(GlobalState.logfile);
                    }
                }
                else {
                    output_the_game = TRUE;
                }
            }
            else {
                /* Not wanted. */
            }

            if(current_game.moves == NULL &&
                    current_game.tags[RESULT_TAG] == NULL) {
                    fprintf(GlobalState.logfile, "Game with no moves and no result.\n");
                    report_details(GlobalState.logfile);
                    if(output_the_game) {
                        current_game.tags[RESULT_TAG] = copy_string("*");
                    }
            }
            if(output_the_game) {
                /* This game is to be kept and output. */
                FILE *outputfile = select_output_file(&GlobalState,
                        current_game.tags[ECO_TAG]);

                /* See if we wish to separate out duplicates. */
                if ((original_filename != NULL) &&
                        (GlobalState.duplicate_file != NULL)) {
                    static const char *last_input_file = NULL;

                    outputfile = GlobalState.duplicate_file;
                    if ((last_input_file != GlobalState.current_input_file) &&
                            (GlobalState.current_input_file != NULL)) {
                        if(GlobalState.keep_comments) {
                            /* Record which file this and succeeding
                             * duplicates come from.
                             */
                            print_str(outputfile, "{ From: ");
                            print_str(outputfile,
                                    GlobalState.current_input_file);
                            print_str(outputfile, " }");
                            terminate_line(outputfile);
                        }
                        last_input_file = GlobalState.current_input_file;
                    }
                    if(GlobalState.keep_comments) {
                        print_str(outputfile, "{ First found in: ");
                        print_str(outputfile, original_filename);
                        print_str(outputfile, " }");
                        terminate_line(outputfile);
                    }
                }
                if(! GlobalState.suppress_matched) {
                    /* Now output what we have. */
                    output_game(&current_game, outputfile);
                    if (GlobalState.verbosity > 1) {
                        /* Report progress on logfile. */
                        report_details(GlobalState.logfile);
                    }
                }
            }
        }
    }
    if (!game_matches && (GlobalState.non_matching_file != NULL) &&
            GlobalState.current_file_type != CHECKFILE) {
        /* The user wants to keep everything else. */
        if (! current_game.moves_checked) {
            /* Make sure that the move text is in a reasonable state.
             * Force checking of the whole game.
             */
            (void) apply_move_list(&current_game, &plycount, 0, FALSE);
        }
        if (current_game.moves_ok || GlobalState.keep_broken_games) {
            if(GlobalState.json_format) {
                if(GlobalState.num_non_matching_games == 0) {
                    fputs("[\n", GlobalState.non_matching_file);
                }
                else {
                    fputs(",\n", GlobalState.non_matching_file);
                }
            }
            GlobalState.num_non_matching_games++;
            output_game(&current_game, GlobalState.non_matching_file);
        }
    }
    if (game_matches && GlobalState.matching_game_numbers != NULL &&
            in_game_number_range(GlobalState.num_games_matched, 
                                 GlobalState.next_game_number_to_output)) {
        if(GlobalState.num_games_matched == GlobalState.next_game_number_to_output->max) {
            GlobalState.next_game_number_to_output = GlobalState.next_game_number_to_output->next;
        }
    }

    /* Game is finished with, so free everything. */
    if (GameHeader.prefix_comment != NULL) {
        free_comment_list(GameHeader.prefix_comment);
    }
    /* Ensure that the GameHeader's prefix comment is NULL for
     * the next game.
     */
    GameHeader.prefix_comment = NULL;

    free_tags();
    free_move_list(current_game.moves);
    if (current_game.position_counts != NULL) {
        free_position_count_list(current_game.position_counts);
        current_game.position_counts = NULL;
    }
    if (GlobalState.verbosity != 0 && (GlobalState.num_games_processed % PROGRESS_RATE) == 0) {
        fprintf(stderr, "Games: %lu\r", GlobalState.num_games_processed);
    }
}

/*
 * Output the given game to the output file.
 * If GlobalState.split_variants then this will involve outputting 
 * each variation separately.
 */
static void
output_game(Game *game, FILE *outputfile)
{
    if(GlobalState.split_variants && GlobalState.keep_variations) {
        split_variants(game, outputfile, 0);
    }
    else {
        format_game(game, outputfile);
    }
}

/*
 * Output each variation separately, to the required depth.
 * NB: This involves the removal of all variations from the game.
 * This is done recursively and depth (>=0) defines the current
 * level of recursion.
 */
static void
split_variants(Game *game, FILE *outputfile, unsigned depth)
{
    /* Gather all the suffix comments at this level. */
    Move *move = game->moves;
    while(move != NULL) {
        Variation *variants = move->Variants;
        while (variants != NULL) {
            if(variants->suffix_comment != NULL) {
                move->comment_list = append_comment(variants->suffix_comment, move->comment_list);
                variants->suffix_comment = NULL;
            }
            variants = variants->next;
        }
        move = move->next;
    }

    /* Format the main line at this level. */
    format_game(game, outputfile);

    if(GlobalState.split_depth_limit == 0 ||
           GlobalState.split_depth_limit > depth) {
        /* Now all the variations. */
        char *result_tag = game->tags[RESULT_TAG];
        game->tags[RESULT_TAG] = copy_string("*");
        move = game->moves;
        Move *prev = NULL;
        while(move != NULL) {
            Variation *variants = move->Variants;
            while (variants != NULL) {
                Variation *next_variant = variants->next;
                Move *variant_moves = variants->moves;
                if(variant_moves != NULL) {
                    /* Supply a result if it is missing. */
                    Move *last_move = variant_moves;
                    while(last_move->next != NULL) {
                        last_move = last_move->next;
                    }
                    if(last_move->terminating_result == NULL) {
                        last_move->terminating_result = copy_string("*");
                    }
                    /* Replace the main line with the variants. */
                    if(prev != NULL) {
                        prev->next = variant_moves;
                    }
                    else {
                        game->moves = variant_moves;
                    }
                    /* Detach following variations. */
                    variants->next = NULL;
                    CommentList *prefix_comment = variants->prefix_comment;
                    if(prefix_comment != NULL) {
                        if(prev != NULL) {
                            prev->comment_list = append_comment(prefix_comment, prev->comment_list);
                        }
                        else {
                            game->prefix_comment = append_comment(prefix_comment,
                                                                  game->prefix_comment);
                        }
                    }
                    split_variants(game, outputfile, depth+1);
                    if(prefix_comment != NULL) {
                        /* Remove the appended comments. */
                        CommentList *list;
                        if(prev != NULL) {
                            list  = prev->comment_list;
                        }
                        else {
                            list = game->prefix_comment;
                        }
                        if(list == prefix_comment) {
                            if(prev != NULL) {
                                prev->comment_list = NULL;
                            }
                            else {
                                game->prefix_comment = NULL;
                            }
                        }
                        else {
                            while(list->next != prefix_comment && list->next != NULL) {
                                list = list->next;
                            }
                            list->next = NULL;
                        }
                    }
                    variants->next = next_variant;
                }
                variants = next_variant;
            }
            if(move->Variants != NULL) {
                /* The variation can now be disposed of. */
                free_variation(move->Variants);
                move->Variants = NULL;
                /* Restore the move replaced by its variants. */
                if(prev != NULL) {
                    prev->next = move;
                }
                else {
                    game->moves = move;
                }
            }
            prev = move;
            move = move->next;
        }
        /* Put everything back as it was. */
        (void) free((void *) game->tags[RESULT_TAG]);
        game->tags[RESULT_TAG] = result_tag;
    }
}

static void
deal_with_ECO_line(Move *move_list)
{
    Game current_game;
    /* We need to know the length of a game to store with the
     * hash information as a sanity check.
     */
    unsigned number_of_half_moves;

    /* Fill in the information currently known. */
    current_game.tags = GameHeader.Tags;
    current_game.tags_length = GameHeader.header_tags_length;
    current_game.prefix_comment = GameHeader.prefix_comment;
    current_game.moves = move_list;
    current_game.moves_checked = FALSE;
    current_game.moves_ok = FALSE;
    current_game.error_ply = 0;

    /* apply_eco_move_list checks out the moves.
     * It will also fill in the
     *                 current_game.final_hash_value and
     *                 current_game.cumulative_hash_value
     * fields of current_game.
     */
    Board *final_position = apply_eco_move_list(&current_game, &number_of_half_moves);
    if(final_position != NULL) {
        /* Store the ECO code in the appropriate hash location. */
        save_eco_details(&current_game, final_position, number_of_half_moves);
    }

    /* Game is finished with, so free everything. */
    if (GameHeader.prefix_comment != NULL) {
        free_comment_list(GameHeader.prefix_comment);
    }
    /* Ensure that the GameHeader's prefix comment is NULL for
     * the next game.
     */
    GameHeader.prefix_comment = NULL;

    free_tags();
    free_move_list(current_game.moves);
}

/* If file_type == ECOFILE we are dealing with a file of ECO
 * input rather than a normal game file.
 */
int
yyparse(SourceFileType file_type)
{
    setup_for_new_game();
    current_symbol = skip_to_next_game(NO_TOKEN);
    parse_opt_game_list(file_type);
    if (current_symbol == EOF_TOKEN) {
        /* Ok -- EOF. */
        return 0;
    }
    else if (finished_processing()) {
        /* Ok -- done all we need to. */
        return 0;
    }
    else {
        fprintf(GlobalState.logfile, "End of input reached before end of file.\n");
        return 1;
    }
}

