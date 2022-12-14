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

#include <algorithm>
#include <fstream>
#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/fixed_array.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ecclesia/lib/file/cc_embed_interface.h"
#include "ecclesia/lib/file/dir.h"
#include "ecclesia/lib/file/path.h"
#include "ecclesia/lib/http/cred.pb.h"
#include "ecclesia/lib/http/curl_client.h"
#include "ecclesia/lib/redfish/dellicius/engine/config.h"
#include "ecclesia/lib/redfish/dellicius/engine/query_engine.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/redfish/dellicius/query/query_result.pb.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/transport/cache.h"
#include "ecclesia/lib/redfish/transport/http.h"
#include "ecclesia/lib/redfish/transport/http_redfish_intf.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "ecclesia/lib/redfish/transport/metrical_transport.h"
#include "ecclesia/lib/redfish/transport/transport_metrics.pb.h"
#include "ecclesia/lib/time/clock.h"
#include "google/protobuf/io/zero_copy_stream_impl.h"
#include "google/protobuf/text_format.h"

ABSL_FLAG(bool, devpath_enabled, false, "Boolean to enable devpath extension.");
ABSL_FLAG(bool, metrics_enabled, false, "Boolean to enable redfish metrics.");
ABSL_FLAG(std::string, hostname, "localhost",
          "Hostname of the Redfish server.");
ABSL_FLAG(int, port, 8000, "Port number of the server.");
ABSL_FLAG(std::string, input_dir, "",
          "Absolute path to directory containing textproto files for Queries.");
ABSL_FLAG(std::string, transport_metric_output_dir, "",
          "Absolute path to output directory to store transport metrics.");
ABSL_FLAG(std::vector<std::string>, query_ids, {},
          "List of Identifiers for the Dellicius Queries to execute.");

