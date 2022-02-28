#ifndef BOARD_H
#define BOARD_H

#include <stdint.h>

// ===========================================================================
// Piece Representation
// ===========================================================================
// Bits 0-2: Roles
#define ROLE(x) ((x) & 0x07)
#define NO_ROLE (0)
#define PAWN (1)
#define ROOK (2)
#define KNIGHT (3)
#define BISHOP (4)
#define QUEEN (5)
#define KING (6)
#define IS_VACANT(x) (!ROLE((x)))

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

// Coordinate conversion between 0x88 and algebraic coordinates.
// Returns 0xFF if invalid.
uint8_t coord_to_0x88(char coord[2]);
uint8_t from_0x88_to_coord(uint8_t pos, char coord[2]);

// Position relationships
#define LEFT        (-0x01)
#define RIGHT       (+0x01)
#define UP          (+0x10)
#define DOWN        (-0x10)
#define UP_LEFT     (+0x0F)
#define UP_RIGHT    (+0x11)
#define DOWN_LEFT   (-0x11)
#define DOWN_RIGHT  (-0x0F)

// ===========================================================================
// Game states
// Two states are considered equal if (board) is equal and (ply) is both odd or both even
// ===========================================================================

// Move notation (in 0x88 notation)
struct Move {
    uint8_t orig, dest;
    uint8_t role; // Piece that was moved, only bottom three bits for the role.
    uint8_t valid; // Either a piece was captured or the position is in bounds.
    uint8_t pieceCaptured;
};

struct State {
    // Position and state of each piece on board.
    uint8_t board[128];
    // Current ply (half-move), starting at zero.
    // Even means white to move, Odd means black to move.
    uint8_t ply;

    // Progress of successor state generation.
    // First we generate everything, except castles.
    // We then generate Castles separately, as they require is_in_check() to be recursively called.
    // Finally, we remove the moves that would cause the King to be in Check.
    uint8_t castlesExpanded, checksRemoved;
    // Whether the player to move is in check.
    // Combined with nSucc = 0 means either stalemate (0) or checkmate (1)
    // Only valid when castlesExpanded is true and is_in_check() is called.
    uint8_t check;
    
    // Previous and next states
    struct Move lastMove;
    const struct State* last;
    struct State* succ; // Dynamic array
    uint8_t cSucc, nSucc; // Array capacity and size

    // For MCTS: Number of times each side won
    uint64_t winsB, winsW;
};

#define BLACK_TO_MOVE(s) ((s)->ply % 2)
#define WHITE_TO_MOVE(s) (!BLACK_TO_MOVE((s)))

void print_move(const struct Move* m);
void print_state(const struct State* s);

// ===========================================================================
// Legal moves
// ===========================================================================
// Populates s->succ with successor states as a result of legal moves.
// Turn off recursion when querying for check.
void get_legal_moves(struct State* s);

#endif // BOARD_H
