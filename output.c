/*
 *  This file is part of pgn-extract: a Portable Game Notation (PGN) extractor.
 *  Copyright (C) 1994-2025 David J. Barnes
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
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <inttypes.h>
#include "bool.h"
#include "defs.h"
#include "typedef.h"
#include "taglist.h"
#include "tokens.h"
#include "lex.h"
#include "grammar.h"
#include "apply.h"
#include "output.h"
#include "mymalloc.h"


/* Functions for outputting games in the required format. */

/* Define the width in which to print a CM move and move number. */
#define MOVE_NUMBER_WIDTH 3
#define MOVE_WIDTH 15
#define CM_COMMENT_CHAR ';'
/* Define the width of the moves area before a comment. */
#define COMMENT_INDENT (MOVE_NUMBER_WIDTH+2+2*MOVE_WIDTH)

/* Define a macro to calculate an array's size. */
#define ARRAY_SIZE(arr) (sizeof(arr)/sizeof(*arr))

/* A small size for formatted move numbers. */
#define FORMATTED_NUMBER_SIZE (20)

/* How much text we have output on the current line. */
static size_t line_length = 0;
/* The buffer in which each output line of a game is built. */
static char *output_line = NULL;

static Boolean print_move(FILE *outputfile, unsigned move_number,
        Boolean print_move_number, Boolean white_to_move,
        const Move *move_details);
static Boolean
print_items_following_move(FILE *outputfile, const Move *move_details,
        unsigned move_number, Boolean white_to_move);
static void output_STR(FILE *outfp, char **Tags);
static void show_tags(FILE *outfp, char **Tags, int tags_length);
static char promoted_piece_letter(Piece piece);
static void print_algebraic_game(Game *current_game, FILE *outputfile,
        unsigned move_number, Boolean white_to_move,
        Board *final_board);
static void print_EPD_game(Game *current_game, FILE *outputfile,
        unsigned move_number, Boolean white_to_move,
        Board *final_board);
static void print_FEN_game(Game *current_game, FILE *outputfile,
        unsigned move_number, Boolean white_to_move,
        Board *final_board);
static void print_EPD_move_list(Game *current_game, FILE *outputfile,
        unsigned move_number, Boolean white_to_move,
        Board *final_board);
static void print_FEN_move_list(Game *current_game, FILE *outputfile,
        unsigned move_number, Boolean white_to_move,
        Board *final_board);
static const char *build_FEN_comment(const Board *board);
static void add_hashcode_tag(const Game *game);
static unsigned count_single_move_ply(const Move *move_details, Boolean count_variations);
static unsigned count_move_list_ply(Move *move_list, Boolean count_variations);
static void print_space_separated_str(FILE *outputfile, const char *str);
static void start_comment(FILE *outputfile);
static void end_comment(FILE *outputfile);
static void print_as_comment(FILE *outputfile, const char *str);
static CommentList *create_line_number_comment(const Game *game);

/* List, the order in which the tags should be output.
 * The first seven should be the Seven Tag Roster that should
 * be present in every game.
 * The order of the remainder is, I believe, a matter of taste.
 * any PSEUDO_*_TAGs should not appear in this list.
 */

/* Give the array an int type, because a negative value is
 * used as a terminator.
 */
static int DefaultTagOrder[] = {
    EVENT_TAG, SITE_TAG, DATE_TAG, ROUND_TAG, WHITE_TAG, BLACK_TAG, RESULT_TAG,
#if 1
    /* @@@ Consider omitting some of these from the default ordering,
     * and allow the output order to be determined from the
     * input order.
     */
    WHITE_TITLE_TAG, BLACK_TITLE_TAG, WHITE_ELO_TAG, BLACK_ELO_TAG,
    WHITE_USCF_TAG, BLACK_USCF_TAG,
    WHITE_TYPE_TAG, BLACK_TYPE_TAG,
    WHITE_NA_TAG, BLACK_NA_TAG,
    ECO_TAG, NIC_TAG, OPENING_TAG, VARIATION_TAG, SUB_VARIATION_TAG,
    LONG_ECO_TAG,
    TIME_CONTROL_TAG,
    ANNOTATOR_TAG,
    EVENT_DATE_TAG, EVENT_SPONSOR_TAG, SECTION_TAG, STAGE_TAG, BOARD_TAG,
    TIME_TAG, UTC_DATE_TAG, UTC_TIME_TAG,
    VARIANT_TAG,
    SETUP_TAG,
    FEN_TAG,
    TERMINATION_TAG, MODE_TAG, PLY_COUNT_TAG,
    MATERIAL_MATCH_TAG,
#endif
    /* The final value should be negative. */
    -1
};

/* Provision for a user-defined tag ordering.
 * See add_to_output_tag_order().
 * Once allocated, the end of the list must be negative.
 */
static int *TagOrder = NULL;
static int tag_order_space = 0;

void
set_output_line_length(unsigned length)
{
    if (output_line != NULL) {
        (void) free((void *) output_line);
    }
    output_line = (char *) malloc_or_die(length + 1);
    GlobalState.max_line_length = length;
}

/* Which output format does the user require, based upon the
 * given command line argument?
 */
OutputFormat
which_output_format(const char *arg)
{
    int i;

    struct {
        const char *arg;
        OutputFormat format;
    } formats[] = {
        { "san", SAN},
        { "SAN", SAN},
        { "epd", EPD},
        { "EPD", EPD},
        { "fen", FEN},
        { "FEN", FEN},
        { "lalg", LALG},
        { "halg", HALG},
        { "CM", CM},
        { "LALG", LALG},
        { "HALG", HALG},
        { "ELALG", ELALG},
        { "elalg", ELALG},
        { "XLALG", XLALG},
        { "xlalg", XLALG},
        { "XOLALG", XOLALG},
        { "xolalg", XOLALG},
        { "uci", UCI},
        { "cm", CM},
        { "", SOURCE},
        /* Add others before the terminating NULL. */
        { (const char *) NULL, SAN}
    };

    for (i = 0; formats[i].arg != NULL; i++) {
        const char *format_prefix = formats[i].arg;
        const size_t format_prefix_len = strlen(format_prefix);
        if (strncmp(arg, format_prefix, format_prefix_len) == 0) {
            OutputFormat format = formats[i].format;
            /* Sanity check. */
            if (*format_prefix == '\0' && *arg != '\0') {
                fprintf(GlobalState.logfile,
                        "Unknown output format %s.\n", arg);
                exit(1);
            }
            /* If the format is SAN, it is possible to supply
             * a 6-piece suffix listing language-specific
             * letters to use in the output.
             */
            if ((format == SAN ||
                 format == ELALG ||
                 format == XLALG ||
                 format == XOLALG) &&
                (strlen(arg) > format_prefix_len)) {
                set_output_piece_characters(&arg[format_prefix_len]);
            }
            return format;
        }
    }
    fprintf(GlobalState.logfile, "Unknown output format %s.\n", arg);
    return SAN;
}

