#pragma once

#include <atomic>
#include <vector>
#include <memory>
#include <thread>
#include "missions.h"
#include "buddies.h"

class WorkerHandler {
private:
    int workers = 0;
    bool is_active = false;

public:
    std::atomic<std::vector<std::unique_ptr<Task>> *> atomic_tasks = nullptr;
    std::atomic<int> ind = 0;
    std::atomic<int> finish = 0;
    std::atomic<int> start = 0;

    WorkerHandler() = default;

    void init(int n);

    void set_tasks(std::vector<std::unique_ptr<Task>> *);

    void wait_until_end();

private:
    static void _worker_impl(WorkerHandler &);
};

inline void WorkerHandler::_worker_impl(WorkerHandler &handler) {
    handler.start.wait(0);
    int last_start = handler.start;
    while (true) {
        auto tasks = handler.atomic_tasks.load();
        int my_ind = handler.ind.fetch_add(1);
        if (my_ind >= tasks->size()) {
            handler.finish.fetch_add(1);
            handler.finish.notify_one();
            handler.start.wait(last_start);
            last_start = handler.start;
            continue;
        }
        tasks->at(my_ind)->doit();
    }
}

inline void WorkerHandler::set_tasks(std::vector<std::unique_ptr<Task>> *tasks) {
    is_active = true;
    ind.store(0);
    finish.store(0);
    atomic_tasks.store(tasks);
    start.fetch_add(1);
    start.notify_all();
}

inline void WorkerHandler::wait_until_end() {
    if (not is_active) {
        return;
    }
    int last;
    while ((last = finish) != workers) {
        finish.wait(last);
    }
    is_active = false;
}

inline void WorkerHandler::init(int n) {
    workers = n;
    for (int i = 0; i < workers; i++) {
        std::thread(_worker_impl, std::ref(*this)).detach();
    }
}
