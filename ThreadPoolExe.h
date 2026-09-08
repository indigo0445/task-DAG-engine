#pragma once
// custom threadpool implementation for this task
#include <thread>
#include <memory>
#include "TaskNode.h"
#include "DAGUtil.h"
#include <iostream>
#include <queue>
#include <mutex>
#include <functional>
#include <stdexcept>
#include <vector>

class ThreadPoolExe {
private:
    int num_threads;
    std::atomic<int> num_nodes_left;
    std::unique_ptr<std::jthread[]> pool;
    std::counting_semaphore<1000> new_task{0}; // releases whenever new item in q
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
                } catch (const std::runtime_error& e) {
                    std::cout << "A task failed\n";
                }

                num_nodes_left--;
                // if truly done, successor loop below shouldn't affect q
                if (num_nodes_left == 0) {
                    all_completed.release();
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

    void compute_DAG(TaskNode& source) {
        compute_DAG(std::vector<std::reference_wrapper<TaskNode>>{source});
    }

    void compute_DAG(std::vector<std::reference_wrapper<TaskNode>> sources) {
        // takes in references for convenience, to avoid typing &
        auto sources_ptrs = sources | std::views::transform([](TaskNode& source){ return &source; })
                                    | std::ranges::to<std::vector<TaskNode*>>();

        DAGUtil::reset_graph_state(sources_ptrs);
        DAGUtil::check_validity(sources_ptrs);
        num_nodes_left = DAGUtil::get_size(sources_ptrs);
        node_q.push_range(sources_ptrs);
        
        new_task.release(sources.size());
    }

    void wait() {
        all_completed.acquire();
    }

    void stop_workers() {
        // does not interrupt if thread is working; I don't think C++ can interrupt a working thread?
        stopping = true;
        for (int i = 0; i < num_threads; i++) {
            new_task.release();
        }
    }
};
