// custom threadpool implementation for this task
#include <thread>
#include <memory>
#include "PermaWorker.cpp"
#include "TaskNode.cpp"
#include <iostream>

class ThreadPoolExe {
private:
    int num_threads;
    std::unique_ptr<PermaWorker[]> pool;
    std::shared_ptr<

public:
    ThreadPoolExe() {
        num_threads = std::thread::hardware_concurrency();
        pool = std::make_unique<PermaWorker[]>(num_threads); // fills w/ default constructor
        std::cout << "Running on " << num_threads << " hardware threads\n";
    }

    void compute_DAG(TaskNode&) {
        // allow rvalue as well?
        
        // how is Exe alerted when a thread finishes?
        pool
    }
};
