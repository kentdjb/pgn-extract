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
#include "defs.h"
#include "typedef.h"
#include "lines.h"
#include "taglist.h"
#include "tokens.h"
#include "lex.h"
#include "taglines.h"
#include "moves.h"
#include "end.h"
#include "eco.h"
#include "argsfile.h"
#include "apply.h"
#include "output.h"
#include "lists.h"
#include "mymalloc.h"
#include "fenmatcher.h"

#define CURRENT_VERSION "v26-01"
#define URL "https://www.cs.kent.ac.uk/people/staff/djb/pgn-extract/"

/* The prefix of the arguments allowed in an argsfile.
 * The full format is:
 *         :-?
 * where ? is an argument letter.
 *
 * A line of the form:
 *         :filename
 * means use filename as a NORMALFILE source of games.
 *
 * A line with no leading colon character is taken to apply to the
 * move-reason argument line. Currently, this only applies to the
 *        -t -v -x -z
 * arguments.
 */
static const char argument_prefix[] = ":-";
static const int argument_prefix_len = sizeof (argument_prefix) - 1;

static ArgType classify_arg(const char *line);
static game_number *extract_game_number_list(const char *number_list);
static void read_args_file(const char *infile);
static Boolean set_move_bounds(char bounds_or_ply, char limit, unsigned number);

#if defined(__unix__) || defined(__linux__) || defined(__APPLE__)
int strcasecmp(const char *, const char *);
#else
int _stricmp(const char *s1, const char *s2);
#endif

/* Select the correct function according to operating system. */
static int
stringcompare(const char *s1, const char *s2)
{
#if defined(__unix__) || defined(__linux__) || defined(__APPLE__)
    return strcasecmp(s1, s2);
#else
    return _stricmp(s1, s2);
#endif
}

#if 0

/* Return TRUE if str contains prefix as a prefix, FALSE otherwise. */
static Boolean
prefixMatch(const char *prefix, const char *str)
{
    size_t prefixLen = strlen(prefix);
    if (strlen(str) >= prefixLen) {
#if defined(__unix__) || defined(__linux__)
        return strncasecmp(prefix, str, prefixLen) == 0;
#else
        return _strnicmp(prefix, str, prefixLen) == 0;
#endif
    }
    else {
        return FALSE;
    }
}
#endif

/* Skip over leading spaces from the string. */
static const char *skip_leading_spaces(const char *str)
{
    while (*str == ' ') {
        str++;
    }
    return str;
}

/* Print a usage message, and exit. */
static void
usage_and_exit(void)
{
    const char *help_data[] = {
        "-7 -- output only the seven tag roster for each game. Other tags (apart",
        "      from FEN and possibly ECO) are discarded (See -e).",
        "-#num[,num] -- output num games per file, to files named 1.pgn, 2.pgn, etc.",

        "",

        "-aoutputfile -- append extracted games to outputfile. (See -o).",
        "-Aargsfile -- read the program's arguments from argsfile.",
        "-b[elu]num -- restricted bounds on the number of moves in a game.",
        "       lnum set a lower bound of 'num' moves,",
        "       unum set an upper bound of 'num' moves,",
        "       otherwise num (or enum) means equal-to 'num' moves.",
        "-cfile[.pgn] -- Use file.pgn as a check-file for duplicates or",
        "      contents of file (no pgn suffix) as a list of check-file names.",
        "-C -- don't include comments in the output. Ordinarily these are retained.",
        "-dduplicates -- write duplicate games to the file duplicates.",
        "-D -- don't output duplicate games.",
        "-eECO_file -- perform ECO classification of games. The optional",
        "      ECO_file should contain a PGN format list of ECO lines",
        "      Default is to use eco.pgn from the current directory.",
        "-E[123 etc.] -- split output into separate files according to ECO.",
        "      E1 : Produce files from ECO letter, A.pgn, B.pgn, ...",
        "      E2 : Produce files from ECO letter and first digit, A0.pgn, ...",
        "      E3 : Produce files from full ECO code, A00.pgn, A01.pgn, ...",
        "      Further digits may be used to produce non-standard further",
        "      refined division of games.",
        "      All files are opened in append mode.",
        "-F[text] -- output a FEN string comment after the final (or other selected) move.",
        "-ffile_list  -- file_list contains the list of PGN source files, one per line.",
        "-Hhash -- match games in which the given Zobrist polyglot hash value occurs",
        "-h -- print details of the arguments.",
        "-llogfile  -- Save the diagnostics in logfile rather than using stderr.",
        "-Llogfile  -- Append all diagnostics to logfile, rather than overwriting.",
        "-M -- Match only games which end in checkmate.",
        "-noutputfile -- Write all valid games not otherwise matched to outputfile.",
        "-N -- don't include NAGs in the output. Ordinarily these are retained.",
        "-ooutputfile -- write extracted games to outputfile (existing contents lost).",
        "-p[elu]num -- restricted bounds on the number of ply in a game.",
        "       lnum set a lower bound of 'num' ply,",
        "       unum set an upper bound of 'num' ply,",
        "       otherwise num (or enum) means equal-to 'num' ply.",
        "-P -- don't match permutations of the textual variations (-v).",
        "-Rtagorder -- Use the tag ordering specified in the file tagorder.",
        "-r -- report any errors but don't extract.",
        "-S -- Use a simple soundex algorithm for some tag matches. If used",
        "      this option must precede the -t or -T options.",
        "-s -- silent mode: don't report each game as it is extracted.",
        "-ttagfile -- file of player, date, result or FEN extraction criteria.",
        "-Tcriterion -- player, date, eco code, hashcode, FEN position, annotator or result, extraction criteria.",
        "-U -- don't output games that only occur once. (See -d).",
        "-vvariations -- the file variations contains the textual lines of interest.",
        "-V -- don't include variations in the output. Ordinarily these are retained.",
        "-wwidth -- set width as an approximate line width for output.",
        "-W[cm|epd|halg|lalg|elalg|xlalg|xolalg|san] -- specify the output format to use.",
        "      Default is SAN.",
        "      -W means use the input format.",
        "      -Wcm is (a possibly obsolete) ChessMaster format.",
        "      -Wepd is EPD format.",
        "      -Wfen is FEN format.",
        "      -Wsan[PNBRQK] for language specific output.",
        "      -Whalg is hyphenated long algebraic.",
        "      -Wlalg is long algebraic.",
        "      -Welalg is enhanced long algebraic.",
        "      -Wxlalg is enhanced long algebraic with x for captures and - for non capture moves.",
        "      -Wxolalg is -Wxlalg but with O-O and O-O-O for castling.",
        "      -Wuci is output compatible with the UCI protocol.",
        "-xvariations -- the file variations contains the lines resulting in",
        "                positions of interest.",
        "-yfile -- file contains a material balance of interest.",
        "-zfile -- file contains a material balance of interest.",
        "-Z -- use the file virtual.tmp as an external hash table for duplicates.",
        "      Use when MallocOrDie messages occur with big datasets.",

        "",

        "--50 - only output games that include fifty moves with no capture or pawn move.",
        "--75 - only output games that include seventy-five moves with no capture or pawn move.",
        "--addfencastling - add potentially missing castling rights to FEN tags",
        "--addhashcode - output a HashCode tag",
        "--addlabeltag - output a MatchLabel tag with FENPattern",
        "--addmatchtag - output a MaterialMatch tag with -z",
        "--allownullmoves - allow NULL moves in the main line",
        "--append - see -a",
	"--btm - match position only if Black is to move (see -t)",
        "--checkfile - see -c",
        "--checkmate - see -M",
        "--commented - only match games with at least one comment",
        "--commentlines - output each comment on a separate line",
        "--deletesamesetup - suppress games with the same initial position as one already processed",
        "--detag tag - don't include tag in the output",
        "--dropbefore - drop opening ply before a matching comment string",
        "--dropply - drop the given number of ply from the beginning of the game",
        "--duplicates - see -d",
        "--evaluation - include a position evaluation after each move",
        "--fencomments - include a FEN string after each move",
        "--fenpattern pattern - match games reaching a position matching the given FEN pattern",
        "--fenpatterni pattern - match games reaching a position matching the given FEN pattern for either side",
        "--fifty - only output games that include fifty moves with no capture or pawn move.",
        "--firstgame N - start matching from game number N (default 1).",
        "--fixresulttags - correct Result tags that conflict with the game outcome or terminating result.",
        "--fixtagstrings - attempt to correct tag strings that are not properly terminated.",
        "--fuzzydepth plies - positional duplicates match",
        "--gamelimit N - only process up to and including game number N.",
        "--hashcomments - include a hashcode string after each move",
        "--help - see -h",
        "--higherratedwinner - match only if the winner has the higher rating",
        "--insufficient - only output games that end with insufficient mating material.",
        "--json - output the game in JSON format",
        "--keepbroken - retain games with errors",
        "--lichesscommentfix - move comments at the start of a variation to after the first move of the variation.",
        "--linelength - see -w",
	"--linenumbers marker - include a comment with the source line numbers of each game { marker:start:end }",
        "--lowerratedwinner - match only if the winner has the lower rating",
        "--matchplylimit - maximum ply depth to search for positional matches",
        "--markmatches - mark positional and material matches with a comment; see -t, -v, and -z",
        "--materialy material - material is a string describing a material balance; see -y",
        "--materialz material - material is a string describing a material balance; see -z",
        "--minmoves N - only output games with at least N moves.",
        "--minply N - only output games with at least N ply.",
        "--maxmoves N - only output games with at N or fewer moves.",
        "--maxply N - only output games with at N or fewer ply.",
        "--nestedcomments - allow nested comments.",
        "--nobadresults - reject games with inconsistent result indications.",
        "--nochecks - don't output + and # after moves.",
        "--nocomments - see -C",
        "--noduplicates - see -D",
        "--nofauxep - don't output ep squares in FEN when the capture is not possible",
        "--nomovenumbers - don't output move numbers.",
        "--nonags - see -N",
        "--noresults - don't output results.",
        "--nosetuptags - don't match games with a SetUp tag.",
        "--notags - don't output any tags.",
        "--nounique - see -U",
        "--novars - see -V",
        "--odds - only match games played at odds.",
        "--onlysetuptags - only match games with a SetUp tag.",
        "--output - see -o",
        "--piececount N - match games that reach a position with N pieces.",
        "--plycount - include a PlyCount tag.",
        "--plylimit - limit the number of plies output.",
        "--quiescent N - position quiescence length (default 0)",
        "--quiet - No status processing output (see, also, -s).",
        "--repetition - only output games that include 3-fold repetition.",
        "--repetition5 - only output games that include 5-fold repetition.",
        "--selectonly range[,range ...] - only output the selected matched game(s)",
        "--seven - see -7",
        "--seventyfive - only output games that include seventy-five moves with no capture or pawn move.",
        "--skipmatching range[,range ...] - don't output the selected matched game(s)",
        "--splitvariants [depth] - output each variation (to the given depth) as a separate game.",
        "--stalemate - only output games that end in stalemate.",
        "--startply N - only start matching after N ply (N >= 1).",
        "--stopafter N - stop after matching N games (N > 0)",
        "--suppressmatched - don't output matched games (see -n).",
        "--tagsubstr - match in any part of a tag (see -T and -t).",
        "--totalplycount - include a tag with the total number of plies in a game.",
        "--underpromotion - match only games that contain an underpromotion.",
        "--version - print the current version number and exit.",
        "--vanywhere - apply variation matching (-v) through the whole game.",
	"--wtm - match position only if White is to move (see -t)",
        "--xroster - don't output tags not included with the -R option (see -R).",

        /* Must be NULL terminated. */
        (char *) NULL,
    };

    const char **data = help_data;

    fprintf(GlobalState.logfile,
            "pgn-extract %s (%s): a Portable Game Notation (PGN) manipulator.\n",
            CURRENT_VERSION, __DATE__);
    fprintf(GlobalState.logfile,
            "Copyright (C) 1994-2026 David J. Barnes (d.j.barnes@kent.ac.uk)\n");
    fprintf(GlobalState.logfile, "%s\n\n", URL);
    fprintf(GlobalState.logfile, "Usage: pgn-extract [arguments] [file.pgn ...]\n");

    for (; *data != NULL; data++) {
        fprintf(GlobalState.logfile, "%s\n", *data);
    }
    exit(1);
}