namespace ecclesia {

namespace {

constexpr absl::string_view kUsage =
    "A debug tool to Query a given Redfish Service using Dellicius "
    "Queries.\n\nThe tool requires the following at minimum:\n  "
    "1. An absolute path to the directory containing textproto files for all "
    "Dellicius Queries that DelliciusQueryEngine should be configured for.\n  "
    "2. A list of string identifiers corresponding to the DelliciusQuery files "
    "in the directory that the user expects to execute using the tool.\n"
    "Example:\n"
    "  blaze run ecclesia/lib/redfish/dellicius/debug:query_cli"
    " -- --input_dir=/usr/local/google/home/foo/repos/query_in "
    "--query_ids=SensorCollector,AssemblyCollector --devpath_enabled=true"
    " --hostname='localhost' --port=8000\n";

// Identifier for the output metrics file.
constexpr absl::string_view kMetricOutId = "query_cli_redfish_metrics";

// Defines attributes used to construct EmbeddedFile object from Dellicius Query
struct DelliciusQueryMetadata {
  // Captures raw string value of query_id attribute in DelliciusQuery proto.
  std::string query_id;
  // Stores parsed DelliciusQuery proto formatted in human readable debug string
  std::string query_data;
};

absl::StatusOr<std::vector<DelliciusQueryMetadata>> GetQueriesFromLocation(
    absl::string_view loc) {
  std::vector<DelliciusQueryMetadata> metadata;
  absl::Status status =
      WithEachFileInDirectory(loc, [&](absl::string_view dir_entry) {
        auto query_file_path = JoinFilePaths(loc, dir_entry);
        std::ifstream input(query_file_path);
        if (!input) {
          LOG(ERROR) << "File doesn't exist: " << dir_entry;
          return;
        }
        DelliciusQuery query_in;
        google::protobuf::io::IstreamInputStream input_stream(&input);
        if (!google::protobuf::TextFormat::Parse(&input_stream, &query_in)) {
          LOG(ERROR) << "Cannot parse given textproto";
          return;
        }
        metadata.push_back({.query_id = query_in.query_id(),
                            .query_data = query_in.DebugString()});
      });
  if (!status.ok()) return status;
  if (metadata.empty()) {
    return absl::NotFoundError(absl::StrFormat(
        "Cannot process Dellicius Queries from given location %s", loc));
  }
  return metadata;
}

int QueryMain(int argc, char **argv) {
  absl::SetProgramUsageMessage(kUsage);
  absl::ParseCommandLine(argc, argv);
  absl::InitializeLog();
  // Parse Dellicius Queries from the given directory.
  absl::StatusOr<std::vector<DelliciusQueryMetadata>> queries_metadata =
      GetQueriesFromLocation(absl::GetFlag(FLAGS_input_dir));
  if (!queries_metadata.ok()) {
    LOG(ERROR) << queries_metadata.status();
    return -1;
  }
  // Parse DelliciusQuery identifiers for the queries to dispatch.
  std::vector<std::string> query_ids_provided = absl::GetFlag(FLAGS_query_ids);
  if (query_ids_provided.empty()) {
    LOG(ERROR) << "Query Identifier list empty. Try again with non-empty set of"
               << "DelliciusQuery Identifiers.";
    return -1;
  }
  // Construct EmbeddedFile objects to be used with DelliciusQueryEngine.
  std::vector<EmbeddedFile> embedded_files;
  std::for_each(
      queries_metadata->begin(), queries_metadata->end(),
      [&](DelliciusQueryMetadata &query_id_and_data) {
        embedded_files.push_back({.name = query_id_and_data.query_id,
                                  .data = query_id_and_data.query_data});
      });
  // Build QueryEngineConfiguration from command line arguments.
  QueryEngineConfiguration config{
      .flags{.enable_devpath_extension = absl::GetFlag(FLAGS_devpath_enabled),
             .enable_cached_uri_dispatch = false},
      .query_files{embedded_files}};
  // Configure HTTP transport.
  auto curl_http_client = std::make_unique<CurlHttpClient>(
      LibCurlProxy::CreateInstance(), HttpCredential());
  std::unique_ptr<HttpRedfishTransport> base_transport =
      HttpRedfishTransport::MakeNetwork(
          std::move(curl_http_client),
          absl::StrCat(absl::GetFlag(FLAGS_hostname), ":",
                       absl::GetFlag(FLAGS_port)));
  RedfishMetrics transport_metrics;
  std::unique_ptr<RedfishInterface> intf;
  std::unique_ptr<NullCache> cache;
  std::unique_ptr<RedfishTransport> transport = std::move(base_transport);
  {
    if (absl::GetFlag(FLAGS_metrics_enabled)) {
      transport = std::make_unique<MetricalRedfishTransport>(
          std::move(transport), Clock::RealClock(), transport_metrics);
    }
    cache = std::make_unique<NullCache>(transport.get());
    intf = NewHttpInterface(std::move(transport), std::move(cache),
                            RedfishInterface::kTrusted);
    if (auto root = intf->GetRoot(); root.AsObject() == nullptr) {
      LOG(ERROR) << "Error connecting to redfish service. "
                 << "Check host configuration";
      return -1;
    }
    // Build QueryEngine
    QueryEngine query_engine(config, Clock::RealClock(), std::move(intf));
    // Dispatch Dellicius Queries
    std::vector<DelliciusQueryResult> response_entries =
        query_engine.ExecuteQuery(absl::FixedArray<absl::string_view>(
            query_ids_provided.begin(), query_ids_provided.end()));
    std::cout << "Output from Dellicius Query Engine: " << std::endl;
    std::for_each(
        response_entries.begin(), response_entries.end(),
        [](auto &entry) { std::cout << entry.DebugString() << std::endl; });
  }
  if (absl::GetFlag(FLAGS_metrics_enabled)) {
    std::string output_directory(
        absl::GetFlag(FLAGS_transport_metric_output_dir));
    std::ofstream metric_file_stream(
        JoinFilePaths(output_directory, kMetricOutId), std::ofstream::out);
    if (metric_file_stream.bad() || !metric_file_stream.is_open()) {
      LOG(ERROR) << "Error opening output file for storing transport metrics ";
      return -1;
    }
    metric_file_stream << std::setw(2) << transport_metrics.DebugString()
                       << std::endl;
  }
  return 0;
}

}  // namespace

}  // namespace ecclesia

int main(int argc, char **argv) { return ecclesia::QueryMain(argc, argv); }
