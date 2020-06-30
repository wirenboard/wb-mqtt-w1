#include "threaded_runner.h"

#include <algorithm>
#include <wblib/utils.h>

using namespace std;
using namespace WBMQTT;

IPeriodicalWorker::~IPeriodicalWorker() {}

TThreadedPeriodicalRunner::TThreadedPeriodicalRunner(unique_ptr<IPeriodicalWorker> worker,
                                                     uint32_t                      pollIntervalMs,
                                                     const string&                 threadName,
                                                     TLogger&                      logger)
    : Worker(move(worker)), Active(false)
{
    if (pollIntervalMs == 0) {
        throw invalid_argument("polling intervall must be greater than zero");
    }

    Active = true;
    chrono::milliseconds interval(pollIntervalMs);
    WorkerThread = MakeThread(threadName, {[this, interval, threadName, &logger] {
                                  logger.Log() << threadName << "Started";

                                  while (1) {
                                      Worker->RunIteration();

                                      unique_lock<mutex> lk(ActiveMutex);
                                      if (ActiveCV.wait_for(lk, interval, [&] { return !Active; })) {
                                          break;
                                      }
                                  }
                                  logger.Log() << threadName << "Stopped";
                              }});
}

TThreadedPeriodicalRunner::~TThreadedPeriodicalRunner()
{
    {
        lock_guard<mutex> lock(ActiveMutex);
        if (!Active) {
            return;
        }
        Active = false;
    }
    ActiveCV.notify_one();

    if (WorkerThread->joinable()) {
        WorkerThread->join();
    }

    WorkerThread.reset();
}
