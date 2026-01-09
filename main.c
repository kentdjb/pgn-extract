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
#include "tokens.h"
#include "taglist.h"
#include "lex.h"
#include "moves.h"
#include "map.h"
#include "lists.h"
#include "output.h"
#include "end.h"
#include "grammar.h"
#include "hashing.h"
#include "argsfile.h"

/* The maximum length of an output line.  This is conservatively
 * slightly smaller than the PGN export standard of 80.
 */
#define MAX_LINE_LENGTH 75

/* Define a file name relative to the current directory representing
 * a file of ECO classificiations.
 */
#ifndef DEFAULT_ECO_FILE
#define DEFAULT_ECO_FILE "eco.pgn"
#endif

/* This structure holds details of the program state
 * available to all parts of the program.
 * This goes against the grain of good structured programming
 * principles, but most of these fields are set from the program's
 * arguments and are read-only thereafter. If I had done this in
 * C++ there would have been a cleaner interface!
 */
StateInfo GlobalState = {
    FALSE,              /* skipping_current_game */
    FALSE,              /* check_only (-r) */
    2,                  /* verbosity level (-s and --quiet) */
    TRUE,               /* keep_NAGs (-N) */
    TRUE,               /* keep_comments (-C) */
    TRUE,               /* keep_variations (-V) */
    ALL_TAGS,           /* tag_output_form (-7, --notags) */
    TRUE,               /* match_permutations (-v) */
    FALSE,              /* positional_variations (-x) */
    FALSE,              /* use_soundex (-S) */
    FALSE,              /* suppress_duplicates (-D) */
    FALSE,              /* suppress_originals (-U) */
    FALSE,              /* fuzzy_match_duplicates (--fuzzy) */
    0,                  /* fuzzy_match_depth (--fuzzy) */
    FALSE,              /* check_tags */
    FALSE,              /* add_ECO (-e) */
    FALSE,              /* parsing_ECO_file (-e) */
    DONT_DIVIDE,        /* ECO_level (-E) */
    SAN,                /* output_format (-W) */
    MAX_LINE_LENGTH,    /* max_line_length (-w) */
    FALSE,              /* use_virtual_hash_table (-Z) */
    FALSE,              /* check_move_bounds (-b) */
    FALSE,              /* match_only_checkmate (-M) */
    FALSE,              /* match_only_stalemate (--stalemate) */
    FALSE,              /* match_only_insufficient_material (--insufficient) */
    TRUE,               /* keep_move_numbers (--nomovenumbers) */
    TRUE,               /* keep_results (--noresults) */
    TRUE,               /* keep_checks (--nochecks) */
    FALSE,              /* output_evaluation (--evaluation) */
    FALSE,              /* keep_broken_games (--keepbroken) */
    FALSE,              /* suppress_redundant_ep_info (--nofauxep) */
    FALSE,              /* json_format (--json) */
    FALSE,              /* tag_match_anywhere (--tagsubstr) */
    FALSE,              /* match_underpromotion (--underpromotion) */
    FALSE,              /* suppress_matched (--suppressmatched) */
    FALSE,              /* higher_rated_winner (--higherratedwinner) */
    FALSE,              /* lower_rated_winner (--lowerratedwinner) */
    FALSE,              /* find_odds_games (--odds) */
    0,                  /* depth_of_positional_search */
    0,                  /* num_games_processed */
    0,                  /* num_games_matched */
    0,                  /* num_non_matching_games */
    0,                  /* games_per_file (-#) */
    1,                  /* next_file_number */
    0,                  /* lower_move_bound */
    10000,              /* upper_move_bound */
    -1,                 /* output_ply_limit (--plylimit) */
    0,                  /* stability_threshold (--stable) */
    1,                  /* first */
    ~0,                 /* game_limit */
    0,                  /* maximum_matches */
    0,                  /* drop_ply_number (--dropply) */
    1,                  /* startply (--startply) */
    0,                  /* check_for_repetition (--repetition) */
    0,                  /* check_for_N_move_rule (--fifty, --seventyfive) */
    0,                  /* piece_count (--piececount) */
    FALSE,              /* output_FEN_string */
    FALSE,              /* add_FEN_comments (--fencomments) */
    FALSE,              /* add_hashcode_comments (--hashcomments) */
    FALSE,              /* add_position_match_comments (--markmatches) */
    FALSE,              /* output_plycount (--plycount) */
    FALSE,              /* output_total_plycount (--totalplycount) */
    FALSE,              /* add_hashcode_tag (--addhashcode) */
    FALSE,              /* fix_result_tags (--fixresulttags) */
    FALSE,              /* fix_tag_strings (--fixtagstrings) */
    FALSE,              /* add_fen_castling (--addfencastling) */
    FALSE,              /* separate_comment_lines (--commentlines) */
    FALSE,              /* split_variants (--separatevariants) */
    FALSE,              /* reject_inconsistent_results (--nobadresults) */
    FALSE,              /* allow_null_moves (--allownullmoves) */
    FALSE,              /* allow_nested_comments (--nestedcomments) */
    FALSE,              /* add_match_tag (--addmatchtag) */
    FALSE,              /* add_matchlabel_tag (--addlabeltag) */
    FALSE,              /* only_output_wanted_tags (--xroster) */
    FALSE,              /* delete_same_setup (--deletesamesetup) */
    FALSE,              /* lichess_comment_fix (--lichesscommentfix) */
    FALSE,              /* keep_only_commented_games (--only_commented_games) */
    FALSE,              /* variation_match_anywhere (--anywhere) */
    0,                  /* split_depth_limit */
    NORMALFILE,         /* current_file_type */
    SETUP_TAG_OK,       /* setup_status */
    EITHER_TO_MOVE,     /* whose_move */
    "MATCH",            /* position_match_comment (--markmatches) */
    (char *) NULL,      /* FEN_comment_pattern (-Fpattern) */
    (char *) NULL,      /* drop_comment_pattern (--dropbefore) */
    (char *) NULL,      /* line_number_marker (--linenumbers) */
    (char *) NULL,      /* current_input_file */
    DEFAULT_ECO_FILE,   /* eco_file (-e) */
    (FILE *) NULL,      /* outputfile (-o, -a). Default is stdout */
    (char *) NULL,      /* output_filename (-o, -a) */
    (FILE *) NULL,      /* logfile (-l). Default is stderr */
    (FILE *) NULL,      /* duplicate_file (-d) */
    (FILE *) NULL,      /* non_matching_file (-n) */
    NULL,               /* matching_game_numbers */
    NULL,               /* next_game_number_to_output */
    NULL,               /* skip_game_numbers */
    NULL,               /* next_game_number_to_skip */
};