/* Which file suffix should be used for this output format. */
const char *
output_file_suffix(OutputFormat format)
{
    /* Define a suffix for the output files. */
    static const char PGN_suffix[] = ".pgn";
    static const char EPD_suffix[] = ".epd";
    static const char FEN_suffix[] = ".fen";
    static const char CM_suffix[] = ".cm";

    switch (format) {
        case SOURCE:
        case SAN:
        case LALG:
        case HALG:
        case ELALG:
        case XLALG:
        case XOLALG:
        case UCI:
            return PGN_suffix;
        case EPD:
            return EPD_suffix;
        case FEN:
            return FEN_suffix;
        case CM:
            return CM_suffix;
        default:
            return PGN_suffix;
    }
}

static const char *
select_tag_string(TagName tag)
{
    const char *tag_string;

    if ((tag == PSEUDO_PLAYER_TAG) || 
            (tag == PSEUDO_ELO_TAG) || 
            (tag == PSEUDO_FEN_PATTERN_TAG) ||
            (tag == PSEUDO_FEN_PATTERN_I_TAG)) {
        tag_string = NULL;
    }
    else {
        tag_string = tag_header_string(tag);
    }
    return tag_string;
}

static Boolean
is_STR(TagName tag)
{
    switch (tag) {
        case EVENT_TAG:
        case SITE_TAG:
        case DATE_TAG:
        case ROUND_TAG:
        case WHITE_TAG:
        case BLACK_TAG:
        case RESULT_TAG:
            return TRUE;
        default:
            return FALSE;
    }
}

/* Output the tags held in the Tags structure.
 * The full Seven Tag Roster is printed unless
 * an element is explicitly suppressed..
 */
static void
output_tag(TagName tag, char **Tags, FILE *outfp)
{
    const char *tag_string;

    if(is_suppressed_tag(tag)) {
    }
    else if ((is_STR(tag)) || (Tags[tag] != NULL)) {
        /* Must print STR elements and other non-NULL tags. */
        tag_string = select_tag_string(tag);

        if (tag_string != NULL) {
            const char *tag_value;
            if (Tags[tag] != NULL) {
                tag_value = Tags[tag];
            }
            else {
                if (tag == DATE_TAG) {
                    tag_value = "????.??.??";
                }
                else {
                    tag_value = "?";
                }
            }
            if (GlobalState.json_format) {
                fprintf(outfp, "\"%s\" : \"%s\",\n", tag_string, tag_value);
            }
            else {
                fprintf(outfp, "[%s \"%s\"]\n", tag_string, tag_value);
            }
        }
    }
}

/* Output the Seven Tag Roster. */
static void
output_STR(FILE *outfp, char **Tags)
{
    unsigned tag_index;

    /* Use the default ordering to ensure that STR is output
     * in the way it should be.
     */
    for (tag_index = 0; tag_index < 7; tag_index++) {
        output_tag(DefaultTagOrder[tag_index], Tags, outfp);
    }
    /* @@@ NB: Strictly speaking, a game with a FEN tag would not be
     * meaningful without including it in the output but, historically,
     * that has never been done and no one has pointed that out.
     * So things have been left as they are until such time as it is
     * requested!
     */
}

/* Print out on outfp the current details.
 * These can be used in the case of an error.
 */
static void
show_tags(FILE *outfp, char **Tags, int tags_length)
{
    int tag_index;
    /* Take a copy of the Tags data, so that we can keep
     * track of what has been printed. This will make
     * it possible to print tags that were identified
     * in the source but are not defined with _TAG values.
     * See lex.c for how these extra tags are handled.
     */
    char **copy_of_tags =
            (char **) malloc_or_die(tags_length * sizeof (*copy_of_tags));
    int i;
    for (i = 0; i < tags_length; i++) {
        copy_of_tags[i] = Tags[i];
    }

    /* Ensure that a tag ordering is available. */
    if (TagOrder == NULL) {
        /* None set by the user - use the default. */
        /* Handle the standard tags.
         * The end of the list is marked with a negative value.
         */
        for (tag_index = 0; DefaultTagOrder[tag_index] >= 0; tag_index++) {
            TagName tag = DefaultTagOrder[tag_index];
            output_tag(tag, copy_of_tags, outfp);
            copy_of_tags[tag] = (char *) NULL;
        }
    }
    else {
        for (tag_index = 0; TagOrder[tag_index] >= 0; tag_index++) {
            TagName tag = TagOrder[tag_index];
            output_tag(tag, copy_of_tags, outfp);
            copy_of_tags[tag] = (char *) NULL;
        }
    }
    /* Handle the remaining tags. */
    if(! GlobalState.only_output_wanted_tags) {
        for (tag_index = 0; tag_index < tags_length; tag_index++) {
            if (copy_of_tags[tag_index] != NULL) {
                output_tag(tag_index, copy_of_tags, outfp);
            }
        }
    }
    (void) free(copy_of_tags);
    putc('\n', outfp);
}

/* Ensure that there is room for len more characters on the
 * current line.
 */
static void
check_line_length(FILE *fp, size_t len)
{
    if ((line_length + len) > GlobalState.max_line_length) {
        terminate_line(fp);
    }
}

/* Print ch to fp and update how much of the line
 * has been printed on.
 */
static void
print_single_char(FILE *fp, char ch)
{
    check_line_length(fp, 1);
    output_line[line_length] = ch;
    line_length++;
}

/* Print a space, unless at the beginning of a line. */
static void
print_separator(FILE *fp)
{
    /* Lines shouldn't have trailing spaces, so ensure that there
     * will be room for at least one more character after the space.
     */
    check_line_length(fp, 2);
    if (line_length != 0 && output_line[line_length - 1] != ' ') {
        output_line[line_length] = ' ';
        line_length++;
    }
}

/* Ensure that what comes next starts on a fresh line. */
void
terminate_line(FILE *fp)
{
    /* Delete any trailing space(s). */
    while (line_length >= 1 && output_line[line_length - 1] == ' ') {
        line_length--;
    }
    if (line_length > 0) {
        output_line[line_length] = '\0';
        fprintf(fp, "%s\n", output_line);
        line_length = 0;
    }
}

/* Print str to fp and update how much of the line
 * has been printed on.
 */
void
print_str(FILE *fp, const char *str)
{
    size_t len = strlen(str);

    check_line_length(fp, len);
    if (len > GlobalState.max_line_length) {
        fprintf(GlobalState.logfile,
                "String length %lu is too long for the line length of %lu:\n",
                (unsigned long) len,
                (unsigned long) GlobalState.max_line_length);
        fprintf(GlobalState.logfile, "%s\n", str);
        report_details(GlobalState.logfile);
        fprintf(fp, "%s\n", str);
    }
    else {
        sprintf(&(output_line[line_length]), "%s", str);
        line_length += len;
    }
}

/* Print the given str in separate space-separated
 * pieces to take account of line-breaks.
 * The str should not contain newline characters.
 */
