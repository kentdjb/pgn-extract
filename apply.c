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
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <inttypes.h>
#include <errno.h>
#include "bool.h"
#include "mymalloc.h"
#include "defs.h"
#include "typedef.h"
#include "map.h"
#include "apply.h"
#include "tokens.h"
#include "taglist.h"
#include "output.h"
#include "lex.h"
#include "grammar.h"
#include "moves.h"
#include "eco.h"
#include "decode.h"
#include "hashing.h"
#include "fenmatcher.h"
#include "zobrist.h"
#include "end.h"

/* Define a positional search depth that should look at the
 * full length of a game.  This is used in play_moves().
 */
#define DEFAULT_POSITIONAL_DEPTH 300

/* Prototypes of functions limited to this file. */
static Boolean check_move_validity(Game *game_details, Board *board, Move *moves, Boolean mainline);
static Boolean check_variation_validity(const Game *game_details, const Board *board, Variation *variation);
static const char *position_matches(const Board *board);
static Boolean play_moves(Game *game_details, Board *board, Move *moves,
        unsigned max_depth, Boolean check_move_validity,
        Boolean mainline);
static Boolean apply_variations(const Game *game_details, const Board *board,
        Variation *variation, Boolean check_move_validity);
static Boolean rewrite_variations(const Board *board, Variation *variation);
static Boolean rewrite_moves(Game *game, Board *board, Move *move_details);
static void build_FEN_components(const Board *board, char *epd, char *fen_suffix);
static unsigned plies_in_move_sequence(Move *moves);
static Boolean drop_plies_from_start(Game *game, Move *moves, int plies_to_drop);
#if 0
static void append_evaluation(Move *move_details, const Board *board);
static void append_FEN_comment(Move *move_details, const Board *board);
static void append_hashcode_comment(Move *move_details, Board *board);
#endif
static double evaluate(const Board *board);
static double shannonEvaluation(const Board *board);
static void print_board(const Board *board, FILE *outfp);
static StringList *find_matching_comment(const char *comment_pattern,
                                          const CommentList *comment);

/* The English SAN piece characters. These are
 * always used when building a FEN string, rather
 * than using any language-dependent user settings.
 */
static char SAN_piece_characters[NUM_PIECE_VALUES] = {
    '?', '?',
    'P', 'N', 'B', 'R', 'Q', 'K'
};


/* These letters may be changed via a call to set_output_piece_characters
 * with a string of the form "PNBRQK".
 * This would normally be done with the -Wsan argument.
 */
static const char *output_piece_characters[NUM_PIECE_VALUES] = {
    "?", "?",
    "P", "N", "B", "R", "Q", "K"
};

/* Check whether the given result is valid. */
static Boolean valid_result(const char *result)
{
    return (strcmp(result,"1-0") == 0) || (strcmp(result,"0-1") == 0) ||
           (strcmp(result,"1/2-1/2") == 0) || (strcmp(result,"*") == 0);
}

/* letters should contain a string of the form: "PNBRQK" */
void set_output_piece_characters(const char *letters)
{
    if (letters == NULL) {
        fprintf(GlobalState.logfile,
                "NULL string passed to set_output_piece_characters.\n");
    }
    else {
        Piece piece;
        int piece_index;
        for (piece_index = 0, piece = PAWN; piece <= KING &&
                letters[piece_index] != '\0'; piece++) {
            /* Check whether we have a single character piece, 
             * or one of the form X+Y, where the piece is represented
             * by the combination XY.
             */
            if (letters[piece_index + 1] == '+') {
                /* A two-char piece. */
                static char double_char_piece[] = "XY";
                double_char_piece[0] = letters[piece_index];
                piece_index++;
                /* Skip the plus. */
                piece_index++;
                if (letters[piece_index] != '\0') {
                    double_char_piece[1] = letters[piece_index];
                    output_piece_characters[piece] = copy_string(double_char_piece);
                    piece_index++;
                }
                else {
                    fprintf(GlobalState.logfile,
                            "Missing piece letter following + in -Wsan%s.\n",
                            letters);
                    exit(1);
                }
            }
            else {
                static char single_char_piece[] = "X";
                *single_char_piece = letters[piece_index];
                output_piece_characters[piece] = copy_string(single_char_piece);
                piece_index++;
            }
        }
        if (piece < NUM_PIECE_VALUES) {
            fprintf(GlobalState.logfile,
                    "Insufficient piece letters found with -Wsan%s.\n",
                    letters);
            fprintf(GlobalState.logfile,
                    "The argument should be of the form -Wsan%s.\n",
                    "PNBRQK");
            exit(1);
        }
        else if (letters[piece_index] != '\0') {
            fprintf(GlobalState.logfile,
                    "Too many piece letters found with -Wsan%s.\n",
                    letters);
            fprintf(GlobalState.logfile,
                    "The argument should be of the form -Wsan%s.\n",
                    "PNBRQK");
            exit(1);
        }
        else {
            /* Ok. */
        }
    }
}

/* Return a fresh copy of the given string. */
char *
copy_string(const char *str)
{
    char *result;
    if(str != NULL) {
        size_t len = strlen(str);

        result = (char *) malloc_or_die(len + 1);
        strcpy(result, str);
    }
    else {
        result = NULL;
    }
    return result;
}

/* Allocate space for a new board. */
static Board *
allocate_new_board(void)
{
    return (Board *) malloc_or_die(sizeof (Board));
}

/* Free the board space. */
void
free_board(Board *board)
{
    (void) free((void *) board);
}

Piece
convert_FEN_char_to_piece(char c)
{
    Piece piece = EMPTY;

    switch (c) {
        case 'K': case 'k':
            piece = KING;
            break;
        case 'Q': case 'q':
            piece = QUEEN;
            break;
        case 'R': case 'r':
            piece = ROOK;
            break;
        case 'N': case 'n':
            piece = KNIGHT;
            break;
        case 'B': case 'b':
            piece = BISHOP;
            break;
        case 'P': case 'p':
            piece = PAWN;
            break;
    }
    return piece;
}

/* Return the SAN letter associated with the given piece. */
char
SAN_piece_letter(Piece piece)
{
    if (piece < NUM_PIECE_VALUES) {
        return SAN_piece_characters[piece];
    }
    else {
        return '?';
    }
}

/* Return the SAN letter for the given Piece. */
char
coloured_piece_to_SAN_letter(Piece coloured_piece)
{
    Piece piece = EXTRACT_PIECE(coloured_piece);
    char letter = SAN_piece_letter(piece);
    if (EXTRACT_COLOUR(coloured_piece) == BLACK) {
        letter = tolower(letter);
    }
    return letter;
}

/* Find the position of the innermost or outermost rook for
 * the given castling move.
 */
static Col
find_rook_starting_position(const Board *board, Colour colour, MoveClass castling, Boolean outermost)
{
    Rank rank;
    Col col, boundary;
    int direction;
    Piece rook = MAKE_COLOURED_PIECE(colour, ROOK);
    /* Default search is outermost for the first rook and rook_count is 1.
     * If the innermost is required then the rook_count is 2.
     */
    int rook_count;
    if (outermost) {
        rook_count = 1;
    }
    else {
        rook_count = 2;
    }

    if (colour == WHITE) {
        rank = FIRSTRANK;
    }
    else {
        rank = LASTRANK;
    }
    if (castling == KINGSIDE_CASTLE) {
        direction = -1;
        col = LASTCOL;
        boundary = FIRSTCOL - 1;
    }
    else {
        direction = 1;
        col = FIRSTCOL;
        boundary = LASTCOL + 1;
    }
    while (col != boundary && rook_count > 0) {
        if (board->board[RankConvert(rank)][ColConvert(col)] != rook) {
            col += direction;
        }
        else {
            rook_count--;
            if (rook_count != 0) {
                col += direction;
            }
        }
    }
    if (col != boundary) {
        return colour == WHITE ? col : col;
    }
    else {
        return '\0';
    }
}

/* Set up the board from the FEN string passed as
 * argument.
 * This has the form:
 *        Forsythe string of the setup position.
 *        w/b - colour to move.
 *        castling permissions.
 *        en-passant square.
 *        half-moves since pawn move/piece capture.
 *        move number.
 */
