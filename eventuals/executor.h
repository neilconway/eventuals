#pragma once

#include "eventuals/catch.h"
#include "eventuals/concurrent.h"
#include "eventuals/eventual.h"
#include "eventuals/interrupt.h"
#include "eventuals/just.h"
#include "eventuals/lock.h"
#include "eventuals/loop.h"
#include "eventuals/map.h"
#include "eventuals/pipe.h"
#include "eventuals/task.h"
#include "eventuals/type-check.h"
#include "stout/borrowable.h"

////////////////////////////////////////////////////////////////////////

namespace eventuals {

////////////////////////////////////////////////////////////////////////

// Helper for running "fire and forget" eventuals. The eventual you
// submit _should not_ be terminated, but you can still determine when
// your eventuals have completed by composing with a 'Finally()' or a
// raw 'Eventual()` that handles each possibility.
template <typename E_>
class Executor final
  : public Synchronizable,
    public stout::enable_borrowable_from_this<Executor<E_>> {
 public:
  static_assert(
      HasValueFrom<E_>::value,
      "'Executor' expects an eventual type to execute");

  Executor(std::string&& name)
    : name_(name),
      wait_until_finished_(&lock()),
      executor_(this->Borrow([this]() {
        return pipe_.Read()
            | Concurrent([this]() {
                 return Map([this](E_ e) {
                   return std::move(e)
                       // NOTE: returning 'std::monostate' since
                       // 'Concurrent' does not yet support 'void'.
                       | Just(std::monostate{})
                       | Catch()
                             .raised<std::exception>(
                                 [this](std::exception&& e) {
                                   EVENTUALS_LOG(1)
                                       << "Executor '" << name_
                                       << "' caught: " << e.what();
                                   return std::monostate{};
                                 });
                 });
               })
            | Loop()
            | Synchronized(
                   Eventual<void>()
                       .start([this](auto& k) {
                         finished_ = true;
                         wait_until_finished_.Notify();
                         k.Start();
                       })
                       .fail([this](auto& k, auto&&...) {
                         finished_ = true;
                         wait_until_finished_.Notify();
                         k.Start();
                       })
                       .stop([this](auto& k) {
                         finished_ = true;
                         wait_until_finished_.Notify();
                         k.Start();
                       }));
      })) {
    // Start the executor task!
    executor_.Start(
        interrupt_,
        []() {},
        [](std::exception_ptr) { LOG(FATAL) << "Unreachable"; },
        []() { LOG(FATAL) << "Unreachable"; });
  }

  auto Submit(E_ e) {
    return TypeCheck<void>(
        pipe_.Write(std::move(e)));
  }

  auto Shutdown() {
    return TypeCheck<void>(
        pipe_.Close());
  }

  auto InterruptAndShutdown() {
    interrupt_.Trigger();
    return Shutdown();
  }

  auto Wait() {
    return TypeCheck<void>(
        Synchronized(
            wait_until_finished_.Wait(this->Borrow([this]() {
              return /* wait = */ !finished_;
            }))));
  }

 private:
  std::string name_;
  ConditionVariable wait_until_finished_;
  Task::Of<void> executor_;
  class Interrupt interrupt_;
  Pipe<E_> pipe_;
  bool finished_ = false;
};

////////////////////////////////////////////////////////////////////////

} // namespace eventuals

////////////////////////////////////////////////////////////////////////