/* Prepare the output file handles in GlobalState. */
static void
init_default_global_state(void)
{
    GlobalState.outputfile = stdout;
    GlobalState.logfile = stderr;
    set_output_line_length(MAX_LINE_LENGTH);
}

int
main(int argc, char *argv[])
{
    int argnum;

    /* Prepare global state. */
    init_default_global_state();
    /* Prepare the Game_Header. */
    init_game_header();
    /* Prepare the tag lists for -t/-T matching. */
    init_tag_lists();
    /* Prepare the hash tables for transposition detection. */
    init_hashtab();
    /* Initialise the lexical analyser's tables. */
    init_lex_tables();
    /* Allow for some arguments. */
    for (argnum = 1; argnum < argc;) {
        const char *argument = argv[argnum];
        if (argument[0] == '-') {
            switch (argument[1]) {
                    /* Arguments with no additional component. */
                case SEVEN_TAG_ROSTER_ARGUMENT:
                case DONT_KEEP_COMMENTS_ARGUMENT:
                case DONT_KEEP_DUPLICATES_ARGUMENT:
                case DONT_KEEP_VARIATIONS_ARGUMENT:
                case DONT_KEEP_NAGS_ARGUMENT:
                case DONT_MATCH_PERMUTATIONS_ARGUMENT:
                case CHECK_ONLY_ARGUMENT:
                case KEEP_SILENT_ARGUMENT:
                case USE_SOUNDEX_ARGUMENT:
                case MATCH_CHECKMATE_ARGUMENT:
                case SUPPRESS_ORIGINALS_ARGUMENT:
                case USE_VIRTUAL_HASH_TABLE_ARGUMENT:
                    process_argument(argument[1], "");
                    argnum++;
                    break;

                    /* Argument rewritten as a different one. */
                case ALTERNATIVE_HELP_ARGUMENT:
                    process_argument(HELP_ARGUMENT, "");
                    argnum++;
                    break;

                    /* Arguments where an additional component is required.
                     * It must be adjacent to the argument and not separated from it.
                     */
                case TAG_EXTRACTION_ARGUMENT:
                    process_argument(argument[1], &(argument[2]));
                    argnum++;
                    break;

                    /* Arguments where an additional component is optional.
                     * If it is present, it must be adjacent to the argument
                     * letter and not separated from it.
                     */
                case HELP_ARGUMENT:
                case OUTPUT_FORMAT_ARGUMENT:
                case USE_ECO_FILE_ARGUMENT:
                    process_argument(argument[1], &(argument[2]));
                    argnum++;
                    break;

                    /* Long form arguments. */
                case LONG_FORM_ARGUMENT:
                {
                    /* How many args (1 or 2) are processed. */
                    int args_processed;
                    /* This argument might need the following argument
                     * as an associated value.
                     */
                    const char *possible_associated_value = "";
                    if (argnum + 1 < argc) {
                        possible_associated_value = argv[argnum + 1];
                    }
                    /* Find out how many arguments were consumed
                     * (1 or 2).
                     */
                    args_processed =
                            process_long_form_argument(&argument[2],
                            possible_associated_value);
                    argnum += args_processed;
                }
                    break;

                    /* Arguments with a required filename component. */
                case FILE_OF_ARGUMENTS_ARGUMENT:
                case APPEND_TO_OUTPUT_FILE_ARGUMENT:
                case CHECK_FILE_ARGUMENT:
                case DUPLICATES_FILE_ARGUMENT:
                case FILE_OF_FILES_ARGUMENT:
                case WRITE_TO_LOG_FILE_ARGUMENT:
                case APPEND_TO_LOG_FILE_ARGUMENT:
                case NON_MATCHING_GAMES_ARGUMENT:
                case WRITE_TO_OUTPUT_FILE_ARGUMENT:
                case TAG_ROSTER_ARGUMENT:
                { /* We require an associated file argument. */
                    const char argument_letter = argument[1];
                    const char *filename = &(argument[2]);
                    if (*filename == '\0') {
                        /* Try to pick it up from the next argument. */
                        argnum++;
                        if (argnum < argc) {
                            filename = argv[argnum];
                            argnum++;
                        }
                        /* Make sure the associated_value does not look
                         * like the next argument.
                         */
                        if ((*filename == '\0') || (*filename == '-')) {
                            fprintf(GlobalState.logfile,
                                    "Usage: -%c filename\n",
                                    argument_letter);
                            exit(1);
                        }
                    }
                    else {
                        argnum++;
                    }
                    process_argument(argument[1], filename);
                }
                    break;

                    /* Arguments with a required following value. */
                case ECO_OUTPUT_LEVEL_ARGUMENT:
                case GAMES_PER_FILE_ARGUMENT:
                case LINE_WIDTH_ARGUMENT:
                case MOVE_BOUNDS_ARGUMENT:
                case PLY_BOUNDS_ARGUMENT:
                { /* We require an associated argument. */
                    const char argument_letter = argument[1];
                    const char *associated_value = &(argument[2]);
                    if (*associated_value == '\0') {
                        /* Try to pick it up from the next argument. */
                        argnum++;
                        if (argnum < argc) {
                            associated_value = argv[argnum];
                            argnum++;
                        }
                        /* Make sure the associated_value does not look
                         * like the next argument.
                         */
                        if ((*associated_value == '\0') ||
                                (*associated_value == '-')) {
                            fprintf(GlobalState.logfile,
                                    "Usage: -%c value\n",
                                    argument_letter);
                            exit(1);
                        }
                    }
                    else {
                        argnum++;
                    }
                    process_argument(argument[1], associated_value);
                }
                    break;

                case OUTPUT_FEN_STRING_ARGUMENT:
                    /* May be following by an optional argument immediately after
                     * the argument letter.
                     */
                    process_argument(argument[1], &argument[2]);
                    argnum++;
                    break;
                    /* Argument that require different treatment because they
                     * are present on the command line rather than an argsfile.
                     */
                case TAGS_ARGUMENT:
                case MOVES_ARGUMENT:
                case POSITIONS_ARGUMENT:
                case ENDINGS_ARGUMENT:
                case ENDINGS_COLOURED_ARGUMENT:
                { 
                    /* From the command line, we require an
                     * associated file argument.
                     * Check this here, as it is not the case
                     * when reading arguments from an argument file.
                     */
                    const char *filename = &(argument[2]);
                    const char argument_letter = argument[1];
                    if (*filename == '\0') {
                        /* Try to pick it up from the next argument. */
                        argnum++;
                        if (argnum < argc) {
                            filename = argv[argnum];
                            argnum++;
                        }
                        /* Make sure the filename does not look
                         * like the next argument.
                         */
                        if ((*filename == '\0') || (*filename == '-')) {
                            fprintf(GlobalState.logfile,
                                    "Usage: -%cfilename or -%c filename\n",
                                    argument_letter, argument_letter);
                            exit(1);
                        }
                    }
                    else {
                        argnum++;
                    }
                    process_argument(argument_letter, filename);
                }
                    break;
                case HASHCODE_MATCH_ARGUMENT:
                    process_argument(argument[1], &argument[2]);
                    argnum++;
                    break;
                default:
                    fprintf(GlobalState.logfile,
                            "Unknown flag %s. Use -%c for usage details.\n",
                            argument, HELP_ARGUMENT);
                    exit(1);
                    break;
            }
        }
        else {
            /* Should be a file name containing games. */
            add_filename_to_source_list(argument, NORMALFILE);
            argnum++;
        }
    }

    /* Make some adjustments to other settings if JSON output is required. */
    if (GlobalState.json_format) {
        if (GlobalState.output_format != EPD &&
                GlobalState.output_format != CM &&
                GlobalState.ECO_level == DONT_DIVIDE) {
            GlobalState.keep_comments = FALSE;
            GlobalState.keep_variations = FALSE;
            GlobalState.keep_results = FALSE;
        }
        else {
            fprintf(GlobalState.logfile, "JSON output is not currently supported with -E, -Wepd or -Wcm\n");
            GlobalState.json_format = FALSE;
        }
    }

    /* Prepare the hash tables for duplicate detection. */
    init_duplicate_hash_table();

    if (GlobalState.add_ECO) {
        /* Read in a list of ECO lines in order to classify the games. */
        if (open_eco_file(GlobalState.eco_file)) {
            /* Indicate that the ECO file is currently being parsed. */
            GlobalState.parsing_ECO_file = TRUE;
            yyparse(ECOFILE);
            reset_line_number();
            GlobalState.parsing_ECO_file = FALSE;
        }
        else {
            fprintf(GlobalState.logfile, "Unable to open the ECO file %s.\n",
                    GlobalState.eco_file);
            exit(1);
        }
    }

    /* Open up the first file as the source of input. */
    if (!open_first_file()) {
        exit(1);
    }

    yyparse(GlobalState.current_file_type);

    /* @@@ I would prefer this to be somewhere else. */
    if (GlobalState.json_format && !GlobalState.check_only) {
        if(GlobalState.num_games_matched > 0) {
            fputs("\n]\n", GlobalState.outputfile);
        }
        if(GlobalState.non_matching_file != NULL &&
                GlobalState.num_non_matching_games > 0) {
            fputs("\n]\n", GlobalState.non_matching_file);
        }
    }

    /* Remove any temporary files. */
    clear_duplicate_hash_table();
    if (!GlobalState.suppress_matched && GlobalState.verbosity > 1) {
        fprintf(GlobalState.logfile, "%lu game%s matched out of %lu.\n",
                GlobalState.num_games_matched,
                GlobalState.num_games_matched == 1 ? "" : "s",
                GlobalState.num_games_processed);
    }
    if ((GlobalState.logfile != stderr) && (GlobalState.logfile != NULL)) {
        (void) fclose(GlobalState.logfile);
    }
    return 0;
}
