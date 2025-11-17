#include "ma.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>

typedef struct output_destinations outList_t;
typedef struct input_origins origin_t;

// Liczy ile uintów trzeba żeby przechować x bitów w size_t razy wielkość uinta
#define SIZEOF_64_UINT(x) (sizeof(uint64_t) * ((x + 63) / 64))

// Liczy ile uintów trzeba, żeby przechować x bitów w size_t
#define CEIL64(x) ((x + 63) / 64)

#define IS_1(x, n) (((x) & (1ULL << (n))) != 0)
#define SET_1(x, n) (x | (1ULL << (n)))
#define SET_0(x, n) (x & ~(1ULL << (n)))
#define COPY(x, y, n, k) (IS_1(y, k) ? SET_1(x, n) : SET_0(x, n)) //ustawia n-ty bit x-a na k-ty bit y-ka

struct moore {
    size_t n,m,s; // liczba wejść, wyjść, stanów
    uint64_t *input, *output, *state, *new_state;
    transition_function_t transition;
    output_function_t output_function;
    outList_t *head; // wskaźnik na początek listy podłączeń
    origin_t *origins; // wskaźnik na tablicę, bitów mówiącą które inputy są podłączone
};



struct output_destinations { // jeden node w tej liście odpowiada połączeniu z jednym automatem
    size_t num; // ile jest bitów podłączonych do tego automatu
    moore_t * ma; // do jakiego automatu jesteśmy podłączeni
    outList_t *next;
    outList_t *prev;
};

// Struktura przechowująca z wyjśćia jakiego automatu pochodzi poszczególny bit, ma = NULL kiedy jest oryginalny
struct input_origins {
    size_t out;
    moore_t *ma;
    outList_t *dest; // Wskaźnik na listę outList_t dla łatwiejszego usuwania
};

// Tworzy listę z atrapą na początku
outList_t* create() {
    outList_t* head = (outList_t*)calloc(1,sizeof(outList_t));
    if (!head) {
        errno = ENOMEM;
        return NULL;
    }
    head->next = NULL;
    head->prev = NULL;
    head->ma = NULL;
    return head;
}

// Dodaje na drugie miejsce w liście node'a, zwraca -1 dla nieudanej alokacji pamięci
outList_t* add_node(outList_t* head, moore_t* ma){
    outList_t *temp = head;
    while (temp) {
        if (temp->ma == ma) {
            return temp;
        }
        temp = temp->next;
    }
    outList_t* node = (outList_t*)malloc(sizeof(outList_t));
    if (!node) {
        errno = ENOMEM;
        return NULL;
    }
    node->ma = ma;
    node->num = 0;
    if (head->next) {
        node->next = head->next;
        node->next->prev = node;
    }
    else node->next = NULL;
    node->prev = head;
    head->next = node;
    return node;
}

// Usuwa node'a który został przekazany w argumencie funkcji
void remove_node(outList_t* node) {
    node->prev = node->next;
    if (node->next) node->next->prev = node->prev;
    free(node);
}
// Usuwa całą listę, zwalniając pamięć
void clear_list(outList_t* head) {
    while (head) {
        outList_t *temp = head;
        head = head->next;
        free(temp);
    }
}

// Funkcja identycznościowa dla prostego automatu moora
void ID (uint64_t *output, uint64_t const *state,size_t m, size_t s) {
    (void)s;
    memcpy(output, state, SIZEOF_64_UINT(m));
}

// Tworzy automat, callocując wszystkie bity i ustawiając całego structa
moore_t * ma_create_full(size_t n, size_t m, size_t s, transition_function_t t,
                         output_function_t y, uint64_t const *q) { // czy checemy zwolnić q czy programista się tym zajmie
    if (!m || !s || !t || !y || !q) { // czemu dla n = 0 mamy wywalone?
        errno = EINVAL;
        return NULL;
    }
    moore_t *ma = (moore_t*)malloc(sizeof(moore_t));
    if (ma == NULL) {
        errno = ENOMEM;
        return NULL;
    }
    ma->input = (uint64_t*)calloc(CEIL64(n), sizeof(uint64_t));
    ma->output = (uint64_t*)calloc(CEIL64(m), sizeof(uint64_t));
    ma->state = (uint64_t*)calloc(CEIL64(s), sizeof(uint64_t));
    ma->new_state = (uint64_t*)calloc(CEIL64(s), sizeof(uint64_t));
    ma->head = create();
    ma->origins = (origin_t*)calloc(n,sizeof(origin_t));
    if (!ma->input || !ma->output || !ma->state || !ma->head || !ma->origins || !ma->new_state) {
        errno = ENOMEM;
        if (ma->input) free(ma->input);
        if (ma->output) free(ma->output);
        if (ma->state) free(ma->state);
        if (ma->head) free(ma->head);
        if (ma->origins) free(ma->origins);
        if (ma->new_state) free(ma->new_state);
        free(ma);
        return NULL;
    }
    ma->n = n;
    ma->m = m;
    ma->s = s;
    ma->transition = t;
    ma->output_function = y;
    memcpy(ma->state, q, SIZEOF_64_UINT(s));
    y(ma->output, ma->state, ma->m, ma->s);
    return ma;
}

