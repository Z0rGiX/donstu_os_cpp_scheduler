#include "fcfs.h"
#include "sjf.h"
#include "srtn.h"
#include "rr.h"
#include "priority.h"
#include "mlfq.h"
#include "simulator.h"
#include "testsets.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <memory>
#include "hrrn.h"
#include <functional>
#include "edf.h"


using Factory = std::function<std::unique_ptr<Scheduler>(const std::vector<Process>&)>;

template <class T, class... Args>
Factory factory(Args... args) {
  return [=](const std::vector<Process>& s) { return std::make_unique<T>(s, args...); };
}

void runExperiment10() {
  std::vector<std::pair<std::string, Factory>> algos = {
    {"FCFS",       factory<FcfsScheduler>()},
    {"SJF",        factory<SjfScheduler>()},
    {"SRTN",       factory<SrtnScheduler>()},
    {"HRRN",       factory<HrrnScheduler>()},
    {"RR q=4",     factory<RrScheduler>(4)},
    {"Prio",       factory<PriorityScheduler>(false, false)},
    {"Prio+aging", factory<PriorityScheduler>(true, true)},
    {"MLFQ",       factory<MlfqScheduler>()},
  };
  const int sizes[5] = {12, 14, 16, 18, 20};

  std::cout << "Average waiting time\n";
  std::cout << std::left << std::setw(12) << "Algorithm" << std::right;
  for (int i = 1; i <= 5; ++i) std::cout << std::setw(9) << "set " + std::to_string(i);
  std::cout << std::setw(9) << "average" << "\n";

  for (auto& [label, make] : algos) {
    std::cout << std::left << std::setw(12) << label << std::right;
    double sum = 0;
    for (unsigned seed = 1; seed <= 5; ++seed) {
      auto set = makeRandomSet(seed, sizes[seed - 1]);
      auto sched = make(set);
      SimResult r = runSimulation(*sched);
      sum += r.avgWaiting;
      std::cout << std::setw(9) << std::fixed << std::setprecision(2) << r.avgWaiting;
    }
    std::cout << std::setw(9) << sum / 5 << "\n";
  }
}


std::vector<Process> makeTestSet() {
  std::vector<Process> procs;
  auto add = [&](int pid, const std::string& name, std::uint64_t arrival,
                 std::uint64_t burst, int priority,
                 std::vector<IoBlock> io = {}) {
    Process p;
    p.pid = pid;
    p.name = name;
    p.arrivalTime = arrival;
    p.burstTime = burst;
    p.remainingTime = burst;
    p.priority = priority;
    p.dynamicPriority = priority;
    p.ioBlocks = std::move(io);
    procs.push_back(p);
  };
  add(1, "P1", 0, 8, 3);
  add(2, "P2", 1, 4, 1);
  add(3, "P3", 2, 9, 4);
  add(4, "P4", 3, 5, 2);
  return procs;
}