Board *
new_fen_board(const char *fen)
{
    Board *new_board = allocate_new_board();
    /* Start with a clear board. */
    static const Board initial_board = {
        {
            { OFF, OFF, OFF, OFF, OFF, OFF, OFF, OFF, OFF, OFF, OFF, OFF},
            { OFF, OFF, OFF, OFF, OFF, OFF, OFF, OFF, OFF, OFF, OFF, OFF},
            { OFF, OFF, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, OFF, OFF},
            { OFF, OFF, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, OFF, OFF},
            { OFF, OFF, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, OFF, OFF},
            { OFF, OFF, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, OFF, OFF},
            { OFF, OFF, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, OFF, OFF},
            { OFF, OFF, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, OFF, OFF},
            { OFF, OFF, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, OFF, OFF},
            { OFF, OFF, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, OFF, OFF},
            { OFF, OFF, OFF, OFF, OFF, OFF, OFF, OFF, OFF, OFF, OFF, OFF},
            { OFF, OFF, OFF, OFF, OFF, OFF, OFF, OFF, OFF, OFF, OFF, OFF}
        },
        /* Who to move next. */
        WHITE,
        /* Move number. */
        1,
        /* Default castling rights. Support Chess960. */
        'h', 'a', 'h', 'a',
        /* Initial king positions. */
        'e', FIRSTRANK,
        'e', LASTRANK,
        /* En Passant rights. */
        FALSE, 0, 0,
        /* Initial hash value. */
        0ul,
        /* half-move_clock */
        0,
    };
    Rank rank = LASTRANK;
    Col col;
    const char *fen_char = fen;
    Boolean Ok = TRUE;
    /* In some circumstances we will try to parse the game data,
     * even if there are errors.
     */
    Boolean try_to_parse_game = FALSE;

    /* Reset the contents of the new board. */
    *new_board = initial_board;
    /* Extract the piece positions. */
    col = FIRSTCOL;
    while (Ok && (*fen_char != ' ') && (*fen_char != '\0') &&
            (rank >= FIRSTRANK)) {
        Piece piece;
        char ch = *fen_char;
        Colour colour;

        if ((piece = convert_FEN_char_to_piece(ch)) != EMPTY) {
            if (isupper((int) ch)) {
                colour = WHITE;
            }
            else {
                colour = BLACK;
            }
            if (col <= LASTCOL) {
                new_board->board[RankConvert(rank)][ColConvert(col)] =
                        MAKE_COLOURED_PIECE(colour, piece);
                if (piece == KING) {
                    if (colour == WHITE) {
                        new_board->WKingCol = col;
                        new_board->WKingRank = rank;
                    }
                    else {
                        new_board->BKingCol = col;
                        new_board->BKingRank = rank;
                    }
                }
                col++;
                fen_char++;
            }
            else {
                Ok = FALSE;
            }
        }
        else if (isdigit((int) ch)) {
            if (('1' <= ch) && (ch <= '8')) {
                col += ch - '0';
                /* In filling up the remaining columns of a rank we will
                 * temporarily exceed LASTCOL, but we expect the
                 * next character to be '/' so that we reset.
                 */
                if (col <= (LASTCOL + 1)) {
                    fen_char++;
                }
                else {
                    Ok = FALSE;
                }
            }
            else {
                Ok = FALSE;
            }
        }
        else if (ch == '/') {
            /* End of that rank. We should have completely filled the
             * previous rank.
             */
            if (col == (LASTCOL + 1)) {
                col = FIRSTCOL;
                rank--;
                fen_char++;
            }
            else {
                Ok = FALSE;
            }
        }
        else {
            /* Unknown character. */
            Ok = FALSE;
        }
    }
    /* As we don't print any error messages until the end of the function,
     * we don't need to guard everything with if(Ok).
     */
    if (*fen_char == ' ') {
        /* Find out who is to move. */
        fen_char++;
    }
    else {
        Ok = FALSE;
    }
    if (*fen_char == 'w') {
        new_board->to_move = WHITE;
        fen_char++;
    }
    else if (*fen_char == 'b') {
        new_board->to_move = BLACK;
        fen_char++;
    }
    else {
        Ok = FALSE;
    }
    if (*fen_char == ' ') {
        fen_char++;
    }
    else {
        Ok = FALSE;
    }
    /* Determine castling rights. */
    if (*fen_char == '-') {
        /* No castling rights -- default above. */
        new_board->WKingCastle = new_board->WQueenCastle =
                new_board->BKingCastle = new_board->BQueenCastle = '\0';
        fen_char++;
    }
    else {
        /* Check to make sure that this section isn't empty. */
        if (*fen_char == ' ') {
            Ok = FALSE;
        }

        /* Accommodate Chess960 notation for castling. */
        if (*fen_char == 'K') {
            new_board->WKingCastle = find_rook_starting_position(new_board, WHITE, KINGSIDE_CASTLE, TRUE);
            fen_char++;
        }
        else if (*fen_char >= 'A' && *fen_char <= 'H') {
            new_board->WKingCastle = tolower(*fen_char);
            fen_char++;
        }
        else {
            new_board->WKingCastle = '\0';
        }
        if (*fen_char == 'Q') {
            new_board->WQueenCastle = find_rook_starting_position(new_board, WHITE, QUEENSIDE_CASTLE, TRUE);
            fen_char++;
        }
        else if (*fen_char >= 'A' && *fen_char <= 'H') {
            new_board->WQueenCastle = tolower(*fen_char);
            fen_char++;
        }
        else {
            new_board->WQueenCastle = '\0';
        }
        if (*fen_char == 'k') {
            new_board->BKingCastle = find_rook_starting_position(new_board, BLACK, KINGSIDE_CASTLE, TRUE);
            fen_char++;
        }
        else if (*fen_char >= 'a' && *fen_char <= 'h') {
            new_board->BKingCastle = *fen_char;
            fen_char++;
        }
        else {
            new_board->BKingCastle = '\0';
        }
        if (*fen_char == 'q') {
            new_board->BQueenCastle = find_rook_starting_position(new_board, BLACK, QUEENSIDE_CASTLE, TRUE);
            fen_char++;
        }
        else if (*fen_char >= 'a' && *fen_char <= 'h') {
            new_board->BQueenCastle = *fen_char;
            fen_char++;
        }
        else {
            new_board->BQueenCastle = '\0';
        }
    }
    if (*fen_char == ' ') {
        fen_char++;
    }
    else {
        Ok = FALSE;
    }
    /* If we are ok to this point, try to make a best efforts approach
     * to handle the game, even if there are subsequent errors.
     */
    if (Ok) {
        try_to_parse_game = TRUE;
    }
    /* Check for an en-passant square. */
    if (*fen_char == '-') {
        /* None. */
        fen_char++;
    }
    else if (is_col(*fen_char)) {
        col = *fen_char;
        fen_char++;
        if (is_rank(*fen_char)) {
            rank = *fen_char;
            fen_char++;
            /* Make sure that the en-passant indicator is consistent
             * with whose move it is.
             */
            if (((new_board->to_move == WHITE) && (rank == '6')) ||
                    ((new_board->to_move == BLACK) && (rank == '3'))) {
                /* Consistent. */
                new_board->EnPassant = TRUE;
                new_board->ep_rank = rank;
                new_board->ep_col = col;
            }
            else {
                Ok = FALSE;
            }
        }
        else {
            Ok = FALSE;
        }
    }
    else {
        Ok = FALSE;
    }
    if (*fen_char == ' ') {
        fen_char++;
    }
    else {
        Ok = FALSE;
    }
    /* Check for half-move count since last pawn move
     * or capture.
     */
    if (isdigit((int) *fen_char)) {
        unsigned halfmove_clock = *fen_char - '0';
        fen_char++;
        while (isdigit((int) *fen_char)) {
            halfmove_clock = (halfmove_clock * 10)+(*fen_char - '0');
            fen_char++;
        }
        new_board->halfmove_clock = halfmove_clock;
    }
    else {
        Ok = FALSE;
    }
    if (*fen_char == ' ') {
        fen_char++;
    }
    else {
        Ok = FALSE;
    }
    /* Check for current move number. */
    if (isdigit((int) *fen_char)) {
        unsigned move_number = 0;

        move_number = *fen_char - '0';
        fen_char++;
        while (isdigit((int) *fen_char)) {
            move_number = (move_number * 10)+(*fen_char - '0');
            fen_char++;
        }
        if (move_number < 1) {
            move_number = 1;
        }
        new_board->move_number = move_number;
    }
    else {
        Ok = FALSE;
    }
    /* Allow trailing space. */
    while (isspace((int) *fen_char)) {
        fen_char++;
    }
    if (*fen_char != '\0') {
        Ok = FALSE;
    }
    if (Ok) {
        /* Check whether either side is in check and, if so, whether that
         * is consistent with the side to move.
         */
        CheckStatus status = king_is_in_check(new_board, new_board->to_move == WHITE ? BLACK : WHITE);
        if(status != NOCHECK) {
            fprintf(GlobalState.logfile,
                    "Illegal FEN string %s as one side is in check but it is not their move.\n", fen);
            try_to_parse_game = FALSE;
            Ok = FALSE;
        }
    }
    else {
        fprintf(GlobalState.logfile, "Illegal FEN string %s at %s\n", fen, fen_char);
    }
    if(! Ok) {
        if (try_to_parse_game) {
            fprintf(GlobalState.logfile, " Attempting to parse the game, anyway.\n");
        }
        else {
            free_board(new_board);
            new_board = NULL;
        }
        report_details(GlobalState.logfile);
	print_error_context(GlobalState.logfile);
    }
    return new_board;
}

/* add_fen_castling is TRUE and castling permissions are absent.
 * Liberally assume them based on the King and Rook positions.
 */
void add_fen_castling(Game *game_details, Board *board)
{
    Boolean added = FALSE;
    /* NB: This is not coded for Chess 960 positions. */
    if(board->WKingCol == 'e' && board->WKingRank == '1') {
        board->WKingCastle = 
            board->board[RankConvert('1')][ColConvert('h')] ==
                    MAKE_COLOURED_PIECE(WHITE, ROOK) ? 'h' : '\0';
        board->WQueenCastle = 
            board->board[RankConvert('1')][ColConvert('a')] ==
                    MAKE_COLOURED_PIECE(WHITE, ROOK) ? 'a' : '\0';
        added = TRUE;
    }
    else {
        board->WKingCastle = '\0';
        board->WQueenCastle = '\0';
    }
    if(board->BKingCol == 'e' && board->BKingRank == '8') {
        board->BKingCastle = 
            board->board[RankConvert('8')][ColConvert('h')] ==
                    MAKE_COLOURED_PIECE(BLACK, ROOK) ? 'h' : '\0';
        board->BQueenCastle = 
            board->board[RankConvert('8')][ColConvert('a')] ==
                    MAKE_COLOURED_PIECE(BLACK, ROOK) ? 'a' : '\0';
        added = TRUE;
    }
    else {
        board->BKingCastle = '\0';
        board->BQueenCastle = '\0';
    }
    if(added) {
        char *old_fen = game_details->tags[FEN_TAG];
        free(old_fen);
        game_details->tags[FEN_TAG] = get_FEN_string(board);
    }
}

/* Set up a board structure for a new game.
 * This involves placing the pieces in their initial positions,
 * setting up castling and en-passant rights, and initialising
 * the hash positions.
 * If the fen argument is NULL then a completely new board is
 * setup, otherwise the indicated FEN position is returned.
 */
Board *
new_game_board(const char *fen)
{
    Board *new_board = NULL;
    static const Board initial_board ={
        {
            { OFF, OFF, OFF, OFF, OFF, OFF, OFF, OFF, OFF, OFF, OFF, OFF},
            { OFF, OFF, OFF, OFF, OFF, OFF, OFF, OFF, OFF, OFF, OFF, OFF},
            { OFF, OFF, W(ROOK), W(KNIGHT), W(BISHOP), W(QUEEN),
                W(KING), W(BISHOP), W(KNIGHT), W(ROOK), OFF, OFF},
            { OFF, OFF, W(PAWN), W(PAWN), W(PAWN), W(PAWN),
                W(PAWN), W(PAWN), W(PAWN), W(PAWN), OFF, OFF},
            { OFF, OFF, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, OFF, OFF},
            { OFF, OFF, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, OFF, OFF},
            { OFF, OFF, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, OFF, OFF},
            { OFF, OFF, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, OFF, OFF},
            { OFF, OFF, B(PAWN), B(PAWN), B(PAWN), B(PAWN),
                B(PAWN), B(PAWN), B(PAWN), B(PAWN), OFF, OFF},
            { OFF, OFF, B(ROOK), B(KNIGHT), B(BISHOP), B(QUEEN),
                B(KING), B(BISHOP), B(KNIGHT), B(ROOK), OFF, OFF},
            { OFF, OFF, OFF, OFF, OFF, OFF, OFF, OFF, OFF, OFF, OFF, OFF},
            { OFF, OFF, OFF, OFF, OFF, OFF, OFF, OFF, OFF, OFF, OFF, OFF}
        },
        /* Who to move next. */
        WHITE,
        /* Move number. */
        1,
        /* Castling rights. Support Chess960. */
        'h', 'a', 'h', 'a',
        /* Initial king positions. */
        'e', FIRSTRANK,
        'e', LASTRANK,
        /* En Passant rights. */
        FALSE, 0, 0,
        /* Initial hash value. */
        0ul,
        /* half-move_clock */
        0,
    };
    /* Iterate over the columns. */
    Col col;

    if (fen != NULL) {
        new_board = new_fen_board(fen);
    }
    /* Guard against failure of new_fen_board as well as the
     * normal game situation.
     */
    if ((fen == NULL) || (new_board == NULL)) {
        /* Use the initial board setup. */
        new_board = allocate_new_board();
        *new_board = initial_board;
    }

    /* Generate the hash value for the initial position. */
    for (col = FIRSTCOL; col <= LASTCOL; col++) {
        Rank rank;

        for (rank = FIRSTRANK; rank <= LASTRANK; rank++) {
            /* Find the basic components. */
            Piece coloured_piece = new_board->board[
                    RankConvert(rank)][ColConvert(col)];
            Piece piece = EXTRACT_PIECE(coloured_piece);
            Colour colour = EXTRACT_COLOUR(coloured_piece);

            if (coloured_piece != EMPTY) {
                new_board->weak_hash_value ^= hash_lookup(col, rank, piece, colour);
            }
        }
    }
    return new_board;
}

/* Print out the current occupant of the given square. */
static void
print_square(Col col, Rank rank, const Board *board, FILE *outfp)
{
    int r = RankConvert(rank);
    int c = ColConvert(col);

    Piece coloured_piece = board->board[r][c];
    switch ((int) coloured_piece) {
        case W(PAWN):
        case W(KNIGHT):
        case W(BISHOP):
        case W(ROOK):
        case W(QUEEN):
        case W(KING):
        case B(PAWN):
        case B(KNIGHT):
        case B(BISHOP):
        case B(ROOK):
        case B(QUEEN):
        case B(KING):
            putc(coloured_piece_to_SAN_letter(coloured_piece), outfp);
            break;
        case EMPTY:
            putc('.', outfp);
            break;
        case OFF:
            putc('?', outfp);
            break;
        default:
            fprintf(GlobalState.logfile,
                    "Attempt to print illegal square %c%c in print_square.\n",
                    col, rank);
            break;
    }
}

/* Print out the contents of the given board. */
static void
print_board(const Board *board, FILE *outfp)
{
    Rank rank;
    Col col;

    for (rank = LASTRANK; rank >= FIRSTRANK; rank--) {
        for (col = FIRSTCOL; col <= LASTCOL; col++) {
            print_square(col, rank, board, outfp);
        }
        putc('\n', outfp);
    }
    putc('\n', outfp);
}

#if INCLUDE_UNUSED_FUNCTIONS

/* Check the consistency of the board. */
static void
check_board(const Board *board, const char *where)
{
    Rank rank;
    Col col;

    for (rank = LASTRANK; rank >= FIRSTRANK; rank--) {
        for (col = FIRSTCOL; col <= LASTCOL; col++) {
            int r = RankConvert(rank);
            int c = ColConvert(col);

            switch (board->board[r][c]) {
                case W(PAWN):
                case W(KNIGHT):
                case W(BISHOP):
                case W(ROOK):
                case W(QUEEN):
                case W(KING):
                case B(PAWN):
                case B(KNIGHT):
                case B(BISHOP):
                case B(ROOK):
                case B(QUEEN):
                case B(KING):
                case EMPTY:
                    break;
                default:
                    fprintf(GlobalState.logfile,
                            "%s: Illegal square %c%c (%u %u) contains %d.\n",
                            where, col, rank, r, c, board->board[r][c]);
                    report_details(GlobalState.logfile);
                    abort();
                    break;
            }
        }
    }
}
#endif

