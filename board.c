#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <err.h>
#include <time.h>
#include "board.h"

// Enumeration of roles
static const char roleSyms[7] = {' ', 'p', 'R', 'N', 'B', 'Q', 'K'};

// ===========================================================================
// Piece movement patterns
// ===========================================================================
// ROOK: 0-3, BISHOP: 4-7, QUEEN and KING: 0-7;
static const int8_t slideDirns[8] = {LEFT, RIGHT, UP, DOWN, UP_LEFT, UP_RIGHT, DOWN_LEFT, DOWN_RIGHT};
static const int8_t knightDirns[8] = {UP+UP_LEFT, UP+UP_RIGHT, RIGHT+UP_RIGHT, RIGHT+DOWN_RIGHT, DOWN+DOWN_LEFT, DOWN+DOWN_RIGHT, LEFT+DOWN_LEFT, LEFT+UP_LEFT};
// PAWN: Forward: 0, Two-step: 1, Capture: 2-3, En passant: 4-5 (corresponding to 2-3, respectively)
static const int8_t pawnDirnsBlack[6] = {DOWN, DOWN+DOWN, DOWN_LEFT, DOWN_RIGHT, LEFT, RIGHT};
static const int8_t pawnDirnsWhite[6] = {UP, UP+UP, UP_LEFT, UP_RIGHT, LEFT, RIGHT};
// CASTLING: Relative Rook positons: 0-1, Rook targets / Direction of King movement: 2-3, King targets: 4-5
// In each pair, the Queenside is listed first.
static const int8_t castlingSquares[6] = {4*LEFT, 3*RIGHT, LEFT, RIGHT, 2*LEFT, 2*RIGHT};
// PROMOTION: Promotable roles
static const int8_t promotableRoles[4] = {BISHOP, KNIGHT, ROOK, QUEEN};

static const struct State initialState = {
    .board = {
        WHITE|ROOK, WHITE|KNIGHT, WHITE|BISHOP, WHITE|QUEEN, WHITE|KING, WHITE|BISHOP, WHITE|KNIGHT, WHITE|ROOK,    0, 0, 0, 0, 0, 0, 0, 0,
        WHITE|PAWN, WHITE|PAWN, WHITE|PAWN, WHITE|PAWN, WHITE|PAWN, WHITE|PAWN, WHITE|PAWN, WHITE|PAWN,    0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
        BLACK|PAWN, BLACK|PAWN, BLACK|PAWN, BLACK|PAWN, BLACK|PAWN, BLACK|PAWN, BLACK|PAWN, BLACK|PAWN,     0, 0, 0, 0, 0, 0, 0, 0,
        BLACK|ROOK, BLACK|KNIGHT, BLACK|BISHOP, BLACK|QUEEN, BLACK|KING, BLACK|BISHOP, BLACK|KNIGHT, BLACK|ROOK,    0, 0, 0, 0, 0, 0, 0, 0,
    }
};

#ifdef DEBUG
// ===========================================================================
// Performance tests (perft)
// https://www.chessprogramming.org/Perft_Results
// ===========================================================================
static const struct State perft2 = {
    .board = {
        WHITE|ROOK, 0, 0, 0, WHITE|KING, 0, 0, WHITE|ROOK,    0, 0, 0, 0, 0, 0, 0, 0,
        WHITE|PAWN, WHITE|PAWN, WHITE|PAWN, WHITE|BISHOP, WHITE|BISHOP, WHITE|PAWN, WHITE|PAWN, WHITE|PAWN,    0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, WHITE|KNIGHT, 0, 0, WHITE|QUEEN, 0, BLACK|PAWN,    0, 0, 0, 0, 0, 0, 0, 0,
        0, BLACK|PAWN, 0, 0, WHITE|PAWN, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, WHITE|PAWN, WHITE|KNIGHT, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
        BLACK|BISHOP, BLACK|KNIGHT, 0, 0, BLACK|PAWN, BLACK|KNIGHT, BLACK|PAWN, 0,    0, 0, 0, 0, 0, 0, 0, 0,
        BLACK|PAWN, 0, BLACK|PAWN, BLACK|PAWN, BLACK|QUEEN, BLACK|PAWN, BLACK|BISHOP, 0,    0, 0, 0, 0, 0, 0, 0, 0,
        BLACK|ROOK, 0, 0, 0, BLACK|KING, 0, 0, BLACK|ROOK,     0, 0, 0, 0, 0, 0, 0, 0,
    }
};

