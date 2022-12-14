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

#include "ecclesia/lib/redfish/pci.h"

#include <optional>

#include "ecclesia/lib/io/pci/location.h"
#include "ecclesia/lib/io/pci/signature.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/numbers.h"
#include "ecclesia/lib/redfish/property_definitions.h"

namespace ecclesia {

std::optional<ecclesia::PciDbdfLocation> ReadPciLocation(
    const RedfishObject &pci_location_obj) {
  auto maybe_domain =
      RedfishStrTo32Base<OemGooglePropertyDomain>(pci_location_obj);
  if (!maybe_domain.has_value()) return std::nullopt;

  auto maybe_bus = RedfishStrTo32Base<OemGooglePropertyBus>(pci_location_obj);
  if (!maybe_bus.has_value()) return std::nullopt;

  auto maybe_device =
      RedfishStrTo32Base<OemGooglePropertyDevice>(pci_location_obj);
  if (!maybe_device.has_value()) return std::nullopt;

  auto maybe_function =
      RedfishStrTo32Base<OemGooglePropertyFunction>(pci_location_obj);
  if (!maybe_function.has_value()) return std::nullopt;

  return ecclesia::PciDbdfLocation::TryMake(*maybe_domain, *maybe_bus,
                                            *maybe_device, *maybe_function);
}

std::optional<ecclesia::PciFullSignature> ReadPciFullSignature(
    const RedfishObject &pcie_function_obj) {
  auto maybe_vendor_id =
      RedfishStrTo32Base<PropertyVendorId>(pcie_function_obj);
  if (!maybe_vendor_id.has_value()) return std::nullopt;

  auto maybe_device_id =
      RedfishStrTo32Base<PropertyDeviceId>(pcie_function_obj);
  if (!maybe_device_id.has_value()) return std::nullopt;

  auto maybe_subsystem_id =
      RedfishStrTo32Base<PropertySubsystemId>(pcie_function_obj);
  if (!maybe_subsystem_id.has_value()) return std::nullopt;

  auto maybe_subsystem_vendor_id =
      RedfishStrTo32Base<PropertySubsystemVendorId>(pcie_function_obj);
  if (!maybe_subsystem_vendor_id.has_value()) return std::nullopt;

  return ecclesia::PciFullSignature::TryMake(*maybe_vendor_id, *maybe_device_id,
                                             *maybe_subsystem_vendor_id,
                                             *maybe_subsystem_id);
}

}  // namespace ecclesia