static void
read_args_file(const char *infile)
{
    char *line;
    FILE *fp = fopen(infile, "r");

    if (fp == NULL) {
        fprintf(GlobalState.logfile, "Cannot open %s for reading.\n", infile);
        exit(1);
    }
    else {
        ArgType linetype = NO_ARGUMENT_MATCH;
        ArgType nexttype;
        while ((line = read_line(fp)) != NULL) {
            if (blank_line(line)) {
                (void) free((void *) line);
                continue;
            }
            nexttype = classify_arg(line);
            if (nexttype == NO_ARGUMENT_MATCH) {
                if (*line == argument_prefix[0]) {
                    /* Treat the line as a source file name. */
                    add_filename_to_source_list(&line[1], NORMALFILE);
                }
                else if (linetype != NO_ARGUMENT_MATCH) {
                    /* Handle the line. */
                    switch (linetype) {
                        case MOVES_ARGUMENT:
                            add_textual_variation_from_line(line);
                            break;
                        case POSITIONS_ARGUMENT:
                            add_positional_variation_from_line(line);
                            break;
                        case TAGS_ARGUMENT:
                            process_tag_line(infile, line, TRUE);
                            break;
                        case TAG_ROSTER_ARGUMENT:
                            process_roster_line(line);
                            break;
                        case ENDINGS_ARGUMENT:
                        case ENDINGS_COLOURED_ARGUMENT:
                            process_material_description(line, linetype == ENDINGS_ARGUMENT, FALSE);
                            (void) free((void *) line);
                            break;
                        default:
                            fprintf(GlobalState.logfile,
                                    "Internal error: unknown linetype %d in read_args_file\n",
                                    linetype);
                            (void) free((void *) line);
                            exit(-1);
                    }
                }
                else {
                    /* It should have been a line applying to the
                     * current linetype.
                     */
                    fprintf(GlobalState.logfile,
                            "Missing argument type for line %s in the argument file.\n",
                            line);
                    exit(1);
                }
            }
            else {
                switch (nexttype) {
                        /* Arguments with a possible additional
                         * argument value.
                         * All of these apply only to the current
                         * line in the argument file.
                         */
                    case WRITE_TO_OUTPUT_FILE_ARGUMENT:
                    case APPEND_TO_OUTPUT_FILE_ARGUMENT:
                    case WRITE_TO_LOG_FILE_ARGUMENT:
                    case APPEND_TO_LOG_FILE_ARGUMENT:
                    case DUPLICATES_FILE_ARGUMENT:
                    case USE_ECO_FILE_ARGUMENT:
                    case CHECK_FILE_ARGUMENT:
                    case FILE_OF_FILES_ARGUMENT:
                    case MOVE_BOUNDS_ARGUMENT:
                    case PLY_BOUNDS_ARGUMENT:
                    case GAMES_PER_FILE_ARGUMENT:
                    case ECO_OUTPUT_LEVEL_ARGUMENT:
                    case FILE_OF_ARGUMENTS_ARGUMENT:
                    case NON_MATCHING_GAMES_ARGUMENT:
                    case TAG_EXTRACTION_ARGUMENT:
                    case LINE_WIDTH_ARGUMENT:
                    case OUTPUT_FORMAT_ARGUMENT:
                        process_argument(line[argument_prefix_len],
                                &line[argument_prefix_len + 1]);
                        linetype = NO_ARGUMENT_MATCH;
                        break;
                    case LONG_FORM_ARGUMENT:
                    {
                        char *arg = &line[argument_prefix_len + 1];
                        char *space = strchr(arg, ' ');
                        if (space != NULL) {
                            /* We need to drop an associated value from arg. */
                            int arglen = space - arg;
                            char *just_arg = (char *) malloc_or_die(arglen + 1);
                            strncpy(just_arg, arg, arglen);
                            just_arg[arglen] = '\0';
                            process_long_form_argument(just_arg,
                                    skip_leading_spaces(space));
			    (void) free((void *) just_arg);
                        }
                        else {
                            process_long_form_argument(arg, "");
                            linetype = NO_ARGUMENT_MATCH;
                        }
                    }
                        break;

                        /* Arguments with no additional
                         * argument value.
                         * All of these apply only to the current
                         * line in the argument file.
                         */
                    case SEVEN_TAG_ROSTER_ARGUMENT:
                    case HELP_ARGUMENT:
                    case ALTERNATIVE_HELP_ARGUMENT:
                    case DONT_KEEP_COMMENTS_ARGUMENT:
                    case DONT_KEEP_DUPLICATES_ARGUMENT:
                    case DONT_MATCH_PERMUTATIONS_ARGUMENT:
                    case DONT_KEEP_NAGS_ARGUMENT:
                    case CHECK_ONLY_ARGUMENT:
                    case KEEP_SILENT_ARGUMENT:
                    case USE_SOUNDEX_ARGUMENT:
                    case MATCH_CHECKMATE_ARGUMENT:
                    case SUPPRESS_ORIGINALS_ARGUMENT:
                    case DONT_KEEP_VARIATIONS_ARGUMENT:
                    case USE_VIRTUAL_HASH_TABLE_ARGUMENT:
                        process_argument(line[argument_prefix_len], "");
                        linetype = NO_ARGUMENT_MATCH;
                        break;

                        /* Arguments whose values persist beyond
                         * the current line.
                         */
                    case ENDINGS_ARGUMENT:
                    case ENDINGS_COLOURED_ARGUMENT:
                    case HASHCODE_MATCH_ARGUMENT:
                    case MOVES_ARGUMENT:
                    case OUTPUT_FEN_STRING_ARGUMENT:
                    case POSITIONS_ARGUMENT:
                    case TAG_ROSTER_ARGUMENT:
                        process_argument(line[argument_prefix_len],
                                &line[argument_prefix_len + 1]);
                    case TAGS_ARGUMENT:
                        /* Apply this type to subsequent lines. */
                        linetype = nexttype;
                        break;
                    default:
                        linetype = nexttype;
                        break;
                }
                (void) free((void *) line);
            }
        }
        (void) fclose(fp);
    }
}