/* Return the number of half moves that have been completed
 * on the board.
 */
static int
half_moves_played(const Board *board)
{
    int half_moves = 2 * (board->move_number - 1);
    if (board->to_move == BLACK) {
        half_moves++;
    }
    return half_moves;
}

/* Implement move_details on the board.
 * Return TRUE if the move is ok, FALSE otherwise.
 * move_details is completed by the call to determine_move_details.
 * Thereafter, it is safe to make the move on board.
 */
Boolean
apply_move(Move *move_details, Board *board)
{   /* Assume success. */
    Boolean Ok = TRUE;
    Colour colour = board->to_move;

    if (determine_move_details(colour, move_details, board)) {
        Piece piece_to_move = move_details->piece_to_move;

        if (move_details->class != NULL_MOVE) {
            make_move(move_details->class,
                      move_details->from_col, move_details->from_rank,
                      move_details->to_col, move_details->to_rank,
                      piece_to_move, colour, board);
        }
        /* See if there are any subsidiary actions. */
        switch (move_details->class) {
            case PAWN_MOVE:
            case PIECE_MOVE:
            case ENPASSANT_PAWN_MOVE:
                /* Nothing more to do. */
                break;
            case PAWN_MOVE_WITH_PROMOTION:
                if (move_details->promoted_piece != EMPTY) {
                    /* Now make the promotion. */
                    make_move(move_details->class, move_details->to_col, move_details->to_rank,
                            move_details->to_col, move_details->to_rank,
                            move_details->promoted_piece, colour, board);
                }
                else {
                    Ok = FALSE;
                }
                break;
            case KINGSIDE_CASTLE:
                break;
            case QUEENSIDE_CASTLE:
                break;
            case NULL_MOVE:
                /* Nothing more to do. */
                break;
            case UNKNOWN_MOVE:
            default:
                Ok = FALSE;
                break;
        }
        /* Determine whether or not this move gives check. */
        if (Ok) {
            move_details->check_status =
                    king_is_in_check(board, OPPOSITE_COLOUR(colour));
            if (move_details->check_status == CHECK) {
                /* See whether it is checkmate. */
                if (king_is_in_checkmate(OPPOSITE_COLOUR(colour), board)) {
                    move_details->check_status = CHECKMATE;
                }
            }
            /* Get ready for the next move. */
            board->to_move = OPPOSITE_COLOUR(board->to_move);
            if (board->to_move == WHITE) {
                board->move_number++;
            }
            if (GlobalState.output_format == EPD || GlobalState.add_FEN_comments) {
                char epd[FEN_SPACE], fen_suffix[FEN_SPACE];
                build_FEN_components(board, epd, fen_suffix);
                move_details->epd = copy_string(epd);
                move_details->fen_suffix = copy_string(fen_suffix);
            }

        }
    }
    else {
        Ok = FALSE;
    }
    return Ok;
}

/* Play out the moves on the given board.
 * These could be either the main line or a variation.
 * game_details is updated with the final_ and cumulative_ hash values.
 * Check move validity unless a NULL_MOVE has been found in this
 * variation.
 * Return TRUE if the game is valid and matches all matching criteria,
 * FALSE otherwise.
 */
static Boolean
play_moves(Game *game_details, Board *board, Move *moves, unsigned max_depth,
        Boolean check_move_validity,
        Boolean mainline)
{
    Boolean game_ok = TRUE;
    /* Ply number at which any error was found. */
    int error_ply = 0;
    /* Force a match if we aren't looking for positional variations. */
    Boolean game_matches = !GlobalState.positional_variations;
    Move *next_move = moves;
    /* Keep track of the final ECO match. */
    EcoLog *eco_match = NULL;
    Boolean null_move_in_main_line = FALSE;
    /* Whether the fifty-move rule was available in the main line. */
    Boolean N_move_rule_applies = FALSE;
    /* Number of ply, based on the current move number. */
    unsigned plies = board->move_number * 2 - (board->to_move == WHITE ? 1 : 0);
    /* Whether there has been an underpromotion. */
    Boolean underpromotion = FALSE;
    
    const char *match_label = NULL;
    
    /* Try the initial board position for a match.
     * This is required because the game might have been set up
     * from a FEN string, rather than being the normal starting
     * position.
     */
    if (!game_matches &&
            plies >= GlobalState.startply &&
            (match_label = position_matches(board)) != NULL) {
        game_matches = TRUE;
        if (GlobalState.add_position_match_comments) {
            CommentList *comment = create_match_comment(board);
            comment->next = game_details->prefix_comment;
            game_details->prefix_comment = comment;
        }
    }

    /* Ensure that the RESULT_TAG (if present) is valid. */
    if(game_details->tags[RESULT_TAG] != NULL &&
            !valid_result(game_details->tags[RESULT_TAG])) {
        /* Assume an indefinite one rather than an invalid one. */
        free((void *) game_details->tags[RESULT_TAG]);
        game_details->tags[RESULT_TAG] = copy_string("*");
    }

    /* Keep going while the game is ok, and we have some more
     * moves and we haven't exceeded the search depth without finding
     * a match.
     */
    while (game_ok &&
              (next_move != NULL) &&
              (game_matches || (plies <= max_depth))) {
        if (*(next_move->move) != '\0') {
            /* There might be a restriction on when to start checking for a match. */
            Boolean check_for_match = plies >= GlobalState.startply;

            /* See if there are any variations associated with this move. */
            if ((next_move->Variants != NULL) && GlobalState.keep_variations) {
                game_matches |= apply_variations(game_details, board,
                        next_move->Variants,
                        check_move_validity);
            }
            /* Now try the main move. */
            if (next_move->class == NULL_MOVE) {
                null_move_in_main_line = TRUE;
                /* We might not be able to check the validity of
                 * subsequent moves.
                 */
#if 0
                check_move_validity = FALSE;
#endif
            }
            if (check_move_validity) {
                if (apply_move(next_move, board)) {
                    /* Don't try for a positional match if we already have one. */
                    if (check_for_match && !game_matches && (match_label = position_matches(board)) != NULL) {
                        game_matches = TRUE;
                        if (GlobalState.add_position_match_comments) {
                            CommentList *comment = create_match_comment(board);
                            append_comments_to_move(next_move, comment);
                        }
                    }
                    /* Combine this hash value with the cumulative one. */
                    game_details->cumulative_hash_value += board->weak_hash_value;
                    if (check_for_match && GlobalState.fuzzy_match_duplicates) {
                        /* Consider remembering this hash value for fuzzy matches. */
                        if (GlobalState.fuzzy_match_depth == plies) {
                            /* Remember it. */
                            game_details->fuzzy_duplicate_hash = board->weak_hash_value;
                        }
                    }

                    if (check_for_match && GlobalState.check_for_repetition > 0) {
                        unsigned repetition =
                            update_position_counts(game_details->position_counts, board);
                        if (repetition >= GlobalState.check_for_repetition &&
                                GlobalState.add_position_match_comments) {
                            CommentList *comment = create_match_comment(board);
                            append_comments_to_move(next_move, comment);
                        }
                    }

                    if (check_for_match && GlobalState.check_for_N_move_rule > 0 && mainline) {
                        if (board->halfmove_clock >= 2 * GlobalState.check_for_N_move_rule) {
                            /* N moves by both players with no pawn move or capture. */
                            N_move_rule_applies = TRUE;
                            if (GlobalState.add_position_match_comments) {
                                CommentList *comment = create_match_comment(board);
                                append_comments_to_move(next_move, comment);
                            }
                        }
                    }

                    if(check_for_match && GlobalState.match_underpromotion &&
                       next_move->class == PAWN_MOVE_WITH_PROMOTION) {
                         if(next_move->promoted_piece != QUEEN) {
                             underpromotion = TRUE;
                         }
                    }

                    if (next_move->next == NULL && mainline) {
                        /* End of the game. */
                        if (check_for_match && GlobalState.fuzzy_match_duplicates &&
                                GlobalState.fuzzy_match_depth == 0) {
                            game_details->fuzzy_duplicate_hash = board->weak_hash_value;
                        }
                        /* Ensure that the result tag is consistent with the
                         * final status of the game.
                         */
                        const char *result = game_details->tags[RESULT_TAG];
                        const char *corrected_result = NULL;
                        if (result != NULL) {
                            if (next_move->check_status == CHECKMATE) {
                                if (board->to_move == BLACK) {
                                    if (strncmp(result, "1-0", 3) != 0) {
                                        if (GlobalState.fix_result_tags) {
                                            corrected_result = "1-0";
                                        }
                                        else {
                                            fprintf(GlobalState.logfile,
                                                    "Warning: Result of %s is inconsistent with checkmate by white in\n",
                                                    result);
                                            report_details(GlobalState.logfile);
                                            print_error_context(GlobalState.logfile);
                                        }
                                    }
                                }
                                else {
                                    if (strncmp(result, "0-1", 3) != 0) {
                                        if (GlobalState.fix_result_tags) {
                                            corrected_result = "0-1";
                                        }
                                        else {
                                            fprintf(GlobalState.logfile,
                                                    "Warning: Result of %s is inconsistent with checkmate by black in\n",
                                                    result);
                                            report_details(GlobalState.logfile);
                                            print_error_context(GlobalState.logfile);
                                        }
                                    }
                                }
                            }
                            else if (strncmp(result, "1/2", 3) != 0) {
                                if (is_stalemate(board, NULL)) {
                                    if (GlobalState.fix_result_tags) {
                                        corrected_result = "1/2-1/2";
                                    }
                                    else {
                                        fprintf(GlobalState.logfile,
                                                "Warning: Result of %s is inconsistent with stalemate in\n",
                                                result);
                                        report_details(GlobalState.logfile);
                                        print_error_context(GlobalState.logfile);
                                    }
                                }
                                else if(insufficient_material(board)) {
                                    if (GlobalState.fix_result_tags) {
                                        corrected_result = "1/2-1/2";
                                    }
                                    else {
                                        fprintf(GlobalState.logfile,
                                                "Warning: Result of %s is inconsistent with insufficient material in\n",
                                                result);
                                        report_details(GlobalState.logfile);
                                        print_error_context(GlobalState.logfile);
                                    }
                                }
                            }

                            if (corrected_result != NULL) {
                                free((void *) result);
                                game_details->tags[RESULT_TAG] = copy_string(corrected_result);
                                if(next_move->terminating_result != NULL) {
                                    free((void *) next_move->terminating_result);
                                    next_move->terminating_result = NULL;
                                }
                                next_move->terminating_result = copy_string(corrected_result);
                            }
                        }


                        /* Check for inconsistency between Result tag and game result. */
                        const char *move_result = next_move->terminating_result;
                        const char *result_tag = game_details->tags[RESULT_TAG];
                        if (result_tag != NULL && move_result != NULL &&
                                strcmp(result_tag, move_result) != 0) {
                            /* Mismatch. */
                            /* Whether to report it. */
                            Boolean report = TRUE;
                            if (GlobalState.fix_result_tags) {
                                if(strcmp(move_result, "*") == 0 || 
                                        strcmp(result_tag, "*") == 0) {
                                    /* Prefer the move result.
                                       NB: This could lead to an inconsistency if there were no moves in the game
                                       because the move-less result is discarded and the Result tag's value is
                                       used in that case. However, if there are no moves the result is largely
                                       irrelevant.
                                     */
                                    free((void *) result_tag);
                                    game_details->tags[RESULT_TAG] = copy_string(move_result);
                                    report = FALSE;
                                }
                                else {
                                    /* Irreconcilable conflict. */
                                }
                            }
                            else if(strcmp(move_result, "*") == 0) {
                                /* Don't report this form of inconsistency. */
                                report = FALSE;
                            }
                            if(report) {
                                print_error_context(GlobalState.logfile);
                                fprintf(GlobalState.logfile,
                                        "Inconsistent result strings %s vs %s in the following game.\n",
                                        result_tag, move_result);
                                report_details(GlobalState.logfile);
                                if (GlobalState.reject_inconsistent_results) {
                                    game_ok = FALSE;
                                }
                            }
                        }
                    }

                    if (GlobalState.add_ECO && mainline && !GlobalState.parsing_ECO_file) {
                        int half_moves = half_moves_played(board);
                        EcoLog *entry = eco_matches(
                                board,
                                game_details->cumulative_hash_value,
                                half_moves);
                        if (entry != NULL) {
                            /* Consider keeping the match.
                             * Could try to avoid spurious matches which become
                             * more likely with larger ECO files and
                             * the longer a game goes on.
                             * Could be mitigated partly by preferring
                             * an ECO line of exactly the same length as
                             * the current game line.
                             * Not currently implemented.
                             */
                            if (eco_match == NULL) {
                                /* We don't have one yet. */
                                eco_match = entry;
                            }
                            else {
                                /* Keep it anyway.
                                 * This logic always prefers a longer match
                                 * to a shorter, irrespective of whether
                                 * either match is exact or not.
                                 */
                                eco_match = entry;
                            }
                        }
                    }
                    next_move = next_move->next;
                }
                else {
                    print_error_context(GlobalState.logfile);
                    fprintf(GlobalState.logfile,
                            "Failed to make move %u%s %s in the game:\n",
                            board->move_number,
                            (board->to_move == WHITE) ? "." : "...",
                            next_move->move);
                    print_board(board, GlobalState.logfile);
                    report_details(GlobalState.logfile);
		    print_error_context(GlobalState.logfile);
                    game_ok = FALSE;
                    /* Work out where the error was. */
                    error_ply = 2 * board->move_number - 1;
                    /* Check who has just moved. */
                    if (board->to_move == BLACK) {
                        error_ply++;
                    }
                }
            }
            else {
                /* Go through the motions as if the move were checked. */
                board->to_move = OPPOSITE_COLOUR(board->to_move);
                if (board->to_move == WHITE) {
                    board->move_number++;
                }
                next_move = next_move->next;
            }
        }
        else {
            /* An empty move. */
            fprintf(GlobalState.logfile,
                    "Internal error: Empty move in play_moves.\n");
            report_details(GlobalState.logfile);
            game_ok = FALSE;
            /* Work out where the error was. */
            error_ply = 2 * board->move_number - 1;
            /* Check who has just moved. */
            if (board->to_move == BLACK) {
                error_ply++;
            }
        }
        plies++;
    }
    /* Record whether the full game was checked or not. */
    game_details->moves_checked = next_move == NULL;

    if (null_move_in_main_line) {
        /* From v17.50: Don't automatically rule this game out. */
        if(!GlobalState.allow_null_moves) {
            game_ok = FALSE;
        }
    }
    if (game_ok || GlobalState.keep_broken_games) {
        if (eco_match != NULL) {
            /* Free any details of the old one. */
            if (game_details->tags[ECO_TAG] != NULL) {
                (void) free((void *) game_details->tags[ECO_TAG]);
                game_details->tags[ECO_TAG] = NULL;
            }
            if (game_details->tags[OPENING_TAG] != NULL) {
                (void) free((void *) game_details->tags[OPENING_TAG]);
                game_details->tags[OPENING_TAG] = NULL;
            }
            if (game_details->tags[VARIATION_TAG] != NULL) {
                (void) free((void *) game_details->tags[VARIATION_TAG]);
                game_details->tags[VARIATION_TAG] = NULL;
            }
            if (game_details->tags[SUB_VARIATION_TAG] != NULL) {
                (void) free((void *) game_details->tags[SUB_VARIATION_TAG]);
                game_details->tags[SUB_VARIATION_TAG] = NULL;
            }

            /* Add in the new one. */
            if (eco_match->ECO_tag != NULL) {
                game_details->tags[ECO_TAG] = copy_string(eco_match->ECO_tag);
            }
            if (eco_match->Opening_tag != NULL) {
                game_details->tags[OPENING_TAG] = copy_string(eco_match->Opening_tag);
            }
            if (eco_match->Variation_tag != NULL) {
                game_details->tags[VARIATION_TAG] =
                        copy_string(eco_match->Variation_tag);
            }
            if (eco_match->Sub_Variation_tag != NULL) {
                game_details->tags[SUB_VARIATION_TAG] =
                        copy_string(eco_match->Sub_Variation_tag);
            }
        }

        if(game_matches && GlobalState.check_for_N_move_rule > 0 && mainline) {
            game_matches = N_move_rule_applies;
        }
        if(game_matches && GlobalState.match_underpromotion && ! underpromotion) {
            game_matches = FALSE;
        }
        
        /* Add a tag containing the matching FENPattern's label
         * if appropriate.
         */
        if(GlobalState.add_matchlabel_tag && match_label != NULL && *match_label != '\0') {
            game_details->tags[MATCHLABEL_TAG] = copy_string(match_label);            
        }
    }
    /* Fill in the hash value of the final position reached. */
    game_details->final_hash_value = board->weak_hash_value;
    game_details->moves_ok = game_ok;
    game_details->error_ply = error_ply;
    if (! game_ok) {
        /* Decide whether to keep it anyway. */
        if (GlobalState.keep_broken_games) {
        }
#if 0
        else if (GlobalState.positional_variations) {
            /* NB: Not rejecting the match when a game is not ok led to
             * a memory bug in v18-10. This is now fixed so this code
             * could be retained. However, ...
             * I think I felt it appropriate to retain broken games
             * when positional variations are being sought so that games
             * to be truncated before the end would still be matched, but it
             * has the consequence that errors are reported twice and broken
             * games may be counted as matches.
             * For the time being, I am closing this route but it might
             * be appropriate to re-open it at some point.
             */
        }
#endif
        else {
            /* Only return a match if it genuinely matched a variation
             * in which we were interested.
             */
            /* We can't have found a genuine match. */
            game_matches = FALSE;
        }
    }
    return game_matches;
}

