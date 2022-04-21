#include "eventuals/grpc/server.h"
#include "gtest/gtest.h"
#include "test/grpc/test.h"

using eventuals::grpc::ServerBuilder;

TEST(BuildAndStartTest, Success) {
  ServerBuilder builder;

  builder.AddListeningPort("0.0.0.0:0", grpc::InsecureServerCredentials());

  auto build = builder.BuildAndStart();

  ASSERT_TRUE(build.status.ok());
  ASSERT_TRUE(build.server);
}
