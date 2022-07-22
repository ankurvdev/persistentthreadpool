#include "PersistentThreadPool.h"

struct FileShasumTask : ptp::ITask
{
    struct Factory : ptp::ITaskFactory
    {
        virtual std::shared_ptr<ITask> CreateTask(std::span<std::byte const> const& taskDesc,
                                                  std::span<std::byte const> const& serializedState) override
        {
            auto task   = std::make_shared<FileShasumTask>();
            task->_desc = TaskDesc::Deserialize(taskDesc);
            if (serializedState.size_bytes() > 0) { task->_state = SerializedState::Deserialize(serializedState); }
        }
        // TODO : Progress notifications if needed
    };

    private:
    FileShasumTask() = default;
    struct TaskDesc
    {
        std::filesystem::path path;

        std::vector<std::byte> Serialize();                                                // TODO
        static TaskDesc        Deserialize(std::span<std::byte const> const& taskDesc);    // TODO
    };

    struct SerializedState
    {
        // TODO : Shasum state
        size_t fileOffset;

        std::vector<std::byte> Serialize();                                                // TODO
        static SerializedState Deserialize(std::span<std::byte const> const& taskDesc);    // TODO
    };

    SerializedState _state;
    TaskDesc        _desc;
};

struct NeverEndingHealthPingTaskFactory : ptp::ITask
{
    struct TaskDesc
    {
        std::string remoteUrl;
        uint32_t intervalTimeInMs;

        std::vector<std::byte> Serialize();                                                // TODO
        static TaskDesc        Deserialize(std::span<std::byte const> const& taskDesc);    // TODO
    };

    struct Factory : ptp::ITaskFactory
    {
        virtual std::shared_ptr<ITask> CreateTask(std::span<std::byte const> const& taskDesc,
                                                  std::span<std::byte const> const& serializedState) override
        {
            auto task   = std::make_shared<FileShasumTask>();
            task->_desc = TaskDesc::Deserialize(taskDesc);
            if (serializedState.size_bytes() > 0) { task->_state = SerializedState::Deserialize(serializedState); }
        }
    };

    NeverEndingEchoServerTaskFactory() = default;
};

// Scenarios to test
// Workers must spin up to max workers
// Workers must spin down to min workers
// Parallel Queued Tasks within max workers must start and finish
// Parallel Queued Tasks more than max workers given round robin execution-time with suspend
// ThreadPool-stop cancels and suspends ongoing tasks
// ThreadPool-stop persists the ongoing task states
// ThreadPool-start resumes the ongoing tasks
// ThreadPool-start does not resume completed tasks
// ThreadPool-stop does not persist completed tasks.