/* Determine which (if any) type of argument is
 * indicated by the contents of the current line.
 * Arguments are assumed to start with the prefix ":-"
 */
static ArgType
classify_arg(const char *line)
{
    /* Valid arguments must have at least one character beyond
     * the prefix.
     */
    static const size_t min_argument_length = 1 + sizeof (argument_prefix) - 1;
    size_t line_length = strlen(line);

    /* Check for a line of the form:
     *            :-argument
     */
    if ((strncmp(line, argument_prefix, argument_prefix_len) == 0) &&
            (line_length >= min_argument_length)) {
        char argument_letter = line[argument_prefix_len];
        switch (argument_letter) {
            case TAGS_ARGUMENT:
            case MOVES_ARGUMENT:
            case POSITIONS_ARGUMENT:
            case ENDINGS_ARGUMENT:
            case ENDINGS_COLOURED_ARGUMENT:
            case TAG_EXTRACTION_ARGUMENT:
            case LINE_WIDTH_ARGUMENT:
            case OUTPUT_FORMAT_ARGUMENT:
            case SEVEN_TAG_ROSTER_ARGUMENT:
            case FILE_OF_ARGUMENTS_ARGUMENT:
            case NON_MATCHING_GAMES_ARGUMENT:
            case DONT_KEEP_COMMENTS_ARGUMENT:
            case DONT_KEEP_DUPLICATES_ARGUMENT:
            case DONT_KEEP_NAGS_ARGUMENT:
            case DONT_MATCH_PERMUTATIONS_ARGUMENT:
            case OUTPUT_FEN_STRING_ARGUMENT:
            case CHECK_ONLY_ARGUMENT:
            case KEEP_SILENT_ARGUMENT:
            case USE_SOUNDEX_ARGUMENT:
            case MATCH_CHECKMATE_ARGUMENT:
            case SUPPRESS_ORIGINALS_ARGUMENT:
            case DONT_KEEP_VARIATIONS_ARGUMENT:
            case WRITE_TO_OUTPUT_FILE_ARGUMENT:
            case WRITE_TO_LOG_FILE_ARGUMENT:
            case APPEND_TO_LOG_FILE_ARGUMENT:
            case APPEND_TO_OUTPUT_FILE_ARGUMENT:
            case DUPLICATES_FILE_ARGUMENT:
            case USE_ECO_FILE_ARGUMENT:
            case CHECK_FILE_ARGUMENT:
            case FILE_OF_FILES_ARGUMENT:
            case MOVE_BOUNDS_ARGUMENT:
            case PLY_BOUNDS_ARGUMENT:
            case GAMES_PER_FILE_ARGUMENT:
            case ECO_OUTPUT_LEVEL_ARGUMENT:
            case HELP_ARGUMENT:
            case ALTERNATIVE_HELP_ARGUMENT:
            case TAG_ROSTER_ARGUMENT:
            case LONG_FORM_ARGUMENT:
            case HASHCODE_MATCH_ARGUMENT:
                return (ArgType) argument_letter;
            default:
                fprintf(GlobalState.logfile,
                        "Unrecognized argument: %s in the argument file.\n",
                        line);
                exit(1);
                return NO_ARGUMENT_MATCH;
        }
    }
    else {
        return NO_ARGUMENT_MATCH;
    }
}

/* Process the argument character and its associated value.
 * This function processes arguments from the command line and
 * from an argument file associated with the -A argument.
 *
 * An argument -ofile.pgn would be passed in as:
 *                'o' and "file.pgn".
 * A zero-length string for associated_value is not necessarily
 * an error, e.g. -e has an optional following filename.
 * NB: If the associated_value is to be used beyond this function,
 * it must be copied.
 */