std::vector<Process> makeIoTestSet() {
  std::vector<Process> procs;
  auto add = [&](int pid, const std::string& name,
                 std::uint64_t arrival, std::uint64_t burst,
                 int priority,
                 std::vector<IoBlock> io = {}) {
    Process p;
    p.pid = pid; p.name = name;
    p.arrivalTime = arrival;
    p.burstTime = burst;
    p.remainingTime = burst;
    p.priority = priority;
    p.dynamicPriority = priority;
    p.ioBlocks = std::move(io);
    procs.push_back(p);
  };
  add(1, "CPU1", 0, 20, 2);
  add(2, "IO1",  1,  2, 1, {{1, 5}, {1, 5}, {0, 0}});
  add(3, "IO2",  2,  2, 1, {{1, 5}, {1, 5}, {0, 0}});
  add(4, "CPU2", 3, 10, 3);
  return procs;
}
std::vector<Process> makeConvoySet() {
  std::vector<Process> procs;
  auto add = [&](int pid, const std::string& name, std::uint64_t arrival,
                 std::uint64_t burst, int priority,
                 std::vector<IoBlock> io = {}) {
    Process p;
    p.pid = pid; p.name = name;
    p.arrivalTime = arrival;
    p.burstTime = burst;
    p.remainingTime = burst;
    p.priority = priority;
    p.dynamicPriority = priority;
    p.ioBlocks = std::move(io);
    procs.push_back(p);
  };
  add(1, "CPU1", 0, 20, 2);
  add(2, "IO1",  1,  6, 1, {{1, 4}, {2, 4}, {3, 4}, {4, 4}, {5, 4}});
  add(3, "IO2",  2,  6, 1, {{1, 4}, {2, 4}, {3, 4}, {4, 4}, {5, 4}});
  add(4, "CPU2", 3, 12, 3);
  return procs;
}
void printResult(const SimResult& r) {
  std::cout << std::left << std::setw(30) << r.algorithm
            << " | wait=" << std::setw(7) << std::fixed << std::setprecision(2) << r.avgWaiting
            << " | turn=" << std::setw(7) << r.avgTurnaround
            << " | resp=" << std::setw(7) << r.avgResponse
            << " | CPU=" << std::setw(6) << r.cpuUtilization << "%"
            << " | CS=" << std::setw(4) << r.contextSwitches
            << " | done=" << r.throughput
            << "\n";
}

void printGantt(const SimResult& r) {
  std::cout << "Gantt (" << r.algorithm << "):\n";
  for (auto& [pid, span] : r.gantt) {
    std::cout << "  [" << span.first << "-" << span.second << ") ";
    if (pid == -1) std::cout << "IDLE\n";
    else if (pid == -2) std::cout << "CS\n";
    else std::cout << "P" << pid << "\n";
  }
  std::cout << "\n";
}

