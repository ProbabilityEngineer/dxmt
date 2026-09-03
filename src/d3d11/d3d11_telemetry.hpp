#pragma once

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

template <typename... Args>
inline void
eliteTelemetry(Args &&...args) {
  if (eliteTelemetryEnabled())
    Logger::info(str::format("[elite-d3d11] ", std::forward<Args>(args)...));
}

} // namespace dxmt