static void
print_space_separated_str(FILE *fp, const char *str)
{
    char *copy = copy_string(str);
    const char *chunk = strtok(copy, " ");
    while (chunk != NULL) {
        print_str(fp, chunk);
        chunk = strtok((char *) NULL, " ");
        if(chunk != NULL) {
            print_separator(fp);
        }
    }
    (void) free((void *) copy);
}

static void
print_comment_list(FILE *fp, CommentList *comment_list)
{
    CommentList *next_comment;

    for (next_comment = comment_list; next_comment != NULL;
            next_comment = next_comment->next) {
        StringList *comment = next_comment->comment;

        if (comment != NULL) {
            start_comment(fp);
            for (; comment != NULL; comment = comment->next) {
                print_space_separated_str(fp, comment->str);
                if(comment->next != NULL) {
                    print_separator(fp);
                }
            }
            end_comment(fp);
        }
    }
}

static void
print_move_list(FILE *outputfile, unsigned move_number, Boolean white_to_move,
        const Move *move_details, const Board *final_board)
{
    Boolean print_move_number = TRUE;
    const Move *move = move_details;
    Boolean keepPrinting;
    int plies;
    /* Keep track of the number of consecutive quiescent moves:
     * captures, Checks and promotion are non-quiescent.
     * @@@ NB: This does not strictly apply to just the main line
     * and could trigger quiescence in a variation, which is undesirable.
     */
    unsigned quiescense_count = 0;

    /* Work out the ply depth. */
    plies = 2 * (move_number) - 1;
    if (!white_to_move) {
        plies++;
    }
    if (GlobalState.output_ply_limit >= 0 &&
            plies > GlobalState.output_ply_limit) {
        keepPrinting = FALSE;
    }
    else {
        keepPrinting = TRUE;
    }

    while (move != NULL && keepPrinting) {
        if (GlobalState.json_format) {
            fputs("{ ", outputfile);
        }
        /* Reset print_move number if a variation was printed. */
        print_move_number = print_move(outputfile, move_number,
                print_move_number,
                white_to_move, move);

        /* See if there is a result attached.  This may be attached either
         * to a move or a comment.
         */
        if (!GlobalState.check_only && (move != NULL) &&
                (move->terminating_result != NULL)) {
            if (GlobalState.output_FEN_string && 
                    GlobalState.FEN_comment_pattern == NULL &&
                    final_board != NULL) {
                if(GlobalState.json_format) {
                    if(!GlobalState.add_FEN_comments) {
                        char *fen = get_FEN_string(final_board);
                        fprintf(outputfile, ", \"FEN\" : \"%s\" ", fen);
                        (void) free((void *) fen);
                    }
                    else {
                        /* The final FEN position will have been output anyway. */
                    }
                }
                else {
                    print_separator(outputfile);
                    const char *comment = build_FEN_comment(final_board);
                    print_str(outputfile, comment);
                    (void) free((void *) comment);
                }
            }
            if (GlobalState.keep_results) {
                print_separator(outputfile);
                print_str(outputfile, move->terminating_result);
            }
        }
        if (move->move[0] != '\0') {
            /* A genuine move was just printed, rather than a comment. */
            if (white_to_move) {
                white_to_move = FALSE;
            }
            else {
                move_number++;
                white_to_move = TRUE;
            }
            plies++;
            if (move->captured_piece != EMPTY ||
                    move->check_status != NOCHECK ||
                    move->promoted_piece != EMPTY) {
                quiescense_count = 0;
            }
            else {
                quiescense_count++;
            }
            if (GlobalState.output_ply_limit >= 0 &&
                    plies > GlobalState.output_ply_limit &&
                    quiescense_count >= GlobalState.quiescence_threshold) {
                keepPrinting = FALSE;
            }
        }
        if (GlobalState.json_format) {
            fputs(" }", outputfile);
        }
        move = move->next;
        /* The following is slightly inaccurate.
         * If the previous value of move was a comment and
         * we aren't printing comments, then this results in two
         * separators being printed after the move preceding the comment.
         * Not sure how to cleanly fix it, because there might have
         * been nags attached to the comment that were printed, for instance!
         */
        if (move != NULL && keepPrinting) {
            if (GlobalState.json_format) {
                fputs(", ", outputfile);
            }
            else {
                print_separator(outputfile);
            }
        }
    }

    if (move != NULL && !keepPrinting) {
        /* We ended printing the game prematurely.
         *
         * Decide whether to print a result indicator.
         */
        if (GlobalState.keep_results) {
            /* Find the final move to see if there was a result there. */
            while (move->next != NULL) {
                move = move->next;
            }
            if (move->terminating_result != NULL) {
                print_separator(outputfile);
                print_str(outputfile, "*");
            }
        }
    }
}

/* Output the current move along with associated information.
 * Return TRUE if either a variation or comment was printed,
 * FALSE otherwise.
 * This is needed to determine whether a new move number
 * is to be printed after a variation.
 */
/* A length to accommodate move numbers. */
#define SMALL_MOVE_NUMBER_LENGTH (20)

