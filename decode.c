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

/* This file contains functions concerned with decoding
 * the original text of a move in order to determine:
 *     which MoveClass it is in;
 *     any start and end square information.
 * It extracts this information purely from the move text
 * rather than analysing the move within the context of
 * a board position.
 * This information is later refined by the semantic analysis
 * phase of the program as part of the checking of a game score.
 */

#include <stdio.h>
#include <string.h>
#include "bool.h"
#include "mymalloc.h"
#include "defs.h"
#include "typedef.h"
#include "decode.h"
#include "tokens.h"
#include "taglist.h"
#include "lex.h"

/* Does the character represent a column of the board? */
Boolean
is_col(char c)
{
    return (FIRSTCOL <= c) && (c <= LASTCOL);
}

/* Does the character represent a rank of the board? */
Boolean
is_rank(char c)
{
    return (FIRSTRANK <= c) && (c <= LASTRANK);
}

/* What kind of piece is *move likely to represent?
 * Note, the provision for double-character pieces,
 * like a Russian King, means we need access to a
 * string rather than a single char.
 */
Piece
is_piece(const unsigned char *move)
{
    Piece piece = EMPTY;

    switch (*move) {
        case 'K': case 'k':
            piece = KING;
            break;
        case 'Q': case 'q':
        case 'D': /* Dutch/German. */
        case RUSSIAN_QUEEN:
            piece = QUEEN;
            break;
        case 'R': case 'r':
        case 'T': /* Dutch/German. */
        case RUSSIAN_ROOK:
            piece = ROOK;
            break;
        case 'N': case 'n':
        case 'P': /* Dutch. */
        case 'S': /* German. */
            piece = KNIGHT;
            break;
        case 'B':
        case 'L': /* Dutch/German. */
        case RUSSIAN_BISHOP:
            /* Lower case 'b' is most likely to be a pawn reference. */
            piece = BISHOP;
            break;
        case RUSSIAN_KNIGHT_OR_KING:
            if (RUSSIAN_PIECE_CHECK(*(move + 1)) == RUSSIAN_KING_SECOND_LETTER) {
                piece = KING;
            }
            else {
                piece = KNIGHT;
            }
            break;
    }
    return piece;
}

/* Is the symbol a capturing one?
 * In fact, this is used to recognise any general separator
 * between two parts of a move, e.g.:
 *        Nxc3, e2-e4, etc.
 */

static Boolean
is_capture(char c)
{
    return (c == 'x') || (c == 'X') || (c == ':') || (c == '-');
}

static Boolean
is_castling_character(char c)
{
    return (c == 'O') || (c == '0') || (c == 'o');
}

Boolean
is_check(char c)
{
    return (c == '+') || (c == '#');
}

/* Allocate space in which to return the information that
 * has been gleaned from the move.
 */
Move *
new_move_structure(void)
{
    Move *move = (Move *) malloc_or_die(sizeof (Move));

    move->terminating_result = NULL;
    move->piece_to_move = EMPTY;
    move->captured_piece = EMPTY;
    move->promoted_piece = EMPTY;
    move->check_status = NOCHECK;
    move->epd = NULL;
    move->fen_suffix = NULL;
    move->zobrist = ~0;
    move->evaluation = 0;
    move->NAGs = NULL;
    move->comment_list = NULL;
    move->Variants = NULL;
    move->prev = NULL;
    move->next = NULL;
    return move;
}

/* Work out whatever can be gleaned from move_string of
 * the starting and ending points of the given move.
 * The move may be any legal string.
 * The scanning here is libertarian, so it relies heavily on
 * illegal moves having already been filtered out by the process
 * of lexical analysis.
 */