/* Play out the moves on the given board to check their validity.
 * These could be either the main line or a variation.
 * game_details is updated with the final_ and cumulative_ hash values.
 * Check move validity unless a NULL_MOVE has been found in this
 * variation.
 * Return TRUE if the game is valid, FALSE otherwise.
 */
static Boolean
check_move_validity(Game *game_details, Board *board, Move *moves, Boolean mainline)
{
    Boolean game_ok = TRUE;
    /* Ply number at which any error was found. */
    int error_ply = 0;
    Move *next_move = moves;
    Boolean null_move_in_main_line = FALSE;
    EcoLog *eco_match = NULL;
    
    /* Ensure that the RESULT_TAG (if present) is valid. */
    if(game_details->tags[RESULT_TAG] != NULL &&
            !valid_result(game_details->tags[RESULT_TAG])) {
        /* Assume an indefinite one rather than an invalid one. */
        free((void *) game_details->tags[RESULT_TAG]);
        game_details->tags[RESULT_TAG] = copy_string("*");
    }

    /* Keep going while the game is ok, and we have some more moves. */
    while (game_ok && (next_move != NULL)) {
        if (*(next_move->move) != '\0') {
            /* See if there are any variations associated with this move. */
            if ((next_move->Variants != NULL) && GlobalState.keep_variations) {
                /* Make sure all variations are valid. */
                game_ok &= check_variation_validity(game_details, board, next_move->Variants);
            }
            /* Now try the main move. */
            if (next_move->class == NULL_MOVE) {
                null_move_in_main_line = TRUE;
                /* We might not be able to check the validity of
                 * subsequent moves.
                 */
            }
            if (game_ok && apply_move(next_move, board)) {
                /* Combine this hash value with the cumulative one. */
                game_details->cumulative_hash_value += board->weak_hash_value;
                if (next_move->next == NULL && mainline) {
                    /* End of the game. */
                    /* Ensure that the result tag is consistent with the
                     * final status of the game.
                     */
                    const char *result = game_details->tags[RESULT_TAG];
                    const char *corrected_result = NULL;
                    if (result != NULL) {
                        if (next_move->check_status == CHECKMATE) {
                            if (board->to_move == BLACK) {
                                if (strncmp(result, "1-0", 3) != 0) {
                                    if (GlobalState.fix_result_tags) {
                                        corrected_result = "1-0";
                                    }
                                    else {
                                        fprintf(GlobalState.logfile,
                                                "Warning: Result of %s is inconsistent with checkmate by white in\n",
                                                result);
                                        report_details(GlobalState.logfile);
                                        print_error_context(GlobalState.logfile);
                                    }
                                }
                            }
                            else {
                                if (strncmp(result, "0-1", 3) != 0) {
                                    if (GlobalState.fix_result_tags) {
                                        corrected_result = "0-1";
                                    }
                                    else {
                                        fprintf(GlobalState.logfile,
                                                "Warning: Result of %s is inconsistent with checkmate by black in\n",
                                                result);
                                        report_details(GlobalState.logfile);
                                        print_error_context(GlobalState.logfile);
                                    }
                                }
                            }
                        }
                        else if (is_stalemate(board, NULL)) {
                            if (strncmp(result, "1/2", 3) != 0) {
                                if (GlobalState.fix_result_tags) {
                                    corrected_result = "1/2-1/2";
                                }
                                else {
                                    fprintf(GlobalState.logfile,
                                            "Warning: Result of %s is inconsistent with stalemate in\n",
                                            result);
                                    report_details(GlobalState.logfile);
                                    print_error_context(GlobalState.logfile);
                                }
                            }
                        }

                        if (corrected_result != NULL) {
                            free((void *) result);
                            game_details->tags[RESULT_TAG] = copy_string(corrected_result);
                            if(next_move->terminating_result != NULL) {
                                free((void *) next_move->terminating_result);
                                next_move->terminating_result = NULL;
                            }
                            next_move->terminating_result = copy_string(corrected_result);
                        }
                    }


                    /* Check for inconsistency between Result tag and game result. */
                    const char *move_result = next_move->terminating_result;
                    const char *result_tag = game_details->tags[RESULT_TAG];
                    if (result_tag != NULL && move_result != NULL &&
                            strcmp(result_tag, move_result) != 0) {
                        /* Mismatch. */
                        /* Whether to report it. */
                        Boolean report = TRUE;
                        if (GlobalState.fix_result_tags) {
                            if(strcmp(move_result, "*") == 0 || 
                                    strcmp(result_tag, "*") == 0) {
                                /* Prefer the move result.
                                   NB: This could lead to an inconsistency if there were no moves in the game
                                   because the move-less result is discarded and the Result tag's value is
                                   used in that case. However, if there are no moves the result is largely
                                   irrelevant.
                                 */
                                free((void *) result_tag);
                                game_details->tags[RESULT_TAG] = copy_string(move_result);
                                report = FALSE;
                            }
                            else {
                                /* Irreconcilable conflict. */
                            }
                        }
                        else if(strcmp(move_result, "*") == 0) {
                            /* Don't report this form of inconsistency. */
                            report = FALSE;
                        }
                        if(report) {
                            print_error_context(GlobalState.logfile);
                            fprintf(GlobalState.logfile,
                                    "Inconsistent result strings %s vs %s in the following game.\n",
                                    result_tag, move_result);
                            report_details(GlobalState.logfile);
                            if (GlobalState.reject_inconsistent_results) {
                                game_ok = FALSE;
                            }
                        }
                    }
                }

                if (GlobalState.add_ECO && mainline && !GlobalState.parsing_ECO_file) {
                    int half_moves = half_moves_played(board);
                    EcoLog *entry = eco_matches(
                            board,
                            game_details->cumulative_hash_value,
                            half_moves);
                    if (entry != NULL) {
                        /* Consider keeping the match.
                         * Could try to avoid spurious matches which become
                         * more likely with larger ECO files and
                         * the longer a game goes on.
                         * Could be mitigated partly by preferring
                         * an ECO line of exactly the same length as
                         * the current game line.
                         * Not currently implemented.
                         */
                        if (eco_match == NULL) {
                            /* We don't have one yet. */
                            eco_match = entry;
                        }
                        else {
                            /* Keep it anyway.
                             * This logic always prefers a longer match
                             * to a shorter, irrespective of whether
                             * either match is exact or not.
                             */
                            eco_match = entry;
                        }
                    }
                }
                next_move = next_move->next;
            }
            else {
                print_error_context(GlobalState.logfile);
                fprintf(GlobalState.logfile,
                        "Failed to make move %u%s %s in the game:\n",
                        board->move_number,
                        (board->to_move == WHITE) ? "." : "...",
                        next_move->move);
                print_board(board, GlobalState.logfile);
                report_details(GlobalState.logfile);
                print_error_context(GlobalState.logfile);
                game_ok = FALSE;
                /* Work out where the error was. */
                error_ply = 2 * board->move_number - 1;
                /* Check who has just moved. */
                if (board->to_move == BLACK) {
                    error_ply++;
                }
            }
        }
        else {
            /* An empty move. */
            fprintf(GlobalState.logfile,
                    "Internal error: Empty move in play_moves.\n");
            report_details(GlobalState.logfile);
            game_ok = FALSE;
            /* Work out where the error was. */
            error_ply = 2 * board->move_number - 1;
            /* Check who has just moved. */
            if (board->to_move == BLACK) {
                error_ply++;
            }
        }
    }
    /* Record whether the full game was checked or not. */
    game_details->moves_checked = next_move == NULL;

    if (null_move_in_main_line) {
        /* From v17.50: Don't automatically rule this game out. */
        if(!GlobalState.allow_null_moves) {
            game_ok = FALSE;
        }
    }
    if (game_ok || GlobalState.keep_broken_games) {
        if (eco_match != NULL) {
            /* Free any details of the old one. */
            if (game_details->tags[ECO_TAG] != NULL) {
                (void) free((void *) game_details->tags[ECO_TAG]);
                game_details->tags[ECO_TAG] = NULL;
            }
            if (game_details->tags[OPENING_TAG] != NULL) {
                (void) free((void *) game_details->tags[OPENING_TAG]);
                game_details->tags[OPENING_TAG] = NULL;
            }
            if (game_details->tags[VARIATION_TAG] != NULL) {
                (void) free((void *) game_details->tags[VARIATION_TAG]);
                game_details->tags[VARIATION_TAG] = NULL;
            }
            if (game_details->tags[SUB_VARIATION_TAG] != NULL) {
                (void) free((void *) game_details->tags[SUB_VARIATION_TAG]);
                game_details->tags[SUB_VARIATION_TAG] = NULL;
            }

            /* Add in the new one. */
            if (eco_match->ECO_tag != NULL) {
                game_details->tags[ECO_TAG] = copy_string(eco_match->ECO_tag);
            }
            if (eco_match->Opening_tag != NULL) {
                game_details->tags[OPENING_TAG] = copy_string(eco_match->Opening_tag);
            }
            if (eco_match->Variation_tag != NULL) {
                game_details->tags[VARIATION_TAG] =
                        copy_string(eco_match->Variation_tag);
            }
            if (eco_match->Sub_Variation_tag != NULL) {
                game_details->tags[SUB_VARIATION_TAG] =
                        copy_string(eco_match->Sub_Variation_tag);
            }
        }
    }
    /* Fill in the hash value of the final position reached. */
    game_details->final_hash_value = board->weak_hash_value;
    game_details->moves_ok = game_ok;
    game_details->error_ply = error_ply;
    return game_ok;;
}

