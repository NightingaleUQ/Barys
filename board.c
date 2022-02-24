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
        WHITE | ROOK, WHITE | KNIGHT, WHITE | BISHOP, WHITE | QUEEN, WHITE | KING, WHITE | BISHOP, WHITE | KNIGHT, WHITE | ROOK, 0, 0, 0, 0, 0, 0, 0, 0,
        WHITE | PAWN, WHITE | PAWN, WHITE | PAWN, WHITE | PAWN, WHITE | PAWN, WHITE | PAWN, WHITE | PAWN, WHITE | PAWN,  0, 0, 0, 0, 0, 0, 0, 0,
        NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE,  0, 0, 0, 0, 0, 0, 0, 0,
        NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE,  0, 0, 0, 0, 0, 0, 0, 0,
        NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE,  0, 0, 0, 0, 0, 0, 0, 0,
        NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE,  0, 0, 0, 0, 0, 0, 0, 0,
        BLACK | PAWN, BLACK | PAWN, BLACK | PAWN, BLACK | PAWN, BLACK | PAWN, BLACK | PAWN, BLACK | PAWN, BLACK | PAWN,  0, 0, 0, 0, 0, 0, 0, 0,
        BLACK | ROOK, BLACK | KNIGHT, BLACK | BISHOP, BLACK | QUEEN, BLACK | KING, BLACK | BISHOP, BLACK | KNIGHT, BLACK | ROOK, 0, 0, 0, 0, 0, 0, 0, 0,
    },
    .ply = 0,
    .lastState = NULL
};

static const struct State whiteTest = {
    .board = {
        WHITE | ROOK, WHITE | KNIGHT, WHITE | BISHOP, WHITE | QUEEN, WHITE | KING, WHITE | BISHOP, WHITE | KNIGHT, WHITE | ROOK, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE,  0, 0, 0, 0, 0, 0, 0, 0,
        NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE,  0, 0, 0, 0, 0, 0, 0, 0,
        NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE,  0, 0, 0, 0, 0, 0, 0, 0,
        NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE,  0, 0, 0, 0, 0, 0, 0, 0,
        BLACK | PAWN, BLACK | PAWN, BLACK | PAWN, BLACK | PAWN, BLACK | PAWN, BLACK | PAWN, BLACK | PAWN, BLACK | PAWN,  0, 0, 0, 0, 0, 0, 0, 0,
        BLACK | ROOK, BLACK | KNIGHT, BLACK | BISHOP, BLACK | QUEEN, BLACK | KING, BLACK | BISHOP, BLACK | KNIGHT, BLACK | ROOK, 0, 0, 0, 0, 0, 0, 0, 0,
    },
    .ply = 0,
    .lastState = NULL
};

static const int8_t moveDirns[8] = {LEFT, RIGHT, UP, DOWN, UP_LEFT, UP_RIGHT, DOWN_LEFT, DOWN_RIGHT};

// Returns non-zero pointer if successful
static struct State* add_result(const struct State* s, struct State* results, uint8_t maxResults, uint8_t* nResults) {
    if (*nResults < maxResults) {
        memcpy(&results[*nResults], s, sizeof(struct State));
        results[*nResults].ply++;
        results[*nResults].lastState = s;
        return &results[(*nResults)++];
    } else {
        return NULL;
    }
}

static void slide_piece(const struct State* s, struct State* results, uint8_t maxResults, uint8_t* nResults, uint8_t pos, int8_t dirn) {
    // TODO King
    uint8_t tgt = pos;
    for(;;) {
        tgt += dirn;
        if (!is_on_board(tgt)) break;
        uint8_t piece = s->board[tgt];
        if (IS_VACANT(piece) || ((BLACK_TO_MOVE(s) && IS_WHITE(piece)) || (WHITE_TO_MOVE(s) && IS_BLACK(piece)))) {
            // Move piece to either a vacant square or capture.
            // Record rook and king movement for castling
            struct State* stateNext = add_result(s, results, maxResults, nResults);
            if (!stateNext) return;
            uint8_t pieceCapture = !IS_VACANT(s->board[tgt]);
            stateNext->board[tgt] = s->board[pos] | PIECE_MOVED;
            stateNext->board[pos] = 0;
            if (pieceCapture) break;
        } else {
            break;
        }
    }
}

uint8_t get_legal_moves(const struct State* s, struct State* results, uint8_t maxResults) {
    uint8_t nResults = 0;
    struct State* stateNext;
    for (int8_t r = 0; r < 8; r++)
    for (int8_t f = 0; f < 8; f++) {
        uint8_t pos = to_0x88(r, f);
        // CHECK COLOUR
        if ((BLACK_TO_MOVE(s) && IS_WHITE(s->board[pos])) || (WHITE_TO_MOVE(s) && IS_BLACK(s->board[pos]))) continue;
        uint8_t piece = PIECE(s->board[pos]);
        // ROOK
        if (piece == ROOK) {
            for (int8_t dirn = ROOK_DIRN_START; dirn <= ROOK_DIRN_END; dirn++) {
                slide_piece(s, results, maxResults, &nResults, pos, moveDirns[dirn]);
            }
        }
        // BISHOP
        if (piece == BISHOP) {
            for (int8_t dirn = BISHOP_DIRN_START; dirn <= BISHOP_DIRN_END; dirn++) {
                slide_piece(s, results, maxResults, &nResults, pos, moveDirns[dirn]);
            }
        }
        // QUEEN
        if (piece == QUEEN) {
            for (int8_t dirn = ROYAL_DIRN_START; dirn <= ROYAL_DIRN_END; dirn++) {
                slide_piece(s, results, maxResults, &nResults, pos, moveDirns[dirn]);
            }
        }
        // TODO King, Knight, Pawn (two-step, promotion), En passant, Castling
    }
    // TODO Remove entries that are in check
    return nResults;
}

int main() {
    print_state(&initialState);

    /*
    print_state(&whiteTest);
    struct State* results = malloc(64 * sizeof(struct State));
    uint8_t nResults = get_legal_moves(&whiteTest, results, 64);
    for (uint8_t i = 0; i < nResults; i++) {
        print_state(&results[i]);
    }
    printf("%u Legal moves\n", nResults);
    */
    return 0;
}