static const struct State perft3 = {
    .board = {
        0, 0, 0, 0, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, WHITE|PAWN, 0, WHITE|PAWN, 0,    0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
        0, WHITE|ROOK, 0, 0, 0, BLACK|PAWN, 0, BLACK|KING|PIECE_MOVED,    0, 0, 0, 0, 0, 0, 0, 0,
        WHITE|KING|PIECE_MOVED, WHITE|PAWN, 0, 0, 0, 0, 0, BLACK|ROOK,    0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, BLACK|PAWN, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, BLACK|PAWN, 0, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
    }
};

static const struct State perft5 = {
    .board = {
        WHITE|ROOK, WHITE|KNIGHT, WHITE|BISHOP, WHITE|QUEEN, WHITE|KING, 0, 0, WHITE|ROOK,    0, 0, 0, 0, 0, 0, 0, 0,
        WHITE|PAWN, WHITE|PAWN, WHITE|PAWN, 0, WHITE|KNIGHT, BLACK|KNIGHT, WHITE|PAWN, WHITE|PAWN,    0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, WHITE|BISHOP, 0, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, BLACK|PAWN, 0, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
        BLACK|PAWN, BLACK|PAWN, 0, WHITE|PAWN, BLACK|BISHOP, BLACK|PAWN, BLACK|PAWN, BLACK|PAWN,     0, 0, 0, 0, 0, 0, 0, 0,
        BLACK|ROOK, BLACK|KNIGHT, BLACK|BISHOP, BLACK|QUEEN, 0, BLACK|KING|PIECE_MOVED, 0, BLACK|ROOK,    0, 0, 0, 0, 0, 0, 0, 0,
    }
};

static const struct State promo1 = {
    .board = {
        WHITE|KING, 0, 0, 0, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, WHITE|PAWN, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
        BLACK|KING, 0, 0, 0, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
    }
};

static const struct State promo2 = {
    .board = {
        WHITE|KING, 0, 0, 0, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, WHITE|PAWN, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
        BLACK|KING, 0, 0, 0, BLACK|ROOK, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
    }
};

static const struct State promo3 = {
    .board = {
        WHITE|KING, 0, 0, 0, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, WHITE|PAWN, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
        BLACK|KING, 0, 0, BLACK|ROOK, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
    }
};
static const struct State promo4 = {
    .board = {
        WHITE|KING, 0, 0, 0, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, BLACK|PAWN, WHITE|PAWN, BLACK|PAWN, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
        BLACK|KING, 0, 0, 0, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
    }
};
#endif // DEBUG

// ===========================================================================
// Static function declarations
// ===========================================================================
static uint8_t is_in_check(const struct State* s);

