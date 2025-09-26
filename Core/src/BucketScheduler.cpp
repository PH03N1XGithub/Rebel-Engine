#include "Core/CorePch.h"
#include "Core/Delegate.h"
#include "Core/MultiThreading/BucketScheduler.h"

namespace Rebel::Core::Threds
{
    
// ---------- Worker ----------
void Worker::Run()
{
    while (true)
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        cv.wait(lock, [this]{ return shutdown || !taskQueue.empty(); });

        if (shutdown && taskQueue.empty())
            break;

        while (!taskQueue.empty())
        {
            auto [bucket, task] = taskQueue.front();
            taskQueue.pop();
            lock.unlock();

            // Execute the task

            task();
             

            // Decrement bucket count and possibly fire completion
            if (bucket->remainingTasks.fetch_sub(1) == 1)
            {
                // This thread is the one that transitioned to zero
                bucket->onComplete.Broadcast();

            }

            // tasks were allocated by caller; delete after run
            //delete task;

            lock.lock();
        }
    }
}

// ---------- Scheduler ----------
BucketScheduler::BucketScheduler(size_t bucketCount, size_t workerCount)
{
    buckets.Reserve(bucketCount);
    for (size_t i = 0; i < bucketCount; ++i)
        buckets.Emplace(Memory::MakeUnique<Bucket>());

    workers.Reserve(workerCount);
    for (size_t i = 0; i < workerCount; ++i)
    {
        auto w = Memory::MakeUnique<Worker>();
        w->thread = std::thread(&Worker::Run, w.Get());
        workers.Emplace(std::move(w));
    }
}

BucketScheduler::~BucketScheduler()
{
    // Signal shutdown to all workers
    for (auto& w : workers)
    {
        {
            std::lock_guard<std::mutex> lock(w->queueMutex);
            w->shutdown = true;
        }
        w->cv.notify_all();
    }

    // Join threads
    for (auto& w : workers)
    {
        if (w->thread.joinable())
            w->thread.join();
    }
}

void BucketScheduler::AddTask(size_t bucketIndex, std::function<void()> task)
{
    Bucket* bucket = buckets[bucketIndex].Get();
    bucket->remainingTasks.fetch_add(1, std::memory_order_relaxed);

    // Optional: track in bucket->tasks if you want to inspect later
    // (guarded if you mutate from multiple threads)
    // {
    //     std::lock_guard<std::mutex> lock(bucket->mutex);
    //     bucket->tasks.push_back(task);
    // }

    // Round-robin worker selection
    Worker* target = nullptr;
    {
        std::lock_guard<std::mutex> pickLock(workerPickMutex);
        target = workers[nextWorker].Get();
        nextWorker = (nextWorker + 1) % workers.Num();
    }

    {
        std::lock_guard<std::mutex> qlock(target->queueMutex);
        target->taskQueue.push({ bucket, task });
    }
    target->cv.notify_one();
}

void BucketScheduler::SetBucketCallback(size_t bucketIndex, std::function<void()> callback)
{
    buckets[(bucketIndex)]->onComplete.Bind(callback);
}

bool BucketScheduler::IsBucketDone(size_t bucketIndex) const
{
    return buckets[(bucketIndex)]->remainingTasks.load(std::memory_order_acquire) == 0;
}
}