static Boolean
print_move(FILE *outputfile, unsigned move_number, Boolean print_move_number,
        Boolean white_to_move, const Move *move_details)
{
    Boolean something_printed = FALSE;
    OutputFormat output_format = GlobalState.output_format;

    if (move_details == NULL) {
        /* Shouldn't happen. */
        fprintf(GlobalState.logfile,
                "Internal error: NULL move in print_move.\n");
        report_details(GlobalState.logfile);
    }
    else {
        if (GlobalState.check_only) {
            /* Nothing to be output. */
        }
        else {
            const unsigned char *move_text = move_details->move;
            /* What move text to print. */
            char *move_to_print;

            if (*move_text != '\0') {
                if (GlobalState.keep_move_numbers &&
                        (white_to_move || print_move_number)) {
                    static char small_number[SMALL_MOVE_NUMBER_LENGTH];

                    /* @@@ Should 1... be written as 1. ... ? */
                    sprintf(small_number,
                            "%u.%s", move_number,
                            white_to_move ? "" : "..");
                    print_str(outputfile, small_number);
                    print_separator(outputfile);
                }
                switch (output_format) {
                    case SAN:
                    case SOURCE:
                        /* @@@ move_text should be handled as unsigned
                         * char text, as the source may be 8-bit rather
                         * than 7-bit.
                         */
                        move_to_print = copy_string((const char *) move_text);
                        if (!GlobalState.keep_checks) {
                            /* Look for a check or mate symbol. */
                            char *check = strchr((const char *) move_text, '+');
                            if (check == NULL) {
                                check = strchr((const char *) move_text, '#');
                            }
                            if (check != NULL) {
                                /* We need to drop it from move_text. */
                                int len = check - ((char *) move_text);
                                move_to_print[len] = '\0';
                            }
                        }
                        break;
                    case HALG:
                    {
                        char algebraic[MAX_MOVE_LEN + 1];

                        *algebraic = '\0';
                        switch (move_details->class) {
                            case PAWN_MOVE:
                            case ENPASSANT_PAWN_MOVE:
                            case KINGSIDE_CASTLE:
                            case QUEENSIDE_CASTLE:
                            case PIECE_MOVE:
                                sprintf(algebraic,
                                        "%c%c-%c%c",
                                        move_details->from_col,
                                        move_details->from_rank,
                                        move_details->to_col,
                                        move_details->to_rank);
                                break;
                            case PAWN_MOVE_WITH_PROMOTION:
                                sprintf(algebraic,
                                        "%c%c-%c%c%c",
                                        move_details->from_col,
                                        move_details->from_rank,
                                        move_details->to_col,
                                        move_details->to_rank,
                                        promoted_piece_letter(move_details->promoted_piece));
                                break;
                            case NULL_MOVE:
                                strcpy(algebraic, NULL_MOVE_STRING);
                                break;
                            case UNKNOWN_MOVE:
                                strcpy(algebraic, "???");
                                break;
                        }
                        if (GlobalState.keep_checks) {
                            switch (move_details->check_status) {
                                case NOCHECK:
                                    break;
                                case CHECK:
                                    strcat(algebraic, "+");
                                    break;
                                case CHECKMATE:
                                    strcat(algebraic, "#");
                                    break;
                            }
                        }
                        move_to_print = copy_string(algebraic);
                    }
                        break;
                    case LALG:
                    case ELALG:
                    case XLALG:
                    case XOLALG:
                    case UCI:
                    {
                        char algebraic[MAX_MOVE_LEN + 1];
                        size_t ind = 0;

                        if(output_format == XOLALG &&
                           (move_details->class == KINGSIDE_CASTLE ||
                            move_details->class == QUEENSIDE_CASTLE)) {
                            strcpy(algebraic, (char *) move_text);
                            ind = strlen((char *) algebraic);
                            /* Remove any check/mate symbol. */
                            if(algebraic[ind-1] == '+' || algebraic[ind-1] == '#') {
                                algebraic[ind-1] = '\0';
                                ind--;
                            }
                        }
                        else {
                            /* Prefix with a piece name if ELALG. */
                            if ((output_format == ELALG ||
                                 output_format == XLALG ||
                                 output_format == XOLALG) &&
                                    move_details->class == PIECE_MOVE) {
                                strcpy(algebraic,
                                        piece_str(move_details->piece_to_move));
                                ind = strlen(algebraic);
                            }
                            /* Format the basics. */
                            if (move_details->class != NULL_MOVE) {
                                sprintf(&algebraic[ind],
                                        "%c%c",
                                        move_details->from_col,
                                        move_details->from_rank);

                                ind += 2;
                                if (output_format == XLALG ||
                                    output_format == XOLALG) {
                                    /* Add a separating - or x. */
                                    char separator;
                                    if (move_details->captured_piece != EMPTY) {
                                        separator = 'x';
                                    }
                                    else {
                                        separator = '-';
                                    }
                                    sprintf(&algebraic[ind],
                                            "%c", separator);
                                    ind++;
                                }
                                sprintf(&algebraic[ind],
                                        "%c%c",
                                        move_details->to_col,
                                        move_details->to_rank);
                                ind += 2;
                            }
                            else {
                                strcpy(algebraic, NULL_MOVE_STRING);
                                ind += strlen(NULL_MOVE_STRING);
                            }
                            switch (move_details->class) {
                                case PAWN_MOVE:
                                case KINGSIDE_CASTLE:
                                case QUEENSIDE_CASTLE:
                                case PIECE_MOVE:
                                case NULL_MOVE:
                                    /* Nothing more to do at this stage. */
                                    break;
                                case ENPASSANT_PAWN_MOVE:
                                    if (output_format == ELALG ||
				          output_format == XLALG ||
					  output_format == XOLALG) {
                                        strcat(algebraic, "ep");
                                        ind += 2;
                                    }
                                    break;
                                case PAWN_MOVE_WITH_PROMOTION:
                                    sprintf(&algebraic[ind],
                                            "%s",
                                            piece_str(move_details->promoted_piece));
                                    ind = strlen(algebraic);
                                    break;
                                case UNKNOWN_MOVE:
                                    strcpy(algebraic, "???");
                                    ind += 3;
                                    break;
                            }
                        }
                        if (GlobalState.keep_checks) {
                            switch (move_details->check_status) {
                                case NOCHECK:
                                    break;
                                case CHECK:
                                    strcat(algebraic, "+");
                                    ind++;
                                    break;
                                case CHECKMATE:
                                    strcat(algebraic, "#");
                                    ind++;
                                    break;
                            }
                        }
                        move_to_print = copy_string(algebraic);
                    }
                        break;
                    default:
                        fprintf(GlobalState.logfile,
                                "Unknown output format %d in print_move()\n",
                                output_format);
                        exit(1);
                        move_to_print = NULL;
                        break;
                }
            }
            else {
                /* An empty move. */
                fprintf(GlobalState.logfile,
                        "Internal error: Empty move in print_move.\n");
                report_details(GlobalState.logfile);
                move_to_print = NULL;
            }
            if (GlobalState.json_format) {
                fputs("\"move\" : ", outputfile);
                fputs("\"", outputfile);
                if (move_to_print != NULL) {
                    fputs(move_to_print, outputfile);
                }
                fputs("\"", outputfile);
            }
            else {
                if (move_to_print != NULL) {
                    print_str(outputfile, move_to_print);
                }
            }
            if (move_to_print != NULL) {
                (void) free(move_to_print);
            }
            if(print_items_following_move(outputfile, move_details, move_number, white_to_move)) {
                something_printed = TRUE;
            }            
        }
    }
    return something_printed;
}

/* Maximum length of a 64-bit unsigned int in decimal. 
 * NB: At the moment, the output is hex, which requires
 * only 16 characters.
 */
#define HASH_64_BIT_SPACE 20

/* Print further information, that may be attached to moves,
 * such as NAGs and comments.
 * Return TRUE if something was printed for non JSON format output.
 */
