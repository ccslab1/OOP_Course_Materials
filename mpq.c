// mpq.c
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ========= Message ========= */
typedef struct Message {
    char *msgstr;
} Message;

Message* Message_new(const char* s) {
    Message* m = (Message*)malloc(sizeof(Message));
    if (!m) exit(1);
    if (s) {
        size_t n = strlen(s) + 1;
        m->msgstr = (char*)malloc(n);
        if (!m->msgstr) exit(1);
        memcpy(m->msgstr, s, n);
    } else {
        m->msgstr = NULL;
    }
    return m;
}
const char* Message_get(const Message* m) { return m ? m->msgstr : NULL; }
void Message_delete(Message* m) {
    if (!m) return;
    free(m->msgstr);
    free(m);
}

/* ========= MessageQueue (FIFO, dynamic capacity growth) ========= */
// growable array, enqueue/dequeue FIFO, owns Messages when destroyed
typedef struct MessageQueue {
    Message** messages;
    int size;
    int capacity;
} MessageQueue;

#ifndef DEFAULT_QUEUE_CAPACITY
#define DEFAULT_QUEUE_CAPACITY 16
#endif

MessageQueue* MessageQueue_new(void) {
    MessageQueue* q = (MessageQueue*)malloc(sizeof(MessageQueue));
    if (!q) exit(1);
    q->size = 0;
    q->capacity = DEFAULT_QUEUE_CAPACITY;
    q->messages = (Message**)malloc(sizeof(Message*) * q->capacity);
    if (!q->messages) exit(1);
    return q;
}

static void MessageQueue_ensureCapacity(MessageQueue* q) {
    if (q->size == q->capacity) {
        q->capacity *= 2;
        Message** nm = (Message**)realloc(q->messages, sizeof(Message*) * q->capacity);
        if (!nm) exit(1);
        q->messages = nm;
    }
}

void MessageQueue_enqueue(MessageQueue* q, Message* m) {
    assert(q && m);
    MessageQueue_ensureCapacity(q);
    q->messages[q->size++] = m;
}

Message* MessageQueue_dequeue(MessageQueue* q) {
    if (!q || q->size == 0) return NULL;
    Message* m = q->messages[0];
    // shift (simple and clear; fine for teaching; a ring buffer would be O(1))
    for (int i = 1; i < q->size; ++i) q->messages[i - 1] = q->messages[i];
    q->size--;
    return m; // caller becomes owner
}

int MessageQueue_size(const MessageQueue* q) { return q ? q->size : 0; }
int MessageQueue_capacity(const MessageQueue* q) { return q ? q->capacity : 0; }

void MessageQueue_delete(MessageQueue* q) {
    if (!q) return;
    for (int i = 0; i < q->size; ++i) {
        Message_delete(q->messages[i]); // own & free undelivered messages
    }
    free(q->messages);
    free(q);
}

/* ========= MessagePriorityQueue ========= */
// array of MessageQueue*, one per priority; dequeue scans from highest  :contentReference[oaicite:4]{index=4}
typedef enum { PRIORITY_HIGHEST = 0, PRIORITY_HIGH, PRIORITY_LOW, PRIORITY_LOWEST, PRIORITY_COUNT } Priority;

typedef struct MessagePriorityQueue {
    MessageQueue** queues; // size PRIORITY_COUNT
} MessagePriorityQueue;

MessagePriorityQueue* MPQ_new(void) {
    MessagePriorityQueue* pq = (MessagePriorityQueue*)malloc(sizeof(MessagePriorityQueue));
    if (!pq) exit(1);
    pq->queues = (MessageQueue**)malloc(sizeof(MessageQueue*) * PRIORITY_COUNT);
    if (!pq->queues) exit(1);
    for (int p = 0; p < PRIORITY_COUNT; ++p) pq->queues[p] = MessageQueue_new();
    return pq;
}

