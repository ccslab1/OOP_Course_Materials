// mpq.cpp
#include <cassert>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace CSE_OOP {

// ===== Message =====
// optional C-string message with getMessage(); we’ll store as std::string safely.
class Message {
    std::string msgstr;
public:
    explicit Message(const char* s = nullptr) : msgstr(s ? s : "") {}
    const char* getMessage() const { return msgstr.empty() ? nullptr : msgstr.c_str(); }
};

// ===== MessageQueue (FIFO, dynamic growth) =====
// implement a growable array and own messages on destruction.
class MessageQueue {
    std::vector<Message*> q; // owns pointers
public:
    MessageQueue() = default;
    ~MessageQueue() {
        for (auto* m : q) delete m; // free undelivered
        q.clear();
    }
    void enqueue(Message* m) {
        assert(m != nullptr);
        q.push_back(m);
    }
    Message* dequeue() {
        if (q.empty()) return nullptr;
        Message* m = q.front();
        q.erase(q.begin()); // simple for clarity; ring buffer would be O(1)
        return m; // caller owns
    }
    int getSize() const { return static_cast<int>(q.size()); }
};

// ===== MessagePriorityQueue =====
// enum Priority contiguous highest..lowest; scan from highest on dequeue.
class MessagePriorityQueue {
public:
    enum Priority { highest = 0, high, low, lowest };
private:
    MessageQueue* queues[lowest - highest + 1];
public:
    MessagePriorityQueue() {
        for (int p = highest; p <= lowest; ++p) queues[p] = new MessageQueue();
    }
    ~MessagePriorityQueue() {
        for (int p = highest; p <= lowest; ++p) { delete queues[p]; queues[p] = nullptr; }
    }
    void enqueue(Message* m, Priority p) {
        assert(m != nullptr);
        queues[p]->enqueue(m);
    }
    Message* dequeue() {
        for (int p = highest; p <= lowest; ++p) {
            if (auto* m = queues[p]->dequeue()) return m;
        }
        return nullptr;
    }
    int getSize(Priority p) const { return queues[p]->getSize(); }
    int getSize() const {
        int n = 0; for (int p = highest; p <= lowest; ++p) n += queues[p]->getSize(); return n;
    }
};

} // namespace CSE_OOP

// ===== Unit Tests =====
using namespace CSE_OOP;

static void test_Message() {
    Message a("hello");
    assert(std::strcmp(a.getMessage(), "hello") == 0);
    Message b(nullptr);
    assert(b.getMessage() == nullptr);
}

static void test_MessageQueue() {
    MessageQueue q;
    for (int i = 0; i < 20; ++i) {
        std::string s = "m" + std::to_string(i);
        q.enqueue(new Message(s.c_str()));
    }
    for (int i = 0; i < 20; ++i) {
        std::string expect = "m" + std::to_string(i);
        Message* m = q.dequeue();
        assert(m);
        assert(std::strcmp(m->getMessage(), expect.c_str()) == 0);
        delete m;
    }
    assert(q.dequeue() == nullptr);
}

static void test_MessagePriorityQueue() {
    MessagePriorityQueue pq;
    pq.enqueue(new Message("L1"), MessagePriorityQueue::low);
    pq.enqueue(new Message("H1"), MessagePriorityQueue::highest);
    pq.enqueue(new Message("H2"), MessagePriorityQueue::highest);
    pq.enqueue(new Message("Hi1"), MessagePriorityQueue::high);
    pq.enqueue(new Message("L2"), MessagePriorityQueue::low);

    assert(pq.getSize() == 5);
    const char* order[] = {"H1","H2","Hi1","L1","L2"};
    for (auto* expected : order) {
        Message* m = pq.dequeue();
        assert(m);
        assert(std::strcmp(m->getMessage(), expected) == 0);
        delete m;
    }
    assert(pq.dequeue() == nullptr);
}

int main() {
    test_Message();
    test_MessageQueue();
    test_MessagePriorityQueue();
    std::cout << "All C++ tests passed.\n";
    return 0;
    //g++ -std=c++20 -O2 -Wall -Wextra -o mpq_cpp mpq.cpp
}