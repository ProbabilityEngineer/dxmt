#include "../src/dxmt/dxmt_metal4_memory_diagnostics.hpp"
#include <cstdio>
#include <thread>
#include <vector>

int main() {
  using D = dxmt::Metal4MemoryDiagnostics;
  unsigned failures = 0;
  auto check = [&](bool ok, const char *message) {
    if (!ok) {
      std::printf("FAIL: %s\n", message);
      failures++;
    }
  };
  D d;
  check(d.claimReport(0) == D::Report::Skip, "no output before Metal 4 activation");
  d.accepted = 1;
  check(d.claimReport(0) == D::Report::Sample, "first active sample");
  check(d.claimReport(4999) == D::Report::Skip, "no per-frame logging");
  check(d.claimReport(5000) == D::Report::Sample, "five-second cadence");
  for (uint32_t i = 2; i < D::kMaxReports; ++i)
    check(d.claimReport(i * D::kIntervalMs) == D::Report::Sample, "bounded sample allowance");
  check(d.claimReport(D::kMaxReports * D::kIntervalMs) == D::Report::Limit, "one limit notice");
  check(d.claimReport((D::kMaxReports + 1) * D::kIntervalMs) == D::Report::Skip,
        "no output after limit");
  check(D::pending(5, 3) == 2 && D::pending(3, 5) == 0,
        "approximate snapshots do not underflow");
  D concurrent;
  concurrent.accepted = 1;
  std::atomic<unsigned> samples{0};
  std::vector<std::thread> threads;
  for (unsigned i = 0; i < 8; ++i) {
    threads.emplace_back([&] {
      if (concurrent.claimReport(100) == D::Report::Sample)
        samples++;
    });
  }
  for (auto &thread : threads)
    thread.join();
  check(samples == 1, "one sample per interval under concurrent calls");
  std::printf("Metal4 memory diagnostic policy: %s\n", failures ? "FAIL" : "PASS");
  return failures ? 1 : 0;
}
