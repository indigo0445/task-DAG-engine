#pragma once
// permanent worker for thread pool
#include <thread>
#include <semaphore>
#include <functional>
#include <queue>
#include <mutex>
#include <iostream>
#include "TaskNode.h"
#include <stdexcept>

class PermaWorker {
private:
    std::jthread worker;
    std::binary_semaphore &new_task; // sets to 1 whenever new item in q
    std::queue<TaskNode*> &node_q; // todo: some better q scheduling?
    std::mutex &q_mtx;

    void loop() {
        while (true) {
            std::unique_lock<std::mutex> lock(q_mtx);
            if (!node_q.empty()) {
                auto node = node_q.front();
                node_q.pop();
                lock.unlock();

                try {
                    node->task();
                } catch (const std::runtime_error& e) {
                    std::cout << "a task failed\n";
                }

                // update successors and see if any are ready
                for (auto succ : node->successors) {
                    succ->pending_deps--;
                    if (succ->pending_deps == 0) {
                        lock.lock();
                        node_q.push(succ);
                        lock.unlock();
                    }
                }
            } else {
                new_task.acquire();
            }
        }
    }
    
public:
    PermaWorker(std::counting_semaphore<1> &new_task, std::queue<TaskNode*> &node_q, std::mutex &q_mtx)
        : new_task(new_task), node_q(node_q), q_mtx(q_mtx) {
        worker = std::jthread(&PermaWorker::loop, this);
        worker.detach();
    }
};
