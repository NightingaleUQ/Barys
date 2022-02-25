#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <err.h>
#include "board.h"

// Enumeration of pieces
static const char pieceSyms[7] = {' ', 'p', 'R', 'N', 'B', 'Q', 'K'};

// Returns result in algebra as five characters + \0
static void move_to_algebra(const struct Move* m, char algebra[6]) {
    if (m == NULL || !PIECE(m->piece)) {
        snprintf(algebra, 6, "none");
    } else {
        algebra[0] = pieceSyms[PIECE(m->piece)];
        from_0x88_to_coord(m->orig, algebra + 1);
        from_0x88_to_coord(m->dest, algebra + 3);
        algebra[5] = 0;
    }
}

void print_state(const struct State* s) {
    char alge[6];
    move_to_algebra(&s->lastMove, alge);
    printf("%s", alge);
    if (BLACK_TO_MOVE(s))
        printf(", black to move\n");
    else
        printf(", white to move\n");

    printf("   ");
    for (char f = 'a'; f <= 'h'; f++) {
        printf(" %c ", f);
    }
    printf("\n");

    for (int8_t r = 7; r >= 0; r--) {
        printf(" %d ", r + 1);
        for (int8_t f = 0; f <= 7; f++) {
            // Whether the square is white
            uint8_t white_square = 0;
            if ((!(r % 2) && (f % 2)) || ((r % 2) && !(f % 2))) white_square = 1;
            if (white_square)
                printf("\033[107m");
            else
                printf("\033[40m");
            printf(" ");
            uint8_t sq = s->board[to_0x88(r, f)];
            // Determine the piece's colour
            if (IS_BLACK(sq))
                printf("\033[31m");
            else
                printf("\033[39m");
            // Print out the piece
            char sym = pieceSyms[PIECE(sq)];
            printf("%c", sym);
            printf(" ");
            printf("\033[39m");
        }
        printf("\033[40m");
        printf("\n");
    }
    printf("\n");
}

uint8_t is_on_board(uint8_t pos) {
    return !(pos & 0x88);
}

uint8_t to_0x88(uint8_t rank, uint8_t file) {
    return (rank << 4) + file;
}

void from_0x88(uint8_t pos, uint8_t* rank, uint8_t* file) {
    *rank = (pos >> 4);
    *file = (pos & 0x07);
}

uint8_t coord_to_0x88(char coord[2]) {
    uint8_t f = tolower(coord[0]) - 'a' + 1;
    uint8_t r = coord[1] - '1';
    if (r < '1' || r > '7') return 0xFF;
    if (f < 'a' || f > 'h') return 0xFF;
    return to_0x88(r, f);
}

uint8_t from_0x88_to_coord(uint8_t pos, char coord[2]) {
    uint8_t r, f;
    if (!is_on_board(pos)) return 0xFF;
    from_0x88(pos, &r, &f);
    coord[0] = f + 'a';
    coord[1] = r + '1';
    return 1;
}

static const struct State initialState = {
    .board = {
        WHITE|ROOK, WHITE|KNIGHT, WHITE|BISHOP, WHITE|QUEEN, WHITE|KING, WHITE|BISHOP, WHITE|KNIGHT, WHITE|ROOK, 0, 0, 0, 0, 0, 0, 0, 0,
        WHITE|PAWN, WHITE|PAWN, WHITE|PAWN, WHITE|PAWN, WHITE|PAWN, WHITE|PAWN, WHITE|PAWN, WHITE|PAWN,  0, 0, 0, 0, 0, 0, 0, 0,
        NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE,  0, 0, 0, 0, 0, 0, 0, 0,
        NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE,  0, 0, 0, 0, 0, 0, 0, 0,
        NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE,  0, 0, 0, 0, 0, 0, 0, 0,
        NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE,  0, 0, 0, 0, 0, 0, 0, 0,
        BLACK|PAWN, BLACK|PAWN, BLACK|PAWN, BLACK|PAWN, BLACK|PAWN, BLACK|PAWN, BLACK|PAWN, BLACK|PAWN,  0, 0, 0, 0, 0, 0, 0, 0,
        BLACK|ROOK, BLACK|KNIGHT, BLACK|BISHOP, BLACK|QUEEN, BLACK|KING, BLACK|BISHOP, BLACK|KNIGHT, BLACK|ROOK, 0, 0, 0, 0, 0, 0, 0, 0,
    },
    .ply = 0,
    .last = NULL,
    .succ = NULL,
    .cSucc = 0,
    .nSucc = 0
};