Move *
decode_move(const unsigned char *move_string)
{ /* The four components of the co-ordinates when known. */
    Rank from_rank = 0, to_rank = 0;
    Col from_col = 0, to_col = 0;
    MoveClass class;
    Boolean Ok = TRUE;
    /* Temporary locations until known whether they are from_ or to_. */
    Col col = 0;
    Rank rank = 0;
    /* A pointer to move along the move string. */
    const unsigned char *move = move_string;
    /* A pointer to the structure containing the details to be returned. */
    Move *move_details;
    Piece piece_to_move = EMPTY;

    /* Make an initial distinction between pawn moves and piece moves. */
    if (is_col(*move)) {
        /* Pawn move. */
        class = PAWN_MOVE;
        piece_to_move = PAWN;
        col = *move;
        move++;
        if (is_rank(*move)) {
            /* e4, e2e4 */
            rank = *move;
            move++;
            if (is_capture(*move)) {
                move++;
            }
            if (is_col(*move)) {
                from_col = col;
                from_rank = rank;
                to_col = *move;
                move++;
                if (is_rank(*move)) {
                    to_rank = *move;
                    move++;
                }
            }
            else {
                to_col = col;
                to_rank = rank;
            }
        }
        else {
            if (is_capture(*move)) {
                /* axb */
                move++;
            }
            if (is_col(*move)) {
                /* ab, or bg8 for liberal bishop moves. */
                from_col = col;
                to_col = *move;
                move++;
                if (is_rank(*move)) {
                    to_rank = *move;
                    move++;
                    /* Check the sanity of this. */
                    if ((from_col != 'b') &&
                            (from_col != (to_col + 1)) && (from_col != (to_col - 1))) {
                        Ok = FALSE;
                    }
                }
                else {
                    /* Check the sanity of this. */
                    if ((from_col != (to_col + 1)) && (from_col != (to_col - 1))) {
                        Ok = FALSE;
                    }
                }
            }
            else {
                print_error_context(GlobalState.logfile);
                fprintf(GlobalState.logfile, "Unknown pawn move %s.\n",
                        move_string);
                Ok = FALSE;
            }
        }
        if (Ok) {
            /* Look for promotions. */
            if (*move == '=') {
                move++;
            }
            /* djb From v17-27 allow a trailing 'b' as a Bishop promotion. */
            if (is_piece(move) != EMPTY || *move == 'b') {
                class = PAWN_MOVE_WITH_PROMOTION;
                /* @@@ Strictly speaking, if the piece is a RUSSIAN_KING
                 * then we should skip two chars.
                 */
                move++;
            }
        }
    }
    else if ((piece_to_move = is_piece(move)) != EMPTY) {
        class = PIECE_MOVE;
        /* Check for a two-character piece. */
        if ((RUSSIAN_PIECE_CHECK(*move) == RUSSIAN_KNIGHT_OR_KING) &&
                (piece_to_move == KING)) {
            move++;
        }
        move++;
        if (is_rank(*move)) {
            /* A disambiguating rank.
             * R1e1, R1xe3.
             */
            from_rank = *move;
            move++;
            if (is_capture(*move)) {
                move++;
            }
            if (is_col(*move)) {
                to_col = *move;
                move++;
                if (is_rank(*move)) {
                    to_rank = *move;
                    move++;
                }
            }
            else {
                Ok = FALSE;
                print_error_context(GlobalState.logfile);
                fprintf(GlobalState.logfile, "Unknown piece move %s.\n",
                        move_string);
            }
        }
        else {
            if (is_capture(*move)) {
                /* Rxe1 */
                move++;
                if (is_col(*move)) {
                    to_col = *move;
                    move++;
                    if (is_rank(*move)) {
                        to_rank = *move;
                        move++;
                    }
                    else {
                        Ok = FALSE;
                        print_error_context(GlobalState.logfile);
                        fprintf(GlobalState.logfile,
                                "Unknown piece move %s.\n", move_string);
                    }
                }
                else {
                    Ok = FALSE;
                    print_error_context(GlobalState.logfile);
                    fprintf(GlobalState.logfile, "Unknown piece move %s.\n",
                            move_string);
                }
            }
            else if (is_col(*move)) {
                col = *move;
                move++;
                if (is_capture(*move)) {
                    move++;
                }
                if (is_rank(*move)) {
                    /* Re1, Re1d1, Re1xd1 */
                    rank = *move;
                    move++;
                    if (is_capture(*move)) {
                        move++;
                    }
                    if (is_col(*move)) {
                        /* Re1d1 */
                        from_col = col;
                        from_rank = rank;
                        to_col = *move;
                        move++;
                        if (is_rank(*move)) {
                            to_rank = *move;
                            move++;
                        }
                        else {
                            Ok = FALSE;
                            print_error_context(GlobalState.logfile);
                            fprintf(GlobalState.logfile,
                                    "Unknown piece move %s.\n", move_string);
                        }
                    }
                    else {
                        to_col = col;
                        to_rank = rank;
                    }
                }
                else if (is_col(*move)) {
                    /* Rae1 */
                    from_col = col;
                    to_col = *move;
                    move++;
                    if (is_rank(*move)) {
                        to_rank = *move;
                        move++;
                    }
                }
                else {
                    Ok = FALSE;
                    print_error_context(GlobalState.logfile);
                    fprintf(GlobalState.logfile, "Unknown piece move %s.\n",
                            move_string);
                }
            }
            else {
                Ok = FALSE;
                print_error_context(GlobalState.logfile);
                fprintf(GlobalState.logfile, "Unknown piece move %s.\n", move_string);
            }
        }
    }
    else if (is_castling_character(*move)) {
        /* Some form of castling. */
        move++;
        /* Allow separators to be optional. */
        if (*move == '-') {
            move++;
        }
        if (is_castling_character(*move)) {
            move++;
            if (*move == '-') {
                move++;
            }
            if (is_castling_character(*move)) {
                class = QUEENSIDE_CASTLE;
                move++;
            }
            else {
                class = KINGSIDE_CASTLE;
            }
        }
        else {
            print_error_context(GlobalState.logfile);
            fprintf(GlobalState.logfile, "Unknown castling move %s.\n", move_string);
            Ok = FALSE;
        }
    }
    else if (strcmp((char *) move_string, NULL_MOVE_STRING) == 0) {
        class = NULL_MOVE;
    }
    else {
        print_error_context(GlobalState.logfile);
        fprintf(GlobalState.logfile, "Unknown move %s.\n", move_string);
        Ok = FALSE;
    }
    if (Ok && class != NULL_MOVE) {
        /* Allow trailing checks. */
        while (is_check(*move)) {
            move++;
        }
        if (*move == '\0') {
            /* Nothing more to check. */
        }
        else if (((strcmp((const char *) move, "ep") == 0) ||
                (strcmp((const char *) move, "e.p.") == 0)) &&
                (class == PAWN_MOVE)) {
            /* These are ok. */
            class = ENPASSANT_PAWN_MOVE;
        }
        else {
            Ok = FALSE;
            print_error_context(GlobalState.logfile);
            fprintf(GlobalState.logfile,
                    "Unknown text trailing move %s <%s>.\n", move_string, move);
        }
    }
    /* Store all of the details gathered, even if the move is illegal. */
    if (!Ok) {
        class = UNKNOWN_MOVE;
    }
    move_details = new_move_structure();
    strcpy((char *) move_details->move, (const char *) move_string);
    move_details->class = class;
    move_details->piece_to_move = piece_to_move;
    move_details->from_col = from_col;
    move_details->from_rank = from_rank;
    move_details->to_col = to_col;
    move_details->to_rank = to_rank;
    move_details->captured_piece = EMPTY;
    move_details->check_status = NOCHECK;
    return move_details;
}

