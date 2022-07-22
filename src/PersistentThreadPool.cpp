#include "PersistentThreadPool.h"

using namespace ptp;

void ptp::PresistentThreadPool::_LoadTasksFromDisk()
{
    struct PersistedTasks
    {
        uint32_t               factoryId;
        uint32_t               taskId;
        std::vector<std::byte> desc;
        std::vector<std::byte> state;
    };

    std::vector<PersistedTasks> persistedTasks;
    // Tasks are stored in _stateStoreDir in the following format
    // <factoryId>.<taskId>.desc.bin
    // <factoryId>.<taskId>.state.bin
    // Load state from disk

    // No need for mutex because we're just starting
    for (auto const& ptask : persistedTasks)
    {
        auto task            = _taskFactories[ptask.factoryId]->CreateTask(ptask.desc, ptask.state);
        _tasks[ptask.taskId] = _CreateTaskContext(task);
    }
}

void ptp::PresistentThreadPool::Start()
{
    _LoadTasksFromDisk();

    // For each file in statestore dir
    // load the bin files and use them to recreate the tasks from their task-factories
    std::lock_guard<std::mutex> guard(_mutex);
    _monitoringThread = std::thread([this] { return this->_MonitoringThread(); });
    _workers.push_back(std::async(std::launch::async | std::launch::deferred, [this] { _WorkerEventLoop(); }));
}

bool ptp::PresistentThreadPool::_NeedsMoreWorkers(std::lock_guard<std::mutex> const& /*guard*/)
{}

bool ptp::PresistentThreadPool::_WorkerSpinDown()
{
    // TODO: Conditional wait for _taskStateChanged and sleep for _workerIdleThresholdForSpinDown if nothing enqueue for that long
}

// The State Machine
// 1. Suspended -> Working
// 2. Working -> Suspending
// 3. Working -> Error
// 4. Suspending -> Suspended
// 5. Suspended -> Cancelling
// 6. Error -> Suspended (Recovery)
void ptp::PresistentThreadPool::_WorkerEventLoop()
{
    // Find a task to do and assign yourself to it
    // If worker count less than min or there's more tasks and workcount hasnt reached max. start one more event loop
    // Start doing the work for specific period of time.
    // If the task hasnt yielded

    while (!_stopRequested)
    {
        TaskContext* assigned = nullptr;

        std::lock_guard<std::mutex> guard(_mutex);
        {
            if (_suspended.size() > 0)
            {
                assigned = _suspended.back();
                _suspended.pop_back();
                assigned->status = TaskStatus::Working;
                _working.push_back(assigned);
            }
            // Spin up more workers if needed
            if (_NeedsMoreWorkers(guard))
            {
                _workers.push_back(std::async(std::launch::async | std::launch::deferred, [this] { _WorkerEventLoop(); }));
            }
        }

        if (assigned == nullptr)
        {
            if (_WorkerSpinDown()) { return; }
            else
                continue;
        }

        // Now start assigned work
        try
        {
            auto retStatus = assigned->task->DoWork(*this);
            if (retStatus == 0)
            {
                // Completed
            }
            else
            {
                // Suspended
            }
        } catch (std::exception const& ex)
        {
            // Error
        }
        std::lock_guard<std::mutex> guard(_mutex);
        {

        }
    }
}

std::shared_future<void> ptp::PresistentThreadPool::Enqueue(uint32_t factoryId, std::span<std::byte const> const& taskDesc)
{
    auto task = _taskFactories[factoryId]->CreateTask(taskDesc, {});

    std::lock_guard<std::mutex> guard(_mutex);

    auto  taskCtx                   = _CreateTaskContext(task);
    auto& promise                   = taskCtx->completionPromise;
    _tasks[_GenerateUniqueTaskId()] = std::move(taskCtx);
    return std::shared_future<void>(promise.get_future());
}
