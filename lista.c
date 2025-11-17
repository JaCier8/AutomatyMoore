#include <errno.h>
#include <stdlib.h>


#include "ma.h"
//
// Created by jappa on 4/3/25.
//
typedef struct output_destinations outList_t;
typedef struct input_origins origin_t;

struct output_destinations {
    size_t num;
    moore_t * ma;
    outList_t *next;
    outList_t *prev;
};

// Struktura przechowująca z wyjśćia jakiego automatu pochodzi poszczególny bit, ma = NULL kiedy jest oryginalny
struct input_origins {
    size_t num;
    moore_t *ma;
    outList_t *dest; // Wskaźnik na listę outList_t dla łatwiejszego usuwania
};

// Tworzy listę z atrapą na początku
outList_t* create() {
    outList_t* head = (outList_t*)malloc(sizeof(outList_t));
    head->num = 0;
    head->ma = NULL;
    head->next = NULL;
    head->prev = NULL;
    return head;
}

// Dodaje na drugie miejsce w liście node'a, zwraca -1 dla nieudanej alokacji pamięci
int add_node(outList_t* head, moore_t* ma, size_t num) {
    outList_t* node = (outList_t*)malloc(sizeof(outList_t));
    if (!node) {
        errno = ENOMEM;
        return -1;
    }
    node->ma = ma;
    node->num = num;
    if (head->next) {
        node->next = head->next;
        node->next->prev = node;
    }
    else node->next = NULL;
    node->prev = head;
    head->next = node;
    return 0;
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