// Allocates memory for a successor state and pre-populates some fields.
static struct State* add_result(struct State* s, struct Move* m) {
    if (s->succ == NULL) {
        s->succ = malloc(48 * sizeof(struct State));
        s->cSucc = 48;
    } else if (s->nSucc == s->cSucc) {
        s->cSucc += 16;
        s->succ = realloc(s->succ, s->cSucc);
    }

    // Copy state
    struct State* suc = &s->succ[s->nSucc];
    memcpy(suc, s, sizeof(struct State));

    return &s->succ[(s->nSucc)++];
}

// Adds states to successor if the piece can move according to m.
static struct State* move_piece(struct State* s, struct Move* m) {
    m->pieceCaptured = 0;
    m->valid = 0;

    if (!is_on_board(m->dest)) {
        return NULL;
    }
    uint8_t tgt = s->board[m->dest];
    if (IS_VACANT(tgt) || ((BLACK_TO_MOVE(s) && IS_WHITE(tgt)) || (WHITE_TO_MOVE(s) && IS_BLACK(tgt)))) {
        // Get struct for new state.
        struct State* suc = add_result(s, m);

        // Move piece to either a vacant square or capture.
        // Also record whether a piece has been moved before (for castling)
        suc->board[m->dest] = s->board[m->orig] | PIECE_MOVED;
        suc->board[m->orig] = 0;

        // Update move number and information on last move
        m->piece = PIECE(s->board[m->orig]);
        m->pieceCaptured = !IS_VACANT(s->board[m->dest]);
        m->valid = 1;
        memcpy(&suc->lastMove, m, sizeof(struct Move));

        suc->ply++;
        suc->last = s;
        suc->succ = NULL;
        suc->cSucc = 0;
        suc->nSucc = 0;
        suc->movesExpanded = 0;
        suc->checksRemoved = 0;

        return suc;
    }
    return NULL;
}

static void slide_piece(struct State* s, uint8_t orig, int8_t dirn, uint8_t isKing) {
    uint8_t dest = orig;
    for(;;) {
        dest += dirn;
        struct Move m = {.orig = orig, .dest = dest};
        move_piece(s, &m);
        if (isKing || !m.valid || m.pieceCaptured) {
            // The King is limited to one step anyway.
            break;
        }
    }
}

// Checks a state for a pawn that is due to be promoted.
static void check_promotion(struct State* s, const struct Move *m) {
    if (s == NULL) return;
    uint8_t destRank, destFile;
    from_0x88(m->dest, &destRank, &destFile);
    if ((BLACK_TO_MOVE(s) && destRank == 0) || (WHITE_TO_MOVE(s) && destRank == 7)) {
        // Promote this state to Queen TODO Other promotions
        s->board[m->dest] &= PIECE(0xFF);
        s->board[m->dest] |= QUEEN;
    }
}

// Rook: 0-3, Bishop: 4-7, Queen and King: 0-7;
static const int8_t slideDirns[8] = {LEFT, RIGHT, UP, DOWN, UP_LEFT, UP_RIGHT, DOWN_LEFT, DOWN_RIGHT};
static const int8_t knightDirns[8] = {UP+UP_LEFT, UP+UP_RIGHT, RIGHT+UP_RIGHT, RIGHT+DOWN_RIGHT, DOWN+DOWN_LEFT, DOWN+DOWN_RIGHT, LEFT+DOWN_LEFT, LEFT+UP_LEFT};
// Forward: 0, Two-step: 1, Capture: 2-3, En passant: 4-5 (corresponding to 2-3, respectively)
static const int8_t pawnDirnsBlack[6] = {DOWN, DOWN+DOWN, DOWN_LEFT, DOWN_RIGHT, LEFT, RIGHT};
static const int8_t pawnDirnsWhite[6] = {UP, UP+UP, UP_LEFT, UP_RIGHT, LEFT, RIGHT};