void
process_argument(char arg_letter, const char *associated_value)
{
    /* Provide an alias for associated_value because it will
     * often represent a file name.
     */
    const char *filename = skip_leading_spaces(associated_value);

    switch (arg_letter) {
        case WRITE_TO_OUTPUT_FILE_ARGUMENT:
        case APPEND_TO_OUTPUT_FILE_ARGUMENT:
            if (GlobalState.ECO_level > 0) {
                fprintf(GlobalState.logfile, "-%c conflicts with -E\n",
                        arg_letter);
            }
            else if (GlobalState.games_per_file > 0) {
                fprintf(GlobalState.logfile, "-%c conflicts with -#\n",
                        arg_letter);
            }
            else if (GlobalState.output_filename != NULL) {
                fprintf(GlobalState.logfile,
                        "-%c: File %s has already been selected for output.\n",
                        arg_letter, GlobalState.output_filename);
                exit(1);
            }
            else if (*filename == '\0') {
                fprintf(GlobalState.logfile, "Usage: -%cfilename.\n", arg_letter);
                exit(1);
            }
            else {
                if (GlobalState.outputfile != NULL && GlobalState.outputfile != stdout) {
                    (void) fclose(GlobalState.outputfile);
                }
                if (arg_letter == WRITE_TO_OUTPUT_FILE_ARGUMENT) {
                    GlobalState.outputfile = must_open_file(filename, "w");
                }
                else {
                    GlobalState.outputfile = must_open_file(filename, "a");
                }
                GlobalState.output_filename = filename;
            }
            break;
        case WRITE_TO_LOG_FILE_ARGUMENT:
        case APPEND_TO_LOG_FILE_ARGUMENT:
            /* Take precautions against multiple log files. */
            if ((GlobalState.logfile != stderr) && (GlobalState.logfile != NULL)) {
                (void) fclose(GlobalState.logfile);
            }
            if (arg_letter == WRITE_TO_LOG_FILE_ARGUMENT) {
                GlobalState.logfile = fopen(filename, "w");
            }
            else {
                GlobalState.logfile = fopen(filename, "a");
            }
            if (GlobalState.logfile == NULL) {
                fprintf(stderr, "Unable to open %s for writing.\n", filename);
                GlobalState.logfile = stderr;
            }
            break;
        case DUPLICATES_FILE_ARGUMENT:
            if (*filename == '\0') {
                fprintf(GlobalState.logfile, "Usage: -%cfilename.\n", arg_letter);
                exit(1);
            }
            else if (GlobalState.suppress_duplicates) {
                fprintf(GlobalState.logfile,
                        "-%c clashes with the -%c flag.\n", arg_letter,
                        DONT_KEEP_DUPLICATES_ARGUMENT);
                exit(1);
            }
            else {
                GlobalState.duplicate_file = must_open_file(filename, "w");
            }
            break;
        case USE_ECO_FILE_ARGUMENT:
            GlobalState.add_ECO = TRUE;
            if (*filename != '\0') {
                GlobalState.eco_file = copy_string(filename);
            }
            else if ((filename = getenv("ECO_FILE")) != NULL) {
                GlobalState.eco_file = filename;
            }
            else {
                /* Use the default which is already set up. */
            }
            initEcoTable();
            break;
        case ECO_OUTPUT_LEVEL_ARGUMENT:
        {
            unsigned level;

            if (GlobalState.output_filename != NULL) {
                fprintf(GlobalState.logfile,
                        "-%c: File %s has already been selected for output.\n",
                        arg_letter,
                        GlobalState.output_filename);
                exit(1);
            }
            else if (GlobalState.games_per_file > 0) {
                fprintf(GlobalState.logfile,
                        "-%c conflicts with -#.\n",
                        arg_letter);
                exit(1);
            }
            else if (sscanf(associated_value, "%u", &level) != 1) {
                fprintf(GlobalState.logfile,
                        "-%c requires a number attached, e.g., -%c1.\n",
                        arg_letter, arg_letter);
                exit(1);
            }
            else if ((level < MIN_ECO_LEVEL) || (level > MAX_ECO_LEVEL)) {
                fprintf(GlobalState.logfile,
                        "-%c level should be between %u and %u.\n",
                        MIN_ECO_LEVEL, MAX_ECO_LEVEL, arg_letter);
                exit(1);
            }
            else {
                GlobalState.ECO_level = level;
            }
        }
            break;
        case CHECK_FILE_ARGUMENT:
            if (*filename != '\0') {
                /* See if it is a single PGN file, or a list
                 * of files.
                 */
                size_t len = strlen(filename);
                /* Check for a .PGN suffix. */
                const char *suffix = output_file_suffix(SAN);

                if ((len > strlen(suffix)) &&
                        (stringcompare(&filename[len - strlen(suffix)],
                        suffix) == 0)) {
                    add_filename_to_source_list(filename, CHECKFILE);
                }
                else {
                    FILE *fp = must_open_file(filename, "r");
                    add_filename_list_from_file(fp, CHECKFILE);
                    (void) fclose(fp);
                }
            }
            break;
        case FILE_OF_FILES_ARGUMENT:
            if (*filename != '\0') {
                FILE *fp = must_open_file(filename, "r");
                add_filename_list_from_file(fp, NORMALFILE);
                (void) fclose(fp);
            }
            else {
                fprintf(GlobalState.logfile, "Filename expected with -%c\n",
                        arg_letter);
            }
            break;
        case MOVE_BOUNDS_ARGUMENT:
        case PLY_BOUNDS_ARGUMENT:
        {
            /* Bounds on the number of moves to be found.
             * "l#" means less-than-or-equal-to.
             * "g#" means greater-than-or-equal-to.
             * Otherwise "#" (or "e#") means that number.
             */
            /* Equal by default. */
            char which = 'e';
            unsigned number;
            Boolean Ok = TRUE;
            const char *bound = associated_value;

            switch (*bound) {
                case 'l':
                case 'u':
                case 'e':
                    which = *bound;
                    bound++;
                    break;
                default:
                    if (!isdigit((int) *bound)) {
                        fprintf(GlobalState.logfile,
                                "-%c must be followed by e, l, or u.\n",
                                arg_letter);
                        Ok = FALSE;
                    }
                    break;
            }
            if (Ok && (sscanf(bound, "%u", &number) == 1)) {
                Ok = set_move_bounds(arg_letter, which, number);
            }
            else {
                fprintf(GlobalState.logfile,
                        "-%c should be in the form -%c[elu]number.\n",
                        arg_letter, arg_letter);
                Ok = FALSE;
            }
            if (!Ok) {
                exit(1);
            }
        }
            break;
        case GAMES_PER_FILE_ARGUMENT:
            if (GlobalState.ECO_level > 0) {
                fprintf(GlobalState.logfile,
                        "-%c conflicts with -E.\n", arg_letter);
                exit(1);
            }
            else if (GlobalState.output_filename != NULL) {
                fprintf(GlobalState.logfile,
                        "-%c: File %s has already been selected for output.\n",
                        arg_letter,
                        GlobalState.output_filename);
                exit(1);
            }
            else {
                if(strchr(associated_value, ',') != NULL) {
                    unsigned games, file_number;
                    if(sscanf(associated_value, "%u,%u", &games, &file_number) == 2) {
                        GlobalState.games_per_file = games;
                        GlobalState.next_file_number = file_number;
                    }
                    else {
                        fprintf(GlobalState.logfile,
                                "-%c should be followed by either one or two unsigned integers.\n",
                                arg_letter);
                        exit(1);
                    }
                }
                else if (sscanf(associated_value, "%u",
                        &GlobalState.games_per_file) != 1) {
                    fprintf(GlobalState.logfile,
                            "-%c should be followed by an unsigned integer.\n",
                            arg_letter);
                    exit(1);
                }
                else {
                    /* Value set. */
                }
            }
            break;
        case FILE_OF_ARGUMENTS_ARGUMENT:
            if (*filename != '\0') {
                /* @@@ Potentially recursive call. Is this safe? */
                read_args_file(filename);
            }
            else {
                fprintf(GlobalState.logfile, "Usage: -%cfilename.\n",
                        arg_letter);
            }
            break;
        case NON_MATCHING_GAMES_ARGUMENT:
            if (*filename != '\0') {
                if (GlobalState.non_matching_file != NULL && GlobalState.non_matching_file != stdout) {
                    (void) fclose(GlobalState.non_matching_file);
                }
                if(strcmp(filename, "stdout") == 0) {
                    GlobalState.non_matching_file = stdout;
                }
                else {
                    GlobalState.non_matching_file = must_open_file(filename, "w");
                }
            }
            else {
                fprintf(GlobalState.logfile, "Usage: -%cfilename.\n", arg_letter);
                exit(1);
            }
            break;
        case TAG_EXTRACTION_ARGUMENT:
            /* A single tag extraction criterion. */
            extract_tag_argument(associated_value, TRUE);
            break;
        case LINE_WIDTH_ARGUMENT:
        { /* Specify an output line width. */
            unsigned length;

            if (sscanf(associated_value, "%u", &length) > 0) {
                set_output_line_length(length);
            }
            else {
                fprintf(GlobalState.logfile,
                        "-%c should be followed by an unsigned integer.\n",
                        arg_letter);
                exit(1);
            }
        }
            break;
        case HELP_ARGUMENT:
            usage_and_exit();
            break;
        case OUTPUT_FORMAT_ARGUMENT:
            /* Whether to use the source form of moves or
             * rewrite them into another format.
             */
        {
            OutputFormat format = which_output_format(associated_value);
            if (format == UCI) {
                /* Rewrite the game in a format suitable for input to
                 * a UCI-compatible engine.
                 * This is actually LALG but involves adjusting a lot of
                 * the other statuses, too.
                 */
                GlobalState.keep_NAGs = FALSE;
                GlobalState.keep_comments = FALSE;
                GlobalState.keep_move_numbers = FALSE;
                GlobalState.keep_checks = FALSE;
                GlobalState.keep_variations = FALSE;
                /* @@@ Warning: arbitrary value. */
                set_output_line_length(5000);
            }
            GlobalState.output_format = format;
        }
            break;
        case SEVEN_TAG_ROSTER_ARGUMENT:
            if ((GlobalState.tag_output_format == ALL_TAGS ||
                 GlobalState.tag_output_format == SEVEN_TAG_ROSTER) &&
                     !GlobalState.only_output_wanted_tags) {
                GlobalState.tag_output_format = SEVEN_TAG_ROSTER;
            }
            else {
                fprintf(GlobalState.logfile,
                        "-%c clashes with another roster-related argument.\n",
                        SEVEN_TAG_ROSTER_ARGUMENT);
                exit(1);
            }
            break;
        case DONT_KEEP_COMMENTS_ARGUMENT:
            if(GlobalState.keep_only_commented_games) {
                fprintf(GlobalState.logfile, "-%c clashes with --commented\n",
                        DONT_KEEP_COMMENTS_ARGUMENT);
                exit(1);
            }
            else {
                GlobalState.keep_comments = FALSE;
            }
            break;
        case DONT_KEEP_DUPLICATES_ARGUMENT:
            /* Make sure that this doesn't clash with -d. */
            if (GlobalState.duplicate_file == NULL) {
                GlobalState.suppress_duplicates = TRUE;
            }
            else {
                fprintf(GlobalState.logfile,
                        "-%c clashes with -%c flag.\n",
                        DONT_KEEP_DUPLICATES_ARGUMENT,
                        DUPLICATES_FILE_ARGUMENT);
                exit(1);
            }
            break;
        case DONT_MATCH_PERMUTATIONS_ARGUMENT:
            GlobalState.match_permutations = FALSE;
            break;
        case DONT_KEEP_NAGS_ARGUMENT:
            GlobalState.keep_NAGs = FALSE;
            break;
        case OUTPUT_FEN_STRING_ARGUMENT:
            /* Output a FEN string at one or more positions.
             * Default is at the end of the game.
             * The FEN string is displayed in a comment.
             */
            if(*associated_value != '\0') {
                if(!GlobalState.add_FEN_comments) {
                    GlobalState.FEN_comment_pattern = copy_string(associated_value);
                }
                else {
                    fprintf(GlobalState.logfile, "-%c%s conflicts with --%s\n",
                            OUTPUT_FEN_STRING_ARGUMENT, associated_value, 
                            "fencomments");
                }
            }
            if (GlobalState.add_FEN_comments) {
                /* Already implied. */
                GlobalState.output_FEN_string = FALSE;
            }
            else {
                GlobalState.output_FEN_string = TRUE;
            }
            break;
        case CHECK_ONLY_ARGUMENT:
            /* Report errors, but don't convert. */
            GlobalState.check_only = TRUE;
            break;
        case KEEP_SILENT_ARGUMENT:
            /* Turn off progress reporting
             * and only report the number of games processed.
             */
            GlobalState.verbosity = 1;
            break;
        case USE_SOUNDEX_ARGUMENT:
            /* Use soundex matches for player tags. */
            GlobalState.use_soundex = TRUE;
            break;
        case MATCH_CHECKMATE_ARGUMENT:
            /* Match only games that end in checkmate. */
            if(GlobalState.match_only_insufficient_material) {
                fprintf(GlobalState.logfile,
                        "-%c clashes with the --insufficient.\n", arg_letter);
                exit(1);
            }
            else if(GlobalState.match_only_stalemate) {
                fprintf(GlobalState.logfile,
                        "-%c clashes with the --stalemate.\n", arg_letter);
                exit(1);
            }
            else {
                GlobalState.match_only_checkmate = TRUE;
            }
            break;
        case SUPPRESS_ORIGINALS_ARGUMENT:
            GlobalState.suppress_originals = TRUE;
            break;
        case DONT_KEEP_VARIATIONS_ARGUMENT:
            if(!GlobalState.split_variants) {
                GlobalState.keep_variations = FALSE;
            }
            else {
                fprintf(GlobalState.logfile,
                        "-%c clashes with the --splitvariants flag.\n", arg_letter);
                exit(1);
            }
            break;
        case USE_VIRTUAL_HASH_TABLE_ARGUMENT:
            GlobalState.use_virtual_hash_table = TRUE;
            break;

        case TAGS_ARGUMENT:
            if (*filename != '\0') {
                read_tag_file(filename, TRUE);
            }
            break;
        case TAG_ROSTER_ARGUMENT:
            if (*filename != '\0') {
                read_tag_roster_file(filename);
            }
            break;
        case MOVES_ARGUMENT:
            if (*filename != '\0') {
                /* Where the list of variations of interest are kept. */
                FILE *variation_file = must_open_file(filename, "r");
                /* We wish to search for particular variations. */
                add_textual_variations_from_file(variation_file);
                fclose(variation_file);
            }
            break;
        case POSITIONS_ARGUMENT:
            if (*filename != '\0') {
                FILE *variation_file = must_open_file(filename, "r");
                /* We wish to search for positional variations. */
                add_positional_variations_from_file(variation_file);
                fclose(variation_file);
            }
            break;
        case ENDINGS_ARGUMENT:
        case ENDINGS_COLOURED_ARGUMENT:
            if (*filename != '\0') {
                if (!build_endings(filename,
                                   arg_letter == ENDINGS_ARGUMENT)) {
                    exit(1);
                }
            }
            break;
        case HASHCODE_MATCH_ARGUMENT:
            if(save_polyglot_hashcode(associated_value)) {
                GlobalState.positional_variations = TRUE;
            }
            else {
                fprintf(GlobalState.logfile, 
                        "-%c must be followed by a hexadecimal hash value rather than %s.\n", 
                        arg_letter, associated_value);
                exit(1);
            }
            break;
        default:
            fprintf(GlobalState.logfile,
                    "Unrecognized argument -%c\n", arg_letter);
    }
}

