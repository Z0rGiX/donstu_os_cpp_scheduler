#include "simulator.h"
#include <string>
#include <vector>

SimResult runSimulationMulti(Scheduler& sched, int cores, std::uint64_t maxTicks) {
    SimResult res;
    res.algorithm = sched.name() + " x" + std::to_string(cores);
    res.coreGantt.resize(cores);

    std::vector<int> current(cores, -1);   // кто сейчас на каждом ядре
    std::vector<int> prev(cores, -1);      // кто выполнялся на ядре в прошлом такте

    std::uint64_t busyTicks = 0;           // суммарно по всем ядрам
    std::uint64_t tick = 0;

    auto& procs = sched.processes();

    while (tick < maxTicks) {
        // 1. возврат из I/O
        for (auto& p : procs) {
            if (p.state == ProcessState::WAITING && p.ioReturnTick == tick) {
                p.state = ProcessState::READY;
                p.nextIoIndex++;
                sched.onProcessReady(p.pid, tick);
            }
        }

        // 2. все ли завершены
        bool allDone = true;
        for (auto& p : procs) {
            if (p.state != ProcessState::TERMINATED) { allDone = false; break; }
        }
        if (allDone) break;

        // 3. onTick — ОДИН раз за такт, не по разу на ядро
        sched.onTick(tick);

        // 4. вытеснение: для каждого ядра с процессом
        for (int c = 0; c < cores; ++c) {
            if (current[c] == -1) continue;
            if (sched.shouldPreempt(current[c], tick)) {
                Process* p = sched.find(current[c]);
                if (p) {
                    p->state = ProcessState::READY;
                    sched.onProcessPreempted(current[c], tick);
                }
                current[c] = -1;
            }
        }

        // 5. каждое свободное ядро берёт процесс
        for (int c = 0; c < cores; ++c) {
            if (current[c] != -1) continue;
            int pid = sched.pickNext(tick);
            if (pid == -1) continue;
            Process* p = sched.find(pid);
            if (!p || p->state != ProcessState::READY) continue;

            if (!p->started) {
                p->started = true;
                p->startTime = tick;
                p->responseTime = tick - p->arrivalTime;
            }
            p->state = ProcessState::RUNNING;
            current[c] = pid;
        }

        // 6. каждое ядро выполняет свой такт
        std::vector<int> ranPid(cores, -1);
        for (int c = 0; c < cores; ++c) {
            if (current[c] == -1) continue;
            Process* p = sched.find(current[c]);
            if (!p) { current[c] = -1; continue; }

            p->remainingTime--;
            busyTicks++;
            ranPid[c] = p->pid;

            // I/O
            if (p->nextIoIndex < (int)p->ioBlocks.size() &&
                p->executedTicks + 1 == p->ioBlocks[p->nextIoIndex].atTick) {
                p->ioWaitTime += p->ioBlocks[p->nextIoIndex].duration;
                p->ioReturnTick = tick + 1 + p->ioBlocks[p->nextIoIndex].duration;
                p->state = ProcessState::WAITING;
                sched.onProcessBlocked(p->pid, tick);
                current[c] = -1;
            }
            // завершение
            else if (p->remainingTime == 0) {
                p->finishTime = tick + 1;
                p->turnaroundTime = p->finishTime - p->arrivalTime;
                p->state = ProcessState::TERMINATED;
                sched.onProcessFinished(p->pid, tick);
                current[c] = -1;
            }
            p->executedTicks++;
        }

        // 7. +1 к waitingTime у всех, кто остался в READY (один раз)
        for (auto& p : procs) {
            if (p.state == ProcessState::READY) p.waitingTime++;
        }

        // 8. диаграмма Ганта по ядрам
        for (int c = 0; c < cores; ++c) {
            int pid = ranPid[c];
            auto& g = res.coreGantt[c];
            if (!g.empty() && g.back().first == pid) {
                g.back().second.second = tick + 1;
            } else {
                g.push_back({pid, {tick, tick + 1}});
            }
        }

        // сводная диаграмма (для совместимости с printGantt)
        bool anyRan = false;
        int lastPid = -1;
        for (int c = 0; c < cores; ++c) {
            if (ranPid[c] != -1) { anyRan = true; lastPid = ranPid[c]; }
        }
        if (!res.gantt.empty() && res.gantt.back().first == lastPid && anyRan) {
            res.gantt.back().second.second = tick + 1;
        } else if (anyRan) {
            res.gantt.push_back({lastPid, {tick, tick + 1}});
        }

        for (int c = 0; c < cores; ++c) prev[c] = current[c];
        tick++;
    }

    // метрики
    std::uint64_t totalWait = 0, totalTurn = 0, totalResp = 0, done = 0;
    std::uint64_t firstArrival = UINT64_MAX, lastFinish = 0;
    for (auto& p : procs) {
        if (p.state != ProcessState::TERMINATED) continue;
        totalWait += p.waitingTime;
        totalTurn += p.turnaroundTime;
        totalResp += p.responseTime;
        done++;
        if (p.arrivalTime < firstArrival) firstArrival = p.arrivalTime;
        if (p.finishTime > lastFinish) lastFinish = p.finishTime;
    }
    if (done > 0) {
        res.avgWaiting    = double(totalWait) / done;
        res.avgTurnaround = double(totalTurn) / done;
        res.avgResponse   = double(totalResp) / done;
    }
    res.totalTicks = tick;
    res.cpuUtilization = (tick > 0 && cores > 0)
                         ? 100.0 * busyTicks / double(tick * cores) : 0.0;

    return res;
}
