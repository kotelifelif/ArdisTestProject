#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <regex>

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
constexpr size_t delay_time = 10;

mutex delayed_mutex;

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

AbstractTask create_simple_task(size_t queue_number, DelayedPriorityScheduler& delayed_tasks_scheduler,
    SimplePriorityScheduler& simple_tasks_scheduler);
AbstractTask create_delayed_task(size_t queue_number, DelayedPriorityScheduler& delayed_tasks_scheduler,
    SimplePriorityScheduler& simple_tasks_scheduler);

string get_current_time() {
    std::time_t current_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    string string_time = std::ctime(&current_time);
    auto position = string_time.find("\n");
    if (position != string_time.npos) {
        string_time.erase(position);
    }
    return string_time;
}

AbstractTask create_simple_task(size_t queue_number, DelayedPriorityScheduler& delayed_tasks_scheduler,
    SimplePriorityScheduler& simple_tasks_scheduler) {
    AbstractTask simple_task;
    simple_task.name = "taskS" + to_string(queue_number + 1);
    simple_task.delay = queue_number + 1;
    simple_task.queue_name = "queueS" + to_string(queue_number + 1);
    simple_task.priority = 0;
    Task task(
        // Task action
        [queue_number, &delayed_tasks_scheduler, 
        &simple_tasks_scheduler, simple_task] {
            std::cout << get_current_time() << ": " << simple_task.queue_name
                << ": " << simple_task.name << ": " << simple_task.delay << " running..." << endl;
            sleep(simple_task.delay);
            AbstractTask delayed_task = create_delayed_task(queue_number, delayed_tasks_scheduler,
                simple_tasks_scheduler);
            delayed_tasks_scheduler.schedule<priority<0>>(delayed_task.task);            
            std::cout << get_current_time() << ": " << simple_task.queue_name
                << ": " << simple_task.name << ": " << simple_task.delay << " complited" << endl;
        }
    );
    simple_task.task = task;
    return simple_task;
}

AbstractTask create_delayed_task(size_t queue_number, DelayedPriorityScheduler& delayed_tasks_scheduler,
    SimplePriorityScheduler& simple_tasks_scheduler) {
    AbstractTask delayed_task;
    delayed_task.name = "taskD" + to_string(queue_number + 1);
    delayed_task.delay = delay_time;
    delayed_task.queue_name = "queueD" + to_string(queue_number + 1);
    size_t priority_size;
    if (queue_number % 2 == 0) {
        priority_size = 1;
    }
    else
    {
        priority_size = 0;
    }
    Task task(
        // Task action
        [queue_number, &delayed_tasks_scheduler,
        &simple_tasks_scheduler, delayed_task, priority_size] {
            std::cout << get_current_time() << ": " << delayed_task.name
                << ": " << delayed_task.delay << " created" << endl;
            sleep(delayed_task.delay);
            AbstractTask simple_task = create_simple_task(queue_number, delayed_tasks_scheduler,
                simple_tasks_scheduler);
            switch (priority_size)
            {
            case 0:
                simple_tasks_scheduler.schedule<priority<0>>(simple_task.task);
                break;
            case 1:
                simple_tasks_scheduler.schedule<priority<1>>(simple_task.task);
                break;
            }
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

    AbstractTask delayed_tasks[3];
    for (size_t i = 0; i < number_of_delayed_queues; ++i) {
        delayed_tasks[i] = create_delayed_task(i, delayed_tasks_schedulers[i],
            simple_tasks_scheduler);
        delayed_tasks_schedulers[i].schedule<priority<0>>(delayed_tasks[i].task);
    }

}