#pragma once
#include <functional>
#include <vector>
#include <ranges>
#include <iostream>
#include <memory>
#include <atomic>
#include <initializer_list>

struct TaskNode {
    // operate entirely on internal obj states
    std::function<void()> task;
    std::atomic<int> pending_deps{0};
    std::vector<TaskNode*> successors; // immediate children
    
    template <typename Callable>
    TaskNode (Callable&& c): task(std::forward<Callable>(c)) {}

    template <typename Callable>
    TaskNode (Callable&& c, TaskNode& dep)
        : task(std::forward<Callable>(c)) {
            dep.directs_to(*this);
        }

    template <typename Callable>
    TaskNode (Callable&& c, std::initializer_list<std::reference_wrapper<TaskNode>> deps)
        : task(std::forward<Callable>(c)) {
            for (TaskNode& dep : deps) {
                dep.directs_to(*this);
            }
        }

    int directs_to(TaskNode &succ) {
        successors.push_back(&succ);
        succ.pending_deps++;
        return successors.size();
    }
};
