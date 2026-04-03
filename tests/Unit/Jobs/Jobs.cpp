#include "atlasnet/core/job/JobContext.hpp"
#include "atlasnet/core/job/JobOptions.hpp"
#include "atlasnet/core/job/JobSystem.hpp"
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <mutex>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

TEST(Jobs, SubmitRunsCallable)
{
  AtlasNet::JobSystem system;

  std::atomic<int> calls{0};

  auto handle = system.Submit([&](AtlasNet::JobContext&) { ++calls; },
                              AtlasNet::JobOpts::Name{"SubmitRunsCallable"});

  handle.wait();

  EXPECT_EQ(calls.load(), 1);

  system.Shutdown();
}
TEST(jobs, RepeatingTask)
{
  std::atomic_uint16_t val;

  AtlasNet::JobSystem system;
  std::mutex mtx;
  std::condition_variable cv;
  auto handle = system.Submit(
      [&](AtlasNet::JobContext& ctx)
      {
        val++;
        if (val < 5)
        {
          std::cout << "Requesting repeat from context, val = " << val.load()
                    << std::endl;
          ctx.repeat_once(10ms);
        }
        std::lock_guard lock(mtx);
        cv.notify_all();
      },
      AtlasNet::JobOpts::Name{"RepeatingTask"});

  std::unique_lock lock(mtx);
  cv.wait_for(lock, 2s, [&] { return val.load() >= 5; });
  handle.cancel();
  EXPECT_GE(val.load(), 5);
};

TEST(Jobs, HigherPriorityRunsFirstWithSingleWorker)
{
  AtlasNet::JobSystem system;

  std::mutex mtx;
  std::vector<int> order;

  auto low = system.Submit(
      [&](AtlasNet::JobContext&)
      {
        std::lock_guard lock(mtx);
        order.push_back(1);
      },
      AtlasNet::JobOpts::Name{"Low"},
      AtlasNet::JobOpts::Priority<AtlasNet::JobPriority::eLow>{});

  auto high = system.Submit(
      [&](AtlasNet::JobContext&)
      {
        std::lock_guard lock(mtx);
        order.push_back(2);
      },
      AtlasNet::JobOpts::Name{"High"},
      AtlasNet::JobOpts::Priority<AtlasNet::JobPriority::eHigh>{});

  low.wait();
  high.wait();

  ASSERT_EQ(order.size(), 2u);
  EXPECT_EQ(order[0], 2);
  EXPECT_EQ(order[1], 1);

  system.Shutdown();
}

TEST(Jobs, HandleCanRequestRepeat)
{
  AtlasNet::JobSystem system;

  std::atomic<int> calls{0};
  std::promise<void> done;
  auto doneFuture = done.get_future();
  std::atomic<bool> signaled{false};

  auto handle = system.Submit(
      [&](AtlasNet::JobContext&)
      {
        int n = ++calls;
        if (n >= 2 && !signaled.exchange(true))
        {
          done.set_value();
        }
      },
      AtlasNet::JobOpts::Name{"HandleCanRequestRepeat"});

  handle.request_repeat(25ms);

  EXPECT_EQ(doneFuture.wait_for(2s), std::future_status::ready);
  EXPECT_GE(calls.load(), 2);

  system.Shutdown();
}

TEST(Jobs, RepeatFromContextRunsMoreThanOnce)
{
  using namespace std::chrono_literals;

  AtlasNet::JobSystem system;

  std::atomic<int> calls{0};
  std::promise<void> done;
  auto doneFuture = done.get_future();
  std::atomic<bool> signaled{false};

  auto handle = system.Submit(
      [&](AtlasNet::JobContext& ctx)
      {
        std::cout << "Job run " << (calls.load() + 1) << std::endl;
        int n = ++calls;
        if (n < 2)
        {
          std::cout << "Requesting repeat from context\n";

          ctx.repeat_once(10ms);
        }
        if (n >= 2 && !signaled.exchange(true))
        {
          done.set_value();
        }
      },
      AtlasNet::JobOpts::Name{"RepeatFromContextRunsMoreThanOnce"});

  EXPECT_EQ(doneFuture.wait_for(2s), std::future_status::ready);

  handle.wait(); // Wait for terminal state, not just callback progress.

  EXPECT_GE(calls.load(), 2);
  EXPECT_TRUE(handle.is_completed());

  system.Shutdown();
}

TEST(Jobs, RepeatOptionRequestsAnotherRun)
{
  AtlasNet::JobSystem system;

  std::atomic<int> calls{0};
  std::promise<void> done;
  auto doneFuture = done.get_future();
  std::atomic<bool> signaled{false};

  auto handle = system.Submit(
      [&](AtlasNet::JobContext&)
      {
        int n = ++calls;
        if (n >= 2 && !signaled.exchange(true))
        {
          done.set_value();
        }
      },
      AtlasNet::JobOpts::Name{"RepeatOptionRequestsAnotherRun"},
      AtlasNet::JobOpts::RepeatOnce{10ms});

  EXPECT_EQ(doneFuture.wait_for(2s), std::future_status::ready);
  EXPECT_GE(calls.load(), 2);

  system.Shutdown();
}

