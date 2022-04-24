#include <deque>
#include <string>
#include <vector>

#include "eventuals/callback.h"
#include "eventuals/collect.h"
#include "eventuals/eventual.h"
#include "eventuals/iterate.h"
#include "eventuals/let.h"
#include "eventuals/map.h"
#include "test/concurrent/concurrent.h"
#include "test/expect-throw-what.h"
#include "test/promisify-for-test.h"

namespace eventuals::test {
namespace {

// Tests when at least one of the eventuals fails.
TYPED_TEST(ConcurrentTypedTest, Fail) {
  std::deque<Callback<void()>> callbacks;

  auto e = [&]() {
    return Iterate({1, 2})
        | this->ConcurrentOrConcurrentOrdered([&]() {
            struct Data {
              void* k;
              int i;
            };
            return Map(Let([&](int& i) {
              return Eventual<std::string>()
                  .raises()
                  .start([&, data = Data()](auto& k) mutable {
                    using K = std::decay_t<decltype(k)>;
                    data.k = &k;
                    data.i = i;
                    callbacks.emplace_back([&data]() {
                      if (data.i == 1) {
                        static_cast<K*>(data.k)->Start(std::to_string(data.i));
                      } else {
                        static_cast<K*>(data.k)->Fail(
                            std::runtime_error("error"));
                      }
                    });
                  });
            }));
          })
        | Collect<std::vector<std::string>>();
  };

  static_assert(
      eventuals::tuple_types_unordered_equals_v<
          typename decltype(e())::template ErrorsFrom<void, std::tuple<>>,
          std::tuple<std::exception>>);

  auto [future, k] = PromisifyForTest(e());

  k.Start();

  ASSERT_EQ(2, callbacks.size());

  EXPECT_EQ(
      std::future_status::timeout,
      future.wait_for(std::chrono::seconds(0)));

  for (auto& callback : callbacks) {
    callback();
  }

  EXPECT_THROW_WHAT(future.get(), "error");
}

} // namespace
} // namespace eventuals::test
