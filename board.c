#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "board.h"

void print_state(const struct State* s) {
    for (int8_t r = 7; r >= 0; r--) {
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
            switch (PIECE(sq)) {
                case PAWN:  printf("p"); break;
                case ROOK:  printf("R"); break;
                case KNIGHT:printf("N"); break;
                case BISHOP:printf("B"); break;
                case QUEEN: printf("Q"); break;
                case KING:  printf("K"); break;
                case NO_PIECE: printf(" "); break;
            }
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

static const struct State whiteTest = {
    .board = {
        WHITE|ROOK, WHITE|KNIGHT, WHITE|BISHOP, WHITE|QUEEN, WHITE|KING, WHITE|BISHOP, WHITE|KNIGHT, WHITE|ROOK, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
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

    // Move piece
    // Also record whether a piece has been moved before (for castling)
    suc->board[m->dest] = s->board[m->orig] | PIECE_MOVED;
    suc->board[m->orig] = 0;

    // Update move number and information on last move
    suc->ply++;
    suc->last = s;
    memcpy(&suc->lastMove, m, sizeof(struct Move));
    return &s->succ[(s->nSucc)++];
}

// Adds states to successor if the piece can move there.
static struct State* move_piece(struct State* s, struct Move* m) {
    m->pieceCaptured = 0;
    m->valid = 0;

    if (!is_on_board(m->dest)) {
        return NULL;
    }
    uint8_t tgt = s->board[m->dest];
    if (IS_VACANT(tgt) || ((BLACK_TO_MOVE(s) && IS_WHITE(tgt)) || (WHITE_TO_MOVE(s) && IS_BLACK(tgt)))) {
        // Move piece to either a vacant square or capture.
        struct State* succ = add_result(s, m);
        m->pieceCaptured = !IS_VACANT(s->board[m->dest]);
        m->valid = 1;
        return succ;
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

void get_legal_moves(struct State* s) {
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
                if ((BLACK_TO_MOVE(s) && r == 3 && IS_VACANT(tgt) && IS_WHITE(adjacent) && PIECE(adjacent) == PAWN && IS_PAWN_TWO_STEP(adjacent)) ||
                        (WHITE_TO_MOVE(s) && r == 4 && IS_VACANT(tgt) && IS_BLACK(adjacent) && PIECE(adjacent) == PAWN && IS_PAWN_TWO_STEP(adjacent))) {
                    succ = move_piece(s, &m);
                    // Additionally, remove en passant-ed piece
                    if (succ != NULL)
                        succ->board[adjacent] = NO_PIECE;
                }
            }
        }
    }
    // TODO Remove entries that are in check
}

int main() {
    struct State s0;
    memcpy(&s0, &whiteTest, sizeof(struct State));
    //memcpy(&s0, &initialState, sizeof(struct State));
    print_state(&s0);
    get_legal_moves(&s0);
    for (uint8_t i = 0; i < s0.nSucc; i++) {
        print_state(&s0.succ[i]);
    }
    printf("%u Legal moves\n", s0.nSucc);
    return 0;
}
