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

// This header provides the view aspect of a generic shared data store that can
// be updated with Read-Copy-Update (RCU) style semantics.
//
// The class template provided by this header is the RcuView interface. This
// defines a very simple API: it exposes the Read() function, equivalent to that
// provides by an RcuStore. It is useful for exposing a readable (but not
// writable) store to clients. The RcuDirectView implementation is just that: a
// trivial implementation of the RcuView interface that can be layered directly
// on top of an RcuStore.
//
// However, it can also be used in more powerful ways. Because the view is an
// interface it is also possible to roll your own implementation of it for cases
// where you want to offer different semantics. For example, you could implement
// a view that collects data from several different stores and combines them
// into a single snapshot.

#ifndef ECCLESIA_LIB_CACHE_RCU_VIEW_H_
#define ECCLESIA_LIB_CACHE_RCU_VIEW_H_

#include <optional>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "ecclesia/lib/cache/rcu_snapshot.h"
#include "ecclesia/lib/cache/rcu_store.h"

namespace ecclesia {

template <typename T>
class RcuView {
 public:
  virtual ~RcuView() = default;
  virtual RcuSnapshot<T> Read() const = 0;
};

// Simple implementation of the RcuView<T> interface that does direct
// passthrough data from an underlying RcuStore<T>. Useful in cases where your
// interface and the underlying store perfectly align.
//
// NOTE: This stores a _reference_ to the RcuStore<T> that it wraps. So it is
// very important that the lifetime of the store exceeds the lifetime of the
// view. Usually you want the RcuDirectView object to be owned by the same thing
// that owns the RcuStore.
template <typename T>
class RcuDirectView final : public RcuView<T> {
 public:
  explicit RcuDirectView(const RcuStore<T> &store) : store_(store) {}

  // Copying these can be dangerous because it stores a reference.
  RcuDirectView(const RcuDirectView &other) = delete;
  RcuDirectView &operator=(const RcuDirectView &other) = delete;

  RcuSnapshot<T> Read() const override { return store_.Read(); }

 private:
  const RcuStore<T> &store_;
};

// Adapter implementation of the RcuView<T> to allow translating from a backing
// RcuStore or RcuView of type FromType to a view of type ToType. This
// implementation allows subclasses to provide a Translate function for creating
// the ToType view based on FromType. An internal cache is used to hold the
// translation until the underlying view emits a change notification.
//
// NOTE: This stores a _reference_ to the RcuStore<T> or RcuView<T> that it
// wraps. So it is very important that the lifetime of the underlying object
// exceeds the lifetime of the view.
template <typename FromType, typename ToType>
class TranslatedRcuView : public RcuView<ToType> {
 public:
  // Construct a translated view. It can be wrapped around either a store or a
  // view. Note that in both cases it will capture a reference.
  explicit TranslatedRcuView(const RcuView<FromType> &view) : view_(view) {}
  explicit TranslatedRcuView(const RcuStore<FromType> &store)
      : wrapped_store_(store), view_(*wrapped_store_) {}

  // Copying these can be dangerous because it stores a reference.
  TranslatedRcuView(const TranslatedRcuView &other) = delete;
  TranslatedRcuView &operator=(const TranslatedRcuView &other) = delete;

  // Read will check if the current snapshot is still fresh. If it is not it
  // will rebuild the cache using the subclass Translate function. If the store
  // is still fresh then the cached snapshot will be returned.
  RcuSnapshot<ToType> Read() const override {
    absl::MutexLock ml(&cache_.mutex);
    if (!cache_.to_snapshot.IsFresh()) {
      auto from_snapshot = view_.Read();
      ToType new_translation = Translate(*from_snapshot);
      cache_.to_snapshot = RcuSnapshot<ToType>::CreateDependent(
          RcuSnapshotDependsOn(from_snapshot), std::move(new_translation));
    }
    return cache_.to_snapshot;
  }

  // Subclasses will override this with an implementation which translate the
  // store's data of type FromType to a view of type ToType.
  virtual ToType Translate(const FromType &to) const = 0;

 private:
  // If this translated view is capturing an RcuStore instead of an RcuView then
  // this provides a direct view that the view_ member will capture. Otherwise
  // this will be null. This should never be used directly.
  const std::optional<RcuDirectView<FromType>> wrapped_store_;

  // The underlying view being referenced. When this is constructed on top of a
  // view this will be that view; when constructed on top of a store it will be
  // wrapped by wrapped_store_ and that wrapper will be referenced here.
  const RcuView<FromType> &view_;

  // Internal cache storing the translation of the ToType view based on store_.
  mutable struct Cache {
    Cache() : to_snapshot(RcuSnapshot<ToType>::CreateStale()) {}

    absl::Mutex mutex;
    RcuSnapshot<ToType> to_snapshot ABSL_GUARDED_BY(mutex);
  } cache_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_CACHE_RCU_VIEW_H_
