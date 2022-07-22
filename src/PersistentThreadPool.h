#include "CommonMacros.h"

#include <filesystem>
#include <future>
#include <memory>
#include <span>
#include <vector>

namespace ptp
{
struct AwaitableCoroutineTask
{
    // TODO : Utility class that implements ITask and provides a neater await/async based semantics
    protected:
};

struct ITask
{
    struct IScheduler
    {
        virtual bool RequestExtension() = 0;
        virtual bool Continue()         = 0;
    };

    CLASS_DELETE_COPY_AND_MOVE(ITask);
    virtual ~ITask() = default;

    virtual uint32_t GetFactoryId() const = 0;

    virtual std::vector<std::byte> SerializeState() = 0;    // Will on be called on suspended tasks

    // Suspensions may drag on for a bit to reach a suspendable state or halt ongoing chained async tasks.
    // To allow for a cleaner suspend this is async.
    virtual std::future<void> SuspendAsync() = 0;

    // Pass a scheduler object to have the task request additional time or gracefully yield to avoid suspensions
    // For error
    virtual uint32_t DoWork(IScheduler& scheduler) = 0;    // Return 0 for complete 1 for Yield

    // Cancellations can be pretty intrusive tasks requiring cleanups etc.
    // Always called on suspended tasks. (Guaranteed by thread-pool)
    virtual std::future<void> CancelAsync() = 0;

    // Only called if retrying for an error state to adjust internal state and cleanup from error scenarios
    virtual void Reinitialize() = 0;
};

struct ITaskFactory
{
    CLASS_DELETE_COPY_AND_MOVE(ITaskFactory);
    virtual ~ITaskFactory() = default;

    virtual uint32_t GetId() = 0;

    // taskDes: Task Description data
    // state: Task state data for resuming
    virtual std::shared_ptr<ITask> CreateTask(std::span<std::byte const> const& taskDesc, std::span<std::byte const> const& serializedState)
        = 0;
};

struct PresistentThreadPool : ITask::IScheduler
{
    PresistentThreadPool()  = default;
    ~PresistentThreadPool() = default;
    CLASS_DELETE_COPY_AND_MOVE(PresistentThreadPool);

    using time_point = std::chrono::time_point<std::chrono::system_clock>;

    using TaskId = uint64_t;    // Can change it to guid if needed

    enum class TaskStatus
    {
        Suspended,
        Working,
        Error,
        Canceled
    };

    struct TaskContext
    {
        TaskStatus             status = TaskStatus::Suspended;
        std::shared_ptr<ITask> task;
        std::vector<std::byte> taskDesc;
        std::vector<std::byte> resumeState;
        std::promise<void>     completionPromise;
    };

    // All factories must be registered before Init-Complete.
    void RegisterTaskFactory(std::shared_ptr<ITaskFactory> factory);

    void Start();    // Initialization complete Task Pool can start/resume operation
    void Stop();     // Wind down as quickly as possible. Persistate state for ongoing tasks

    std::shared_future<void> Enqueue(uint32_t factoryId, std::span<std::byte const> const& taskDesc);

    // TODO : Find a better name
    std::shared_future<void> GetFutureForTask(uint32_t id);

    // Manual Adjustment knobs for controlling behavior to adjust runtime resource utilization
    // May not take effect immediately
    void AdjustMaxWorkers(unsigned maxworkers);
    void AdjustMinWorkers(unsigned minworkers);
    void AdjustWorkerIdleThreasholdForSpinDown(time_point::duration dur);

#if defined TESTING    // provide access to some private members for testing and validation

#endif

    private:    // Functions
    // During task initialization create a task-context corresponding to the task
    std::unique_ptr<TaskContext> _CreateTaskContext(std::shared_ptr<ITask> task);

    void     _LoadTasksFromDisk();    // Initially resume enqueued tasks;
    void     _WorkerEventLoop();
    void     _MonitoringThread();
    uint32_t _GenerateUniqueTaskId();
    bool     _WorkerSpinDown();

    private:    // Utility Functions with locked-state
    bool _NeedsMoreWorkers(std::lock_guard<std::mutex> const& guard);

    private:    // Members
    std::filesystem::path                      _stateStoreDir;
    std::vector<std::shared_ptr<ITaskFactory>> _taskFactories;

    std::atomic<bool> _stopRequested = false;

    // Mutable state : protected by locks
    std::mutex _mutex;

    std::thread                                              _monitoringThread;
    std::vector<std::future<void>>                           _workers;
    std::unordered_map<TaskId, std::unique_ptr<TaskContext>> _tasks;

    std::condition_variable _taskStateChanged;
    // The State Machine
    // 1. Suspended -> Working
    // 2. Working -> Suspending
    // 3. Working -> Error
    // 4. Suspending -> Suspended
    // 5. Suspended -> Cancelling
    // 6. Error -> Suspended (Recovery)
    std::vector<TaskContext*> _suspended;
    std::vector<TaskContext*> _suspending;
    std::vector<TaskContext*> _working;
    std::vector<TaskContext*> _error;
    std::vector<TaskContext*> _cancelling;

    unsigned             _minworkers                     = 0;
    unsigned             _maxworkers                     = 4;    // TODO query cpu/thread count and initialize
    time_point::duration _workerIdleThresholdForSpinDown = std::chrono::milliseconds(1000);
};
}    // namespace ptp