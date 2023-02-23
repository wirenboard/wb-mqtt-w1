#pragma once

#include <condition_variable>
#include <wblib/log.h>

/**
 * @brief An interface for workers that a TThreadedPeriodicalRunner can execute
 * 
 */
class IPeriodicalWorker
{
public:
    virtual ~IPeriodicalWorker();

    /**
     * @brief The method is called by TThreadedPeriodicalRunner periodically
     * 
     */
    virtual void RunIteration() = 0;
};

/**
 * @brief The class executes RunIteration method of an IPeriodicalWorker object in a separate thread
 * every pollIntervalMs ms.
 *
 */
class TThreadedPeriodicalRunner
{
public:
    /**
     * @brief Construct a new TThreadedPeriodicalRunner object
     * 
     * @param worker pointer to a IPeriodicalWorker object
     * @param pollInterval execution interval in ms
     * @param threadName Name for execution thread
     * @param logger logger object for information messages
     */
    TThreadedPeriodicalRunner(std::unique_ptr<IPeriodicalWorker> worker,
                              std::chrono::milliseconds          pollInterval,
                              const std::string&                 threadName,
                              WBMQTT::TLogger&                   logger);
    ~TThreadedPeriodicalRunner();

private:
    std::unique_ptr<IPeriodicalWorker> Worker;
    bool                               Active;
    std::mutex                         ActiveMutex;
    std::condition_variable            ActiveCV;
    std::unique_ptr<std::thread>       WorkerThread;
};