static Boolean
print_items_following_move(FILE *outputfile, const Move *move_details,
        unsigned move_number, Boolean white_to_move)
{
    Boolean something_printed = FALSE;
    Nag *nags = move_details->NAGs;
    Variation *variants = move_details->Variants;
    if (move_details->comment_list != NULL && GlobalState.keep_comments) {
        print_comment_list(outputfile, move_details->comment_list);
        something_printed = TRUE;
    }
    if(nags != NULL) {
        /* We don't need to output move numbers after just
         * NAGs, so don't set something_printed just for NAGs.
         */
        if (GlobalState.keep_NAGs && GlobalState.json_format) {
            fputs(", \"nags\" : [", outputfile);
        }
        while (nags != NULL) {
            if(GlobalState.keep_NAGs) {
                StringList *text = nags->text;
                while(text != NULL) {
                    if(GlobalState.json_format) {
                        fprintf(outputfile, "\"%s\"", text->str);
                        if(nags->next != NULL) {
                            fputs(", ", outputfile);
                        }
                    }
                    else {
                        print_separator(outputfile);
                        print_str(outputfile, text->str);
                    }
                    text = text->next;
                }
            }
            if(nags->comments != NULL && GlobalState.keep_comments) {
                // @@@ JSON option needed.
                
                print_comment_list(outputfile, nags->comments);
                something_printed = TRUE;
            }
            nags = nags->next;
        }
        if(GlobalState.keep_NAGs && GlobalState.json_format) {
            fputs("] ", outputfile);
        }
    }
    if (GlobalState.output_evaluation) {
        if(GlobalState.json_format) {
            fprintf(outputfile, ", \"evaluation\" : \"%.2f\"", 
                    move_details->evaluation);
        } 
        else {
            const char valueSpace[] = "-012456789.00";
            char *evaluation = (char *) malloc_or_die(sizeof (valueSpace));
            sprintf(evaluation, "%.2f", move_details->evaluation);
            if (strlen(evaluation) > strlen(valueSpace)) {
                fprintf(GlobalState.logfile,
                        "Internal error: Overflow in evaluation space in print_items_following_move()\n");
                exit(1);
            }
            
            print_as_comment(outputfile, evaluation);
            (void) free((void *) evaluation);
            something_printed = TRUE;
        }
    }
    if(GlobalState.add_FEN_comments) {
        if(move_details->epd != NULL && move_details->fen_suffix != NULL) {
            if(GlobalState.json_format) {
                fprintf(outputfile, ", \"FEN\" : \"%s %s\"", 
                        move_details->epd,
                        move_details->fen_suffix);
            }
            else {
                start_comment(outputfile);
                print_space_separated_str(outputfile, move_details->epd);
                print_separator(outputfile);
                print_space_separated_str(outputfile, move_details->fen_suffix);
                end_comment(outputfile);
                something_printed = TRUE;
            }
        }
    }
    if (GlobalState.add_hashcode_comments) {
        if(GlobalState.json_format) {
            fprintf(outputfile, ", \"HashCode\" : \"");
            fprintf(outputfile, "%016" PRIx64, move_details->zobrist);
            fprintf(outputfile, "\"");
        }
        else {
            char *hashcode = (char *) malloc_or_die(HASH_64_BIT_SPACE + 1);
            sprintf(hashcode, "%016" PRIx64, move_details->zobrist);
            print_as_comment(outputfile, hashcode);
            (void) free((void *) hashcode);
            something_printed = TRUE;
        }
    }
    if (variants != NULL) {
        if(GlobalState.keep_variations) {
            if(!GlobalState.split_variants) {
                while (variants != NULL) {
                    print_separator(outputfile);
                    print_single_char(outputfile, '(');
                    if (GlobalState.keep_comments &&
                            (variants->prefix_comment != NULL)) {
                        print_comment_list(outputfile, variants->prefix_comment);
                        print_separator(outputfile);
                    }
                    /* Always start with a move number.
                     * The final board position is not needed.
                     */
                    print_move_list(outputfile, move_number,
                            white_to_move, variants->moves,
                            (const Board *) NULL);
                    print_single_char(outputfile, ')');
                    if (GlobalState.keep_comments &&
                            (variants->suffix_comment != NULL)) {
                        print_comment_list(outputfile, variants->suffix_comment);
                    }
                    variants = variants->next;
                }
                something_printed = TRUE;
            }
            else {
                /* Variations are being split so don't output them. */
            }
        }
        else if(GlobalState.keep_comments) {
            /* Avoid losing suffix comments of variations.
             * NB: @@@ I am not entirely convinced that comments that
             * follow variations actually belong with the move immediately
             * preceding the variation. However, this is one way to avoid
             * their being lost if variations are not being output.
             */
            while(variants != NULL) {
                if(variants->suffix_comment != NULL) {
                    print_comment_list(outputfile, variants->suffix_comment);
                    something_printed = TRUE;
                }
                variants = variants->next;
            }
        }
    }
    return something_printed;
}

/* Start a comment, taking into account separators
 * and possible placement of comments on separate lines.
 */
static void
start_comment(FILE *outputfile)
{
    if(GlobalState.separate_comment_lines) {
        terminate_line(outputfile);
    }
    else {
        print_separator(outputfile);
    }
    print_single_char(outputfile, '{');
    print_separator(outputfile);
}

/* End a comment, taking into account separators
 * and possible placement of comments on separate lines.
 */
static void
end_comment(FILE *outputfile)
{
    print_separator(outputfile);
    print_single_char(outputfile, '}');
    if(GlobalState.separate_comment_lines) {
        terminate_line(outputfile);
    }
}

/* Print str as a comment. */
static void
print_as_comment(FILE *outputfile, const char *str)
{
    start_comment(outputfile);
    print_str(outputfile, str);
    end_comment(outputfile);
}

/* Return the letter associated with the given piece. */
static char
promoted_piece_letter(Piece piece)
{
    switch (piece) {
        case QUEEN:
            return 'Q';
        case ROOK:
            return 'R';
        case BISHOP:
            return 'B';
        case KNIGHT:
            return 'N';
        default:
            return '?';
    }
}

/* Output a comment in CM format. */
static void
output_cm_comment(CommentList *comment, FILE *outputfile, unsigned indent)
{ /* Don't indent for the first comment line, because
     * we should already be positioned at the correct spot.
     */
    unsigned indent_for_this_line = 0;

    putc(CM_COMMENT_CHAR, outputfile);
    line_length++;
    while (comment != NULL) {
        /* We will use strtok to break up the comment string,
         * with chunk to point to each bit in turn.
         */
        char *chunk;
        StringList *comment_str = comment->comment;

        for (; comment_str != NULL; comment_str = comment_str->next) {
            char *str = copy_string(comment_str->str);
            chunk = strtok(str, " ");
            while (chunk != NULL) {
                size_t len = strlen(chunk);

                if ((line_length + 1 + len) > GlobalState.max_line_length) {
                    /* Start a new line. */
                    fputc('\n', outputfile);
                    indent_for_this_line = indent;
                    for (unsigned in = 0; in < indent_for_this_line; in++) {
                        fputc(' ', outputfile);
                    }
                    fputc(CM_COMMENT_CHAR, outputfile);
                    fputc(' ', outputfile);
                    line_length = indent_for_this_line + 2;
                }
                else {
                    fputc(' ', outputfile);
                    line_length++;
                }
                fprintf(outputfile, "%s", chunk);
                line_length += len;
                chunk = strtok((char *) NULL, " ");
            }
            (void) free((void *) str);
        }
        comment = comment->next;
    }
    fputc('\n', outputfile);
    line_length = 0;
}

static void
output_cm_result(const char *result, FILE *outputfile)
{
    fprintf(outputfile, "%c ", CM_COMMENT_CHAR);
    if (strcmp(result, "1-0") == 0) {
        fprintf(outputfile, "and black resigns");
    }
    else if (strcmp(result, "0-1") == 0) {
        fprintf(outputfile, "and white resigns");
    }
    else if (strncmp(result, "1/2", 3) == 0) {
        fprintf(outputfile, "draw");
    }
    else {
        fprintf(outputfile, "incomplete result");
    }
}