/* The argument has been expressed in a long-form, i.e. prefixed
 * by --
 * Decode and act on the argument.
 * The associated_value will only be required by some arguments.
 * Return whether one or both were required.
 */
int
process_long_form_argument(const char *argument, const char *associated_value)
{
    if (stringcompare(argument, "addfencastling") == 0) {
        GlobalState.add_fen_castling = TRUE;
        return 1;
    }
    else if (stringcompare(argument, "addhashcode") == 0) {
        GlobalState.add_hashcode_tag = TRUE;
        return 1;
    }
    else if (stringcompare(argument, "addlabeltag") == 0) {
        GlobalState.add_matchlabel_tag = TRUE;
        return 1;
    }
    else if (stringcompare(argument, "addmatchtag") == 0) {
        GlobalState.add_match_tag = TRUE;
        return 1;
    }
    else if (stringcompare(argument, "allownullmoves") == 0) {
        GlobalState.allow_null_moves = TRUE;
        return 1;
    }
    else if (stringcompare(argument, "append") == 0) {
        process_argument(APPEND_TO_OUTPUT_FILE_ARGUMENT, associated_value);
        return 2;
    }
    else if(stringcompare(argument, "btm") == 0) {
        if(GlobalState.whose_move == EITHER_TO_MOVE) {
	    GlobalState.whose_move = BLACK_TO_MOVE;
	}
	else {
	    fprintf(GlobalState.logfile, "%s conflicts with previous setting of white to move.\n", argument);
	}
	return 1;
    }
    else if (stringcompare(argument, "checkfile") == 0) {
        process_argument(CHECK_FILE_ARGUMENT, associated_value);
        return 2;
    }
    else if (stringcompare(argument, "checkmate") == 0) {
        process_argument(MATCH_CHECKMATE_ARGUMENT, "");
        return 1;
    }
    else if (stringcompare(argument, "commented") == 0) {
        if(! GlobalState.keep_comments) {
            fprintf(GlobalState.logfile, "--%s clashes with -%c\n",
                    argument, DONT_KEEP_COMMENTS_ARGUMENT);
            exit(1);
        }
        else {
            GlobalState.keep_only_commented_games = TRUE;
        }
        return 1;
    }
    else if (stringcompare(argument, "commentlines") == 0) {
        GlobalState.separate_comment_lines = TRUE;
        return 1;
    }
    else if (stringcompare(argument, "deletesamesetup") == 0) {
        GlobalState.delete_same_setup = TRUE;
        return 1;
    }
    else if (stringcompare(argument, "detag") == 0) {
        /* Save the tag to be dropped. */
        if (associated_value != NULL) {
            suppress_tag(associated_value);
        }
        else {
            fprintf(GlobalState.logfile,
                    "--%s requires a tag name following it.\n", argument);
            exit(1);
        }
        return 2;
    }
    else if (stringcompare(argument, "dropbefore") == 0) {
        /* Save the comment string to be matched. */
        if (associated_value != NULL) {
            GlobalState.drop_comment_pattern = copy_string(associated_value);
        }
        else {
            fprintf(GlobalState.logfile,
                    "--%s requires a string following it.\n", argument);
            exit(1);
        }
        return 2;
    }
    else if (stringcompare(argument, "dropply") == 0) {
        /* Extract the number. */
        int number = 0;

        if (sscanf(associated_value, "%d", &number) == 1) {
            GlobalState.drop_ply_number = number;
        }
        else {
            fprintf(GlobalState.logfile,
                    "--%s requires a number following it.\n", argument);
            exit(1);
        }
        return 2;
    }
    else if (stringcompare(argument, "duplicates") == 0) {
        process_argument(DUPLICATES_FILE_ARGUMENT, associated_value);
        return 2;
    }
    else if (stringcompare(argument, "evaluation") == 0) {
        /* Output an evaluation is required with each move. */
        GlobalState.output_evaluation = TRUE;
        return 1;
    }
    else if (stringcompare(argument, "fencomments") == 0) {
        if(GlobalState.FEN_comment_pattern == NULL) {
            /* Output a FEN comment after each move. */
            GlobalState.add_FEN_comments = TRUE;
            /* Turn off any separate setting of output_FEN_comment. */
            GlobalState.output_FEN_string = FALSE;
        }
        else {
            fprintf(GlobalState.logfile, "--%s conflicts with -%cpattern", 
                    argument, OUTPUT_FEN_STRING_ARGUMENT);
        }
        return 1;
    }
    else if (stringcompare(argument, "fenpattern") == 0) {
        if(*associated_value != '\0') {
            add_fen_pattern(associated_value, FALSE, "");
            GlobalState.positional_variations = TRUE;
        }
        else {
            fprintf(GlobalState.logfile,
                    "--%s requires a pattern following it.\n", argument);
            exit(1);
        }
        return 2;
    }
    else if (stringcompare(argument, "fenpatterni") == 0) {
        if(*associated_value != '\0') {
            add_fen_pattern(associated_value, TRUE, "");
            GlobalState.positional_variations = TRUE;
        }
        else {
            fprintf(GlobalState.logfile,
                    "--%s requires a pattern following it.\n", argument);
            exit(1);
        }
        return 2;
    }
    else if (stringcompare(argument, "fifty") == 0 || stringcompare(argument, "50") == 0) {
        if(GlobalState.check_for_N_move_rule == 0) {
            GlobalState.check_for_N_move_rule = 50;
            return 1;
        }
        else if(GlobalState.check_for_N_move_rule == 50) {
            return 1;
        }
        else {
            fprintf(GlobalState.logfile, "--%s conflicts with a previous setting of %u.\n",
                    argument, GlobalState.check_for_N_move_rule);
            exit(1);
        }
    }
    else if (stringcompare(argument, "firstgame") == 0) {
        /* Extract the number. */
        unsigned long number;

        if (sscanf(associated_value, "%lu", &number) == 1) {
            if(number >= 1) {
                if(number <= GlobalState.game_limit) {
                    GlobalState.first_game_number = number;
                }
                else {
                    fprintf(GlobalState.logfile,
                            "--%s %lu is incompatible with --gamelimit %lu.\n",
                            argument, number, GlobalState.game_limit);
                    exit(1);
                }
            }
        }
        else {
            fprintf(GlobalState.logfile,
                    "--%s requires a number following it.\n", argument);
            exit(1);
        }
        return 2;
    }
    else if (stringcompare(argument, "fixresulttags") == 0) {
        GlobalState.fix_result_tags = TRUE;
        return 1;
    }
    else if (stringcompare(argument, "fixtagstrings") == 0) {
        GlobalState.fix_tag_strings = TRUE;
        return 1;
    }
    else if (stringcompare(argument, "fuzzydepth") == 0) {
        /* Extract the depth. */
        unsigned depth = 0;

        if (sscanf(associated_value, "%u", &depth) == 1) {
            GlobalState.fuzzy_match_duplicates = TRUE;
            GlobalState.fuzzy_match_depth = depth;
        }
        else {
            fprintf(GlobalState.logfile,
                    "--%s requires a positive number following it.\n", argument);
            exit(1);
        }
        return 2;
    }
    else if (stringcompare(argument, "hashcomments") == 0) {
        /* Output a hashcode comment after each move. */
        GlobalState.add_hashcode_comments = TRUE;
        return 1;
    }
    else if (stringcompare(argument, "help") == 0) {
        process_argument(HELP_ARGUMENT, "");
        return 1;
    }
    else if (stringcompare(argument, "higherratedwinner") == 0) {
        if(! GlobalState.lower_rated_winner) {
            /* The winner must be the higher rated. */
            GlobalState.higher_rated_winner = TRUE;
        }
        else {
            fprintf(GlobalState.logfile,
                    "--%s clashes with --lower_rated_winner.\n", argument);
            exit(1);
        }
        return 1;
    }
    else if (stringcompare(argument, "insufficient") == 0) {
        if(GlobalState.match_only_checkmate) {
            fprintf(GlobalState.logfile,
                    "--%s clashes with the --checkmate.\n", argument);
            exit(1);
        }
        else if(GlobalState.match_only_stalemate) {
            fprintf(GlobalState.logfile,
                    "--%s clashes with the --stalemate.\n", argument);
            exit(1);
        }
        else {
            GlobalState.match_only_insufficient_material = TRUE;
        }
        return 1;
    }
    else if (stringcompare(argument, "json") == 0) {
        GlobalState.json_format = TRUE;
        return 1;
    }
    else if (stringcompare(argument, "keepbroken") == 0) {
        GlobalState.keep_broken_games = TRUE;
        return 1;
    }
    else if (stringcompare(argument, "lichesscommentfix") == 0) {
        GlobalState.lichess_comment_fix = TRUE;
        return 1;
    }
    else if (stringcompare(argument, "linelength") == 0) {
        process_argument(LINE_WIDTH_ARGUMENT,
                associated_value);
        return 2;
    }
    else if (stringcompare(argument, "linenumbers") == 0) {
        /* Save the marker string to be output. */
        if (associated_value != NULL) {
            GlobalState.line_number_marker = copy_string(associated_value);
        }
        else {
            fprintf(GlobalState.logfile,
                    "--%s requires a string following it.\n", argument);
            exit(1);
        }
        return 2;
    }
    else if (stringcompare(argument, "lowerratedwinner") == 0) {
        if(! GlobalState.higher_rated_winner) {
            /* The winner must be the lower rated. */
            GlobalState.lower_rated_winner = TRUE;
        }
        else {
            fprintf(GlobalState.logfile,
                    "--%s clashes with --higher_rated_winner.\n", argument);
            exit(1);
        }
        return 1;
    }
    else if (stringcompare(argument, "markmatches") == 0) {
        if (*associated_value != '\0') {
            GlobalState.add_position_match_comments = TRUE;
            GlobalState.position_match_comment = copy_string(associated_value);
        }
        else {
            fprintf(GlobalState.logfile,
                    "--%s requires a comment string following it.\n", argument);
            exit(1);
        }
        return 2;
    }
    else if (stringcompare(argument, "matchplylimit") == 0) {
        unsigned limit = 0;

        /* Extract the limit. */
        if (sscanf(associated_value, "%u", &limit) == 1) {
            if (limit > 0) {
                if(limit >= GlobalState.depth_of_positional_search) {
                    GlobalState.depth_of_positional_search = limit;
                }
                else {
                    fprintf(GlobalState.logfile,
                            "--%s of %u conflicts with existing higher limit of %u\n",
                            argument,
                            limit,
                            GlobalState.depth_of_positional_search);
                    exit(1);
                }
            }
            else {
                fprintf(GlobalState.logfile,
                        "--%s requires a number greater than or equal to zero.\n", argument);
                exit(1);
            }
        }
        else {
            fprintf(GlobalState.logfile,
                    "--%s requires a number following it.\n", argument);
            exit(1);
        }
        return 2;
    }
    else if (stringcompare(argument, "materialy") == 0) {
        if (*associated_value != '\0') {
            (void) process_material_description(associated_value, FALSE, FALSE);
        }
        else {
            fprintf(GlobalState.logfile,
                    "--%s requires a string of material following it.\n", argument);
            exit(1);
        }
        return 2;
    }
    else if (stringcompare(argument, "materialz") == 0) {
        if (*associated_value != '\0') {
            (void) process_material_description(associated_value, TRUE, FALSE);
        }
        else {
            fprintf(GlobalState.logfile,
                    "--%s requires a string of material following it.\n", argument);
            exit(1);
        }
        return 2;
    }
    else if (stringcompare(argument, "minmoves") == 0) {
        /* Extract the number. */
        unsigned number = 0;

        if (sscanf(associated_value, "%u", &number) == 1) {
            set_move_bounds(MOVE_BOUNDS_ARGUMENT, 'l', number);
        }
        else {
            fprintf(GlobalState.logfile,
                    "--%s requires a number following it.\n", argument);
            exit(1);
        }
        return 2;
    }
    else if (stringcompare(argument, "gamelimit") == 0) {
        /* Extract the number. */
        unsigned long number = 0;

        if (sscanf(associated_value, "%lu", &number) == 1) {
            if(number >= GlobalState.first_game_number) {
                GlobalState.game_limit = number;
            }
            else {
                fprintf(GlobalState.logfile,
                        "--%s %lu is incompatible with --firstgame %lu.\n",
                        argument, number, GlobalState.first_game_number);
                exit(1);
            }
        }
        else {
            fprintf(GlobalState.logfile,
                    "--%s requires a number following it.\n", argument);
            exit(1);
        }
        return 2;
    }
    else if (stringcompare(argument, "maxmoves") == 0) {
        /* Extract the number. */
        unsigned number = 0;

        if (sscanf(associated_value, "%u", &number) == 1) {
            set_move_bounds(MOVE_BOUNDS_ARGUMENT, 'u', number);
        }
        else {
            fprintf(GlobalState.logfile,
                    "--%s requires a number following it.\n", argument);
            exit(1);
        }
        return 2;
    }
    else if (stringcompare(argument, "minply") == 0) {
        /* Extract the number. */
        unsigned number = 0;

        if (sscanf(associated_value, "%u", &number) == 1) {
            set_move_bounds(PLY_BOUNDS_ARGUMENT, 'l', number);
        }
        else {
            fprintf(GlobalState.logfile,
                    "--%s requires a number following it.\n", argument);
            exit(1);
        }
        return 2;
    }
    else if (stringcompare(argument, "maxply") == 0) {
        /* Extract the number. */
        unsigned number = 0;

        if (sscanf(associated_value, "%u", &number) == 1) {
            set_move_bounds(PLY_BOUNDS_ARGUMENT, 'u', number);
        }
        else {
            fprintf(GlobalState.logfile,
                    "--%s requires a number following it.\n", argument);
            exit(1);
        }
        return 2;
    }
    else if (stringcompare(argument, "nestedcomments") == 0) {
        GlobalState.allow_nested_comments = TRUE;
        return 1;
    }
    else if (stringcompare(argument, "nobadresults") == 0) {
        GlobalState.reject_inconsistent_results = TRUE;
        return 1;
    }
    else if (stringcompare(argument, "nochecks") == 0) {
        GlobalState.keep_checks = FALSE;
        return 1;
    }
    else if (stringcompare(argument, "nocomments") == 0) {
        process_argument(DONT_KEEP_COMMENTS_ARGUMENT, "");
        return 1;
    }
    else if (stringcompare(argument, "noduplicates") == 0) {
        process_argument(DONT_KEEP_DUPLICATES_ARGUMENT, "");
        return 1;
    }
    else if (stringcompare(argument, "nofauxep") == 0) {
        GlobalState.suppress_redundant_ep_info = TRUE;
        return 1;
    }
    else if (stringcompare(argument, "nomovenumbers") == 0) {
        GlobalState.keep_move_numbers = FALSE;
        return 1;
    }
    else if (stringcompare(argument, "nonags") == 0) {
        process_argument(DONT_KEEP_NAGS_ARGUMENT, "");
        return 1;
    }
    else if (stringcompare(argument, "nosetuptags") == 0) {
        if (GlobalState.setup_status != SETUP_TAG_OK) {
            fprintf(GlobalState.logfile, "--%s conflicts with --onlysetuptagso\n", argument);
            exit(1);
        }
        GlobalState.setup_status = NO_SETUP_TAG;
        return 1;
    }
    else if (stringcompare(argument, "noresults") == 0) {
        GlobalState.keep_results = FALSE;
        return 1;
    }
    else if (stringcompare(argument, "notags") == 0) {
        if (GlobalState.tag_output_format == ALL_TAGS ||
                GlobalState.tag_output_format == NO_TAGS) {
            GlobalState.tag_output_format = NO_TAGS;
        }
        else {
            fprintf(GlobalState.logfile,
                    "--notags clashes with another roster-related argument.\n");
            exit(1);
        }
        return 1;
    }
    else if (stringcompare(argument, "nounique") == 0) {
        process_argument(SUPPRESS_ORIGINALS_ARGUMENT, "");
        return 1;
    }
    else if (stringcompare(argument, "novars") == 0) {
        process_argument(DONT_KEEP_VARIATIONS_ARGUMENT, "");
        return 1;
    }
    else if (stringcompare(argument, "odds") == 0) {
        GlobalState.find_odds_games = TRUE;
        return 1;
    }
    else if (stringcompare(argument, "onlysetuptags") == 0) {
        if (GlobalState.setup_status != SETUP_TAG_OK) {
            fprintf(GlobalState.logfile, "--%s conflicts with --nosetuptags\n", argument);
            exit(1);
        }
        GlobalState.setup_status = SETUP_TAG_ONLY;
        return 1;
    }
    else if (stringcompare(argument, "output") == 0) {
        process_argument(WRITE_TO_OUTPUT_FILE_ARGUMENT, associated_value);
        return 2;
    }
    else if (stringcompare(argument, "plycount") == 0) {
        GlobalState.output_plycount = TRUE;
        return 1;
    }
    else if (stringcompare(argument, "plylimit") == 0) {
        int limit = 0;

        /* Extract the limit. */
        if (sscanf(associated_value, "%d", &limit) == 1) {
            if (limit >= 0) {
                GlobalState.output_ply_limit = limit;
            }
            else {
                fprintf(GlobalState.logfile,
                        "--%s requires a number greater than or equal to zero.\n", argument);
                exit(1);
            }
        }
        else {
            fprintf(GlobalState.logfile,
                    "--%s requires a number following it.\n", argument);
            exit(1);
        }
        return 2;
    }
    else if (stringcompare(argument, "piececount") == 0) {
        int count = 0;

        /* Extract the count. */
        if (sscanf(associated_value, "%d", &count) == 1) {
            if (count >= 2) {
                GlobalState.piece_count = count;
            }
            else {
                fprintf(GlobalState.logfile,
                        "--%s requires a number greater than or equal to 2.\n", argument);
                exit(1);
            }
        }
        else {
            fprintf(GlobalState.logfile,
                    "--%s requires a number following it.\n", argument);
            exit(1);
        }
        return 2;
    }
    else if (stringcompare(argument, "quiescent") == 0) {
        int threshold = 0;

        /* Extract the threshold. */
        if (sscanf(associated_value, "%d", &threshold) == 1) {
            if (threshold >= 0) {
                GlobalState.quiescence_threshold = threshold;
            }
            else {
                fprintf(GlobalState.logfile,
                        "--%s requires a number greater than or equal to zero.\n", argument);
                exit(1);
            }
        }
        else {
            fprintf(GlobalState.logfile,
                    "--%s requires a number following it.\n", argument);
            exit(1);
        }
        return 2;
    }
    else if (stringcompare(argument, "quiet") == 0) {
        /* No progress output at all. */
        GlobalState.verbosity = 0;
        return 1;
    }
    else if (stringcompare(argument, "repetition") == 0) {
        if(GlobalState.check_for_repetition == 0) {
            GlobalState.check_for_repetition = 3;
            return 1;
        }
        else if(GlobalState.check_for_repetition == 3) {
            /* Duplicate. */
            return 1;
        }
        else {
            fprintf(GlobalState.logfile, "--%s conflicts with a previous setting.\n",
                    argument);
            exit(1);
        }
    }
    else if (stringcompare(argument, "repetition5") == 0) {
        if(GlobalState.check_for_repetition == 0) {
            GlobalState.check_for_repetition = 5;
            return 1;
        }
        else if(GlobalState.check_for_repetition == 5) {
            /* Duplicate. */
            return 1;
        }
        else {
            fprintf(GlobalState.logfile, "--%s clashes with a different setting.\n",
                    argument);
            exit(1);
        }
    }
    else if (stringcompare(argument, "selectonly") == 0) {
        /* Extract the selected match numbers from a list. */
        game_number *number_list = extract_game_number_list(associated_value);
        if (number_list != NULL) {
            GlobalState.matching_game_numbers = number_list;
            GlobalState.next_game_number_to_output = number_list;
        }
        else {
            exit(1);
        }
        return 2;
    }
    else if (stringcompare(argument, "seven") == 0) {
        process_argument(SEVEN_TAG_ROSTER_ARGUMENT, "");
        return 1;
    }
    else if (stringcompare(argument, "seventyfive") == 0 || stringcompare(argument, "75") == 0) {
        if(GlobalState.check_for_N_move_rule == 0) {
            GlobalState.check_for_N_move_rule = 75;
            return 1;
        }
        else if(GlobalState.check_for_N_move_rule == 75) {
            return 1;
        }
        else {
            fprintf(GlobalState.logfile, "--%s conflicts with a previous setting of %u.\n",
                    argument, GlobalState.check_for_N_move_rule);
            exit(1);
        }
    }
    else if (stringcompare(argument, "skipmatching") == 0) {
        /* Extract the selected match numbers from a list. */
        game_number *number_list = extract_game_number_list(associated_value);
        if (number_list != NULL) {
            GlobalState.skip_game_numbers = number_list;
            GlobalState.next_game_number_to_skip = number_list;
        }
        else {
            exit(1);
        }
        return 2;
    }
    else if (stringcompare(argument, "splitvariants") == 0) {
        if(GlobalState.keep_variations) {
            GlobalState.split_variants = TRUE;
            if(associated_value != NULL) {
                unsigned limit;
                if(sscanf(associated_value, "%u", &limit) == 1) {
                    GlobalState.split_depth_limit = limit;
                    return 2;
                }
                else {
                    return 1;
                }
            }
            else {
                return 1;
            }
        }
        else {
            fprintf(GlobalState.logfile,
                    "--%s clashes with the -%c flag.\n", argument,
                    DONT_KEEP_VARIATIONS_ARGUMENT);
            exit(1);
            return 1;
        }
    }
    else if (stringcompare(argument, "stalemate") == 0) {
        if(GlobalState.match_only_checkmate) {
            fprintf(GlobalState.logfile,
                    "--%s clashes with the --checkmate.\n", argument);
            exit(1);
        }
        else if(GlobalState.match_only_insufficient_material) {
            fprintf(GlobalState.logfile,
                    "--%s clashes with the --insufficient.\n", argument);
            exit(1);
        }
        else {
            GlobalState.match_only_stalemate = TRUE;
        }
        return 1;
    }
    else if (stringcompare(argument, "startply") == 0) {
        if(associated_value != NULL) {
            int limit;
            if(sscanf(associated_value, "%d", &limit) == 1) {
                if(limit >= 1) {
                    GlobalState.startply = (unsigned) limit;
                    return 2;
                }
                else {
                    fprintf(GlobalState.logfile, 
                            "--%s must be greater than or equal to 1.\n", argument);
                    exit(1);
                }
            }
            else {
                fprintf(GlobalState.logfile,
                        "--%s requires a number greater than or equal to 1.\n", argument);
                exit(1);
            }
        }
        else {
            fprintf(GlobalState.logfile,
                    "--%s requires a number greater than or equal to 1.\n", argument);
            exit(1);
        }
    }
    else if (stringcompare(argument, "stopafter") == 0) {
        int limit = 0;

        /* Extract the limit. */
        if (sscanf(associated_value, "%d", &limit) == 1) {
            if (limit > 0) {
                GlobalState.maximum_matches = limit;
            }
            else {
                fprintf(GlobalState.logfile,
                        "--%s requires a number greater than zero.\n", argument);
                exit(1);
            }
        }
        else {
            fprintf(GlobalState.logfile,
                    "--%s requires a number greater than zero to follow it.\n", argument);
            exit(1);
        }
        return 2;
    }
    else if (stringcompare(argument, "suppressmatched") == 0) {
        GlobalState.suppress_matched = TRUE;
        return 1;
    }
    else if (stringcompare(argument, "tagsubstr") == 0) {
        GlobalState.tag_match_anywhere = TRUE;
        return 1;
    }
    else if (stringcompare(argument, "totalplycount") == 0) {
        GlobalState.output_total_plycount = TRUE;
        return 1;
    }
    else if (stringcompare(argument, "underpromotion") == 0) {
        GlobalState.match_underpromotion = TRUE;
        return 1;
    }
    else if (stringcompare(argument, "vanywhere") == 0) {
        GlobalState.variation_match_anywhere = TRUE;
        return 1;
    }
    else if (stringcompare(argument, "version") == 0) {
        fprintf(GlobalState.logfile, "pgn-extract %s\n", CURRENT_VERSION);
        exit(0);
        return 1;
    }
    else if(stringcompare(argument, "wtm") == 0) {
        if(GlobalState.whose_move == EITHER_TO_MOVE) {
	    GlobalState.whose_move = WHITE_TO_MOVE;
	}
	else {
	    fprintf(GlobalState.logfile, "%s conflicts with previous setting of black to move.\n", argument);
	}
	return 1;
    }
    else if (stringcompare(argument, "xroster") == 0) {
        if(GlobalState.tag_output_format == SEVEN_TAG_ROSTER) {
            fprintf(GlobalState.logfile,
                    "--%s clashes with -%c.\n",
                    argument, SEVEN_TAG_ROSTER_ARGUMENT);
            exit(1);
        }
        GlobalState.only_output_wanted_tags = TRUE;
        return 1;
    }
    else {
        fprintf(GlobalState.logfile,
                "Unrecognised long-form argument: --%s\n",
                argument);
        exit(1);
        return 1;
    }
}