TEST(Jobs, ManyJobsAllExecute)
{
  AtlasNet::JobSystem system;

  constexpr int kJobCount = 20000;
  std::atomic<int> completed{0};
  std::atomic<bool> doneSignaled{false};

  std::promise<void> done;
  auto doneFuture = done.get_future();

  std::vector<AtlasNet::JobHandle> handles;
  handles.reserve(kJobCount);

  for (int i = 0; i < kJobCount; ++i)
  {
    if ((i % 3) == 0)
    {
      handles.push_back(system.Submit(
          [&](AtlasNet::JobContext&)
          {
            const int n = completed.fetch_add(1, std::memory_order_relaxed) + 1;
            if (n == kJobCount && !doneSignaled.exchange(true))
            {
              done.set_value();
            }
          },
          AtlasNet::JobOpts::Priority<AtlasNet::JobPriority::eHigh>{}));
    }
    else if ((i % 3) == 1)
    {
      handles.push_back(system.Submit(
          [&](AtlasNet::JobContext&)
          {
            const int n = completed.fetch_add(1, std::memory_order_relaxed) + 1;
            if (n == kJobCount && !doneSignaled.exchange(true))
            {
              done.set_value();
            }
          },
          AtlasNet::JobOpts::Priority<AtlasNet::JobPriority::eMedium>{}));
    }
    else
    {
      handles.push_back(system.Submit(
          [&](AtlasNet::JobContext&)
          {
            const int n = completed.fetch_add(1, std::memory_order_relaxed) + 1;
            if (n == kJobCount && !doneSignaled.exchange(true))
            {
              done.set_value();
            }
          },
          AtlasNet::JobOpts::Priority<AtlasNet::JobPriority::eLow>{}));
    }
  }

  EXPECT_EQ(doneFuture.wait_for(10s), std::future_status::ready);
  EXPECT_EQ(completed.load(std::memory_order_relaxed), kJobCount);

  system.Shutdown();
}

TEST(Jobs, CompletionCascade)
{
  AtlasNet::JobSystem system;

  std::mutex mtx;
  std::condition_variable cv;

  int value = 0;
  bool done = false;

  auto handle = system.Submit([&](AtlasNet::JobContext&) { value = 1; },
                              AtlasNet::JobOpts::Name{"Level1"},
                              AtlasNet::JobOpts::OnComplete(
                                  [&](AtlasNet::JobContext&) { value = 2; },
                                  AtlasNet::JobOpts::Name{"Level2"},
                                  AtlasNet::JobOpts::OnComplete(
                                      [&](AtlasNet::JobContext&) { value = 3; },
                                      AtlasNet::JobOpts::Name{"Level3"},
                                      AtlasNet::JobOpts::OnComplete(
                                          [&](AtlasNet::JobContext&)
                                          {
                                            value = 4;
                                            {
                                              std::lock_guard lock(mtx);
                                              done = true;
                                            }
                                            cv.notify_one();
                                          },
                                          AtlasNet::JobOpts::Name{"Level4"}))));

  {
    std::unique_lock lock(mtx);
    EXPECT_TRUE(cv.wait_for(lock, 2s, [&] { return done; }));
  }

  handle.wait();
  EXPECT_EQ(value, 4);
  EXPECT_TRUE(done);

  system.Shutdown();
}

TEST(Jobs, HandleWaitObservesCompletion)
{
  AtlasNet::JobSystem system;

  std::atomic<bool> ran{false};

  auto handle =
      system.Submit([&](AtlasNet::JobContext&) { ran = true; },
                    AtlasNet::JobOpts::Name{"HandleWaitObservesCompletion"});

  handle.wait();

  EXPECT_TRUE(ran.load());
  EXPECT_TRUE(handle.is_completed());
  EXPECT_FALSE(handle.is_failed());

  system.Shutdown();
}

TEST(Jobs, FailureIsCapturedInHandle)
{
  AtlasNet::JobSystem system;

  auto handle = system.Submit(
      [&](AtlasNet::JobContext&) { throw std::runtime_error("boom"); },
      AtlasNet::JobOpts::Name{"FailureIsCapturedInHandle"});

  handle.wait();

  EXPECT_TRUE(handle.is_failed());

  EXPECT_THROW(handle.rethrow_if_failed(), std::runtime_error);

  system.Shutdown();
}

TEST(Jobs, CancelPendingJob)
{
  AtlasNet::JobSystem system;

  std::promise<void> blockerStarted;
  auto blockerStartedFuture = blockerStarted.get_future();

  std::promise<void> releaseBlocker;
  auto releaseBlockerFuture = releaseBlocker.get_future();

  std::atomic<bool> cancelledJobRan{false};

  auto blocker = system.Submit(
      [&](AtlasNet::JobContext&)
      {
        blockerStarted.set_value();
        releaseBlockerFuture.wait();
      },
      AtlasNet::JobOpts::Name{"Blocker"},
      AtlasNet::JobOpts::Priority<AtlasNet::JobPriority::eHigh>{});

  EXPECT_EQ(blockerStartedFuture.wait_for(1s), std::future_status::ready);

  auto victim =
      system.Submit([&](AtlasNet::JobContext&) { cancelledJobRan = true; },
                    AtlasNet::JobOpts::Name{"Victim"},
                    AtlasNet::JobOpts::Priority<AtlasNet::JobPriority::eLow>{});

  victim.cancel();

  releaseBlocker.set_value();
  blocker.wait();
  victim.wait();

  EXPECT_FALSE(cancelledJobRan.load());
  EXPECT_EQ(victim.state(), AtlasNet::JobState::eCancelled);

  system.Shutdown();
}

int main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
