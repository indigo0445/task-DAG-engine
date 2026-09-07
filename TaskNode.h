#pragma once
#include <functional>
#include <vector>
#include <ranges>
#include <iostream>
#include <memory>
#include <atomic>

struct TaskNode {
    // operate entirely on internal obj states
    std::function<void()> task;
    std::atomic<int> pending_deps{0};
    std::vector<TaskNode*> successors; // immediate children
    
    template <typename Callable>
    TaskNode (Callable&& c): task(std::forward<Callable>(c)) {}

    int directs_to(TaskNode &succ) {
        successors.push_back(&succ);
        succ.pending_deps++;
        return successors.size();
    }
};