static void _get_moves(struct State* s) {
    if (s->movesExpanded) return;
    struct State* succ;
    for (int8_t r = 0; r < 8; r++)
    for (int8_t f = 0; f < 8; f++) {
        uint8_t orig = to_0x88(r, f);
        // CHECK COLOUR and then extract only piece type.
        uint8_t piece = s->board[orig];
        if ((BLACK_TO_MOVE(s) && IS_WHITE(piece)) || (WHITE_TO_MOVE(s) && IS_BLACK(piece))) continue;
        piece = PIECE(piece);

        // ROOK
        if (piece == ROOK) {
            for (int8_t dirn = 0; dirn <= 3; dirn++) {
                slide_piece(s, orig, slideDirns[dirn], 0);
            }
        }
        // BISHOP
        if (piece == BISHOP) {
            for (int8_t dirn = 4; dirn <= 7; dirn++) {
                slide_piece(s, orig, slideDirns[dirn], 0);
            }
        }
        // QUEEN
        if (piece == QUEEN) {
            for (int8_t dirn = 0; dirn <= 7; dirn++) {
                slide_piece(s, orig, slideDirns[dirn], 0);
            }
        }
        // KING
        if (piece == KING) {
            for (int8_t dirn = 0; dirn <= 7; dirn++) {
                slide_piece(s, orig, slideDirns[dirn], 1);
            }
            // TODO Castling
        }
        // KNIGHT
        if (piece == KNIGHT) {
            for (int8_t dirn = 0; dirn <= 7; dirn++) {
                uint8_t dest = orig + knightDirns[dirn];
                struct Move m = {.orig = orig, .dest = dest};
                move_piece(s, &m);
            }
        }
        // PAWN
        if (piece == PAWN) {
            // Clear the two-step flag from the last move
            if ((BLACK_TO_MOVE(s) && IS_BLACK(piece)) || (WHITE_TO_MOVE(s) && IS_WHITE(piece))) {
                s->board[orig] &= ~PAWN_TWO_STEP;
            }

            struct Move m;
            m.orig = orig;
            const int8_t* pawnDirns = BLACK_TO_MOVE(s) ? pawnDirnsBlack : pawnDirnsWhite;

            // Square in front is clear: move forward one or two ranks.
            m.dest = orig + pawnDirns[0];
            if (IS_VACANT(s->board[m.dest])) {
                succ = move_piece(s, &m);
                check_promotion(succ, &m);

                // Two-step
                m.dest = orig + pawnDirns[1];
                if (IS_VACANT(s->board[m.dest]))
                if ((BLACK_TO_MOVE(s) && r == 6) || (WHITE_TO_MOVE(s) && r == 1)) {
                    succ = move_piece(s, &m);
                    if (succ != NULL) {
                        // Record two-step
                        succ->board[m.dest] |= PAWN_TWO_STEP;
                        check_promotion(succ, &m);
                    }
                }
            }

            for (uint8_t i = 2; i <= 3; i++) {
                // Capture: Square along diagonal contains a piece of opposite colour.
                m.dest = orig + pawnDirns[i];
                uint8_t tgt = s->board[m.dest];
                if ((BLACK_TO_MOVE(s) && IS_WHITE(tgt)) || (WHITE_TO_MOVE(s) && IS_BLACK(tgt))) {
                    succ = move_piece(s, &m);
                    check_promotion(succ, &m);
                }

                // En passant: Check rank and pieces beside and clear destination.
                // m.dest is the same value used for normal capture above.
                uint8_t adjacent = s->board[orig + pawnDirns[i + 2]];
                if ((BLACK_TO_MOVE(s) && r == 3 && IS_WHITE(adjacent)) || (WHITE_TO_MOVE(s) && r == 4 && IS_BLACK(adjacent)))
                if (IS_VACANT(tgt) && PIECE(adjacent) == PAWN && IS_PAWN_TWO_STEP(adjacent)) {
                    succ = move_piece(s, &m);
                    // Additionally, remove en passant-ed piece
                    if (succ != NULL)
                        succ->board[orig + pawnDirns[i + 2]] = 0;
                }
            }
        }
    }
    s->movesExpanded = 1;
}

