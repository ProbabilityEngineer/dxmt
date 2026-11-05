#pragma once

#include <atomic>
#include <cstdint>

namespace dxmt {

// Per command queue. These are ownership counts, not unique allocation/byte
// counts. Concurrent snapshots are approximate, not a synchronization primitive.
struct Metal4MemoryDiagnostics {
  std::atomic<uint64_t> accepted{0}, submitted{0}, completed{0};
  std::atomic<uint64_t> chunks_submitted{0}, chunks_completed{0};
  std::atomic<uint64_t> error_chunks{0};
  std::atomic<uint64_t> retained_objects{0}, resource_bindings{0};
  std::atomic<uint64_t> scaler_cache_entries{0}, scalers_created{0};
  std::atomic<uint64_t> next_report_ms{0};
  std::atomic<uint32_t> reports{0};

  enum class Report { Skip, Sample, Limit };
  static constexpr uint64_t kIntervalMs = 5000;
  static constexpr uint32_t kMaxReports = 120;

  Report claimReport(uint64_t now_ms) {
    if (!accepted.load(std::memory_order_relaxed) ||
        reports.load(std::memory_order_relaxed) > kMaxReports)
      return Report::Skip;
    auto next = next_report_ms.load(std::memory_order_relaxed);
    if (now_ms < next || !next_report_ms.compare_exchange_strong(
          next, now_ms + kIntervalMs, std::memory_order_relaxed))
      return Report::Skip;
    const auto n = reports.fetch_add(1, std::memory_order_relaxed);
    return n < kMaxReports ? Report::Sample : Report::Limit;
  }

  static uint64_t pending(uint64_t issued, uint64_t retired) {
    return issued > retired ? issued - retired : 0;
  }
};

} // namespace dxmt