int main() {
  std::cout << "Case 1: CPU-bound\n";
  {
    auto set1 = makeTestSet();
    std::vector<std::unique_ptr<Scheduler>> scheds;
    scheds.push_back(std::make_unique<FcfsScheduler>(set1));
    scheds.push_back(std::make_unique<SjfScheduler>(set1));
    scheds.push_back(std::make_unique<SrtnScheduler>(set1));
    scheds.push_back(std::make_unique<RrScheduler>(set1, 1));
    scheds.push_back(std::make_unique<RrScheduler>(set1, 2));
    scheds.push_back(std::make_unique<RrScheduler>(set1, 4));
    scheds.push_back(std::make_unique<PriorityScheduler>(set1, false, false));
    scheds.push_back(std::make_unique<PriorityScheduler>(set1, true, false));
    scheds.push_back(std::make_unique<PriorityScheduler>(set1, true, true));
    scheds.push_back(std::make_unique<MlfqScheduler>(set1));
    for (auto& s : scheds) printResult(runSimulation(*s));
  }

  std::cout << "\nCase 2. Mixed loading: I/O + CPU\n";
  {
    auto set2 = makeIoTestSet();
    std::vector<std::unique_ptr<Scheduler>> scheds;
    scheds.push_back(std::make_unique<FcfsScheduler>(set2));
    scheds.push_back(std::make_unique<SrtnScheduler>(set2));
    scheds.push_back(std::make_unique<RrScheduler>(set2, 4));
    scheds.push_back(std::make_unique<PriorityScheduler>(set2, true, true));
    scheds.push_back(std::make_unique<MlfqScheduler>(set2));
    for (auto& s : scheds) printResult(runSimulation(*s));
  }

  std::cout << "\n";
  {
    auto set = makeTestSet();
    RrScheduler rr(set, 2);
    SimResult r = runSimulation(rr);
    printGantt(r);
  }
  {
   auto set = makeTestSet();
   RrScheduler rr(set, 2);
   SimResult r = runSimulation(rr);
   std::cout << "\nПо процессам, RR (q=2):\n";
   printProcessTable(rr.processes());
  }
  {
  auto set = makeTestSet();
  saveSet("set_basic.txt", set);

  std::vector<Process> loaded;
  if (!loadSet("set_basic.txt", loaded)) {
    std::cerr << "Не удалось загрузить set_basic.txt\n";
    return 1;
  }
  FcfsScheduler a(set), b(loaded);
  printResult(runSimulation(a));
  printResult(runSimulation(b));   // строки должны совпасть
  }
  {
  auto set = makeTestSet();
  std::cout << "\nВлияние кванта в RR (набор makeTestSet)\n";
  std::cout << "q   wait   turn   resp   CS\n";
  for (std::uint64_t q : {1, 2, 4, 8, 16}) {
    RrScheduler rr(set, q);
    SimResult r = runSimulation(rr);
    std::cout << std::setw(2) << q << "  " << std::fixed << std::setprecision(2)
              << r.avgWaiting << "  " << r.avgTurnaround << "  " << r.avgResponse
              << "  " << std::setw(3) << r.contextSwitches << "  "
              << std::string(r.contextSwitches, '#') << "\n";
  }
  }
  {
    const int sizes[5] = {12, 14, 16, 18, 20};
    for (unsigned seed = 1; seed <= 5; ++seed) {
    saveSet("sets/set" + std::to_string(seed) + ".txt", makeRandomSet(seed, sizes[seed - 1]));
  }
{
    auto set = makeTestSet();

    std::cout << "\n=== FCFS x1/x2/x4 ===\n";
    for (int cores : {1, 2, 4}) {
        FcfsScheduler s(set);
        printResult(runSimulationMulti(s, cores));
    }

    std::cout << "\n=== SJF x1/x2/x4 ===\n";
    for (int cores : {1, 2, 4}) {
        SjfScheduler s(set);
        printResult(runSimulationMulti(s, cores));
    }

    std::cout << "\n=== SRTN x2 ===\n";
    SrtnScheduler s(set);
    printResult(runSimulationMulti(s, 2));
}
std::vector<Process> makeDeadlineSet(bool feasible) {
    std::vector<Process> v;
    auto add = [&](int pid, const std::string& name, std::uint64_t arrival,
                   std::uint64_t burst, std::uint64_t deadline) {
        Process p;
        p.pid = pid; p.name = name;
        p.arrivalTime = arrival;
        p.burstTime = burst;
        p.remainingTime = burst;
        p.priority = 1;
        p.dynamicPriority = 1;
        p.deadline = deadline;
        v.push_back(p);
    };
    if (feasible) {
        add(1, "E1", 0, 3, 7);
        add(2, "E2", 1, 2, 4);
        add(3, "E3", 2, 2, 9);
        add(4, "E4", 4, 3, 10);
    } else {
        add(1, "F1", 0, 4, 5);
        add(2, "F2", 1, 3, 5);
        add(3, "F3", 2, 2, 6);
    }
    return v;
}

void printDeadlines(const std::vector<Process>& procs) {
    for (const auto& p : procs) {
        std::cout << p.name << ": finish=" << p.finishTime
                  << " deadline=" << p.deadline
                  << (p.finishTime <= p.deadline ? " OK" : " MISSED") << "\n";
    }
}
{
    std::cout << "\n=== EDF, набор E (выполнимый) ===\n";
    auto e = makeDeadlineSet(true);
    EdfScheduler s1(e);
    SimResult r1 = runSimulation(s1);
    printResult(r1);
    printGantt(r1);
    printDeadlines(s1.processes());

    std::cout << "\n=== EDF, набор F (невыполнимый) ===\n";
    auto f = makeDeadlineSet(false);
    EdfScheduler s2(f);
    SimResult r2 = runSimulation(s2);
    printResult(r2);
    printGantt(r2);
    printDeadlines(s2.processes());
}
  return 0;
}
}