// Returns result in algebra as five characters + \0
static void move_to_algebra(const struct Move* m, char algebra[6]) {
    if (m == NULL || !(m->role)) {
        snprintf(algebra, 6, "none");
    } else {
        uint8_t i = 0;
        char role = roleSyms[m->role];
        if (role != 'p') {
            algebra[i] = role;
            i++;
        }
        from_0x88_to_coord(m->orig, algebra + i);
        i += 2;
        from_0x88_to_coord(m->dest, algebra + i);
        i += 2;
        if (role == 'p' && m->promoRole) {
            algebra[i] = roleSyms[m->promoRole];
            i++;
        }
        algebra[i] = 0;
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
            uint8_t sq = s->board[to_0x88(r, f)];
            // Determine the piece's colour
            if (IS_BLACK(sq))
                printf("\033[31m");
            else
                printf("\033[39m");
            // Print out the piece
            char sym = roleSyms[ROLE(sq)];
            printf(" %c ", sym);
            printf("\033[39m");
        }
        printf("\033[49m");
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

// Allocates memory for a successor state
static struct State* add_result(struct State* s) {
    if (s->succ == NULL) {
        s->cSucc = 48;
        s->succ = malloc(s->cSucc * sizeof(struct State));
    } else if (s->nSucc == s->cSucc) {
        s->cSucc += 16;
        s->succ = realloc(s->succ, s->cSucc * sizeof(struct State));
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
    if (IS_VACANT(tgt) || (BLACK_TO_MOVE(s) && IS_WHITE(tgt)) || (WHITE_TO_MOVE(s) && IS_BLACK(tgt))) {
        // Get struct for new state.
        struct State* suc = add_result(s);

        // Move piece to either a vacant square or capture.
        // Also record whether a piece has been moved before (for castling)
        suc->board[m->dest] = s->board[m->orig] | PIECE_MOVED;
        suc->board[m->orig] = 0;

        // Update move number and information on last move
        m->role = ROLE(s->board[m->orig]);
        m->pieceCaptured = !IS_VACANT(s->board[m->dest]);
        m->valid = 1;
        memcpy(&suc->lastMove, m, sizeof(struct Move));

        suc->ply++;
        suc->last = s;
        suc->succ = NULL;
        suc->cSucc = 0;
        suc->nSucc = 0;
        suc->castlesExpanded = 0;
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
// m is already populated with the origin and destination squares.
// If eligible, it will be copied for each role and returned in result.
static void move_pawn_and_check_promotion(struct State* s, struct Move *m) {
    uint8_t destRank, destFile;
    from_0x88(m->dest, &destRank, &destFile);
    m->promoRole = 0;
    if ((BLACK_TO_MOVE(s) && destRank == 0) || (WHITE_TO_MOVE(s) && destRank == 7)) {
        // This pawn can be promoted!
        // There should be a successor state for each role.
        for (uint8_t i = 0; i < 4; i++) {
            m->promoRole = promotableRoles[i];
            struct State* succ = move_piece(s, m);
            succ->board[m->dest] &= ~0x07;
            succ->board[m->dest] |= promotableRoles[i];
        }
    } else {
        m->promoRole = 0;
        move_piece(s, m);
    }
}

// Recursively cleans up successor states, ignoring the state dontfree (if not NULL)
static void clean_up_successors(struct State* s, const struct State* dontfree) {
    if (s->succ) {
        for (uint8_t i = 0; i < s->nSucc; i++) {
            if (&s->succ[i] != dontfree)
                clean_up_successors(&s->succ[i], dontfree);
        }
        free(s->succ);
        s->succ = NULL;
        s->cSucc = 0;
        s->nSucc = 0;
        s->castlesExpanded = 0;
        s->checksRemoved = 0;
    }
}

static void get_moves(struct State* s, uint8_t expandCastles) {
    struct State* succ;
    uint8_t firstCall = (s->nSucc == 0); // First call of this function for this state.

    for (int8_t r = 0; r < 8; r++)
    for (int8_t f = 0; f < 8; f++) {
        uint8_t orig = to_0x88(r, f);
        // CHECK colour, square not empty
        uint8_t piece = s->board[orig];
        if (!piece) continue;
        if ((BLACK_TO_MOVE(s) && IS_WHITE(piece)) || (WHITE_TO_MOVE(s) && IS_BLACK(piece))) continue;

        if (firstCall) {
            // ROOK
            if (ROLE(piece) == ROOK) {
                for (int8_t dirn = 0; dirn <= 3; dirn++) {
                    slide_piece(s, orig, slideDirns[dirn], 0);
                }
            }
            // BISHOP
            if (ROLE(piece) == BISHOP) {
                for (int8_t dirn = 4; dirn <= 7; dirn++) {
                    slide_piece(s, orig, slideDirns[dirn], 0);
                }
            }
            // QUEEN
            if (ROLE(piece) == QUEEN) {
                for (int8_t dirn = 0; dirn <= 7; dirn++) {
                    slide_piece(s, orig, slideDirns[dirn], 0);
                }
            }
            // KING
            if (ROLE(piece) == KING) {
                for (int8_t dirn = 0; dirn <= 7; dirn++) {
                    slide_piece(s, orig, slideDirns[dirn], 1);
                }
            }
            // KNIGHT
            if (ROLE(piece) == KNIGHT) {
                for (int8_t dirn = 0; dirn < 8; dirn++) {
                    uint8_t dest = orig + knightDirns[dirn];
                    struct Move m = {.orig = orig, .dest = dest};
                    move_piece(s, &m);
                }
            }
            // PAWN
            if (ROLE(piece) == PAWN) {
                // Clear the two-step flag from the last move
                if ((BLACK_TO_MOVE(s) && IS_BLACK(piece)) || (WHITE_TO_MOVE(s) && IS_WHITE(piece))) {
                    s->board[orig] &= ~PAWN_TWO_STEP;
                }

                struct Move m;
                m.orig = orig;
                const int8_t* pawnDirns = BLACK_TO_MOVE(s) ? pawnDirnsBlack : pawnDirnsWhite;

                // Square in front is clear: move forward one or two ranks.
                m.dest = orig + pawnDirns[0];
                if (is_on_board(m.dest) && IS_VACANT(s->board[m.dest])) {
                    move_pawn_and_check_promotion(s, &m);

                    // Two-step
                    m.dest = orig + pawnDirns[1];
                    if (is_on_board(m.dest) && IS_VACANT(s->board[m.dest]))
                    if ((BLACK_TO_MOVE(s) && r == 6) || (WHITE_TO_MOVE(s) && r == 1)) {
                        succ = move_piece(s, &m);
                        succ->board[m.dest] |= PAWN_TWO_STEP; // For en passant
                    }
                }

                for (uint8_t i = 2; i <= 3; i++) {
                    // Capture: Square along diagonal contains a piece of opposite colour.
                    m.dest = orig + pawnDirns[i];
                    uint8_t tgt = s->board[m.dest];
                    if ((BLACK_TO_MOVE(s) && IS_WHITE(tgt)) || (WHITE_TO_MOVE(s) && IS_BLACK(tgt))) {
                        move_pawn_and_check_promotion(s, &m);
                    }

                    // En passant: Check rank and pieces beside and clear destination.
                    // m.dest is the same value used for normal capture above.
                    uint8_t adjacent = s->board[orig + pawnDirns[i + 2]];
                    if ((BLACK_TO_MOVE(s) && r == 3 && IS_WHITE(adjacent)) || (WHITE_TO_MOVE(s) && r == 4 && IS_BLACK(adjacent)))
                    if (IS_VACANT(tgt) && ROLE(adjacent) == PAWN && IS_PAWN_TWO_STEP(adjacent)) {
                        succ = move_piece(s, &m);
                        // Additionally, remove en passant-ed piece
                        if (succ != NULL)
                            succ->board[orig + pawnDirns[i + 2]] = 0;
                    }
                }
            }
        }
        if (expandCastles && !s->castlesExpanded) {
            // CASTLING
            if (ROLE(piece) == KING) {
                // Ensure King has not moved and not in check, then check each side
                if(!IS_PIECE_MOVED(piece) && !is_in_check(s))
                for (uint8_t side = 0; side < 2; side++) { // 0: Queenside, 1: Kingside
                    uint8_t cornerPiece = s->board[orig + castlingSquares[side + 0]];
                    if ((BLACK_TO_MOVE(s) && IS_BLACK(cornerPiece)) || (WHITE_TO_MOVE(s) && IS_WHITE(cornerPiece)))
                    if ((ROLE(cornerPiece) == ROOK) && !IS_PIECE_MOVED(cornerPiece)) {
                        // There are no pieces in between. Check squares starting from King, working over to Rook.
                        uint8_t obstruction = 0;
                        for (uint8_t checkPosn = orig + castlingSquares[side + 2]; checkPosn != orig + castlingSquares[side + 0];
                                checkPosn += castlingSquares[side + 2]) {
                            if (ROLE(s->board[checkPosn]) != NO_ROLE) {
                                obstruction = 1;
                                break;
                            }
                        }
                        if (!obstruction) {
                            // The King does not pass through a square that is in check. Check by temporarily moving the King.
                            uint8_t tempPiece = s->board[orig];
                            s->board[orig + castlingSquares[side + 2]] = tempPiece;
                            s->board[orig] = 0;
                            uint8_t danger = is_in_check(s);
                            s->board[orig + castlingSquares[side + 2]] = 0;
                            s->board[orig] = tempPiece;
                            if (!danger) {
                                // Move the King and Rook to Castle.
                                struct Move m = {.orig = orig, .dest = orig + castlingSquares[side + 4]};
                                struct State* succ = move_piece(s, &m);
                                if (succ != NULL) {
                                    succ->board[orig + castlingSquares[side + 2]] = succ->board[orig + castlingSquares[side + 0]];
                                    succ->board[orig + castlingSquares[side + 0]] = 0;
                                }
                            }
                        }
                    }
                }
                s->castlesExpanded = 1;
            }
        }
    }
}

static uint8_t king_captured(const struct State* s) {
    for (uint8_t i = 0; i < 128; i++) {
        if (ROLE(s->board[i]) == KING)
        if ((WHITE_TO_MOVE(s) && IS_WHITE(s->board[i])) || (BLACK_TO_MOVE(s) && IS_BLACK(s->board[i]))) {
            return 0;
        }
    }
    return 1;
}

// Return whether this successor state (su, opponent to move) leaves the King in check.
// That is, whether the opponent has a move that can capture the King.
static uint8_t is_in_check_su(struct State* su) {
    uint8_t result = 0;
    get_moves(su, 0);
    for (uint8_t i = 0; i < su->nSucc; i++) {
        // See if for s' there exists a s'' that has the King captured
        if (king_captured(&su->succ[i])) {
            result = 1;
        }
    }
    clean_up_successors(su, NULL);
    return result;
}

// Return whether the player to move is in check
static uint8_t is_in_check(const struct State* s) {
    // Another state where the player has 'passed' their move
    struct State su;
    memcpy(&su, s, sizeof(struct State));
    bzero(&su.lastMove, sizeof(struct Move));
    su.succ = NULL; // Clear successor states
    su.cSucc = 0;
    su.nSucc = 0;
    su.castlesExpanded = 0;
    su.checksRemoved = 0;
    su.ply++;

    return is_in_check_su(&su);
}

static void remove_check(struct State* s) {
    if (s->checksRemoved) return;
    if (s->succ == NULL) return;
    for (int8_t i = 0; i < s->nSucc; i++) {
        if (is_in_check_su(&s->succ[i])) {
            // Take element from end of array and replace here.
            memcpy(&s->succ[i], &s->succ[s->nSucc - 1], sizeof(struct State));
            s->nSucc--;
            i--; // This replacing element needs to be checked again
        }
    }
    s->checksRemoved = 1;
}

void get_legal_moves(struct State* s) {
    get_moves(s, 1);
    remove_check(s);
    if (is_in_check(s))
        s->check = 1;
}

static void save_game(const struct State* s, const char* gamefn) {
    int gamef;

    gamef = open(gamefn, O_CREAT | O_WRONLY, 0666);
    if (gamef < 0)
        warn("open(): Error saving game");
    if (write(gamef, s, sizeof(struct State)) < 0)
        warn("write(): Error saving game");
    close(gamef);
}

static void load_game(struct State* s, const char* gamefn) {
    int gamef;

    gamef = open(gamefn, O_RDONLY, 0666);
    if (gamef < 0)
        warn("open(): Error loading game");
    if (read(gamef, s, sizeof(struct State)) < 0)
        warn("read(): Error loading game");
    close(gamef);

    // Clear references to successor states
    s->castlesExpanded = 0;
    s->checksRemoved = 0;
    s->check = 0;
    s->last = NULL;
    s->succ = NULL;
    s->cSucc = 0;
    s->nSucc = 0;
}

static void autosave_game(const struct State* s) {
    char gamefn[80];
    mkdir("history", 0777);
    snprintf(gamefn, 80, "history/move%d.game", s->ply);
    save_game(s, gamefn);
}

#ifdef DEBUG
static uint64_t count_succ_recurse(struct State* s, int depth, uint8_t root) {
    get_legal_moves(s);

    if (depth == 0)
        return s->nSucc;

    uint64_t total = 0;
    for (uint8_t i = 0; i < s->nSucc; i++) {
        uint64_t nSucc = count_succ_recurse(&s->succ[i], depth - 1, 0);
        if (root) {
            char move[6];
            move_to_algebra(&s->succ[i].lastMove, move);
            printf("%s: %ld\n", move, nSucc);
        }
        total += nSucc;
        clean_up_successors(&s->succ[i], NULL); // Free memory as we go
    }
    return total;
}
#endif // DEBUG

int main() {
    struct State s;
    memcpy(&s, &initialState, sizeof(struct State));
    // Prompt loop
    for (;;) {
        print_state(&s);
        get_legal_moves(&s);

        // Stalemate? Checkmate?
        if (s.nSucc == 0) {
            if (s.check)
                printf("CHECKMATE\n");
            else
                printf("STALEMATE\n");
            break;
        }

        // Check?
        if (s.check)
            printf("CHECK\n");
        
        // Print legal moves
        for (uint8_t i = 0; i < s.nSucc; i++) {
            char alge[6];
            move_to_algebra(&s.succ[i].lastMove, alge);
            printf("%-5s", alge);
            printf("    ");
            if (((i + 1) % 6) == 0) {
                printf("\n");
            }
        }

        uint8_t cmdValid = 0;
        char buf[80];
        bzero(buf, 80);
        printf("\n\nPlease enter a move, or type a time in seconds to search > ");
        fflush(stdout);
        char* e = fgets(buf, 80, stdin);
        if (e == NULL) break; // Error or end of input
        size_t z = strnlen(buf, 80);
        buf[z - 1] = 0; // Strip trailing newline

        // Makes a move
        // Check that the move is legal, the execute it
        for (uint8_t i = 0; i < s.nSucc; i++) {
            char alge[6];
            move_to_algebra(&s.succ[i].lastMove, alge);
            if (strncasecmp(buf, alge, 6) == 0) {
                // Overwrite current state and save it.
                struct State succ;
                memcpy(&succ, &s.succ[i], sizeof(struct State));
                clean_up_successors(&s, &s.succ[i]);
                memcpy(&s, &succ, sizeof(struct State));

                autosave_game(&s);
                cmdValid = 1;
                break;
            }
        }

        // TODO
        // Manual game save
        // Manual game load

        // Perform MCTS

#ifdef DEBUG
        // Load debug state
        const struct State* ds = NULL;
        if (strncasecmp(buf, "load perft2", 80) == 0)
            ds = &perft2;
        else if (strncasecmp(buf, "load perft3", 80) == 0)
            ds = &perft3;
        else if (strncasecmp(buf, "load perft5", 80) == 0)
            ds = &perft5;
        else if (strncasecmp(buf, "load promo1", 80) == 0)
            ds = &promo1;
        else if (strncasecmp(buf, "load promo2", 80) == 0)
            ds = &promo2;
        else if (strncasecmp(buf, "load promo3", 80) == 0)
            ds = &promo3;
        else if (strncasecmp(buf, "load promo4", 80) == 0)
            ds = &promo4;

        if (ds) {
            clean_up_successors(&s, NULL);
            memcpy(&s, ds, sizeof(struct State));
            cmdValid = 1;
        }

        // Get state tree size to specified depth
        int depth;
        int nparam = sscanf(buf, "perft %d", &depth);
        if (nparam == 1) {
            time_t start = time(NULL);
            printf("Number of successors (recursive): %ld\n", count_succ_recurse(&s, depth - 1, 1));
            time_t finish = time(NULL);
            printf("Time taken: %ld seconds\n", finish - start);
            cmdValid = 1;
        }
#endif // DEBUG

        if (!cmdValid)
            printf("Invalid command, try again.");

        printf("\n");
    }
    clean_up_successors(&s, NULL);

    return 0;
}
