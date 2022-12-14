/*
 * Copyright 2022 Google LLC
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

#include "ecclesia/lib/redfish/dellicius/engine/query_engine.h"

#include <algorithm>
#include <array>
#include <memory>
#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/file/path.h"
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/lib/protobuf/parse.h"
#include "ecclesia/lib/redfish/dellicius/engine/config.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/testing/test_queries_embedded.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/testing/test_query_rules_embedded.h"
#include "ecclesia/lib/redfish/dellicius/engine/query_rules.pb.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/redfish/dellicius/query/query_result.pb.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/testing/fake_redfish_server.h"
#include "ecclesia/lib/testing/proto.h"
#include "ecclesia/lib/time/clock_fake.h"

namespace ecclesia {

namespace {

using ::testing::UnorderedPointwise;

constexpr absl::string_view kQuerySamplesLocation =
    "lib/redfish/dellicius/query/samples";

struct RedPathToQueryParams {
  std::string tracked_path;
  std::string query_params;
};

MATCHER(RedPathExpandConfigsEq, "") {
  const RedPathToQueryParams &lhs = std::get<0>(arg);
  const RedPathToQueryParams &rhs = std::get<1>(arg);
  return std::tie(lhs.tracked_path, lhs.query_params) ==
         std::tie(rhs.tracked_path, rhs.query_params);
}

void VerifyTrackedPathWithParamsMatchExpected(
    const std::vector<RedPathToQueryParams> &actual_configs) {
  const std::vector<RedPathToQueryParams> expected_configs = {
      RedPathToQueryParams{"/Chassis", "$expand=.($levels=1)"},
      RedPathToQueryParams{"/Chassis[*]", ""},
      RedPathToQueryParams{"/Systems[*]/Processors", "$expand=~($levels=1)"},
      RedPathToQueryParams{"/Systems", "$expand=*($levels=1)"},
      RedPathToQueryParams{"/Systems[*]", ""},
      RedPathToQueryParams{"/Systems[*]/Memory", "$expand=*($levels=1)"},
      RedPathToQueryParams{"/Systems[*]/Processors[*]", ""}};
  EXPECT_THAT(actual_configs,
              UnorderedPointwise(RedPathExpandConfigsEq(), expected_configs));
}

class QueryEngineTest : public ::testing::Test {
 protected:
  QueryEngineTest()
      : server_("indus_hmb_shim/mockup.shar"),
        clock_(absl::FromUnixSeconds(10)),
        intf_(server_.RedfishClientInterface()) {}

  FakeRedfishServer server_;
  FakeClock clock_;
  std::unique_ptr<RedfishInterface> intf_;
};

TEST_F(QueryEngineTest, QueryEngineDevpathConfiguration) {
  std::string sensor_out_path = GetTestDataDependencyPath(JoinFilePaths(
      kQuerySamplesLocation, "query_out/devpath_sensor_out.textproto"));

  QueryEngineConfiguration config{
      .flags{.enable_devpath_extension = true,
             .enable_cached_uri_dispatch = false},
      .query_files{kDelliciusQueries.begin(), kDelliciusQueries.end()}};
  QueryEngine query_engine(config, &clock_, std::move(intf_));
  std::vector<DelliciusQueryResult> response_entries =
      query_engine.ExecuteQuery({"SensorCollector"});

  DelliciusQueryResult intent_output_sensor =
      ParseTextFileAsProtoOrDie<DelliciusQueryResult>(sensor_out_path);
  EXPECT_EQ(response_entries.size(), 1);
  EXPECT_THAT(intent_output_sensor,
              IgnoringRepeatedFieldOrdering(EqualsProto(response_entries[0])));
}

TEST_F(QueryEngineTest, QueryEngineDefaultConfiguration) {
  std::string sensor_out_path = GetTestDataDependencyPath(
      JoinFilePaths(kQuerySamplesLocation, "query_out/sensor_out.textproto"));

  QueryEngineConfiguration config{
      .flags{.enable_devpath_extension = false,
             .enable_cached_uri_dispatch = false},
      .query_files{kDelliciusQueries.begin(), kDelliciusQueries.end()}};
  QueryEngine query_engine(config, &clock_, std::move(intf_));
  std::vector<DelliciusQueryResult> response_entries =
      query_engine.ExecuteQuery({"SensorCollector"});

  DelliciusQueryResult intent_output_sensor =
      ParseTextFileAsProtoOrDie<DelliciusQueryResult>(sensor_out_path);
  EXPECT_EQ(response_entries.size(), 1);
  EXPECT_THAT(intent_output_sensor,
              IgnoringRepeatedFieldOrdering(EqualsProto(response_entries[0])));
}

TEST_F(QueryEngineTest, QueryEngineWithExpandConfiguration) {
  std::string sensor_out_path = GetTestDataDependencyPath(
      JoinFilePaths(kQuerySamplesLocation, "query_out/assembly_out.textproto"));

  QueryEngineConfiguration config{
      .flags{.enable_devpath_extension = false,
             .enable_cached_uri_dispatch = false},
      .query_files{kDelliciusQueries.begin(), kDelliciusQueries.end()},
      .query_rules{kQueryRules.begin(), kQueryRules.end()}};

  QueryEngine query_engine(config, &clock_, std::move(intf_));
  QueryTracker query_tracker;
  std::vector<DelliciusQueryResult> response_entries =
      query_engine.ExecuteQuery(
          {"AssemblyCollectorWithPropertyNameNormalization"}, query_tracker);
  DelliciusQueryResult intent_output_sensor =
      ParseTextFileAsProtoOrDie<DelliciusQueryResult>(sensor_out_path);
  EXPECT_EQ(response_entries.size(), 1);
  EXPECT_THAT(intent_output_sensor,
              IgnoringRepeatedFieldOrdering(EqualsProto(response_entries[0])));

  std::vector<RedPathToQueryParams> tracked_configs;
  tracked_configs.reserve(query_tracker.redpaths_queried.size());
  for (auto paths_with_params : query_tracker.redpaths_queried) {
    RedPathToQueryParams redpath_to_query_params;
    redpath_to_query_params.tracked_path = paths_with_params.first;
    if (paths_with_params.second.expand.has_value()) {
      redpath_to_query_params.query_params =
          paths_with_params.second.expand->ToString();
    }
    tracked_configs.push_back(redpath_to_query_params);
  }
  VerifyTrackedPathWithParamsMatchExpected(tracked_configs);
}

TEST_F(QueryEngineTest, QueryEngineInvalidQueries) {
  QueryEngineConfiguration config{
      .flags{.enable_devpath_extension = false,
             .enable_cached_uri_dispatch = false},
      .query_files{kDelliciusQueries.begin(), kDelliciusQueries.end()}};

  // Invalid Query Id
  QueryEngine query_engine(config, &clock_, std::move(intf_));
  std::vector<DelliciusQueryResult> response_entries =
      query_engine.ExecuteQuery({"ThereIsNoSuchId"});
  EXPECT_EQ(response_entries.size(), 0);
}

TEST_F(QueryEngineTest, QueryEngineConcurrentQueries) {
  std::string assembly_out_path = GetTestDataDependencyPath(
      JoinFilePaths(kQuerySamplesLocation, "query_out/assembly_out.textproto"));
  std::string sensor_out_path = GetTestDataDependencyPath(
      JoinFilePaths(kQuerySamplesLocation, "query_out/sensor_out.textproto"));

  QueryEngineConfiguration config{
      .flags{.enable_devpath_extension = false,
             .enable_cached_uri_dispatch = false},
      .query_files{kDelliciusQueries.begin(), kDelliciusQueries.end()}};
  QueryEngine query_engine(config, &clock_, std::move(intf_));
  std::vector<DelliciusQueryResult> response_entries =
      query_engine.ExecuteQuery(
          {"SensorCollector",
           "AssemblyCollectorWithPropertyNameNormalization"});
  DelliciusQueryResult intent_sensor_out =
      ParseTextFileAsProtoOrDie<DelliciusQueryResult>(sensor_out_path);
  DelliciusQueryResult intent_assembly_out =
      ParseTextFileAsProtoOrDie<DelliciusQueryResult>(assembly_out_path);
  EXPECT_EQ(response_entries.size(), 2);
  EXPECT_THAT(intent_sensor_out,
              IgnoringRepeatedFieldOrdering(EqualsProto(response_entries[0])));
  EXPECT_THAT(intent_assembly_out,
              IgnoringRepeatedFieldOrdering(EqualsProto(response_entries[1])));
}

TEST_F(QueryEngineTest, QueryEngineEmptyItemDevpath) {
  std::string assembly_out_path = GetTestDataDependencyPath(JoinFilePaths(
      kQuerySamplesLocation, "query_out/devpath_assembly_out.textproto"));

  QueryEngineConfiguration config{
      .flags{.enable_devpath_extension = true,
             .enable_cached_uri_dispatch = false},
      .query_files{kDelliciusQueries.begin(), kDelliciusQueries.end()}};
  QueryEngine query_engine(config, &clock_, std::move(intf_));
  std::vector<DelliciusQueryResult> response_entries =
      query_engine.ExecuteQuery(
          {"AssemblyCollectorWithPropertyNameNormalization"});
  DelliciusQueryResult intent_assembly_out =
      ParseTextFileAsProtoOrDie<DelliciusQueryResult>(assembly_out_path);
  EXPECT_EQ(response_entries.size(), 1);
  EXPECT_THAT(intent_assembly_out,
              IgnoringRepeatedFieldOrdering(EqualsProto(response_entries[0])));
}

}  // namespace

}  // namespace ecclesia