Move *
decode_algebraic(Move *move_details, Board *board)
{
    int from_r = RankConvert(move_details->from_rank);
    int from_c = ColConvert(move_details->from_col);
    Piece piece_to_move = EXTRACT_PIECE(board->board[from_r][from_c]);

    if (piece_to_move != EMPTY) {
        /* Check for the special case of castling. */
        if ((piece_to_move == KING) && (move_details->from_col == 'e')) {
            if (move_details->to_col == 'g') {
                move_details->class = KINGSIDE_CASTLE;
            }
            else if (move_details->to_col == 'c') {
                move_details->class = QUEENSIDE_CASTLE;
            }
            else {
                move_details->class = PIECE_MOVE;
                move_details->piece_to_move = piece_to_move;
            }
        }
        else {
            if (piece_to_move == PAWN) {
                move_details->class = PAWN_MOVE;
            }
            else {
                move_details->class = PIECE_MOVE;
            }
            move_details->piece_to_move = piece_to_move;
        }
        move_details->captured_piece = EMPTY;
        move_details->check_status = NOCHECK;
    }
    return move_details;
}

/* See if move_string seems to represent the text of a valid move.
 * Don't print any error messages, just return TRUE or FALSE.
 */
Boolean
move_seems_valid(const unsigned char *move_string)
{
    MoveClass class;
    Boolean Ok = TRUE;
    /* A pointer to move along the move string. */
    unsigned const char *move = move_string;

    /* Make an initial distinction between pawn moves and piece moves. */
    if (is_col(*move)) {
        /* Pawn move. */
        class = PAWN_MOVE;
        move++;
        if (is_rank(*move)) {
            /* e4, e2e4 */
            move++;
            if (is_capture(*move)) {
                move++;
            }
            if (is_col(*move)) {
                move++;
                if (is_rank(*move)) {
                    move++;
                }
            }
            else {
            }
        }
        else {
            if (is_capture(*move)) {
                /* axb */
                move++;
            }
            if (is_col(*move)) {
                /* ab */
                move++;
                if (is_rank(*move)) {
                    move++;
                }
            }
            else {
                Ok = FALSE;
            }
        }
        if (Ok) {
            /* Look for promotions. */
            if (*move == '=') {
                move++;
            }
            /* djb From v17-27 allow a trailing 'b' as a Bishop promotion. */
            if (is_piece(move) != EMPTY || *move == 'b') {
                class = PAWN_MOVE_WITH_PROMOTION;
                /* @@@ Strictly speaking, if the piece is a RUSSIAN_KING
                 * then we should skip two chars.
                 */
                move++;
            }
        }
    }
    else if (is_piece(move) != EMPTY) {
        class = PIECE_MOVE;
        /* Check for a two-character piece. */
        if ((RUSSIAN_PIECE_CHECK(*move) == RUSSIAN_KNIGHT_OR_KING) &&
                (is_piece(move) == KING)) {
            move++;
        }
        move++;
        if (is_rank(*move)) {
            /* A disambiguating rank.
             * R1e1, R1xe3.
             */
            move++;
            if (is_capture(*move)) {
                move++;
            }
            if (is_col(*move)) {
                move++;
                if (is_rank(*move)) {
                    move++;
                }
            }
            else {
                Ok = FALSE;
            }
        }
        else {
            if (is_capture(*move)) {
                /* Rxe1 */
                move++;
                if (is_col(*move)) {
                    move++;
                    if (is_rank(*move)) {
                        move++;
                    }
                    else {
                        Ok = FALSE;
                    }
                }
                else {
                    Ok = FALSE;
                }
            }
            else if (is_col(*move)) {
                move++;
                if (is_capture(*move)) {
                    move++;
                }
                if (is_rank(*move)) {
                    /* Re1, Re1d1, Re1xd1 */
                    move++;
                    if (is_capture(*move)) {
                        move++;
                    }
                    if (is_col(*move)) {
                        /* Re1d1 */
                        move++;
                        if (is_rank(*move)) {
                            move++;
                        }
                        else {
                            Ok = FALSE;
                        }
                    }
                }
                else if (is_col(*move)) {
                    /* Rae1 */
                    move++;
                    if (is_rank(*move)) {
                        move++;
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
        }
    }
    else if (is_castling_character(*move)) {
        /* Some form of castling. */
        move++;
        /* Allow separators to be optional. */
        if (*move == '-') {
            move++;
        }
        if (is_castling_character(*move)) {
            move++;
            if (*move == '-') {
                move++;
            }
            if (is_castling_character(*move)) {
                class = QUEENSIDE_CASTLE;
                move++;
            }
            else {
                class = KINGSIDE_CASTLE;
            }
        }
        else {
            Ok = FALSE;
        }
    }
    else {
        Ok = FALSE;
    }
    if (Ok) {
        /* Allow trailing checks. */
        while (is_check(*move)) {
            move++;
        }
        if (*move == '\0') {
            /* Nothing more to check. */
        }
        else if (((strcmp((const char *) move, "ep") == 0) ||
                (strcmp((const char *) move, "e.p.") == 0)) &&
                (class == PAWN_MOVE)) {
            /* These are ok. */
            class = ENPASSANT_PAWN_MOVE;
        }
        else {
            Ok = FALSE;
        }
    }
    return Ok;
}