// Tworzy prosty automat moora, wywołując ma_create_full z odpowiednimi parametrami i funckją wyjścia id
moore_t * ma_create_simple(size_t n,size_t m, transition_function_t t) {\
    if (!m || !t) {
        errno = EINVAL;
        return NULL;
    }
    uint64_t *q = (uint64_t*)calloc(CEIL64(m), sizeof(uint64_t));
    if (!q) {
        errno = ENOMEM;
        return NULL;
    }
    moore_t *ma = ma_create_full(n, m, m, t, ID,q);
    free(q);
    return ma;
}

// Zwalnia pamięc całego automatu
void ma_delete(moore_t *a) {
    if (!a) {
        errno = EINVAL;
        return; // nic nie ma w zadaniu o errno dla ma_delete
    }
    outList_t *node = a->head->next;
    while (node) {
        if (node->num){
            moore_t *aut = node->ma;
            for (size_t i = 0; i < aut->n; i++) {
                if (aut->origins[i].ma == a) {
                    aut->origins[i].ma = NULL;
                    aut->origins[i].dest = NULL;
                    aut->origins[i].out = 0;
                }
            }
        }
        node = node->next;
    }
    for (size_t i = 0 ; i < a->n; i++) {
        if (a->origins[i].ma) {
            a->origins[i].dest->num--;
        }
    }
    free(a->input);
    free(a->output);
    free(a->state);
    free(a->origins);
    free(a->new_state);
    clear_list(a->head);
    free(a);
}

int ma_connect(moore_t *a_in, size_t in, moore_t *a_out, size_t out, size_t num) {
    if (!a_in || !a_out || !num) {
        errno = EINVAL;
        return -1;
    }
    if (in + num > a_in->n || out + num > a_out->m) {
        errno = EINVAL;
        return -1;
    }
    outList_t *node = add_node(a_out->head, a_in);
    if (!node) {
        errno = ENOMEM;
        return -1;
    }
    for (size_t i = 0; i < num; i++) {
        if (a_in->origins[in + i].ma != a_out) {
            node->num++;
            if (a_in->origins[in + i].ma) a_in->origins[in + i].dest->num--;
        }
        a_in->origins[in + i].dest = node;
        a_in->origins[in + i].ma = a_out;
        a_in->origins[in + i].out = out + i;
    }
    return 0;
}

int ma_disconnect(moore_t *a_in, size_t in, size_t num) {
    if (!a_in || !num || in + num > a_in->n) {
        errno = EINVAL;
        return -1;
    }
    for (size_t i = 0; i < num; i++) {
        if (a_in->origins[in + i].ma) {
            a_in->origins[in + i].dest->num--;
            a_in->origins[in + i].ma = NULL;
            a_in->origins[in + i].dest = NULL;
            a_in->origins[in + i].out = 0;
        }
    }
    return 0;
}

int ma_set_input(moore_t *a, uint64_t const *input) { // Bez połączeń
    if (!a || !input) {
        errno = EINVAL;
        return -1;
    }
    memcpy(a->input, input, SIZEOF_64_UINT(a->n));
    return 0;
}

int ma_set_state(moore_t *a, uint64_t const *state) {
    if (!a || !state) {
        errno = EINVAL;
        return -1;
    }
    memcpy(a->state, state, SIZEOF_64_UINT(a->s));
    output_function_t output_function = a->output_function;
    output_function(a->output,a->state,a->m,a->s);
    return 0;
}

uint64_t const * ma_get_output(moore_t const *a) {
    if (!a) {
        errno = EINVAL;
        return NULL;
    }
    output_function_t output_function = a->output_function;
    output_function(a->output,a->state,a->m,a->s);
    return a->output;
}

int ma_step(moore_t *at[], size_t num) {
    if (!at || !num) {
        errno = EINVAL;
        return -1;
    }
    for (size_t i = 0; i < num; i++) {
        if (!at[i]) {
            errno = EINVAL;
            return -1;
        }
    }
    for (size_t i = 0; i < num; i++) {
        origin_t * origin = at[i]->origins;
        for (size_t j = 0; j < at[i]->n; j++) {
            if (origin[j].ma) {
                size_t out = origin[j].out;
                at[i]->input[j/64] = COPY(at[i]->input[j/64], origin[j].ma->output[out/64] , j % 64, out % 64);
            }
        }
        transition_function_t t = at[i]->transition;
        t(at[i]->new_state, at[i]->input, at[i]->state, at[i]->n, at[i]->s);
        memcpy(at[i]->state, at[i]->new_state, SIZEOF_64_UINT(at[i]->s));
    }
    for (size_t i = 0; i < num; i++) {
        output_function_t output_function = at[i]->output_function;
        output_function(at[i]->output, at[i]->state, at[i]->m, at[i]->s);
    }
    return 0;
}