/*
 * Extract a list of game numbers of the form: range[,range ...].
 * Where range is either N or N1:N2.
 * The numbers must be in ascending order and > 0.
 */
static game_number *
extract_game_number_list(const char *number_list)
{
    char *csv = copy_string(number_list);
    Boolean ok = TRUE;
    game_number *head = NULL, *tail = NULL;
    const char *token = strtok(csv, ",");
    unsigned long last_number = 0;
    while(token != NULL && ok) {
        unsigned long min, max;
        if(strchr(token, ':') != NULL) {
            if(sscanf(token, "%lu:%lu", &min, &max) == 2) {
                if(min > last_number && min <= max) {
                    last_number = max;
                }
                else {
                    ok = FALSE;
                }
            }
            else {
                ok = FALSE;
            }
        }
        else if(sscanf(token, "%lu", &min) == 1) {
            if(min > last_number) {
                max = min;
                last_number = max;
            }
            else {
                ok = FALSE;
            }
        }
        else {
            ok = FALSE;
        }
        if(ok) {
            game_number *list_item = (game_number *) malloc_or_die(sizeof(*list_item));
            list_item->min = min;
            list_item->max = max;
            list_item->next = NULL;
            if(tail != NULL) {
                tail->next = list_item;
                tail = list_item;
            }
            else {
                head = tail = list_item;
            }
            token = strtok(NULL, ",");
        }
        else {
            fprintf(GlobalState.logfile, 
                    "Numbers in %s must be in the format N or N:N and in ascending order.",
                    number_list);
        }
    }
    (void) free((void *) csv);
    if(ok) {
        return head;
    }
    else {
        while(head != NULL) {
            game_number *next = head->next;
            (void) free((void *) head);
            head = next;
        }
        return NULL;
    }
}

