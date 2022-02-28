#include <math.h>
#include <stdio.h>
#include <stdlib.h>
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

// Returns a descendant state that has yet to be played out.
// If all successors of a state have been played out, recurse.
struct State* selection(struct State* s) {
    // Base case: Not simulated yet.
    if (GAMES_PLAYED(s) == 0)
        return s;

    // Ensure all successors have been simulated
    if (s->succ == NULL)
        get_legal_moves(s);
    for (uint8_t i = 0; i < s->nSucc; i++) {
        if (GAMES_PLAYED(&s->succ[i]) == 0)
            return &s->succ[i];
    }

    // Pick a successor to recurse.
    struct State* selected = NULL;
    double umax = -INFINITY;
    uint64_t n = 0; // Total moves across all successors

    for (uint8_t i = 0; i < s->nSucc; i++) {
        n += GAMES_PLAYED(&s->succ[i]);
    }
    for (uint8_t i = 0; i < s->nSucc; i++) {
        int64_t wins = BLACK_TO_MOVE(&s->succ[i]) ? s->succ[i].winsB : s->succ[i].winsW;
        int64_t losses = BLACK_TO_MOVE(&s->succ[i]) ? s->succ[i].winsW : s->succ[i].winsB;
        double exploit = (double)(wins - losses) / (double)GAMES_PLAYED(&s->succ[i]);
        double c = 0.5;
        double explore = c * (sqrt(log(n) / GAMES_PLAYED(&s->succ[i])));
        double ucb = exploit + explore;
        if (ucb > umax) {
            selected = &s->succ[i];
            umax = ucb;
        }
    }
    return selection(selected);
}

// Make random moves until someone wins.
void playout(struct State* init) {
    struct State* s = init;
    uint8_t finished = 0;

    for (int i = 0; i < 200; i++) {
        get_legal_moves(s);
        if (s->nSucc == 0) {
            // The game has finished. Proprogate game result.
            if (s->check) {
                // Checkmate
                if (BLACK_TO_MOVE(s)) {
                    init->winsW++;
                } else {
                    init->winsB++;
                }
            } else {
                // Stalemate
                init->draws++;
            }
            finished = 1;
            break;
        } else {
            struct State* succ = &s->succ[random() % s->nSucc];
            s = succ;
        }
    }
    // The game didn't finish within the move limit.
    if (!finished)
        init->draws++;

    clean_up_successors(init, NULL);
}

static void mcts_iter(struct State* s) {
    // SELECTION: Using upper-confidence bound
    struct State* selected = selection(s);

    // SIMULATION
    // TODO Multithreaded playout, with time limit
    int nthreads = 1; // Ask user TODO
    struct State* instance = malloc(nthreads * sizeof(struct State));
    for (int t = 0; t < nthreads; t++) {
        memcpy(&instance[t], selected, sizeof(struct State));
        instance[t].succ = NULL;
        instance[t].cSucc = 0;
        instance[t].nSucc = 0;
        for (int i = 0; i < 5; i++)
            playout(&instance[t]);
    }
    // Collect results
    for (int t = 0; t < nthreads; t++) {
        selected->winsB += instance[t].winsB;
        selected->winsW += instance[t].winsW;
        selected->draws += instance[t].draws;
        // printf("BWD: %ld %ld %ld\n", instance[i].winsB, instance[i].winsW, instance[i].draws);
    }
    free(instance);
    // printf("BWD: %ld %ld %ld\n", selected->winsB, selected->winsW, selected->draws);

    // BACKPROPROGATION
    struct State* cur = selected;
    for (;;) {
        if (cur == s)
            break;
        cur->last->winsB += selected->winsB;
        cur->last->winsW += selected->winsW;
        cur->last->draws += selected->draws;
        cur = cur->last;
    }
}

static void mcts(struct State* s, int time) {
    // TODO Time limit
    for (int i = 0; i < 100; i++) {
        mcts_iter(s);
        printf("i = %d\n", i);
    }

    printf("SEARCH RESULTS after %ld simulated games:\n", GAMES_PLAYED(s));
    for (uint8_t i = 0; i < s->nSucc; i++) {
        int64_t wins = BLACK_TO_MOVE(&s->succ[i]) ? s->succ[i].winsB : s->succ[i].winsW;
        int64_t losses = BLACK_TO_MOVE(&s->succ[i]) ? s->succ[i].winsW : s->succ[i].winsB;
        int64_t advantage = wins - losses;
        printf("%-5s: %.3f (%ld %ld %ld)   ", s->succ[i].lastMove.algebra,
                (double)advantage / (double)GAMES_PLAYED(&s->succ[i]), wins, losses, s->succ[i].draws);
        if ((i + 1)% 5 == 0)
            printf("\n");
    }
}

int main() {
    struct State s;
    memcpy(&s, &initialState, sizeof(struct State));
    // Prompt loop
    for (;;) {
        int nparam;

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
        int timelimit;
        nparam = sscanf(buf, "search %d", &timelimit);
        if (nparam == 1) {
            mcts(&s, timelimit);
            cmdValid = 1;
        }

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
        nparam = sscanf(buf, "perft %d", &depth);
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
