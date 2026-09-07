#pragma once
// custom threadpool implementation for this task
#include <thread>
#include <memory>
#include "TaskNode.h"
#include <iostream>
#include <queue>
#include <functional>
#include <stdexcept>

class ThreadPoolExe {
private:
    int num_threads;
    std::atomic<int> num_nodes_left;
    std::unique_ptr<std::jthread[]> pool;
    std::counting_semaphore<32> new_task{0}; // sets to 1 whenever new item in q
    std::queue<TaskNode*> node_q; // todo: some better q scheduling?
    std::mutex q_mtx;
    std::binary_semaphore all_completed{0};
    bool stopping;

    void worker_loop() {
        while (true) {
            if (stopping) {
                return;
            }
            std::unique_lock<std::mutex> lock(q_mtx);
            if (!node_q.empty()) {
                auto node = node_q.front();
                node_q.pop();
                lock.unlock();

                try {
                    node->task();
                    num_nodes_left--;
                    if (num_nodes_left == 0) {
                        all_completed.release();
                    }
                } catch (const std::runtime_error& e) {
                    std::cout << "a task failed\n";
                }

                // update successors and see if any are ready
                for (auto succ : node->successors) {
                    // pending_deps can be negative
                    if (succ->pending_deps.fetch_sub(1) == 1) {
                        lock.lock();
                        node_q.push(succ);
                        lock.unlock();
                        new_task.release();
                    }
                }
            } else {
                lock.unlock();
                new_task.acquire();
            }
        }
    }

public:
    ThreadPoolExe() {
        num_threads = std::thread::hardware_concurrency();
        stopping = false;
        pool = std::make_unique<std::jthread[]>(num_threads); // fills w/ default constructor
        for (int i = 0; i < num_threads; i++) {
            pool[i] = std::jthread(&ThreadPoolExe::worker_loop, this);
        }
        std::cout << "Running on " << num_threads << " hardware threads\n";
    }

    void compute_DAG(int total_nodes, TaskNode &init) {
        // allow rvalue as well?
        // allow multiple roots?
        
        num_nodes_left = total_nodes;
        node_q.push(&init);
        new_task.release();

        all_completed.acquire();
    }

    void stop_workers() {
        stopping = true;
        for (int i = 0; i < num_threads; i++) {
            new_task.release();
        }
    }
};
