/*
 * Copyright 2010-2020 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license
 * that can be found in the LICENSE file.
 */

#include "AtomicReference.h"

#include "Exceptions.h"

namespace {

struct AtomicReferenceLayout {
  ObjHeader header;
  KRef value_;
  KInt lock_;
  KInt cookie_;
};

inline AtomicReferenceLayout* asAtomicReference(KRef thiz) {
    return reinterpret_cast<AtomicReferenceLayout*>(thiz);
}

}  // namespace

OBJ_GETTER(DerefAtomicReference, KRef thiz) {
    // Here we must take a lock to prevent race when value, while taken here, is CASed and immediately
    // destroyed by an another thread. AtomicReference no longer holds such an object, so if we got
    // rescheduled unluckily, between the moment value is read from the field and RC is incremented,
    // object may go away.
    AtomicReferenceLayout* ref = asAtomicReference(thiz);
    RETURN_RESULT_OF(ReadHeapRefLocked, &ref->value_, &ref->lock_, &ref->cookie_);
}

extern "C" {

void Kotlin_AtomicReference_checkIfFrozen(KRef value) {
    if (value != nullptr && !isPermanentOrFrozen(value)) {
        ThrowInvalidMutabilityException(value);
    }
}

OBJ_GETTER(Kotlin_AtomicReference_compareAndSwap, KRef thiz, KRef expectedValue, KRef newValue) {
    Kotlin_AtomicReference_checkIfFrozen(newValue);
    // See Kotlin_AtomicReference_get() for explanations, why locking is needed.
    AtomicReferenceLayout* ref = asAtomicReference(thiz);
    RETURN_RESULT_OF(SwapHeapRefLocked, &ref->value_, expectedValue, newValue,
        &ref->lock_, &ref->cookie_);
}

KBoolean Kotlin_AtomicReference_compareAndSet(KRef thiz, KRef expectedValue, KRef newValue) {
    Kotlin_AtomicReference_checkIfFrozen(newValue);
    // See Kotlin_AtomicReference_get() for explanations, why locking is needed.
    AtomicReferenceLayout* ref = asAtomicReference(thiz);
    ObjHolder holder;
    auto old = SwapHeapRefLocked(&ref->value_, expectedValue, newValue,
        &ref->lock_, &ref->cookie_, holder.slot());
    return old == expectedValue;
}

void Kotlin_AtomicReference_set(KRef thiz, KRef newValue) {
    Kotlin_AtomicReference_checkIfFrozen(newValue);
    AtomicReferenceLayout* ref = asAtomicReference(thiz);
    SetHeapRefLocked(&ref->value_, newValue, &ref->lock_, &ref->cookie_);
}

OBJ_GETTER(Kotlin_AtomicReference_get, KRef thiz) {
    RETURN_RESULT_OF(DerefAtomicReference, thiz);
}

}  // extern "C"
