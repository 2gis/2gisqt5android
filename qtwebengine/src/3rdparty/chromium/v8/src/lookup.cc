// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/bootstrapper.h"
#include "src/lookup.h"

namespace v8 {
namespace internal {


void LookupIterator::Next() {
  has_property_ = false;
  do {
    state_ = LookupInHolder();
  } while (!IsFound() && NextHolder());
}


Handle<JSReceiver> LookupIterator::GetRoot() const {
  Handle<Object> receiver = GetReceiver();
  if (receiver->IsJSReceiver()) return Handle<JSReceiver>::cast(receiver);
  Context* native_context = isolate_->context()->native_context();
  JSFunction* function;
  if (receiver->IsNumber()) {
    function = native_context->number_function();
  } else if (receiver->IsString()) {
    function = native_context->string_function();
  } else if (receiver->IsSymbol()) {
    function = native_context->symbol_function();
  } else if (receiver->IsBoolean()) {
    function = native_context->boolean_function();
  } else {
    UNREACHABLE();
    function = NULL;
  }
  return handle(JSReceiver::cast(function->instance_prototype()));
}


Handle<Map> LookupIterator::GetReceiverMap() const {
  Handle<Object> receiver = GetReceiver();
  if (receiver->IsNumber()) return isolate_->factory()->heap_number_map();
  return handle(Handle<HeapObject>::cast(receiver)->map());
}


bool LookupIterator::NextHolder() {
  if (holder_map_->prototype()->IsNull()) return false;

  Handle<JSReceiver> next(JSReceiver::cast(holder_map_->prototype()));

  if (!check_derived() &&
      !(check_hidden() &&
         // TODO(verwaest): Check if this is actually necessary currently. If it
         // is, this should be handled by setting is_hidden_prototype on the
         // global object behind the proxy.
        (holder_map_->IsJSGlobalProxyMap() ||
         next->map()->is_hidden_prototype()))) {
    return false;
  }

  holder_map_ = handle(next->map());
  maybe_holder_ = next;
  return true;
}


LookupIterator::State LookupIterator::LookupInHolder() {
  switch (state_) {
    case NOT_FOUND:
      if (holder_map_->IsJSProxyMap()) {
        return JSPROXY;
      }
      if (check_access_check() && holder_map_->is_access_check_needed()) {
        return ACCESS_CHECK;
      }
      // Fall through.
    case ACCESS_CHECK:
      if (check_interceptor() && holder_map_->has_named_interceptor()) {
        return INTERCEPTOR;
      }
      // Fall through.
    case INTERCEPTOR:
      if (holder_map_->is_dictionary_map()) {
        property_encoding_ = DICTIONARY;
      } else {
        DescriptorArray* descriptors = holder_map_->instance_descriptors();
        number_ = descriptors->SearchWithCache(*name_, *holder_map_);
        if (number_ == DescriptorArray::kNotFound) return NOT_FOUND;
        property_encoding_ = DESCRIPTOR;
      }
      return PROPERTY;
    case PROPERTY:
      return NOT_FOUND;
    case JSPROXY:
      UNREACHABLE();
  }
  UNREACHABLE();
  return state_;
}


bool LookupIterator::IsBootstrapping() const {
  return isolate_->bootstrapper()->IsActive();
}


bool LookupIterator::HasAccess(v8::AccessType access_type) const {
  ASSERT_EQ(ACCESS_CHECK, state_);
  ASSERT(is_guaranteed_to_have_holder());
  return isolate_->MayNamedAccess(GetHolder(), name_, access_type);
}


bool LookupIterator::HasProperty() {
  ASSERT_EQ(PROPERTY, state_);
  ASSERT(is_guaranteed_to_have_holder());

  if (property_encoding_ == DICTIONARY) {
    Handle<JSObject> holder = GetHolder();
    number_ = holder->property_dictionary()->FindEntry(name_);
    if (number_ == NameDictionary::kNotFound) return false;

    property_details_ = GetHolder()->property_dictionary()->DetailsAt(number_);
    // Holes in dictionary cells are absent values unless marked as read-only.
    if (holder->IsGlobalObject() &&
        (property_details_.IsDeleted() ||
         (!property_details_.IsReadOnly() && FetchValue()->IsTheHole()))) {
      return false;
    }
  } else {
    property_details_ = holder_map_->instance_descriptors()->GetDetails(
        number_);
  }

  switch (property_details_.type()) {
    case v8::internal::FIELD:
    case v8::internal::NORMAL:
    case v8::internal::CONSTANT:
      property_kind_ = DATA;
      break;
    case v8::internal::CALLBACKS:
      property_kind_ = ACCESSOR;
      break;
    case v8::internal::HANDLER:
    case v8::internal::NONEXISTENT:
    case v8::internal::INTERCEPTOR:
      UNREACHABLE();
  }

  has_property_ = true;
  return true;
}


Handle<Object> LookupIterator::FetchValue() const {
  Object* result = NULL;
  switch (property_encoding_) {
    case DICTIONARY:
      result = GetHolder()->property_dictionary()->ValueAt(number_);
      if (GetHolder()->IsGlobalObject()) {
        result = PropertyCell::cast(result)->value();
      }
      break;
    case DESCRIPTOR:
      if (property_details_.type() == v8::internal::FIELD) {
        FieldIndex field_index = FieldIndex::ForDescriptor(
            *holder_map_, number_);
        return JSObject::FastPropertyAt(
            GetHolder(), property_details_.representation(), field_index);
      }
      result = holder_map_->instance_descriptors()->GetValue(number_);
  }
  return handle(result, isolate_);
}


Handle<Object> LookupIterator::GetAccessors() const {
  ASSERT(has_property_);
  ASSERT_EQ(ACCESSOR, property_kind_);
  return FetchValue();
}


Handle<Object> LookupIterator::GetDataValue() const {
  ASSERT(has_property_);
  ASSERT_EQ(DATA, property_kind_);
  Handle<Object> value = FetchValue();
  if (value->IsTheHole()) {
    ASSERT(property_details_.IsReadOnly());
    return factory()->undefined_value();
  }
  return value;
}


} }  // namespace v8::internal