/* Output the game in Chess Master format.
 * This is probably obsolete.
 */
static void
output_cm_game(FILE *outputfile, unsigned move_number,
        Boolean white_to_move, const Game *game)
{
    const Move *move = game->moves;

    if ((move_number != 1) || (!white_to_move)) {
        fprintf(GlobalState.logfile,
                "Unable to output CM games other than from the starting position.\n");
        report_details(GlobalState.logfile);
    }
    fprintf(outputfile, "WHITE: %s\n",
            game->tags[WHITE_TAG] != NULL ? game->tags[WHITE_TAG] : "");
    fprintf(outputfile, "BLACK: %s\n",
            game->tags[BLACK_TAG] != NULL ? game->tags[BLACK_TAG] : "");
    putc('\n', outputfile);

    if (game->prefix_comment != NULL) {
        line_length = 0;
        output_cm_comment(game->prefix_comment, outputfile, 0);
    }
    while (move != NULL) {
        if (move->move[0] != '\0') {
            /* A genuine move. */
            if (white_to_move) {
                fprintf(outputfile, "%*u. ", MOVE_NUMBER_WIDTH, move_number);
                fprintf(outputfile, "%*s", -MOVE_WIDTH, move->move);
                white_to_move = FALSE;
            }
            else {
                fprintf(outputfile, "%*s", -MOVE_WIDTH, move->move);
                move_number++;
                white_to_move = TRUE;
            }
        }
        if ((move->comment_list != NULL) && GlobalState.keep_comments) {
            const char *result = move->terminating_result;

            if (!white_to_move) {
                fprintf(outputfile, "%*s", -MOVE_WIDTH, "...");
            }
            line_length = COMMENT_INDENT;
            output_cm_comment(move->comment_list, outputfile, COMMENT_INDENT);
            if ((result != NULL) && (move->check_status != CHECKMATE)) {
                /* Give some information on the nature of the finish. */
                if (white_to_move) {
                    fprintf(outputfile, "%*s", COMMENT_INDENT, "");
                }
                else {
                    /* Print out a string representing the result. */
                    fprintf(outputfile, "%*s %*s%*s",
                            MOVE_NUMBER_WIDTH + 1, "", -MOVE_WIDTH, "...",
                            MOVE_WIDTH, "");
                }
                output_cm_result(result, outputfile);
                putc('\n', outputfile);
            }
            else {
                if (!white_to_move) {
                    /* Indicate that the next move is Black's. */
                    fprintf(outputfile, "%*s %*s",
                            MOVE_NUMBER_WIDTH + 1, "", -MOVE_WIDTH, "...");
                }
            }
        }
        else {
            if ((move->terminating_result != NULL) &&
                    (move->check_status != CHECKMATE)) {
                /* Give some information on the nature of the finish. */
                const char *result = move->terminating_result;

                if (!white_to_move) {
                    fprintf(outputfile, "%*s", -MOVE_WIDTH, "...");
                }
                output_cm_result(result, outputfile);
                if (!white_to_move) {
                    putc('\n', outputfile);
                    fprintf(outputfile, "%*s %*s",
                            MOVE_NUMBER_WIDTH + 1, "", -MOVE_WIDTH, "...");
                }
                putc('\n', outputfile);
            }
            else {
                if (white_to_move) {
                    /* Terminate the move pair. */
                    putc('\n', outputfile);
                }
            }
        }
        move = move->next;
    }
    putc('\n', outputfile);
}

/* Output the current game according to the required output format. */
void
format_game(Game *current_game, FILE *outputfile)
{
    Boolean white_to_move = TRUE;
    unsigned move_number = 1;
    Board *initial_board;
    /* The final board position, if available. */
    Board *final_board = NULL;

    if(GlobalState.line_number_marker != NULL) {
	CommentList *comment = create_line_number_comment(current_game);
	comment->next = current_game->prefix_comment;
	current_game->prefix_comment = comment;
    }

    /* We need a copy of the final board.
     * Combine the generation of this with a rewrite
     * of the moves of the game into
     * SAN (Standard Algebraic Notation) unless the original
     * source form is required.
     */
    final_board = rewrite_game(current_game);
    initial_board = new_game_board(current_game->tags[FEN_TAG]);

    /* If we aren't starting from the initial setup, then we
     * need to know the current move number and whose
     * move it is.
     */
    if (current_game->tags[FEN_TAG] != NULL && initial_board != NULL) {
        move_number = initial_board->move_number;
        white_to_move = initial_board->to_move == WHITE;
    }

    /* Start at the beginning of a line. */
    line_length = 0;
    
    if (final_board != NULL) {
        if (GlobalState.output_plycount) {
            add_plycount(current_game);
        }
        if (GlobalState.output_total_plycount) {
            add_total_plycount(current_game, GlobalState.keep_variations && !GlobalState.split_variants);
        }
        if (GlobalState.add_hashcode_tag || current_game->tags[HASHCODE_TAG] != NULL) {
            add_hashcode_tag(current_game);
        }
        switch (GlobalState.output_format) {
            case SAN:
            case SOURCE:
            case LALG:
            case HALG:
            case ELALG:
            case XLALG:
            case XOLALG:
            case UCI:
                print_algebraic_game(current_game, outputfile, move_number, white_to_move,
                        final_board);
                break;
            case EPD:
                print_EPD_game(current_game, outputfile, move_number, white_to_move,
                        initial_board);
                break;
            case FEN:
                print_FEN_game(current_game, outputfile, move_number, white_to_move,
                        initial_board);
                break;
            case CM:
                output_cm_game(outputfile, move_number, white_to_move, current_game);
                break;
            default:
                fprintf(GlobalState.logfile,
                        "Internal error: unknown output type %d in format_game().\n",
                        GlobalState.output_format);
                break;
        }
        fflush(outputfile);
        free_board(final_board);
    }
    free_board(initial_board);
}

/* Add the given tag to the output ordering. */
void
add_to_output_tag_order(TagName tag)
{
    int tag_index;

    if (TagOrder == NULL) {
        tag_order_space = ARRAY_SIZE(DefaultTagOrder);
        TagOrder = (int *) malloc_or_die(tag_order_space * sizeof (*TagOrder));
        /* Always ensure that there is a negative value at the end. */
        TagOrder[0] = -1;
    }
    /* Check to ensure a position has not already been indicated
     * for this tag.
     */
    for (tag_index = 0; (TagOrder[tag_index] != -1) &&
            (TagOrder[tag_index] != (int) tag); tag_index++) {
    }

    if (TagOrder[tag_index] == -1) {
        /* Make sure there is enough space for another. */
        if (tag_index >= tag_order_space) {
            /* Allocate some more. */
            tag_order_space += 10;
            TagOrder = (int *) realloc_or_die((void *) TagOrder,
                    tag_order_space * sizeof (*TagOrder));
        }
        TagOrder[tag_index] = tag;
        TagOrder[tag_index + 1] = -1;
    }
    else {
        fprintf(GlobalState.logfile, "Duplicate position for tag: %s\n",
                select_tag_string(tag));
    }
}