static uint8_t _king_captured(const struct State* s) {
    for (uint8_t i = 0; i < 128; i++) {
        if (PIECE(s->board[i]) == KING)
        if ((WHITE_TO_MOVE(s) && IS_WHITE(s->board[i])) || (BLACK_TO_MOVE(s) && IS_BLACK(s->board[i]))) {
            return 0;
        }
    }
    return 1;
}

// Return whether this successor state leaves the King in check.
static uint8_t _is_in_check(struct State* su) {
    _get_moves(su);
    for (uint8_t i = 0; i < su->nSucc; i++) {
        // See if for s' there exists a s'' that has the King captured
        if (_king_captured(&su->succ[i])) {
            return 1;
        }
    }
    return 0;
}

static void _remove_check(struct State* s) {
    if (s->checksRemoved) return;
    if (s->succ == NULL) return;
    for (int8_t i = 0; i < s->nSucc; i++) {
        if (_is_in_check(&s->succ[i])) {
            // Take element from end of array and replace here.
            memcpy(&s->succ[i], &s->succ[s->nSucc - 1], sizeof(struct State));
            s->nSucc--;
            i--; // This replacing element needs to be checked again
        }
    }
    s->checksRemoved = 1;
}

void get_legal_moves(struct State* s) {
    _get_moves(s);
    _remove_check(s);
}

int main() {
    struct State s;
    memcpy(&s, &initialState, sizeof(struct State));
    char gamefn[80];
    int gamef;
    // Prompt loop
    for (;;) {
        print_state(&s);

        // Stalemate? Checkmate?
        get_legal_moves(&s);
        if (s.nSucc == 0) {
            // TODO
            printf("CHECKMATE / STALEMATE\n");
            break;
        }
        
        // Print legal moves
        for (uint8_t i = 0; i < s.nSucc; i++) {
            char alge[6];
            move_to_algebra(&s.succ[i].lastMove, alge);
            printf("%s", alge);
            printf("     ");
            if (((i + 1) % 6) == 0) {
                printf("\n");
            }
        }

        uint8_t cmdValid = 0;
        char buf[80];
        bzero(buf, 80);
        printf("\nPlease enter a move, or type a time in seconds to search $ ");
        fflush(stdout);
        fgets (buf, 80, stdin);
        size_t z = strnlen(buf, 80);
        if (z == 0) break; // End of input
        buf[z - 1] = 0; // Strip trailing newline

        // Makes a move
        // Check that the move is legal, the execute it
        for (uint8_t i = 0; i < s.nSucc; i++) {
            char alge[6];
            move_to_algebra(&s.succ[i].lastMove, alge);
            if (strncmp(buf, alge, 6) == 0) {
                // Overwrite current state
                memcpy(&s, &s.succ[i], sizeof(struct State));
                // Save new state to disk
                mkdir("history", 0777);
                snprintf(gamefn, 80, "history/move%d.game", s.ply);
                gamef = open(gamefn, O_CREAT | O_WRONLY, 0666);
                if (gamef < 0)
                    warn("open(): Error saving board");
                if (write(gamef, &s, sizeof(struct State)) < 0)
                    warn("write(): Error saving board");
                close(gamef);
                cmdValid = 1;
                break;
            }
        }

        // Load state
        // Remember to clear references to successor states

        // Fork process and perform MCTS

        if (!cmdValid)
            printf("Invalid command, try again.");

        printf("\n");
    }
    return 0;
}
