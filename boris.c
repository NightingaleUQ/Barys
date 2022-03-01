#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <time.h>
#include <pthread.h>

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
static struct State* selection(struct State* s) {
    // Ensure all successors have been simulated
    get_legal_moves(s);
    for (uint8_t i = 0; i < s->nSucc; i++) {
        if (GAMES_PLAYED(&s->succ[i]) == 0)
            return &s->succ[i];
    }

    // Pick a successor to recurse.
    // Whatever move has the best advantage for the person to play.
    struct State* selected = NULL;
    double umax = -INFINITY;
    uint64_t n = GAMES_PLAYED(s);

    for (uint8_t i = 0; i < s->nSucc; i++) {
        int64_t wins = BLACK_TO_MOVE(&s->succ[i]) ? s->succ[i].winsB : s->succ[i].winsW;
        int64_t losses = BLACK_TO_MOVE(&s->succ[i]) ? s->succ[i].winsW : s->succ[i].winsB;
        double exploit = (double)(wins - losses) / (double)GAMES_PLAYED(&s->succ[i]);
        double pieceAdvantage = black_advantage(s) * 0.1; // Small influence
        if (WHITE_TO_MOVE(s))
            pieceAdvantage *= -1;
        double c = 0.5;
        double explore = c * (sqrt(log(n) / GAMES_PLAYED(&s->succ[i])));
        double ucb = pieceAdvantage + exploit + explore;
        if (ucb > umax) {
            selected = &s->succ[i];
            umax = ucb;
        }
    }
#ifdef DEBUG
    if (selected == NULL)
        fprintf(stderr, "selection(): No node selected\n");
#endif // DEBUG
    return selection(selected);
}

// Make random moves until someone wins.
static void* playout(void* args) {
    // Unpack arguments
    struct State* init = (struct State*)args;

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
            // FIXME use random_r()
            struct State* succ = &s->succ[random() % s->nSucc];
            s = succ;
        }
    }
    // The game didn't finish within the move limit.
    if (!finished) {
        init->draws++;
    }

    clean_up_successors(init, NULL);
    return NULL;
}

static void mcts_iter(struct State* s) {
    // SELECTION: Using upper-confidence bound
    struct State* selected = selection(s);

    // SIMULATION
    // Multithreaded playout, with time limit
    int nthreads = 12; // Ask user TODO
    struct State* instance = malloc(nthreads * sizeof(struct State));
    pthread_t* threads = malloc(nthreads * sizeof(pthread_t));
    for (int t = 0; t < nthreads; t++) {
        // Hopefully this will prevent us from trashing memory
        memcpy(&instance[t], selected, sizeof(struct State));
        instance[t].last = NULL;
        instance[t].succ = NULL;
        instance[t].cSucc = 0;
        instance[t].nSucc = 0;
        instance[t].winsB = 0;
        instance[t].winsW = 0;
        instance[t].draws = 0;
        pthread_create(&threads[t], NULL, playout, &instance[t]);
    }
    for (int t = 0; t < nthreads; t++) {
        pthread_join(threads[t], NULL);
    }
    // Collect results
    for (int t = 0; t < nthreads; t++) {
        selected->winsB += instance[t].winsB;
        selected->winsW += instance[t].winsW;
        selected->draws += instance[t].draws;
        // printf("BWD: %ld %ld %ld\n", instance[i].winsB, instance[i].winsW, instance[i].draws);
    }
    free(instance);
    free(threads);
    // printf("BWD: %ld %ld %ld\n", selected->winsB, selected->winsW, selected->draws);

    // BACKPROPROGATION
    struct State* cur = selected;
    for (;;) {
        cur = cur->last;
        cur->winsB += selected->winsB;
        cur->winsW += selected->winsW;
        cur->draws += selected->draws;
        if (cur == s)
            break;
    }
}

struct MCTS_args {
    struct State* s;
    int* searching;
};

static void* mcts(void* args) {
    struct State* s = ((struct MCTS_args*)args)->s;
    int* searching = ((struct MCTS_args*)args)->searching;
    
    while (*searching)
        mcts_iter(s);

    return NULL;
}

int main() {
    struct State s;
    memcpy(&s, &initialState, sizeof(struct State));

    int searchRunning = 0;

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
        
        // Print legal moves with advantages for current player
        struct State* best = NULL;
        double bestAdv = -INFINITY;
        for (uint8_t i = 0; i < s.nSucc; i++) {
            if (i % 4 == 0)
                printf("\n");
            int64_t wins = BLACK_TO_MOVE(&s.succ[i]) ? s.succ[i].winsB : s.succ[i].winsW;
            int64_t losses = BLACK_TO_MOVE(&s.succ[i]) ? s.succ[i].winsW : s.succ[i].winsB;
            double advantage = (double)(wins - losses) / (double)GAMES_PLAYED(&s.succ[i]);
            char moveAdv[80];
            snprintf(moveAdv, 80, "%-5s: %-6.3f (%ld %ld %ld)", s.succ[i].lastMove.algebra,
                    advantage, wins, losses, s.succ[i].draws);
            printf("%-35s", moveAdv);
            if (best == NULL || (advantage > bestAdv)) {
                best = &s.succ[i];
                bestAdv = advantage;
            }
        }
        printf("\nMove with best advantage: %s\n", best->lastMove.algebra);

        // PROMPT user
        uint8_t cmdValid = 0;
        char buf[80];
        bzero(buf, 80);
        printf("\n\nPlease enter a move, or type ");
        if (!searchRunning) {
            printf("\"search\"");
        } else {
            printf("\"stop\"");
        }
        printf(" > ");
        
        fflush(stdout);
        char* e = fgets(buf, 80, stdin);
        if (e == NULL) break; // Error or end of input
        size_t z = strnlen(buf, 80);
        buf[z - 1] = 0; // Strip trailing newline

        // Perform MCTS
        pthread_t mctsThread;
        if (strncasecmp(buf, "search", 80) == 0) {
            if (!searchRunning) {
                searchRunning = 1;
                struct MCTS_args args = {.s = &s, .searching = &searchRunning};
                pthread_create(&mctsThread, NULL, mcts, (void*)&args);
                cmdValid = 1;
            } else {
                printf("The search is already running.\n");
            }
        } else if (strncasecmp(buf, "stop", 80) == 0) {
            if (searchRunning) {
                searchRunning = 0;
                pthread_join(mctsThread, NULL);
                printf("Finished simulating %ld games.\n", GAMES_PLAYED(&s));
                cmdValid = 1;
            } else {
                printf("A search is not running.\n");
            }
        }
        
        // Makes a move
        // Check that the move is legal, the execute it
        for (uint8_t i = 0; i < s.nSucc; i++) {
            if (strncasecmp(buf, s.succ[i].lastMove.algebra, 6) == 0) {
                if (!searchRunning) {
                    // Overwrite current state and save it.
                    struct State succ;
                    memcpy(&succ, &s.succ[i], sizeof(struct State));
                    clean_up_successors(&s, &s.succ[i]);
                    memcpy(&s, &succ, sizeof(struct State));

                    // FIXME Clearing successors at every move should not be necessary.
                    // But something is trashing successor state representations.
                    clean_up_successors(&s, NULL);

                    autosave_game(&s);
                    cmdValid = 1;
                    break;
                } else {
                    printf("Please stop the search first.\n");
                }
            }
        }

        // TODO
        // Manual game save
        // Manual game load

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
            printf("Invalid command, try again.\n");

        printf("\n");
    }
    clean_up_successors(&s, NULL);

    return 0;
}