void MPQ_delete(MessagePriorityQueue* pq) {
    if (!pq) return;
    for (int p = 0; p < PRIORITY_COUNT; ++p) {
        MessageQueue_delete(pq->queues[p]);
        pq->queues[p] = NULL;
    }
    free(pq->queues);
    free(pq);
}

void MPQ_enqueue(MessagePriorityQueue* pq, Message* m, Priority prio) {
    assert(pq && m);
    assert(prio >= PRIORITY_HIGHEST && prio < PRIORITY_COUNT);
    MessageQueue_enqueue(pq->queues[prio], m);
}

Message* MPQ_dequeue(MessagePriorityQueue* pq) {
    if (!pq) return NULL;
    for (int p = PRIORITY_HIGHEST; p < PRIORITY_COUNT; ++p) {
        Message* m = MessageQueue_dequeue(pq->queues[p]);
        if (m) return m;
    }
    return NULL;
}

int MPQ_sizePriority(const MessagePriorityQueue* pq, Priority prio) {
    return MessageQueue_size(pq->queues[prio]);
}
int MPQ_sizeAll(const MessagePriorityQueue* pq) {
    int s = 0;
    for (int p = 0; p < PRIORITY_COUNT; ++p) s += MessageQueue_size(pq->queues[p]);
    return s;
}

/* ========= Unit Tests (Message -> MessageQueue -> MessagePriorityQueue) ========= */
// recommend testing each in dependency order
static void test_Message(void) {
    Message* a = Message_new("hello");
    assert(strcmp(Message_get(a), "hello") == 0);
    Message* b = Message_new(NULL);
    assert(Message_get(b) == NULL);
    Message_delete(a);
    Message_delete(b);
}

static void test_MessageQueue(void) {
    MessageQueue* q = MessageQueue_new();
    // growth
    int startCap = MessageQueue_capacity(q);
    for (int i = 0; i < startCap + 2; ++i) {
        char buf[32]; snprintf(buf, sizeof(buf), "m%d", i);
        MessageQueue_enqueue(q, Message_new(buf));
    }
    assert(MessageQueue_capacity(q) >= startCap * 2);
    assert(MessageQueue_size(q) == startCap + 2);

    // FIFO
    for (int i = 0; i < startCap + 2; ++i) {
        char expect[32]; snprintf(expect, sizeof(expect), "m%d", i);
        Message* m = MessageQueue_dequeue(q);
        assert(m);
        assert(strcmp(Message_get(m), expect) == 0);
        Message_delete(m);
    }
    assert(MessageQueue_dequeue(q) == NULL);
    MessageQueue_delete(q);
}

static void test_MessagePriorityQueue(void) {
    MessagePriorityQueue* pq = MPQ_new();
    // Enqueue interleaved: ensure highest wins, FIFO within same priority
    MPQ_enqueue(pq, Message_new("L1"), PRIORITY_LOW);
    MPQ_enqueue(pq, Message_new("H1"), PRIORITY_HIGHEST);
    MPQ_enqueue(pq, Message_new("H2"), PRIORITY_HIGHEST);
    MPQ_enqueue(pq, Message_new("Hi1"), PRIORITY_HIGH);
    MPQ_enqueue(pq, Message_new("L2"), PRIORITY_LOW);

    assert(MPQ_sizeAll(pq) == 5);
    const char* order[] = {"H1","H2","Hi1","L1","L2"};
    for (int i = 0; i < 5; ++i) {
        Message* m = MPQ_dequeue(pq);
        assert(m);
        assert(strcmp(Message_get(m), order[i]) == 0);
        Message_delete(m);
    }
    assert(MPQ_dequeue(pq) == NULL);
    MPQ_delete(pq);
}

int main(void) {
    test_Message();
    test_MessageQueue();
    test_MessagePriorityQueue();
    puts("All C tests passed.");
    return 0;
    //gcc -std=c11 -O2 -Wall -Wextra -o mpq mpq.c
}
