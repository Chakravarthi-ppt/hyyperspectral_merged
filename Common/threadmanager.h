#ifndef THREADMANAGER_H
#define THREADMANAGER_H

#include <functional>
#include <vector>
#include <QThread>

class ThreadManager
{
public:

    ThreadManager();

    // ---------- 2 Tasks ----------
    static bool runParallel(
            std::function<bool()> task1,
            std::function<bool()> task2);

    // ---------- Multiple Tasks ----------
    static bool runParallel(
            const std::vector<std::function<bool()>> &tasks);

    static int threadCount();

    static void parallelFor(
            int totalRows,
            const std::function<void(int startRow,
                                     int endRow)> &task);
};

#endif