/* Play out the moves of an ECO line on the given board.
 * game_details is updated with the final_ and cumulative_ hash
 * values.
 */
static void
play_eco_moves(Game *game_details, Board *board, Move *moves)
{
    Boolean game_ok = TRUE;
    /* Ply number at which any error was found. */
    int error_ply = 0;
    Move *next_move = moves;

    /* Keep going while the game is ok, and we have some more
     * moves and we haven't exceeded the search depth without finding
     * a match.
     */
    while (game_ok && (next_move != NULL)) {
        if (*(next_move->move) != '\0') {
            /* Ignore variations. */
            if (apply_move(next_move, board)) {
                /* Combine this hash value to the cumulative one. */
                game_details->cumulative_hash_value += board->weak_hash_value;
                next_move = next_move->next;
            }
            else {
                print_error_context(GlobalState.logfile);
                fprintf(GlobalState.logfile,
                        "Failed to make move %u%s %s in the game:\n",
                        board->move_number,
                        (board->to_move == WHITE) ? "." : "...",
                        next_move->move);
                print_board(board, GlobalState.logfile);
                report_details(GlobalState.logfile);
		print_error_context(GlobalState.logfile);
                game_ok = FALSE;
                /* Work out where the error was. */
                error_ply = 2 * board->move_number - 1;
                /* Check who has just moved. */
                if (board->to_move == BLACK) {
                    error_ply++;
                }
            }
        }
        else {
            /* An empty move. */
            fprintf(GlobalState.logfile,
                    "Internal error: Empty move in play_eco_moves.\n");
            report_details(GlobalState.logfile);
            game_ok = FALSE;
            /* Work out where the error was. */
            error_ply = 2 * board->move_number - 1;
            /* Check who has just moved. */
            if (board->to_move == BLACK) {
                error_ply++;
            }
        }
    }
    /* Record whether the full game was checked or not. */
    game_details->moves_checked = next_move == NULL;
    /* Fill in the hash value of the final position reached. */
    game_details->final_hash_value = board->weak_hash_value;
    game_details->moves_ok = game_ok;
    game_details->error_ply = error_ply;
}

/* Play out a variation.
 * Check move validity unless a NULL_MOVE has been found in this
 * variation.
 * Return TRUE if the variation matches a position that
 * we are looking for.
 */
static Boolean
apply_variations(const Game *game_details, const Board *board, Variation *variation,
        Boolean check_move_validity)
{
    /* Force a match if we aren't looking for positional variations. */
    Boolean variation_matches = GlobalState.positional_variations ? FALSE : TRUE;
    /* Allocate space for the copies.
     * Allocation is done, rather than relying on local copies in the body
     * of the loop because the recursive nature of this function has
     * resulted in stack overflow on the PC version.
     */
    Game *copy_game = (Game *) malloc_or_die(sizeof (*copy_game));
    Board *copy_board = allocate_new_board();

    while (variation != NULL) {
        /* Work on the copies. */
        *copy_game = *game_details;
        *copy_board = *board;
        /* Don't look for repetitions. */
        copy_game->position_counts = NULL;

        /* We only need one variation to match to declare a match.
         * Play out the variation to its full depth, because we
         * will want the full move information if the main line
         * later matches.
         */
        variation_matches |= play_moves(copy_game, copy_board, variation->moves,
                DEFAULT_POSITIONAL_DEPTH,
                check_move_validity, FALSE);
        variation = variation->next;
    }
    (void) free((void *) copy_game);
    free_board(copy_board);
    return variation_matches;
}

/* game_details contains a complete move score.
 * Try to apply each move on a new board.
 * Store in plycount the number of ply played.
 * Return TRUE if the game matches a variation that we are
 * looking for.
 */
Boolean
apply_move_list(Game *game_details,unsigned *plycount, unsigned max_depth, Boolean check_for_a_match)
{
    Move *moves = game_details->moves;
    Board *board = new_game_board(game_details->tags[FEN_TAG]);
    Boolean game_matches;

    /* Ensure that we have a sensible search depth. */
    if (max_depth == 0) {
        /* No positional variations specified. */
        max_depth = DEFAULT_POSITIONAL_DEPTH;
    }

    /* Start off the cumulative hash value. */
    game_details->cumulative_hash_value = 0;

    if (check_for_a_match && GlobalState.check_for_repetition > 0 && game_details->position_counts == NULL) {
        game_details->position_counts = new_position_count_list(board);
    }

    /* Play through the moves and see if we have a match.
     * Check move validity.
     */
    if(check_for_a_match) {
        game_matches = play_moves(game_details, board, moves, max_depth, TRUE, TRUE);
    }
    else {
        check_move_validity(game_details, board, moves, TRUE);
        game_matches = FALSE;
    }

    /* Record how long the game was. */
    if (board->to_move == BLACK) {
        *plycount = 2 * (board->move_number - 1) + 1;
    }
    else {
        /* This move number hasn't been played. */
        *plycount = 2 * (board->move_number - 1);
    }

    if(check_for_a_match) {
        if (game_matches) {
            game_matches = check_for_only_stalemate(board, moves);
        }
        if (game_matches) {
            game_matches = check_for_only_insufficient_material(board);
        }
    }

    free_board(board);
    return game_matches;
}

/* Play out a variation to check it is valid.
 * Return TRUE if the variation is valid.
 */
static Boolean
check_variation_validity(const Game *game_details, const Board *board, Variation *variation)
{
    Boolean valid = TRUE;
    /* Allocate space for the copies.
     * Allocation is done, rather than relying on local copies in the body
     * of the loop because the recursive nature of this function has
     * resulted in stack overflow on the PC version.
     */
    Game *copy_game = (Game *) malloc_or_die(sizeof (*copy_game));
    Board *copy_board = allocate_new_board();

    while (valid && variation != NULL) {
        /* Work on the copies. */
        *copy_game = *game_details;
        *copy_board = *board;
        /* Don't look for repetitions. */
        copy_game->position_counts = NULL;

        /* All variations must be valid.
         * Play out the variation to its full depth, because we
         * will want the full move information.
         */
        valid &= check_move_validity(copy_game, copy_board, variation->moves, FALSE);
        variation = variation->next;
    }
    (void) free((void *) copy_game);
    free_board(copy_board);
    return valid;
}


/* game_details contains a complete move score.
 * Try to apply each move on a new board.
 * Store in number_of_moves the length of the game.
 * Return the final Board if the game is ok, otherwise NULL.
 */
Board *
apply_eco_move_list(Game *game_details, unsigned *number_of_half_moves)
{
    Move *moves = game_details->moves;
    Board *board = new_game_board(game_details->tags[FEN_TAG]);

    /* Start off the cumulative hash value. */
    game_details->cumulative_hash_value = 0;
    play_eco_moves(game_details, board, moves);
    /* Record how long the game was. */
    *number_of_half_moves = half_moves_played(board);
    if(! game_details->moves_ok) {
        free_board(board);
        board = NULL;
    }
    return board;
}

/* Return the string associated with the given piece. */
const char *
piece_str(Piece piece)
{
    if (piece < NUM_PIECE_VALUES) {
        return output_piece_characters[piece];
    }
    else {
        return "?";
    }
}

/* Rewrite move_details->move according to the details held
 * within the structure and the current state of the board.
 */
