#pragma once

// Реализацию Thread pool взял отсюда https://github.com/AtomicBiscuit/SE2_CPP_HW3/tree/main
// Т.к. времени до дедлайна осталось мало, а экзамены уже начались

#include <atomic>
#include <vector>
#include <memory>
#include <thread>
#include "missions.h"

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