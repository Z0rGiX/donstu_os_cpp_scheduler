#pragma once
#include "scheduler.h"
#include <algorithm>

// EDF (Earliest Deadline First) — вытесняющий алгоритм реального времени.
// Выполняется процесс с наименьшим абсолютным дедлайном.
class EdfScheduler : public Scheduler {
public:
    explicit EdfScheduler(const std::vector<Process>& procs)
        : Scheduler(procs) {}

    std::string name() const override { return "EDF"; }

    int pickNext(std::uint64_t) override {
        if (ready_.empty()) return -1;
        auto it = std::min_element(ready_.begin(), ready_.end(), [&](int a, int b) {
            const Process* pa = find(a);
            const Process* pb = find(b);
            return pa->deadline < pb->deadline;
        });
        int pid = *it;
        ready_.erase(it);
        return pid;
    }

    bool shouldPreempt(int currentPid, std::uint64_t) override {
        Process* cur = find(currentPid);
        if (!cur || cur->isFinished()) return false;
        for (int pid : ready_) {
            Process* p = find(pid);
            if (p && p->deadline < cur->deadline) return true;
        }
        return false;
    }

    void onProcessReady(int pid, std::uint64_t) override {
        ready_.push_back(pid);
    }

    void onProcessBlocked(int /*pid*/, std::uint64_t) override { }

    void onProcessPreempted(int pid, std::uint64_t) override {
        ready_.push_back(pid);
    }

    void onProcessFinished(int pid, std::uint64_t) override {
        Process* p = find(pid);
        if (p) p->state = ProcessState::TERMINATED;
    }

private:
    // ready_ и другие поля — как в SrtnScheduler
};