/* Set the lower and/or upper bounds limits.
 * which must be one of l/e/u
 */
static Boolean set_move_bounds(char bounds_or_ply, char limit, unsigned number)
{
    Boolean Ok;
    GlobalState.check_move_bounds = TRUE;
    switch (limit) {
        case 'e':
            GlobalState.lower_move_bound =
                    bounds_or_ply == MOVE_BOUNDS_ARGUMENT ? 2 * (number - 1) + 1 : number;
            GlobalState.upper_move_bound =
                    bounds_or_ply == MOVE_BOUNDS_ARGUMENT ? 2 * number : number;
            Ok = TRUE;
            break;
        case 'l':
            if (number <= GlobalState.upper_move_bound) {
                GlobalState.lower_move_bound =
                        bounds_or_ply == MOVE_BOUNDS_ARGUMENT ? 2 * (number - 1) + 1 : number;
                Ok = TRUE;
            }
            else {
                fprintf(GlobalState.logfile,
                        "Lower bound of ply limit is greater than the upper bound: bound ignored.\n");
                Ok = FALSE;
            }
            break;
        case 'u':
            if (number >= GlobalState.lower_move_bound) {
                GlobalState.upper_move_bound =
                        bounds_or_ply == MOVE_BOUNDS_ARGUMENT ? 2 * number : number;
                Ok = TRUE;
            }
            else {
                fprintf(GlobalState.logfile,
                        "Upper bound of ply limit is smaller than the lower bound: bound ignored.\n");
                Ok = FALSE;
            }
            break;
        default:
            fprintf(GlobalState.logfile,
                    "Internal error: %c must be one of e/l/u.\n",
                    limit);
            Ok = FALSE;
            break;
    }
    return Ok;
}