/* Format EPD comments containing tag details.
 * A c0 comment contains player and event info.
 * A c1 comment contains the game result.
 */
static const char *
format_epd_game_comment(char **Tags)
{
    static char c0_prefix[] = "c0 ";
    static char c1_prefix[] = "c1 ";
    static char player_separator[] = "-";
    static size_t prefix_and_separator_len =
            sizeof (c0_prefix) + sizeof (player_separator);
    size_t space_needed = prefix_and_separator_len;
    char *comment;

    if (Tags[WHITE_TAG] != NULL) {
        space_needed += strlen(Tags[WHITE_TAG]);
    }
    if (Tags[BLACK_TAG] != NULL) {
        space_needed += strlen(Tags[BLACK_TAG]);
    }
    /* Allow a space character before each of the remaining tags. */
    if (Tags[EVENT_TAG] != NULL) {
        space_needed += 1 + strlen(Tags[EVENT_TAG]);
    }
    if (Tags[SITE_TAG] != NULL) {
        space_needed += 1 + strlen(Tags[SITE_TAG]);
    }
    if (Tags[DATE_TAG] != NULL) {
        space_needed += 1 + strlen(Tags[DATE_TAG]);
    }
    /* Allow for a terminating semicolon after the c0 comment. */
    space_needed++;

    /* Allow for a space before the c1 comment. */
    space_needed++;
    space_needed += strlen(c1_prefix);
    if(Tags[RESULT_TAG] != NULL) {
        space_needed += 1 + strlen(Tags[RESULT_TAG]);
    }
    else {
        space_needed += 1 + strlen("*");
    }
    /* Allow for a terminating semicolon after the c1 comment. */
    space_needed++;

    comment = (char *) malloc_or_die(space_needed + 1);

    strcpy(comment, c0_prefix);
    if (Tags[WHITE_TAG] != NULL) {
        strcat(comment, Tags[WHITE_TAG]);
    }
    strcat(comment, player_separator);
    if (Tags[BLACK_TAG] != NULL) {
        strcat(comment, Tags[BLACK_TAG]);
    }
    if (Tags[EVENT_TAG] != NULL) {
        strcat(comment, " ");
        strcat(comment, Tags[EVENT_TAG]);
    }
    if (Tags[SITE_TAG] != NULL) {
        strcat(comment, " ");
        strcat(comment, Tags[SITE_TAG]);
    }
    if (Tags[DATE_TAG] != NULL) {
        strcat(comment, " ");
        strcat(comment, Tags[DATE_TAG]);
    }
    strcat(comment, "; ");
    strcat(comment, c1_prefix);
    if(Tags[RESULT_TAG] != NULL) {
        strcat(comment, Tags[RESULT_TAG]);
    }
    else {
        strcat(comment, "*");
    }
    strcat(comment, ";");

    if (strlen(comment) >= space_needed) {
        fprintf(GlobalState.logfile,
                "Internal error: overflow in format_epd_game_comment\n");
    }
    return comment;
}

static void
print_algebraic_game(Game *current_game, FILE *outputfile,
        unsigned move_number, Boolean white_to_move,
        Board *final_board)
{
    if (GlobalState.json_format) {
        /* Need to take account of splitting output over multiple files. */
        Boolean comma_needed;
        if (GlobalState.games_per_file == 0) {
            comma_needed = GlobalState.num_games_matched > 1;
        }
        else {
            comma_needed = GlobalState.games_per_file > 1 &&
                    (GlobalState.num_games_matched % GlobalState.games_per_file) != 1;
        }
        if (comma_needed) {
            fputs(",\n", outputfile);
        }
        fputs("{\n", outputfile);
    }
    /* Report details on the output. */
    if (GlobalState.tag_output_format == ALL_TAGS) {
        show_tags(outputfile, current_game->tags, current_game->tags_length);
    }
    else if (GlobalState.tag_output_format == SEVEN_TAG_ROSTER) {
        output_STR(outputfile, current_game->tags);
        if (GlobalState.add_ECO && !GlobalState.parsing_ECO_file) {
            /* If ECO classification has been requested, then assume
             * that ECO tags are also required.
             */
            output_tag(ECO_TAG, current_game->tags, outputfile);
            output_tag(OPENING_TAG, current_game->tags, outputfile);
            output_tag(VARIATION_TAG, current_game->tags, outputfile);
            output_tag(SUB_VARIATION_TAG, current_game->tags, outputfile);
        }

        if(current_game->tags[FEN_TAG] != NULL) {
            /* Output any FEN that there might be. */
            /* @@@ NB: Strictly speaking, we should check TagOrder for the
             * preferred order of these, in case it is not the default.
             */
            output_tag(VARIANT_TAG, current_game->tags, outputfile);
            output_tag(SETUP_TAG, current_game->tags, outputfile);
            output_tag(FEN_TAG, current_game->tags, outputfile);
        }
        putc('\n', outputfile);
    }
    else if (GlobalState.tag_output_format == NO_TAGS) {
    }
    else {
        fprintf(GlobalState.logfile,
                "Unknown output form for tags: %d\n",
                GlobalState.tag_output_format);
        exit(1);
    }
    if ((GlobalState.keep_comments) &&
            (current_game->prefix_comment != NULL)) {
        print_comment_list(outputfile,
                current_game->prefix_comment);
        terminate_line(outputfile);
        putc('\n', outputfile);
    }
    if (GlobalState.json_format) {
        fputs("\"Moves\":[", outputfile);
    }
    print_move_list(outputfile, move_number, white_to_move,
            current_game->moves, final_board);
    if (GlobalState.json_format) {
        fputs("]\n", outputfile);
    }
    /* Take account of a possible zero move game. */
    if (current_game->moves == NULL) {
        if (current_game->tags[RESULT_TAG] != NULL) {
            if (!GlobalState.json_format) {
                print_str(outputfile, current_game->tags[RESULT_TAG]);
            }
        }
        else {
            fprintf(GlobalState.logfile,
                    "Internal error: Zero move game with no result\n");
        }
    }
    if (GlobalState.json_format) {
        fputs("\n}", outputfile);
    }
    else {
        terminate_line(outputfile);
        putc('\n', outputfile);
    }
}

static void
print_EPD_move_list(Game *current_game, FILE *outputfile,
        unsigned move_number, Boolean white_to_move,
        Board *initial_board)
{
    const char *game_comment;

    if(GlobalState.tag_output_format != NO_TAGS) {
        game_comment = format_epd_game_comment(current_game->tags);
    }
    else {
        game_comment = copy_string("");
    }
    const Move *move = current_game->moves;

    if (initial_board != NULL) {
        char epd[FEN_SPACE];
        build_basic_EPD_string(initial_board, epd);
        fprintf(outputfile, "%s %s\n", epd, game_comment);
    }
    while (move != NULL) {
        if (move->epd != NULL) {
            fprintf(outputfile, "%s %s\n", move->epd, game_comment);
        }
        else {
            fprintf(GlobalState.logfile, "Internal error: Missing EPD\n");
            report_details(GlobalState.logfile);
            exit(1);
        }
        move = move->next;
    }
    (void) free((void *) game_comment);
}

