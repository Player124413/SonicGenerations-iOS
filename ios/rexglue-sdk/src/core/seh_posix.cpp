/**
 * @file        core/seh_posix.cpp
 * @brief       POSIX platform SEH implementations
 *
 * @copyright   Copyright (c) 2026 Tom Clay <tomc@tctechstuff.com>
 * @license     BSD 3-Clause License
 */

#include <rex/platform.h>
#include <rex/platform/seh.h>

static_assert(REX_PLATFORM_LINUX || REX_PLATFORM_MAC, "This file is POSIX-only");

#include <signal.h>
#include <cstdlib>  // std::abort()

#include <rex/platform/exceptions.h>

namespace rex::platform {

static thread_local SehThreadState tls_seh_state;
static thread_local bool tls_seh_active = false;
// Handlers that were installed before seh_initialize() (e.g. the Android
// app's crash reporter); faults outside SEH regions are chained to them.
static struct sigaction g_prev_handlers[NSIG];

SehThreadState& seh_thread_state() {
  return tls_seh_state;
}

int seh_filter(uint32_t /*code*/, void* /*ep*/) {
  // Not used on POSIX - signal handlers throw directly
  return 0;
}

/// Signal handler for SIGSEGV/SIGBUS/SIGFPE/SIGILL
static void signal_handler(int sig, siginfo_t* info, void* ucontext_) {
  // Only handle if we're in SEH-protected code
  if (!tls_seh_active) {
    // Not in SEH region - chain to the previous handler (crash reporter),
    // else restore the default handler and re-raise.
    struct sigaction* prev = (sig > 0 && sig < NSIG) ? &g_prev_handlers[sig] : nullptr;
    if (prev && (prev->sa_flags & SA_SIGINFO) && prev->sa_sigaction) {
      prev->sa_sigaction(sig, info, ucontext_);
      return;
    }
    signal(sig, SIG_DFL);
    raise(sig);
    return;
  }

  // Determine exception code based on signal
  SehException::Code code;
  switch (sig) {
    case SIGSEGV:
      code = SehException::ACCESS_VIOLATION;
      break;
    case SIGBUS:
      code = SehException::IN_PAGE_ERROR;
      break;
    case SIGFPE:
      code = SehException::FLOAT_DIVIDE_BY_ZERO;
      break;
    case SIGILL:
      code = SehException::ILLEGAL_INSTRUCTION;
      break;
    default:
      code = SehException::UNKNOWN;
      break;
  }

  // Get fault address
  uintptr_t address = info ? reinterpret_cast<uintptr_t>(info->si_addr) : 0;

  // Store in thread state for potential rethrow
  tls_seh_state.code = static_cast<uint32_t>(code);
  tls_seh_state.info[0] = 0;
  tls_seh_state.info[1] = address;

  // Use libunwind to throw from signal context
  // This works because libunwind can unwind through signal frames
  throw SehException(code, address);
}

[[noreturn]] void seh_rethrow() {
  // Map stored code back to signal and re-raise
  uint32_t code = tls_seh_state.code;
  int sig;
  switch (code) {
    case SehException::ACCESS_VIOLATION:
      sig = SIGSEGV;
      break;
    case SehException::IN_PAGE_ERROR:
      sig = SIGBUS;
      break;
    case SehException::FLOAT_DIVIDE_BY_ZERO:
    case SehException::INTEGER_DIVIDE_BY_ZERO:
      sig = SIGFPE;
      break;
    case SehException::ILLEGAL_INSTRUCTION:
      sig = SIGILL;
      break;
    default:
      abort();
  }
  // Restore default handler and re-raise
  signal(sig, SIG_DFL);
  raise(sig);
  abort();  // Should not reach here
}

void seh_initialize() {
  if (g_seh_initialized.exchange(true)) {
    return;  // Already initialized
  }

  struct sigaction sa;
  sa.sa_sigaction = signal_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_SIGINFO | SA_NODEFER;  // SA_NODEFER allows re-entry for nested exceptions

  sigaction(SIGSEGV, &sa, &g_prev_handlers[SIGSEGV]);
  sigaction(SIGBUS, &sa, &g_prev_handlers[SIGBUS]);
  sigaction(SIGFPE, &sa, &g_prev_handlers[SIGFPE]);
  sigaction(SIGILL, &sa, &g_prev_handlers[SIGILL]);
}

bool& seh_active() {
  return tls_seh_active;
}

}  // namespace rex::platform