static Boolean
rewrite_SAN_string(Colour colour, Move *move_details, Board *board)
{
    Boolean Ok = TRUE;

    if (move_details == NULL) {
        /* Shouldn't happen. */
        fprintf(GlobalState.logfile,
                "Internal error: NULL move details in rewrite_SAN_string.\n");
        Ok = FALSE;
    }
    else if (move_details->move[0] == '\0') {
        /* Shouldn't happen. */
        fprintf(GlobalState.logfile, "Empty move in rewrite_SAN_string.\n");
        Ok = FALSE;
    }
    else {
        const unsigned char *move = move_details->move;
        MoveClass class = move_details->class;
        MovePair *move_list = NULL;
        Col to_col = move_details->to_col;
        Rank to_rank = move_details->to_rank;
        unsigned char new_move_str[MAX_MOVE_LEN + 1] = "";

        switch (class) {
            case PAWN_MOVE:
            case ENPASSANT_PAWN_MOVE:
            case PAWN_MOVE_WITH_PROMOTION:
                move_list = find_pawn_moves(move_details->from_col,
                        '0', to_col, to_rank,
                        colour, board);
                break;
            case PIECE_MOVE:
                switch (move_details->piece_to_move) {
                    case KING:
                        move_list = find_king_moves(to_col, to_rank, colour, board);
                        break;
                    case QUEEN:
                        move_list = find_queen_moves(to_col, to_rank, colour, board);
                        break;
                    case ROOK:
                        move_list = find_rook_moves(to_col, to_rank, colour, board);
                        break;
                    case KNIGHT:
                        move_list = find_knight_moves(to_col, to_rank, colour, board);
                        break;
                    case BISHOP:
                        move_list = find_bishop_moves(to_col, to_rank, colour, board);
                        break;
                    default:
                        fprintf(GlobalState.logfile, "Unknown piece move %s\n", move);
                        Ok = FALSE;
                        break;
                }
                break;
            case KINGSIDE_CASTLE:
            case QUEENSIDE_CASTLE:
                /* No move list to prepare. */
                break;
            case NULL_MOVE:
                /* No move list to prepare. */
                break;
            case UNKNOWN_MOVE:
            default:
                fprintf(GlobalState.logfile,
                        "Unknown move class in rewrite_SAN_string(%d).\n",
                        move_details->class);
                Ok = FALSE;
                break;
        }
        if (move_list != NULL) {
            move_list = exclude_checks(move_details->piece_to_move, colour,
                    move_list, board);
        }
        if ((move_list == NULL) && (class != KINGSIDE_CASTLE) &&
                (class != QUEENSIDE_CASTLE) && (class != NULL_MOVE)) {
            Ok = FALSE;
        }
        /* We should now have enough information in move_details to compose a
         * SAN string.
         */
        if (Ok) {
            size_t new_move_index = 0;

            switch (class) {
                case PAWN_MOVE:
                case ENPASSANT_PAWN_MOVE:
                case PAWN_MOVE_WITH_PROMOTION:
                    /* See if we need to give the source column. */
                    if (move_details->captured_piece != EMPTY) {
                        new_move_str[new_move_index] = move_details->from_col;
                        new_move_index++;
                        new_move_str[new_move_index] = 'x';
                        new_move_index++;
                    }
                    else if (move_list->next != NULL) {
                        new_move_str[new_move_index] = move_details->from_col;
                        new_move_index++;
                    }
                    /* Add in the destination. */
                    new_move_str[new_move_index] = to_col;
                    new_move_index++;
                    new_move_str[new_move_index] = to_rank;
                    new_move_index++;
                    if (class == PAWN_MOVE_WITH_PROMOTION) {
                        const char *promoted_piece =
                                piece_str(move_details->promoted_piece);
                        new_move_str[new_move_index] = '=';
                        new_move_index++;
                        strcpy((char *) &new_move_str[new_move_index],
                                promoted_piece);
                        new_move_index += strlen(promoted_piece);
                    }
                    new_move_str[new_move_index] = '\0';
                    break;
                case PIECE_MOVE:
                {
                    const char *piece = piece_str(move_details->piece_to_move);
                    strcpy((char *) &new_move_str[0], piece);
                    new_move_index += strlen(piece);
                    /* Check for the need to disambiguate. */
                    if (move_list->next != NULL) {
                        /* It is necessary.  Count how many times
                         * the from_ col and rank occur in the list
                         * of possibles in order to determine which to use
                         * for this purpose.
                         */
                        int col_times = 0, rank_times = 0;
                        MovePair *possible;
                        Col from_col = move_details->from_col;
                        Rank from_rank = move_details->from_rank;

                        for (possible = move_list; possible != NULL;
                                possible = possible->next) {
                            if (possible->from_col == from_col) {
                                col_times++;
                            }
                            if (possible->from_rank == from_rank) {
                                rank_times++;
                            }
                        }
                        if (col_times == 1) {
                            /* Use the col. */
                            new_move_str[new_move_index] = from_col;
                            new_move_index++;
                        }
                        else if (rank_times == 1) {
                            /* Use the rank. */
                            new_move_str[new_move_index] = from_rank;
                            new_move_index++;
                        }
                        else {
                            /* Use both. */
                            new_move_str[new_move_index] = from_col;
                            new_move_index++;
                            new_move_str[new_move_index] = from_rank;
                            new_move_index++;
                        }
                    }
                    /* See if a capture symbol is needed. */
                    if (move_details->captured_piece != EMPTY) {
                        new_move_str[new_move_index] = 'x';
                        new_move_index++;
                    }
                    /* Add in the destination. */
                    new_move_str[new_move_index] = to_col;
                    new_move_index++;
                    new_move_str[new_move_index] = to_rank;
                    new_move_index++;
                    new_move_str[new_move_index] = '\0';
                }
                    break;
                case KINGSIDE_CASTLE:
                    strcpy((char *) new_move_str, "O-O");
                    break;
                case QUEENSIDE_CASTLE:
                    strcpy((char *) new_move_str, "O-O-O");
                    break;
                case NULL_MOVE:
                    strcpy((char *) new_move_str, (char *) NULL_MOVE_STRING);
                    break;
                case UNKNOWN_MOVE:
                default:
                    Ok = FALSE;
                    break;
            }
            if (Ok) {
                if (move_details->check_status != NOCHECK) {
                    if (move_details->check_status == CHECK) {
                        /* It isn't mate. */
                        strcat((char *) new_move_str, "+");
                    }
                    else {
                        if (GlobalState.output_format == CM) {
                            strcat((char *) new_move_str, "++");
                        }
                        else {
                            strcat((char *) new_move_str, "#");
                        }
                    }
                }
            }
            /* Update the move_details structure with the new string. */
            strcpy((char *) move_details->move,
                    (const char *) new_move_str);
        }
        if (move_list != NULL) {
            free_move_pair_list(move_list);
        }
    }
    return Ok;
}

/* Apply the move to board and rewrite move_details->move unless
 * the output format is the original source.
 * Return TRUE if the move is ok, FALSE otherwise.
 */
static Boolean
rewrite_move(Game *game, Colour colour, Move *move_details, Board *board)
{ /* Assume success. */
    Boolean Ok = TRUE;
    
    if(move_details->class == UNKNOWN_MOVE) {
        Ok = FALSE;
    }
    else if (GlobalState.output_format == SOURCE || 
            rewrite_SAN_string(colour, move_details, board)) {
        Piece piece_to_move = move_details->piece_to_move;
        MoveClass class = move_details->class;
        Col castling_rook_col = '\0';

        /* Try to accommodate Chess960 castling format where the King
         * is moved to the position of the Rook.
         * NB: The contents of the Variant tag vary a lot for
         *     chess 960 games..
         */
        if ((class == KINGSIDE_CASTLE || class == QUEENSIDE_CASTLE) &&
                game != NULL &&
                game->tags[VARIANT_TAG] != NULL &&
                strstr(game->tags[VARIANT_TAG], "960") != NULL) {
            /* Remember where the Rook was before the move is applied. */
            castling_rook_col = find_castling_rook_col(colour, board, class);
        }
        if (class != NULL_MOVE) {
            make_move(class, move_details->from_col, move_details->from_rank,
                    move_details->to_col, move_details->to_rank,
                    piece_to_move, colour, board);
            if(castling_rook_col != '\0') {
                /* Rewrite the King's target column to be the original
                 * position of the Rook to accommodate Chess960 format.
                 */
                move_details->to_col = castling_rook_col;
            }
        }
        /* See if there are any subsidiary actions. */
        switch (class) {
            case PAWN_MOVE:
            case PIECE_MOVE:
            case ENPASSANT_PAWN_MOVE:
            case KINGSIDE_CASTLE:
            case QUEENSIDE_CASTLE:
                /* Nothing more to do. */
                break;
            case PAWN_MOVE_WITH_PROMOTION:
                if (move_details->promoted_piece != EMPTY) {
                    /* @@@ Do promoted moves have '+' properly appended? */
                    /* Now make the promotion. */
                    make_move(class,
                              move_details->to_col, move_details->to_rank,
                              move_details->to_col, move_details->to_rank,
                              move_details->promoted_piece, colour, board);
                }
                else {
                    Ok = FALSE;
                }
                break;
            case NULL_MOVE:
                /* Nothing more. */
                break;
            case UNKNOWN_MOVE:
            default:
                Ok = FALSE;
                break;
        }
    }
    else {
        Ok = FALSE;
    }
    return Ok;
}

/* Rewrite the list of moves by playing through the game.
 * If game is NULL then the moves are a variation rather than
 * the main line.
 */
static Boolean
rewrite_moves(Game *game, Board *board, Move *moves)
{
    Boolean game_ok = TRUE;
    Move *move_details = moves;
    int plies_to_drop = GlobalState.drop_ply_number;

    /* See if the prefix comment should be replaced by a FEN comment. */
    if (game != NULL && game->prefix_comment != NULL &&
              GlobalState.FEN_comment_pattern != NULL) {
        StringList *comment_to_replace =
            find_matching_comment(GlobalState.FEN_comment_pattern,
                                  game->prefix_comment);
        if(comment_to_replace != NULL) {
            (void) free((void *) comment_to_replace->str);
            comment_to_replace->str = get_FEN_string(board);
        }
    }

    while (game_ok && (move_details != NULL)) {
        if (*(move_details->move) != '\0') {
            /* See if there are any variations associated with this move. */
            if ((move_details->Variants != NULL) &&
                    GlobalState.keep_variations &&
                    !rewrite_variations(board, move_details->Variants)) {
                /* Something wrong with the variations. */
                game_ok = FALSE;
            }
            if (rewrite_move(game, board->to_move, move_details, board)) {
                if(move_details->class == NULL_MOVE && game != NULL) {
                    /* NULL_MOVE not allowed in the main line. */
                }
                board->to_move = OPPOSITE_COLOUR(board->to_move);
                if (board->to_move == WHITE) {
                    board->move_number++;
                }

                if (GlobalState.output_evaluation) {
                    move_details->evaluation = evaluate(board);
                }

                if (GlobalState.add_hashcode_comments) {
                    /* Append a hashcode comment using the new state of the board
                     * with the move having been played.
                     */
                    move_details->zobrist = generate_zobrist_hash_from_board(board);
                }
                
                if(GlobalState.drop_comment_pattern != NULL &&
                        move_details->comment_list != NULL) {
                    if(find_matching_comment(GlobalState.drop_comment_pattern,
                                              move_details->comment_list) != NULL) {
                        /* We have a match. */
                        plies_to_drop = (board->move_number - 1) * 2;
                        if(board->to_move == BLACK) {
                            plies_to_drop++;
                        }
                    }
                }
                /* See if a comment should be replaced by a FEN comment. */
                if(GlobalState.FEN_comment_pattern != NULL &&
                        move_details->comment_list != NULL) {
                    StringList *comment_to_replace =
                        find_matching_comment(GlobalState.FEN_comment_pattern,
                                              move_details->comment_list);
                    if(comment_to_replace != NULL) {
                        /* Replace it. */
                        (void) free((void *) comment_to_replace->str);
                        comment_to_replace->str = get_FEN_string(board);
                    }
                }

                move_details = move_details->next;
            }
            else {
                /* Broken games that are being kept have probably already been
                 * reported, so don't repeat the error.
                 */
                if (!GlobalState.keep_broken_games) {
                    fprintf(GlobalState.logfile,
                            "Failed to rewrite move %u%s %s in the game:\n",
                            board->move_number,
                            (board->to_move == WHITE) ? "." : "...",
                            move_details->move);
                    report_details(GlobalState.logfile);
		    print_error_context(GlobalState.logfile);
                    print_board(board, GlobalState.logfile);
                }
                game_ok = FALSE;
            }
        }
        else {
            /* An empty move. */
            fprintf(GlobalState.logfile,
                    "Internal error: Empty move in rewrite_moves.\n");
            report_details(GlobalState.logfile);
            game_ok = FALSE;
        }
    }
    if (!game_ok) {
        if(GlobalState.keep_broken_games && move_details != NULL) {
            /* Try to place the remaining moves into a comment. */
            CommentList *comment = (CommentList*) malloc_or_die(sizeof (*comment));
            /* Break the link from the previous move. */
            Move *prev;
            StringList *commented_move_list = NULL;

            /* Find the move before the current one, if any. */
            if (move_details == moves) {
                /* Broken from the first move. */
                prev = NULL;
            }
            else {
                prev = moves;
                while (prev != NULL && prev->next != move_details) {
                    prev = prev->next;
                }
                if (prev != NULL) {
                    prev->next = NULL;
                }
            }
            /* Build the comment string. */
            char *terminating_result = NULL;
            while (move_details != NULL) {
                commented_move_list = save_string_list_item(commented_move_list, (char *) move_details->move);
                if (move_details->next == NULL) {
                    /* Remove the terminating result. */
                    terminating_result = move_details->terminating_result;
                    move_details->terminating_result = NULL;
                }
                move_details = move_details->next;
            }
            comment->comment = commented_move_list;
            comment->next = NULL;

            if (prev != NULL) {
                prev->terminating_result = terminating_result;
                append_comments_to_move(prev, comment);
            }
            else {
                /* The whole game is broken. */
                if (game != NULL) {
                    if (terminating_result != NULL) {
                        commented_move_list = save_string_list_item(commented_move_list, terminating_result);
                    }
                    game->prefix_comment = comment;
                    game->moves = NULL;
                }
            }
        }
    }
    else if(plies_to_drop != 0 && game != NULL) {
        if(plies_to_drop < 0) {
            unsigned plies = plies_in_move_sequence(moves);
            plies_to_drop = plies + plies_to_drop;
        }
        if(plies_to_drop >= 0) {
            game_ok = drop_plies_from_start(game, moves, plies_to_drop);
        }
        else {
            game_ok = FALSE;
        }
    }
    return game_ok;
}

