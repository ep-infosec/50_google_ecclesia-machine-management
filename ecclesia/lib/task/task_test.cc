/*
 * Copyright 2020 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "ecclesia/lib/task/task.h"

#include "gtest/gtest.h"
#include "absl/time/time.h"

namespace ecclesia {
namespace {

class TestTask : public BackgroundTask {
 public:
  TestTask() = default;

  absl::Duration RunOnce() override { return absl::InfiniteDuration(); }
};

TEST(BackgroundTaskTest, IsConstructable) {
  TestTask task;
  EXPECT_EQ(task.RunOnce(), absl::InfiniteDuration());
}

}  // namespace
}  // namespace ecclesia
