#include "threadmanager.h"

#include <QtConcurrent>
#include <QFuture>
#include <QThread>
#include <QDebug>

ThreadManager::ThreadManager()
{

}

bool ThreadManager::runParallel(
        std::function<bool()> task1,
        std::function<bool()> task2)
{
    QFuture<bool> future1 =
            QtConcurrent::run(task1);

    QFuture<bool> future2 =
            QtConcurrent::run(task2);

    future1.waitForFinished();
    future2.waitForFinished();

    return future1.result() &&
           future2.result();
}

bool ThreadManager::runParallel(
        const std::vector<std::function<bool()>> &tasks)
{
    QList<QFuture<bool>> futures;

    for(const auto &task : tasks)
    {
        futures.append(QtConcurrent::run(task));
    }

    bool success = true;

    for(auto &future : futures)
    {
        future.waitForFinished();

        success &= future.result();
    }

    return success;
}
int ThreadManager::threadCount()
{
    return QThread::idealThreadCount();
}

void ThreadManager::parallelFor(
        int totalRows,
        const std::function<void(int startRow,
                                 int endRow)> &task)
{
    int threads = threadCount();

    if (threads <= 0)
        threads = 1;

    int rowsPerThread = totalRows / threads;

    QList<QFuture<void>> futures;

    int startRow = 0;

    for (int i = 0; i < threads; i++)
    {
        int endRow;

        if (i == threads - 1)
        {
            endRow = totalRows;
        }
        else
        {
            endRow = startRow + rowsPerThread;
        }

        futures.append(
                    QtConcurrent::run(
                        [=]()
        {

            task(startRow, endRow);
        }
        )
                    );

        startRow = endRow;
    }

    for (QFuture<void> &future : futures)
    {
        future.waitForFinished();
    }
}
