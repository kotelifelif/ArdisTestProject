#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <random>

#include <unistd.h>

#include "psched/priority_scheduler.h"
#include "psched/aging_policy.h"
#include "psched/queue_size.h"

using namespace psched;
using namespace std;


constexpr size_t delayed_queue_size = 1;
constexpr size_t number_of_delayed_queues = 3;
constexpr size_t simple_threads_size = 1;
constexpr size_t simple_queue_size = 3;
constexpr size_t elements_in_queue_size = 100;
constexpr size_t starvation_time = 250;
constexpr size_t increment_priority_size = 1;
using SimplePriorityScheduler = PriorityScheduler<threads<simple_threads_size>,
    queues<simple_queue_size, maintain_size<elements_in_queue_size, discard::oldest_task>>,
    aging_policy<
    task_starvation_after<std::chrono::milliseconds, starvation_time>,
    increment_priority_by<increment_priority_size>
    >>;
using DelayedPriorityScheduler = PriorityScheduler<threads<delayed_queue_size>,
    queues<delayed_queue_size, maintain_size<elements_in_queue_size, discard::oldest_task>>,
    aging_policy<
    task_starvation_after<std::chrono::milliseconds, starvation_time>,
    increment_priority_by<increment_priority_size>
    >>;

struct AbstractTask {
    Task task;
    string name;
    string queue_name;
    size_t delay;
    size_t priority;
};

AbstractTask create_simple_task(size_t queue_number, std::thread& delayed_thread, DelayedPriorityScheduler& delayed_tasks_scheduler,
    std::thread& simple_thread, SimplePriorityScheduler& simple_tasks_scheduler);
AbstractTask create_delayed_task(size_t queue_number, std::thread& delayed_thread, DelayedPriorityScheduler& delayed_tasks_scheduler,
    std::thread& simple_thread, SimplePriorityScheduler& simple_tasks_scheduler);

string get_current_time() {
    std::time_t current_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    return std::ctime(&current_time);
}

AbstractTask create_simple_task(size_t queue_number, std::thread& delayed_thread, DelayedPriorityScheduler& delayed_tasks_scheduler,
    std::thread& simple_thread, SimplePriorityScheduler& simple_tasks_scheduler) {
    std::random_device device;
    std::mt19937 generator(device());
    std::uniform_int_distribution<std::mt19937::result_type> delay_generator(1, 10);

    AbstractTask simple_task;
    simple_task.name = "taskS" + to_string(queue_number + 1);
    simple_task.delay = delay_generator(generator);
    simple_task.queue_name = "queueS" + to_string(queue_number + 1);
    simple_task.priority = 0;
    Task task(
        // Task action
        [&, simple_task] {
            std::cout << get_current_time() << ": " << simple_task.queue_name
                << ": " << simple_task.name << ": " << simple_task.delay << " running..." << endl;
            AbstractTask delayed_task = create_delayed_task(queue_number, delayed_thread, delayed_tasks_scheduler,
                simple_thread, simple_tasks_scheduler);
            delayed_tasks_scheduler.schedule<priority<0>>(delayed_task.task);
            sleep(simple_task.delay);
            std::cout << get_current_time() << ": " << simple_task.queue_name
                << ": " << simple_task.name << ": " << simple_task.delay << " complited" << endl;
        }
    );
    simple_task.task = task;
    return simple_task;
}

AbstractTask create_delayed_task(size_t queue_number, std::thread& delayed_thread, DelayedPriorityScheduler& delayed_tasks_scheduler,
    std::thread& simple_thread, SimplePriorityScheduler& simple_tasks_scheduler) {
    std::random_device device;
    std::mt19937 generator(device());
    std::uniform_int_distribution<std::mt19937::result_type> priority_generator(0, 2);
    std::uniform_int_distribution<std::mt19937::result_type> delay_generator(1, 10);

    AbstractTask delayed_task;
    delayed_task.name = "taskD" + to_string(queue_number + 1);
    delayed_task.delay = delay_generator(generator);
    delayed_task.queue_name = "queueD" + to_string(queue_number + 1);
    size_t priority_size = priority_generator(generator);
    Task task(
        // Task action
        [&, delayed_task] { 
            std::cout << get_current_time() << ": " << delayed_task.name
                << ": " << delayed_task.delay << " created" << endl;
            AbstractTask simple_task = create_simple_task(queue_number, delayed_thread, delayed_tasks_scheduler,
                simple_thread, simple_tasks_scheduler);
            switch (priority_size)
            {
            case 0:
                simple_tasks_scheduler.schedule<priority<0>>(simple_task.task);
                break;
            case 1:
                simple_tasks_scheduler.schedule<priority<1>>(simple_task.task);
                break;
            case 2:
                simple_tasks_scheduler.schedule<priority<2>>(simple_task.task);
                break;
            }
            sleep(delayed_task.delay);
            std::cout << get_current_time() << ": " << delayed_task.name
                << ": " << delayed_task.delay << ": " << simple_task.name << ": " 
                << simple_task.queue_name << " pushed" << endl;
        }
    );
    delayed_task.task = task;
    return delayed_task;
}

int main() {
    SimplePriorityScheduler simple_tasks_scheduler;
    DelayedPriorityScheduler delayed_tasks_schedulers[number_of_delayed_queues];

    std::thread delayed_threads[number_of_delayed_queues];
    std::thread simple_thread;
    AbstractTask delayed_tasks[3];
    for (size_t i = 0; i < number_of_delayed_queues; ++i) {
        delayed_tasks[i] = create_delayed_task(i, delayed_threads[i], delayed_tasks_schedulers[i],
            simple_thread, simple_tasks_scheduler);
        delayed_threads[i] = std::thread([&delayed_tasks_schedulers, &delayed_tasks, i]() {
            delayed_tasks_schedulers[i].schedule<priority<0>>(delayed_tasks[i].task);
            });
    }

    for (size_t i = 0; i < number_of_delayed_queues; ++i) {
        delayed_threads[i].join();
    }
}