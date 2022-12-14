/*
 * Copyright 2021 Google LLC
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

#ifndef ECCLESIA_LIB_REDFISH_TRANSPORT_GRPC_TLS_OPTIONS_H_
#define ECCLESIA_LIB_REDFISH_TRANSPORT_GRPC_TLS_OPTIONS_H_

#include <memory>
#include <optional>
#include <string>

#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "grpcpp/security/credentials.h"
#include "grpcpp/security/tls_certificate_provider.h"
#include "grpcpp/security/tls_certificate_verifier.h"

namespace ecclesia {

// A class that generates gRPC ChannelCredentials for Redfish clients.
// This class takes static buffer to certificates or keys.
// Note: use GrpcTransportOptions if you need dynamic certificate loading.
class StaticBufferBasedTlsOptions {
 public:
  enum class AuthType {
    kInsecure,
    kTlsVerifyServer,
    kTlsVerifyServerSkipHostname,
    kTlsNotVerifyServer
  };

  StaticBufferBasedTlsOptions()
      : auth_type_(AuthType::kInsecure), timeout_(absl::Seconds(3)) {}

  virtual ~StaticBufferBasedTlsOptions() = default;

  // Authentication options.
  // Uses gRPC InsecureCredentials, via static credentials buffer.
  void SetToInsecure();

  // clients have been updated to set the `crl_directory`.

  // Uses gRPC TlsCredentials, via static credentials buffer.
  virtual void SetToTls(absl::string_view root_certs_buffer,
                        absl::string_view key_buffer,
                        absl::string_view cert_buffer,
                        std::optional<absl::string_view> crl_directory);
  virtual void SetToTls(absl::string_view root_certs_buffer,
                        absl::string_view key_buffer,
                        absl::string_view cert_buffer);

  // Uses gRPC TlsCredentials, but skip hostname check, via static credentials
  // buffer.
  virtual void SetToTlsSkipHostname(
      absl::string_view root_certs_buffer, absl::string_view key_buffer,
      absl::string_view cert_buffer,
      std::shared_ptr<grpc::experimental::CertificateVerifier> cert_verifier,
      std::optional<absl::string_view> crl_directory);
  virtual void SetToTlsSkipHostname(
      absl::string_view root_certs_buffer, absl::string_view key_buffer,
      absl::string_view cert_buffer,
      std::shared_ptr<grpc::experimental::CertificateVerifier> cert_verifier);

  // Uses gRPC TlsCredentials, but don't verify server at all, via static
  // credentials buffer.
  virtual void SetToTlsNotVerifyServer(
      absl::string_view key_buffer, absl::string_view cert_buffer,
      std::shared_ptr<grpc::experimental::CertificateVerifier> cert_verifier,
      std::optional<absl::string_view> crl_directory);
  virtual void SetToTlsNotVerifyServer(
      absl::string_view key_buffer, absl::string_view cert_buffer,
      std::shared_ptr<grpc::experimental::CertificateVerifier> cert_verifier);

  void SetTimeout(absl::Duration timeout) { timeout_ = timeout; }

  absl::Duration GetTimeout() const { return timeout_; }

  // Gets gRPC channel credentials according to authentication options.
  std::shared_ptr<grpc::ChannelCredentials> GetChannelCredentials() const;

 protected:
  virtual std::shared_ptr<grpc::experimental::CertificateProviderInterface>
  GetCertificateProvider() const;

  AuthType auth_type_;
  std::string root_certs_;
  grpc::experimental::IdentityKeyCertPair key_cert_;
  std::shared_ptr<grpc::experimental::CertificateVerifier> cert_verifier_;
  absl::Duration timeout_;
  std::optional<std::string> crl_directory_;
};

// A class that generates gRPC ChannelCredentials for Redfish clients.
// This class takes file paths to certificates or keys, and specify gRPC backend
// to reload credentials if changed.
class FileWatcherBasedOptions : public StaticBufferBasedTlsOptions {
 public:
  FileWatcherBasedOptions() = default;
  ~FileWatcherBasedOptions() override = default;

  using StaticBufferBasedTlsOptions::SetToTls;
  using StaticBufferBasedTlsOptions::SetToTlsNotVerifyServer;
  using StaticBufferBasedTlsOptions::SetToTlsSkipHostname;

  // Uses gRPC TlsCredentials.
  void SetToTls(absl::string_view root_certs_path, absl::string_view key_path,
                absl::string_view cert_path,
                std::optional<absl::string_view> crl_directory) override;

  // Uses gRPC TlsCredentials, but skip hostname check, via FileWatcher
  void SetToTlsSkipHostname(
      absl::string_view root_certs_path, absl::string_view key_path,
      absl::string_view cert_path,
      std::shared_ptr<grpc::experimental::CertificateVerifier> cert_verifier,
      std::optional<absl::string_view> crl_directory) override;

  // Uses gRPC TlsCredentials, but don't verify server at all, via FileWatcher
  void SetToTlsNotVerifyServer(
      absl::string_view key_path, absl::string_view cert_path,
      std::shared_ptr<grpc::experimental::CertificateVerifier> cert_verifier,
      std::optional<absl::string_view> crl_directory) override;

 private:
  std::shared_ptr<grpc::experimental::CertificateProviderInterface>
  GetCertificateProvider() const override;

  std::string root_certs_path_;
  std::string key_path_;
  std::string cert_path_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_TRANSPORT_GRPC_TLS_OPTIONS_H_