/* Rewrite the list of variations.
 * Return TRUE if the variation are ok. a position that
 */
static Boolean
rewrite_variations(const Board *board, Variation *variation)
{
    Board *copy_board = allocate_new_board();
    Boolean variations_ok = TRUE;

    while ((variation != NULL) && variations_ok) {
        /* Work on the copy. */
        *copy_board = *board;

        variations_ok = rewrite_moves((Game *) NULL, copy_board, variation->moves);
        variation = variation->next;
    }
    free_board(copy_board);
    return variations_ok;
}

/* moves contains a complete game score.
 * Try to rewrite each move into SAN as it is played on a new board.
 * Return the final Board position if the game was rewritten alright,
 * NULL otherwise.
 */
Board *
rewrite_game(Game *current_game)
{
    Board *board = new_game_board(current_game->tags[FEN_TAG]);
    Boolean game_ok;

    /* No null-move found at the start of the game. */
    game_ok = rewrite_moves(current_game, board, current_game->moves);
    if (game_ok) {
    }
    else if (GlobalState.keep_broken_games) {
    }
    else {
        free_board(board);
        board = NULL;
    }
    return board;
}

/* Return the number of plies in the given move
 * sequence.
 */
static unsigned plies_in_move_sequence(Move *moves)
{
    int num_plies = 0;
    while(moves != NULL) {
        moves = moves->next;
        num_plies++;
    }
    return num_plies;
}

/* Drop the given number of play from the start of this game.
 * If plies_to_drop is negative, drop all but that number
 * from the beginning.
 * Return FALSE if there are insufficient.
 * If there are sufficient replace the game's FEN tag with
 * the revised starting state.
 */
static Boolean
drop_plies_from_start(Game *game, Move *moves, int plies_to_drop)
{
    char *fen = game->tags[FEN_TAG];
    Board *board = new_game_board(fen);
    Boolean game_ok = TRUE;
    int plies = 0;
    Move *new_head = moves;

    if(plies_to_drop > 0 && game->prefix_comment != NULL) {
        /* Don't free the prefix_comment because that is
         * handled elsewhere.
         */
        game->prefix_comment = NULL;
    }
    while(plies < plies_to_drop && new_head != NULL && game_ok) {
        /* Rewriting is not strictly necessary, because it has already been done,
         * but it provides all the required functionality.
         */
        if (rewrite_move(game, board->to_move, new_head, board)) {
            board->to_move = OPPOSITE_COLOUR(board->to_move);
            if (board->to_move == WHITE) {
                board->move_number++;
            }
            plies++;
            new_head = new_head->next;
        }
        else {
            game_ok = FALSE;
        }
    }
    if(plies == plies_to_drop && game_ok) {
        if(fen != NULL) {
            (void) free((void *) fen);
        }
#if 0
        /* Reset the move number. */
        board->move_number = 1;
#endif
        game->moves = new_head;        
        game->tags[FEN_TAG] = get_FEN_string(board);
        game->tags[SETUP_TAG] = copy_string("1");
        if(game->tags[PLY_COUNT_TAG] != NULL || GlobalState.output_plycount) {
            add_plycount(game);
        }

        if(game->tags[TOTAL_PLY_COUNT_TAG] != NULL || GlobalState.output_total_plycount) {
            add_total_plycount(game, GlobalState.keep_variations);
        }

        /* Make sure we aren't dropping 0 moves. */
        if(new_head != moves) {
            /* Free the dropped moves. */
            Move *move_to_drop = moves;
            while(move_to_drop->next != new_head) {
                move_to_drop = move_to_drop->next;
            }
            move_to_drop->next = NULL;
            free_move_list(moves);
        }
    }
    else {
        game_ok = FALSE;
    }
    (void) free((void *) board);
    return game_ok;
}

/* Define a table to hold the positional hash codes of interest.
 * Size should be a prime number for collision avoidance.
 */
#define MAX_NON_POLYGLOT_CODE 541
static HashLog *non_polyglot_codes_of_interest[MAX_NON_POLYGLOT_CODE];
/* Whether or not the non-polyglot hashcodes are in use. */
Boolean using_non_polyglot = FALSE;

/* move_details is either the start of a variation in which we are interested
 * or it is NULL.
 * fen is either a position we are interested in or it is NULL.
 * Generate and store the hash value for the variation, or the FEN
 * position in non_polyglot_codes_of_interest.
 */
void
store_hash_value(Move *move_details, const char *fen)
{
    Move *move = move_details;
    Board *board = new_game_board(fen);
    Boolean Ok = TRUE;

    while ((move != NULL) && Ok) {
        /* Reset print_move number if a variation was printed. */
        if (*(move->move) == '\0') {
            /* A comment node, not a proper move. */
            move = move->next;
        }
        else if (apply_move(move, board)) {
            move = move->next;
        }
        else {
            print_error_context(GlobalState.logfile);
            fprintf(GlobalState.logfile, "Failed to make move %u%s %s\n",
                    board->move_number,
                    (board->to_move == WHITE) ? "." : "...",
                    move->move);
            Ok = FALSE;
        }
    }

    if (Ok) {
        HashLog *entry = (HashLog *) malloc_or_die(sizeof (*entry));
        unsigned ix = board->weak_hash_value % MAX_NON_POLYGLOT_CODE;

        /* We don't include the cumulative hash value as the sequence
         * of moves to reach this position is not important.
         */
        entry->cumulative_hash_value = 0;
        entry->final_hash_value = board->weak_hash_value;
        /* Link it into the head at this index. */
        entry->next = non_polyglot_codes_of_interest[ix];
        non_polyglot_codes_of_interest[ix] = entry;
        using_non_polyglot = TRUE;
    }
    else {
        exit(1);
    }
    free_board(board);
}

/* Define a table to hold the positional hash codes of interest.
 * Size should be a prime number for collision avoidance.
 */
#define MAX_POLYGLOT_CODE 541
static HashLog *polyglot_codes_of_interest[MAX_POLYGLOT_CODE];
/* Whether or not the polyglot hashcodes are in use. */
static Boolean using_polyglot = FALSE;

/**
 * Convert the given hex string to an int and save it
 * for position matching. 
 * @param value A hexadecimal string of up to 16 characters.
 * @return TRUE if the value is decoded ok; FALSE otherwise.
 */
Boolean save_polyglot_hashcode(const char *value)
{
    Boolean Ok;
    
    if(value != NULL && *value != '\0') {
        size_t len = strspn(value, "0123456789abcdefABCDEF");
        if(len > 0 && value[len] == '\0') {
            /* Versions prior to 22-02 failed to convert correctly values shorter
             * than 16 characters.
             */
            uint64_t hash = 0x0;
            char *end;
            hash = strtoull(value, &end, 16);
            Ok = (errno == 0 && *end == '\0');
            if (Ok) {
                HashLog *entry = (HashLog *) malloc_or_die(sizeof (*entry));
                unsigned ix = hash % MAX_POLYGLOT_CODE;

                /* We don't include the cumulative hash value as the sequence
                 * of moves to reach this position is not important.
                 */
                entry->cumulative_hash_value = 0;
                entry->final_hash_value = hash;
                /* Link it into the head at this index. */
                entry->next = polyglot_codes_of_interest[ix];
                polyglot_codes_of_interest[ix] = entry;
                using_polyglot = TRUE;
            }
            else {
                fprintf(GlobalState.logfile, "Unrecognised hash value %s\n", value);
            }
        }
        else {
            Ok = FALSE;
        }
    }
    else {
        Ok = FALSE;
    }
    return Ok;
}

/* Does the current board match a position of interest.
 * Look in codes_of_interest for current_hash_value.
 * Return NULL if no match, otherwise a possible label for the
 * match to be added to the game's tags. An empty string is
 * used for no label.
 */
static const char *
position_matches(const Board *board)
{
    Boolean found = FALSE;
    
    if(using_non_polyglot) {
        HashCode current_hash_value = board->weak_hash_value;
        unsigned ix = current_hash_value % MAX_NON_POLYGLOT_CODE;
        for (HashLog *entry = non_polyglot_codes_of_interest[ix]; !found && (entry != NULL);
                entry = entry->next) {
            /* We can test against just the position value. */
            if (entry->final_hash_value == current_hash_value) {
                found = TRUE;
            }
        }
    }
    if(!found && using_polyglot) {
        uint64_t current_hash_value = generate_zobrist_hash_from_board(board);
        unsigned ix = current_hash_value % MAX_POLYGLOT_CODE;
        for (HashLog *entry = polyglot_codes_of_interest[ix]; !found && (entry != NULL);
                entry = entry->next) {
            /* We can test against just the position value. */
            if (entry->final_hash_value == current_hash_value) {
                found = TRUE;
            }
        }
    }
    if(found && GlobalState.whose_move != EITHER_TO_MOVE) {
        if(board->to_move == WHITE && GlobalState.whose_move == BLACK_TO_MOVE) {
	    found = FALSE;
	}
        else if(board->to_move == BLACK && GlobalState.whose_move == WHITE_TO_MOVE) {
	    found = FALSE;
	}
    }
    if (found) {
        return "";
    }
    else {
        const char *match_label = pattern_match_board(board);
	if(match_label != NULL && GlobalState.whose_move != EITHER_TO_MOVE) {
	    if(board->to_move == WHITE && GlobalState.whose_move == BLACK_TO_MOVE) {
		match_label = NULL;
	    }
	    else if(board->to_move == BLACK && GlobalState.whose_move == WHITE_TO_MOVE) {
		match_label = NULL;
	    }
	}
        return match_label;
    }
}

