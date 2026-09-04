#pragma once

#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <utility>

#include "config/config.hpp"
#include "log/log.hpp"
#include "util_string.hpp"

namespace dxmt {

inline bool
eliteTelemetryEnabled() {
  static const bool enabled = Config::getInstance().getOption<bool>("d3d11.eliteTelemetry", false);
  return enabled;
}

inline uint64_t
eliteTelemetryObjectId(const void *object) {
  if (!object || !eliteTelemetryEnabled())
    return 0;
  static std::mutex mutex;
  static std::unordered_map<const void *, uint64_t> ids;
  static uint64_t next_id = 1;
  std::lock_guard<std::mutex> lock(mutex);
  auto [it, inserted] = ids.emplace(object, next_id);
  if (inserted)
    ++next_id;
  return it->second;
}

template <typename... Args>
inline void
eliteTelemetry(Args &&...args) {
  if (eliteTelemetryEnabled())
    Logger::info(str::format("[elite-d3d11] ", std::forward<Args>(args)...));
}

} // namespace dxmt
