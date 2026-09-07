// permanent worker for thread pool
#include <thread>
#include <semaphore>
#include <functional>

class PermaWorker {
private:
    std::jthread worker;
    std::counting_semaphore<1> new_task{0}; // flag to alert worker of new task
    std::counting_semaphore<1> done_task{0}; // flag to alert main of completion
    std::function<void()> task;

    void loop() {
        while (true) {
            new_task.acquire();
            task();
            done_task.release();
        }
    }
    
public:
    PermaWorker() {
        worker = std::jthread(&PermaWorker::loop, this);
    }

    void process(std::function<void()> task) {
        // should only be called if worker idle
        this->task = task;
        new_task.release(); // these 2 ops should be synchronous already
    }
};
