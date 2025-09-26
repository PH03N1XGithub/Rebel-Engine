#pragma once
#include "ITask.h"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "Core/Delegate.h"

namespace Rebel::Core::Threds
{
	
// Single-cast delegate for bucket completion
DECLARE_DELEGATE(FBucketCompleteDelegate);
	

// ---------- Bucket ----------
struct Bucket
{
	Memory::TArray<std::function<void()>> tasks;                 // optional bookkeeping
	std::atomic<MemSize> remainingTasks{0};
	std::mutex mutex;                           // for any future per-bucket state
	//std::function<void()> onComplete;           // called exactly once when count hits 0

	// Replace std::function with your delegate
	FBucketCompleteDelegate onComplete;  

	Bucket() = default;
	Bucket(const Bucket&) = delete;
	Bucket& operator=(const Bucket&) = delete;
};

// ---------- Worker ----------
struct Worker
{
	std::thread thread;
	std::queue<std::pair<Bucket*, std::function<void()>>> taskQueue;
	std::mutex queueMutex;
	std::condition_variable cv;
	Bool shutdown = false;

	void Run();
};

// ---------- Scheduler ----------
class BucketScheduler
{
public:
	BucketScheduler(MemSize bucketCount, MemSize workerCount);
	~BucketScheduler();

	// Submit work
	void AddTask(MemSize bucketIndex, std::function<void()> task);

	// Optional async chaining
	void SetBucketCallback(MemSize bucketIndex, std::function<void()> callback);

	// Optional helper to check status without blocking
	bool IsBucketDone(MemSize bucketIndex) const;

	

	void WaitForAllTasks() {
		bool done = false;
		while (!done) {
			done = true;
			for (auto& bucket : buckets) {
				if (bucket->remainingTasks > 0) {
					done = false;
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
				}
			}
		}
	}

	

	

private:
	std::atomic<MemSize> activeTasks{0};
    std::mutex syncMutex;
    std::condition_variable syncCv;

    void TaskFinished() {
        if (--activeTasks == 0) {
            std::lock_guard<std::mutex> lock(syncMutex);
            syncCv.notify_all();
        }
    }
	Memory::TArray<Memory::UniquePtr<Bucket>> buckets;  // <-- pointer storage avoids copy/move
	Memory::TArray<Memory::UniquePtr<Worker>> workers;
	MemSize nextWorker = 0;
	std::mutex workerPickMutex;
};
}
