#pragma once
// custom threadpool implementation for this task
#include <thread>
#include <memory>
#include "PermaWorker.h"
#include "TaskNode.h"
#include <iostream>
#include <queue>
#include <functional>

class ThreadPoolExe {
private:
    int num_threads;
    // std::unique_ptr<PermaWorker[]> pool; // don't even need to track threads
    std::binary_semaphore new_task{0}; // sets to 1 whenever new item in q
    std::queue<TaskNode*> node_q; // todo: some better q scheduling?
    std::mutex q_mtx;

    std::queue<PermaWorker> workers;

public:
    ThreadPoolExe() {
        num_threads = std::thread::hardware_concurrency();
        // pool = std::make_unique<PermaWorker[]>(num_threads); // fills w/ default constructor
        for (int i = 0; i < num_threads; i++) {
            auto worker = PermaWorker(new_task, node_q, q_mtx);
            workers.push(std::move(worker));
        }
        std::cout << "Running on " << num_threads << " hardware threads\n";
    }

    void compute_DAG(TaskNode* init) {
        // allow rvalue as well?
        // allow multiple roots?
        
        node_q.push(init);
        new_task.release();
        std::cout << "pushed\n";
    }
};
