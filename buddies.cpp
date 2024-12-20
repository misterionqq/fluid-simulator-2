#include <iostream>
#include "buddies.h"

void WorkerHandler::_worker_impl(WorkerHandler &handler) {
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

void WorkerHandler::set_tasks(std::vector<std::unique_ptr<Task>> *tasks) {
    is_active = true;
    ind.store(0);
    finish.store(0);
    atomic_tasks.store(tasks);
    start.fetch_add(1);
    start.notify_all();
}

void WorkerHandler::wait_until_end() {
    if (not is_active) {
        return;
    }
    int last;
    while ((last = finish) != workers) {
        finish.wait(last);
    }
    is_active = false;
}

void WorkerHandler::init(int n) {
    workers = n;
    for (int i = 0; i < workers; i++) {
        std::thread(_worker_impl, std::ref(*this)).detach();
    }
}