/* Build a basic EPD string from the given board. */
void
build_basic_EPD_string(const Board *board, char *epd)
{
    Rank rank;
    int ix = 0;
#if 0
    Boolean castling_allowed;
#endif

    /* The board. */
    for (rank = LASTRANK; rank >= FIRSTRANK; rank--) {
        Col col;
        int consecutive_spaces = 0;
        for (col = FIRSTCOL; col <= LASTCOL; col++) {
            int coloured_piece = 
                board->board[RankConvert(rank)][ColConvert(col)];
            if (coloured_piece != EMPTY) {
                if (consecutive_spaces > 0) {
                    epd[ix] = '0' + consecutive_spaces;
                    ix++;
                    consecutive_spaces = 0;
                }
                epd[ix] = coloured_piece_to_SAN_letter(coloured_piece);
                ix++;
            }
            else {
                consecutive_spaces++;
            }
        }
        if (consecutive_spaces > 0) {
            epd[ix] = '0' + consecutive_spaces;
            ix++;
        }
        /* Terminate the row. */
        if (rank != FIRSTRANK) {
            epd[ix] = '/';
            ix++;
        }
    }
    epd[ix] = ' ';
    ix++;
    epd[ix] = board->to_move == WHITE ? 'w' : 'b';
    ix++;
    epd[ix] = ' ';
    ix++;

    /* Castling details. */
#if 1
    /* Try to cover Chess960 requirements. */
    if (board->WKingCastle == '\0' && board->WQueenCastle == '\0' &&
            board->BKingCastle == '\0' && board->BQueenCastle == '\0') {
        epd[ix] = '-';
        ix++;
    }
    else {
        if (board->WKingCastle != '\0' || board->WQueenCastle != '\0') {
            /* At least one White castling right. */
            if (board->WKingCastle != '\0' && board->WQueenCastle != '\0') {
                /* Both possible. */
                if (board->WKingCastle > board->WQueenCastle) {
                    /* Regular notation.
                     * NB: This loses square specifics, but that seems to be ok in
                     * X-FEN according to https://en.wikipedia.org/wiki/X-FEN
                     */
                    epd[ix] = 'K';
                    ix++;
                    epd[ix] = 'Q';
                    ix++;
                }
                else {
                    /* Out of order so store Rook cols. */
                    epd[ix] = toupper(board->WKingCastle);
                    ix++;
                    epd[ix] = toupper(board->WQueenCastle);
                    ix++;
                }
            }
            else {
                /* Only one. */
                if (board->WKingCastle != '\0') {
                    if (board->WKingCastle != LASTCOL) {
                        epd[ix] = toupper(board->WKingCastle);
                        ix++;
                    }
                    else {
                        /* Only kingside, from the regular position. */
                        epd[ix] = 'K';
                        ix++;
                    }
                }
                else {
                    if (board->WQueenCastle != FIRSTCOL) {
                        epd[ix] = toupper(board->WQueenCastle);
                        ix++;
                    }
                    else {
                        /* Only queenside, from the regular position. */
                        epd[ix] = 'Q';
                        ix++;
                    }
                }
            }
        }
        else {
            /* Only Black castling. */
        }
        /* Same for the queenside. */
        if (board->BKingCastle != '\0' || board->BQueenCastle != '\0') {
            /* At least one Black castling right. */
            if (board->BKingCastle != '\0' && board->BQueenCastle != '\0') {
                /* Both possible. */
                if (board->BKingCastle > board->BQueenCastle) {
                    /* Regular notation. */
                    epd[ix] = 'k';
                    ix++;
                    epd[ix] = 'q';
                    ix++;
                }
                else {
                    /* Out of order. */
                    epd[ix] = board->BKingCastle;
                    ix++;
                    epd[ix] = board->BQueenCastle;
                    ix++;
                }
            }
            else {
                /* Only one. */
                if (board->BKingCastle != '\0') {
                    if (board->BKingCastle != LASTCOL) {
                        epd[ix] = board->BKingCastle;
                        ix++;
                    }
                    else {
                        /* Only kingside, from the regular position. */
                        epd[ix] = 'k';
                        ix++;
                    }
                }
                else {
                    if (board->BQueenCastle != FIRSTCOL) {
                        epd[ix] = board->BQueenCastle;
                        ix++;
                    }
                    else {
                        /* Only queenside, from the regular position. */
                        epd[ix] = 'q';
                        ix++;
                    }
                }
            }
        }
        else {
            /* Only White castling. */
        }
    }
#else
    castling_allowed = FALSE;
    if (board->WKingCastle != '\0') {
        epd[ix] = 'K';
        ix++;
        castling_allowed = TRUE;
    }
    if (board->WQueenCastle != '\0') {
        epd[ix] = 'Q';
        ix++;
        castling_allowed = TRUE;
    }
    if (board->BKingCastle != '\0') {
        epd[ix] = 'k';
        ix++;
        castling_allowed = TRUE;
    }
    if (board->BQueenCastle != '\0') {
        epd[ix] = 'q';
        ix++;
        castling_allowed = TRUE;
    }
    if (!castling_allowed) {
        /* There are no castling rights. */
        epd[ix] = '-';
        ix++;
    }
#endif
    epd[ix] = ' ';
    ix++;

    /* Enpassant. */
    if (board->EnPassant) {
        /* It might be required to suppress redundant ep info. */
        Boolean suppress = FALSE;
        if (GlobalState.suppress_redundant_ep_info) {
            suppress = ep_is_redundant(board);
        }
        if (suppress) {
            epd[ix] = '-';
            ix++;
        }
        else {
            epd[ix] = board->ep_col;
            ix++;
            epd[ix] = board->ep_rank;
            ix++;
        }
    }
    else {
        epd[ix] = '-';
        ix++;
    }
    epd[ix] = '\0';
}

/* Determine whether the ep indication is redundant or not.
 * Assume that it is unless there is a pawn in position to
 * take advantage of it.
 */
Boolean
ep_is_redundant(const Board *board)
{
    Boolean redundant = TRUE;
    Col ep_col = board->ep_col;
    Rank from_rank;
    Piece pawn;
    if (board->to_move == WHITE) {
        /* White pawn on the fifth rank capturing a black pawn. */
        from_rank = '5';
        pawn = W(PAWN);
    }
    else {
        /* Black pawn on the fourth rank capturing a white pawn. */
        from_rank = '4';
        pawn = B(PAWN);
    }
    if ((ep_col > FIRSTCOL) && (board->board[RankConvert(from_rank)][ColConvert(ep_col - 1)] == pawn)) {
        /* Check that the move does not leave the king in check. */
        Board copy_board = *board;
        make_move(UNKNOWN_MOVE, ep_col - 1, from_rank,
                board->ep_col, board->ep_rank, PAWN, board->to_move, &copy_board);
        if (king_is_in_check(&copy_board, copy_board.to_move) == NOCHECK) {
            redundant = FALSE;
        }
    }
    if (redundant && (ep_col < LASTCOL) &&
            (board->board[RankConvert(from_rank)][ColConvert(ep_col + 1)] == pawn)) {
        /* Check that the move does not leave the king in check. */
        Board copy_board = *board;
        make_move(UNKNOWN_MOVE, ep_col + 1, from_rank,
                board->ep_col, board->ep_rank, PAWN, board->to_move, &copy_board);
        if (king_is_in_check(&copy_board, copy_board.to_move) == NOCHECK) {
            redundant = FALSE;
        }
    }
    return redundant;
}

/* Build and return a FEN string for the given board. */
char *get_FEN_string(const Board *board)
{
    char epd[FEN_SPACE], fen_suffix[FEN_SPACE];
    build_FEN_components(board, epd, fen_suffix);
    char *FEN_string = (char *) malloc_or_die(strlen(epd) + 1 + 
            strlen(fen_suffix) + 1);
    sprintf(FEN_string, "%s %s", epd, fen_suffix);
    return FEN_string;
}

/* Build a FEN string from the given board.
 * Place the EPD portion in epd and the half-move
 * count and following in fen_suffix.
 */
static void
build_FEN_components(const Board *board, char *epd, char *fen_suffix)
{

    build_basic_EPD_string(board, epd);
    /* Format the (pseudo) half move count and the full move count. */
    size_t ix = 0;
    /* Half moves since the last capture or pawn move. */
    sprintf(fen_suffix, "%u", board->halfmove_clock);
    ix = strlen(fen_suffix);
    fen_suffix[ix] = ' ';
    ix++;

    /* The full move number. */
    sprintf(&fen_suffix[ix], "%u", board->move_number);
}

#if 0
/* Append to move_details a FEN comment of the board.
 * The board state is immediately following application of the
 * given move.
 */
static void
append_FEN_comment(Move *move_details, const Board *board)
{
    CommentList *comment = (CommentList*) malloc_or_die(sizeof (*comment));
    StringList *current_comment = save_string_list_item(NULL, get_FEN_string(board));

    comment->comment = current_comment;
    comment->next = NULL;
    append_comments_to_move(move_details, comment);
}

/* Maximum length of a 64-bit unsigned int in decimal. 
 * NB: At the moment, the output is hex, which requires
 * only 16 characters.
 */
#define HASH_64_BIT_SPACE 20

/* Append to move_details a hashcode comment from the state of the board.
 * The board state is immediately following application of the
 * given move.
 */
static void
append_hashcode_comment(Move *move_details, Board *board)
{
    uint64_t hash = generate_zobrist_hash_from_board(board);
    char *hashcode_comment = (char *) malloc_or_die(HASH_64_BIT_SPACE + 1);
    CommentList *comment = (CommentList*) malloc_or_die(sizeof (*comment));
    StringList *current_comment = save_string_list_item(NULL, hashcode_comment);
    
    sprintf(hashcode_comment, "%016" PRIx64, hash);

    comment->comment = current_comment;
    comment->next = NULL;
    append_comments_to_move(move_details, comment);
}

/* Append to move_details an evaluation value for board.
 * The board state is immediately following application of the
 * given move.
 */
static void
append_evaluation(Move *move_details, const Board *board)
{
    CommentList *comment = (CommentList*) malloc_or_die(sizeof (*comment));
    /* Space template for the value.
     * @@@ There is a buffer-overflow risk here if the evaluation value
     * is too large.
     */
    const char valueSpace[] = "-012456789.00";
    char *evaluation = (char *) malloc_or_die(sizeof (valueSpace));
    StringList *current_comment;

    double value = evaluate(board);

    /* @@@ Overflow possible here if the value is too big to fit. */
    sprintf(evaluation, "%.2f", value);
    if (strlen(evaluation) > strlen(valueSpace)) {
        fprintf(GlobalState.logfile,
                "Overflow in evaluation space in append_evaluation()\n");
        exit(1);
    }

    current_comment = save_string_list_item(NULL, evaluation);
    comment->comment = current_comment;
    comment->next = NULL;
    append_comments_to_move(move_details, comment);
}
#endif

/* Create a comment indicating in a positional match. */
CommentList *
create_match_comment(const Board *board)
{
    /* The comment string. */
    char *match_comment;
    
    if(strcmp(GlobalState.position_match_comment, "FEN") != 0) {
        match_comment = copy_string(GlobalState.position_match_comment);
    }
    else {
        match_comment = get_FEN_string(board);
    }
    StringList *current_comment = save_string_list_item(NULL, match_comment);
    CommentList *comment = (CommentList*) malloc_or_die(sizeof (*comment));

    comment->comment = current_comment;
    comment->next = NULL;
    return comment;
}

/* Return an evaluation of board. */
static double
evaluate(const Board *board)
{
    return shannonEvaluation(board);
}

/* Return an evaluation of board based on
 * Claude Shannon's technique.
 */
static double
shannonEvaluation(const Board *board)
{
    MovePair *white_moves, *black_moves;
    int whiteMoveCount = 0, blackMoveCount = 0;
    int whitePieceCount = 0, blackPieceCount = 0;
    double shannonValue = 0.0;

    Rank rank;
    Col col;

    /* Determine the mobilities. */
    white_moves = find_all_moves(board, WHITE);
    if (white_moves != NULL) {
        MovePair *m;
        for (m = white_moves; m != NULL; m = m->next) {
            whiteMoveCount++;
        }
        free_move_pair_list(white_moves);
    }

    black_moves = find_all_moves(board, BLACK);
    if (black_moves != NULL) {
        MovePair *m;
        for (m = black_moves; m != NULL; m = m->next) {
            blackMoveCount++;
        }
        free_move_pair_list(black_moves);
    }


    /* Pick up each piece of the required colour. */
    for (rank = LASTRANK; rank >= FIRSTRANK; rank--) {
        int r = RankConvert(rank);
        for (col = FIRSTCOL; col <= LASTCOL; col++) {
            int c = ColConvert(col);
            int pieceValue = 0;
            Piece occupant = board->board[r][c];
            if (occupant != EMPTY) {
                /* This square is occupied by a piece of the required colour. */
                Piece piece = EXTRACT_PIECE(occupant);

                switch (piece) {
                    case PAWN:
                        pieceValue = 1;
                        break;
                    case BISHOP:
                    case KNIGHT:
                        pieceValue = 3;
                        break;
                    case ROOK:
                        pieceValue = 5;
                        break;
                    case QUEEN:
                        pieceValue = 9;
                        break;
                    case KING:
                        break;
                    default:
                        fprintf(GlobalState.logfile,
                                "Internal error: unknown piece %d in append_evaluation().\n",
                                piece);
                }
                if (EXTRACT_COLOUR(occupant) == WHITE) {
                    whitePieceCount += pieceValue;
                }
                else {
                    blackPieceCount += pieceValue;
                }
            }
        }
    }

    shannonValue = (whitePieceCount - blackPieceCount) +
            (whiteMoveCount - blackMoveCount) * 0.1;
    return shannonValue;
}

/* Look for comment_pattern in the given list of comments.
 * If found, return where it was found, otherwise NULL.
 */
static StringList *
find_matching_comment(const char *comment_pattern,
                      const CommentList *comment)
{
    Boolean match = FALSE;
    StringList *string_list;
    while(!match && comment != NULL) {
        string_list = comment->comment;
        while(!match && string_list != NULL) {
            if(strcmp(comment_pattern, string_list->str) == 0) {
                match = TRUE;
            }
            else {
                string_list = string_list->next;
            }
        }
        if(!match) {
            comment = comment->next;
        }
    }
    if(match) {
        return string_list;
    }
    else {
        return (StringList *) NULL;
    }
}
