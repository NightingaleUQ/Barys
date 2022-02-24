#ifndef BOARD_H
#define BOARD_H

#include <stdint.h>

// ===========================================================================
// Piece Representation
// ===========================================================================
// Bits 0-2: Pieces
#define PIECE(x) ((x) & 0x07)
#define NO_PIECE (0)
#define PAWN (1)
#define ROOK (2)
#define KNIGHT (3)
#define BISHOP (4)
#define QUEEN (5)
#define KING (6)
#define IS_VACANT(x) (!PIECE((x)))

// Bits 3-4: Unused

// Bit 5: For pawns: Whether its first move was a two-step.
#define PAWN_TWO_STEP (0x20)
#define IS_PAWN_TWO_STEP(x) ((x) & PAWN_TWO_STEP)

// Bit 6: For Rooks and Kings. Whether the piece has been moved before.
// NOTE: Promoted rooks need to have this set.
#define PIECE_MOVED (0x40)
#define IS_PIECE_MOVED(x) ((x) & PIECE_MOVED)

// Bit 7: Black pieces have bit 7 set. White pieces have bit 7 unset.
#define BLACK (0x80)
#define WHITE (0x00)
#define IS_BLACK(x) ((!IS_VACANT((x)) && ((x) & BLACK)))
#define IS_WHITE(x) ((!IS_VACANT((x)) && (!IS_BLACK((x)))))

// ===========================================================================
// 0x88 Coordinate system
// ===========================================================================
uint8_t is_on_board(uint8_t pos);

// Coordinate conversion between rank-and-file (zero-indexed) and 0x88 representation
uint8_t to_0x88(uint8_t rank, uint8_t file);
void from_0x88(uint8_t pos, uint8_t* rank, uint8_t* file);

// Position relationships
#define LEFT        (-0x01)
#define RIGHT       (+0x01)
#define UP          (+0x10)
#define DOWN        (-0x10)
#define UP_LEFT     (+0x0F)
#define UP_RIGHT    (+0x11)
#define DOWN_LEFT   (-0x11)
#define DOWN_RIGHT  (-0x0F)

static const int8_t moveDirns[8];
#define ROOK_DIRN_START     0
#define ROOK_DIRN_END       3
#define BISHOP_DIRN_START   4
#define BISHOP_DIRN_END     7
#define ROYAL_DIRN_START    0
#define ROYAL_DIRN_END      7

// ===========================================================================
// Game states
// Two states are considered equal if (board) is equal and (ply) is both odd or both even
// ===========================================================================

// Move notation (in 0x88 notation)
struct Move {
    uint8_t piece;
    uint8_t start;
    uint8_t finish;
};

struct State {
    // Position and state of each piece on board.
    uint8_t board[128];
    // Current ply (half-move), starting at zero.
    // Even means white to move, Odd means black to move.
    uint8_t ply;
    
    // NOT USED FOR STATE EQUALITY CHECKING
    const struct State* lastState;
    // struct State nextStates[50]; // TODO Dynamic array
};

#define BLACK_TO_MOVE(s) ((s)->ply % 2)
#define WHITE_TO_MOVE(s) (!BLACK_TO_MOVE((s)))

// Returns 0 if the state in invalid.
void print_state(const struct State* s);

// ===========================================================================
// Legal moves
// ===========================================================================
// Returns the next state from a given position. Limited to size of results array, specified by n.
uint8_t get_legal_moves(const struct State* s, struct State* results, uint8_t maxResults);

#endif // BOARD_H
