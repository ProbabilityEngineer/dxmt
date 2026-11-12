#pragma once
// Private diagnostic (private/pass-census, never upstream): why render passes end.
//
// Every InvalidateCurrentPass call that ends a render pass is counted under its caller (function and line, from
// __builtin_FUNCTION/__builtin_LINE), with the passes begun and the command buffers committed. Every 5 s of presents the
// window is appended to dxmt_pass_census.log beside the game's executable: per frame, the render passes begun, the command
// buffers committed, and the callers that ended render passes, largest first. A question for one session (2026-11-12: with
// ED Unofficial Patch's flat runtime on, the Metal HUD showed about 110 render encoders and 14 command buffers a frame
// against about 60 and 6 with it off), so it is always on in this build and costs a lock and a few adds per pass end.

#include <windows.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>

namespace dxmt {

class PassCensus {
public:
  static PassCensus &get() {
    static PassCensus census;
    return census;
  }

  void noteRenderEnd(const char *fn, unsigned line, bool deferred) {
    std::lock_guard<std::mutex> lock(mutex_);
    Entry *e = find(fn, line, deferred);
    if (e)
      ++e->n;
    else
      ++overflow_;
  }
  void noteRenderBegin() {
    std::lock_guard<std::mutex> lock(mutex_);
    ++begins_;
  }
  void noteCommit(const char *fn, unsigned line) {
    std::lock_guard<std::mutex> lock(mutex_);
    ++commits_;
    Entry *e = find(fn, line, false, true);
    if (e)
      ++e->n;
  }
  void noteTimestampEnd() {
    std::lock_guard<std::mutex> lock(mutex_);
    ++timestamps_;
  }
  void notePresent() {
    std::lock_guard<std::mutex> lock(mutex_);
    ++frames_;
    const auto now = std::chrono::steady_clock::now();
    if (!started_) {
      started_ = true;
      windowStart_ = now;
      return;
    }
    if (now - windowStart_ < std::chrono::seconds(5))
      return;
    write(std::chrono::duration<double>(now - windowStart_).count());
    windowStart_ = now;
    frames_ = begins_ = commits_ = timestamps_ = overflow_ = 0;
    for (auto &e : entries_)
      e.n = 0;
  }

private:
  struct Entry {
    const char *fn = nullptr;
    unsigned line = 0;
    bool deferred = false;
    bool commit = false;
    uint64_t n = 0;
  };
  static constexpr unsigned kEntries = 96;

  Entry *find(const char *fn, unsigned line, bool deferred, bool commit = false) {
    for (unsigned i = 0; i < used_; ++i) {
      Entry &e = entries_[i];
      if (e.line == line && e.deferred == deferred && e.commit == commit && e.fn == fn)
        return &e;
    }
    if (used_ == kEntries)
      return nullptr;
    Entry &e = entries_[used_++];
    e.fn = fn;
    e.line = line;
    e.deferred = deferred;
    e.commit = commit;
    e.n = 0;
    return &e;
  }

  void write(double seconds) {
    if (!file_) {
      char path[MAX_PATH] = {};
      DWORD n = GetModuleFileNameA(nullptr, path, MAX_PATH);
      char *slash = n ? std::strrchr(path, '\\') : nullptr;
      if (slash)
        std::strcpy(slash + 1, "dxmt_pass_census.log");
      else
        std::strcpy(path, "dxmt_pass_census.log");
      file_ = std::fopen(path, "a");
      if (!file_)
        return;
      SYSTEMTIME t;
      GetLocalTime(&t);
      std::fprintf(file_, "%02u:%02u:%02u dxmt pass census started (private/pass-census)\n", t.wHour, t.wMinute, t.wSecond);
    }
    const double f = frames_ ? static_cast<double>(frames_) : 1.0;
    SYSTEMTIME t;
    GetLocalTime(&t);
    std::fprintf(file_,
                 "%02u:%02u:%02u window %.1f s frames=%llu: per frame render passes begun %.1f, command buffers committed %.1f, "
                 "timestamp query ends %.1f%s\n",
                 t.wHour, t.wMinute, t.wSecond, seconds, static_cast<unsigned long long>(frames_), begins_ / f, commits_ / f,
                 timestamps_ / f, overflow_ ? " (some callers not tracked: table full)" : "");
    Entry *order[kEntries];
    unsigned k = 0;
    for (unsigned i = 0; i < used_; ++i)
      if (entries_[i].n)
        order[k++] = &entries_[i];
    std::sort(order, order + k, [](const Entry *a, const Entry *b) { return a->n > b->n; });
    for (unsigned i = 0; i < k; ++i)
      std::fprintf(file_, "    %s %6.1f/frame  %s:%u%s\n", order[i]->commit ? "commit  " : "pass end", order[i]->n / f,
                   order[i]->fn ? order[i]->fn : "?", order[i]->line, order[i]->deferred ? " (deferred context)" : "");
    std::fflush(file_);
  }

  std::mutex mutex_;
  Entry entries_[kEntries];
  unsigned used_ = 0;
  uint64_t frames_ = 0, begins_ = 0, commits_ = 0, timestamps_ = 0, overflow_ = 0;
  bool started_ = false;
  std::chrono::steady_clock::time_point windowStart_{};
  FILE *file_ = nullptr;
};

} // namespace dxmt
