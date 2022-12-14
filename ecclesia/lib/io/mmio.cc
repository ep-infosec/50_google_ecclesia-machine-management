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

#include "ecclesia/lib/io/mmio.h"

#include <cstddef>
#include <cstdint>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ecclesia/lib/codec/bits.h"
#include "ecclesia/lib/codec/endian.h"
#include "ecclesia/lib/file/mmap.h"
#include "ecclesia/lib/status/macros.h"

namespace ecclesia {

absl::StatusOr<MmioRangeFromFile> MmioRangeFromFile::Create(
    uint64_t base_address, size_t size, absl::string_view physical_mem_device) {
  if (size == 0) {
    return absl::InvalidArgumentError("you cannot mmap an empty range");
  }
  auto mmap =
      MappedMemory::Create(std::string(physical_mem_device), base_address, size,
                           MappedMemory::Type::kReadWrite);
  if (mmap.ok()) {
    return MmioRangeFromFile(size, *std::move(mmap));
  }
  return mmap.status();
}
absl::StatusOr<MmioRangeFromFile> MmioRangeFromFile::Create(
    AddressRange address_range, absl::string_view physical_mem_device) {
  return Create(address_range.FirstAddress(),
                address_range.Empty() ? 0
                                      : address_range.LastAddress() -
                                            address_range.FirstAddress() + 1,
                physical_mem_device);
}

absl::StatusOr<uint8_t> MmioRangeFromFile::Read8(OffsetType offset) const {
  uint8_t result;
  ECCLESIA_RETURN_IF_ERROR(
      Read<uint8_t>(offset, absl::MakeSpan(&result, sizeof(result))));
  return result;
}

absl::Status MmioRangeFromFile::Write8(OffsetType offset, uint8_t value) {
  ECCLESIA_RETURN_IF_ERROR(
      Write<uint8_t>(offset, absl::MakeSpan(&value, sizeof(value))));
  return absl::OkStatus();
}

absl::StatusOr<uint16_t> MmioRangeFromFile::Read16(OffsetType offset) const {
  uint8_t data[sizeof(uint16_t)];
  ECCLESIA_RETURN_IF_ERROR(
      Read<uint16_t>(offset, absl::MakeSpan(data, sizeof(data))));
  return LittleEndian::Load16(data);
}

absl::Status MmioRangeFromFile::Write16(OffsetType offset, uint16_t value) {
  uint8_t data[sizeof(uint16_t)];
  LittleEndian::Store16(value, data);
  return Write<uint16_t>(offset, absl::MakeSpan(data, sizeof(data)));
}

absl::StatusOr<uint32_t> MmioRangeFromFile::Read32(OffsetType offset) const {
  uint8_t data[sizeof(uint32_t)];
  ECCLESIA_RETURN_IF_ERROR(
      Read<uint32_t>(offset, absl::MakeSpan(data, sizeof(data))));
  return LittleEndian::Load32(data);
}

absl::Status MmioRangeFromFile::Write32(OffsetType offset, uint32_t value) {
  uint8_t data[sizeof(uint32_t)];
  LittleEndian::Store32(value, data);
  return Write<uint32_t>(offset, absl::MakeSpan(data, sizeof(data)));
}

}  // namespace ecclesia
