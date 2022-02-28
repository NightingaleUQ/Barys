#include <stdio.h>
#include <string.h>

#include <time.h>

#include "board.h"

#ifdef DEBUG
static uint64_t count_succ_recurse(struct State* s, int depth, uint8_t root) {
    get_legal_moves(s);

    if (depth == 0)
        return s->nSucc;

    uint64_t total = 0;
    for (uint8_t i = 0; i < s->nSucc; i++) {
        uint64_t nSucc = count_succ_recurse(&s->succ[i], depth - 1, 0);
        if (root) {
            printf("%s: %ld\n", s->succ[i].lastMove.algebra, nSucc);
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
            printf("%-5s", s.succ[i].lastMove.algebra);
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
            if (strncasecmp(buf, s.succ[i].lastMove.algebra, 6) == 0) {
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
        const struct State* ds = load_debug_state(buf);

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