static void print_FEN_move_list(Game *current_game, FILE *outputfile,
        unsigned move_number, Boolean white_to_move,
        Board *initial_board)
{
    Board *board = initial_board;
    Move *move = current_game->moves;
    Boolean keepPrinting;
    /* Work out the ply depth. */
    int plies = 2 * (move_number) - 1;

    if (!white_to_move) {
        plies++;
    }
    if (GlobalState.output_ply_limit >= 0 &&
            plies > GlobalState.output_ply_limit) {
        keepPrinting = FALSE;
    }
    else {
        keepPrinting = TRUE;
        const char *FEN_string = get_FEN_string(board);
        fprintf(GlobalState.outputfile, "%s\n", FEN_string);
        free((void *) FEN_string);
    }

    while (move != NULL && keepPrinting) {
        if (move->move[0] != '\0') {
            if(apply_move(move, board)) {
                const char *FEN_string = get_FEN_string(board);
                fprintf(GlobalState.outputfile, "%s\n", FEN_string);
                free((void *) FEN_string);
            }
            else {
                keepPrinting = FALSE;
            }
            /* A genuine move was just printed, rather than a comment. */
            if (white_to_move) {
                white_to_move = FALSE;
            }
            else {
                move_number++;
                white_to_move = TRUE;
            }
            plies++;
        }
        move = move->next;
    }
}

static void
print_EPD_game(Game *current_game, FILE *outputfile,
        unsigned move_number, Boolean white_to_move,
        Board *initial_board)
{
    if (!GlobalState.check_only) {
        print_EPD_move_list(current_game, outputfile, move_number, white_to_move,
                initial_board);
        putc('\n', outputfile);
    }
}

static void
print_FEN_game(Game *current_game, FILE *outputfile,
        unsigned move_number, Boolean white_to_move,
        Board *initial_board)
{
    if (!GlobalState.check_only) {
        /* Report details on the output. */
        if (GlobalState.tag_output_format == ALL_TAGS) {
            show_tags(outputfile, current_game->tags, current_game->tags_length);
        }
        else if (GlobalState.tag_output_format == SEVEN_TAG_ROSTER) {
            output_STR(outputfile, current_game->tags);
        }
        else if (GlobalState.tag_output_format == NO_TAGS) {
        }
        else {
            fprintf(GlobalState.logfile,
                    "Unknown output form for tags: %d\n",
                    GlobalState.tag_output_format);
            exit(1);
        }
        print_FEN_move_list(current_game, outputfile, move_number, white_to_move,
                initial_board);
        putc('\n', outputfile);
    }
}

/*
 * Build a comment containing a FEN representation of the board.
 */
static const char *
build_FEN_comment(const Board *board)
{
    char *fen = get_FEN_string(board);
    const char *prefix = "{ \"";
    const char *suffix = "\" }";
    
    char *comment = (char *) malloc_or_die(strlen(prefix) + strlen(fen) + strlen(suffix) + 1);
    sprintf(comment, "%s%s%s", prefix, fen, suffix);
    (void) free((void *) fen);
    return comment;
}

/*
 * Count how many ply recorded for the given move.
 * Include variations if count_variations.
 */
static unsigned count_single_move_ply(const Move *move_details,
        Boolean count_variations)
{
    unsigned count = 1;
    if (count_variations) {
        Variation *variant = move_details->Variants;
        while (variant != NULL) {
            count += count_move_list_ply(variant->moves, count_variations);
            variant = variant->next;
        }
    }
    return count;
}

/*
 * Count how many plies in the game in total.
 * Include variations if count_variations.
 */
static unsigned count_move_list_ply(Move *move_list, Boolean count_variations)
{
    unsigned count = 0;
    while (move_list != NULL) {
        count += count_single_move_ply(move_list, count_variations);
        move_list = move_list->next;
    }
    return count;
}

/*
 * Count how many plies in the game in total.
 * Don't include variations.
 */
void add_plycount(const Game *game)
{
    /* Don't include variations. */
    unsigned count = count_move_list_ply(game->moves, FALSE);
    char formatted_count[FORMATTED_NUMBER_SIZE];
    sprintf(formatted_count, "%u", count);

    if (game->tags[PLY_COUNT_TAG] != NULL) {
        (void) free(game->tags[PLY_COUNT_TAG]);
    }
    game->tags[PLY_COUNT_TAG] = copy_string(formatted_count);
}

/*
 * Count how many plies in the game in total.
 * Include variations if count_variations.
 */
void add_total_plycount(const Game *game, Boolean count_variations)
{
    unsigned count = count_move_list_ply(game->moves, count_variations);
    char formatted_count[FORMATTED_NUMBER_SIZE];
    sprintf(formatted_count, "%u", count);

    if (game->tags[TOTAL_PLY_COUNT_TAG] != NULL) {
        (void) free(game->tags[TOTAL_PLY_COUNT_TAG]);
    }
    game->tags[TOTAL_PLY_COUNT_TAG] = copy_string(formatted_count);
}

/*
 * Include a tag containing a hashcode for the game.
 * Use the cumulative hash value.
 */
static void add_hashcode_tag(const Game *game)
{
    HashCode hashcode = game->cumulative_hash_value;
    char formatted_code[FORMATTED_NUMBER_SIZE];
    sprintf(formatted_code, "%08x", (unsigned) hashcode);

    if (game->tags[HASHCODE_TAG] != NULL) {
        (void) free(game->tags[HASHCODE_TAG]);
    }
    game->tags[HASHCODE_TAG] = copy_string(formatted_code);
}

/* Determine how many characters needed to format the given number.
 * Required for accurate space allocation.
 */
static unsigned lineNumberChars(unsigned long num)
{
    return (unsigned)((ceil(log10(num))+1));
}

static CommentList *create_line_number_comment(const Game *game)
{
    unsigned numbytes = strlen(GlobalState.line_number_marker) + 1 +
        lineNumberChars(game->start_line) + 1 + lineNumberChars(game->end_line) + 1;
    char *line_number_comment = (char *) malloc_or_die(numbytes);
    sprintf(line_number_comment, "%s:%lu:%lu",
    		GlobalState.line_number_marker,
		game->start_line,
		game->end_line);
    if(numbytes < strlen(line_number_comment) + 1) {
        fprintf(GlobalState.logfile, "Internal error: insufficient space allocated in create_line_number_comment\n");
	exit(1);
    }
    StringList *current_comment = save_string_list_item(NULL, line_number_comment);
    CommentList *comment = (CommentList*) malloc_or_die(sizeof (*comment));

    comment->comment = current_comment;
    comment->next = NULL;
    return comment;
}
