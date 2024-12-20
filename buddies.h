#include <atomic>
#include <vector>
#include <memory>
#include <thread>
#include "missions.h"

class BuddiesForeman {
private:
    int workers = 0;
    bool is_active = false;
    std::atomic<bool> stop_flag = false;
    std::vector<std::thread> threads;

public:
    std::atomic<std::vector<std::unique_ptr<Mission>> *> atomic_tasks = nullptr;
    std::atomic<int> index = 0;
    std::atomic<int> begin = 0;
    std::atomic<int> end = 0;

    BuddiesForeman() = default;

    void init(int n);
    void set(std::vector<std::unique_ptr<Mission>> *);
    void wait();
    void stop_all();

private:
    static void buddy_realisation(BuddiesForeman &);
};

inline void BuddiesForeman::buddy_realisation(BuddiesForeman &handler) {
    handler.begin.wait(0);
    int last_start = handler.begin;
    while (!handler.stop_flag.load()) {
        auto missions = handler.atomic_tasks.load();
        int personal_index = handler.index.fetch_add(1);
        if (personal_index >= missions->size()) {
            handler.end.fetch_add(1);
            handler.end.notify_one();
            handler.begin.wait(last_start);
            last_start = handler.begin;
            continue;
        }
        missions->at(personal_index)->do_this();
    }
}

inline void BuddiesForeman::init(int n) {
    workers = n;
    for (int i = 0; i < workers; i++) {
        threads.emplace_back(buddy_realisation, std::ref(*this));
    }
}

inline void BuddiesForeman::set(std::vector<std::unique_ptr<Mission>> *missions) {
    is_active = true;
    index.store(0);
    end.store(0);
    atomic_tasks.store(missions);
    begin.fetch_add(1);
    begin.notify_all();
}

inline void BuddiesForeman::wait() {
    if (not is_active) {
        return;
    }
    int last;
    while ((last = end) != workers) {
        end.wait(last);
    }
    is_active = false;
}


inline void BuddiesForeman::stop_all() {
    stop_flag.store(true);
    begin.notify_all();
    for (auto &thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    threads.clear();
    is_active = false;
}