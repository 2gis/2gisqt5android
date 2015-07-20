# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generate template contexts of dictionaries for both v8 bindings and
implementation classes that are used by blink's core/modules.
"""

import operator
from v8_globals import includes
import v8_types
import v8_utilities


DICTIONARY_H_INCLUDES = frozenset([
    'bindings/core/v8/V8Binding.h',
    'platform/heap/Handle.h',
])

DICTIONARY_CPP_INCLUDES = frozenset([
    'bindings/core/v8/ExceptionState.h',
    # FIXME: Remove this, http://crbug.com/321462
    'bindings/core/v8/Dictionary.h',
])


def setter_name_for_dictionary_member(member):
    name = v8_utilities.cpp_name(member)
    return 'set%s' % v8_utilities.capitalize(name)


def has_method_name_for_dictionary_member(member):
    name = v8_utilities.cpp_name(member)
    return 'has%s' % v8_utilities.capitalize(name)


def unwrap_nullable_if_needed(idl_type):
    if idl_type.is_nullable:
        return idl_type.inner_type
    return idl_type


# Context for V8 bindings

def dictionary_context(dictionary):
    includes.clear()
    includes.update(DICTIONARY_CPP_INCLUDES)
    return {
        'cpp_class': v8_utilities.cpp_name(dictionary),
        'header_includes': set(DICTIONARY_H_INCLUDES),
        'members': [member_context(member)
                    for member in sorted(dictionary.members,
                                         key=operator.attrgetter('name'))],
        'v8_class': v8_utilities.v8_class_name(dictionary),
    }


def member_context(member):
    idl_type = member.idl_type
    idl_type.add_includes_for_type()
    idl_type = unwrap_nullable_if_needed(idl_type)

    def default_values():
        if not member.default_value:
            return None, None
        if member.default_value.is_null:
            return None, 'v8::Null(isolate)'
        cpp_default_value = str(member.default_value)
        v8_default_value = idl_type.cpp_value_to_v8_value(
            cpp_value=cpp_default_value, isolate='isolate',
            creation_context='creationContext')
        return cpp_default_value, v8_default_value

    cpp_default_value, v8_default_value = default_values()
    cpp_name = v8_utilities.cpp_name(member)

    return {
        'cpp_default_value': cpp_default_value,
        'cpp_name': cpp_name,
        'cpp_type': idl_type.cpp_type,
        'cpp_value_to_v8_value': idl_type.cpp_value_to_v8_value(
            cpp_value='impl.%s()' % cpp_name, isolate='isolate',
            creation_context='creationContext',
            extended_attributes=member.extended_attributes),
        'enum_validation_expression': idl_type.enum_validation_expression,
        'has_method_name': has_method_name_for_dictionary_member(member),
        'is_object': idl_type.name == 'Object',
        'name': member.name,
        'setter_name': setter_name_for_dictionary_member(member),
        'v8_default_value': v8_default_value,
    }


# Context for implementation classes

def dictionary_impl_context(dictionary, interfaces_info):
    includes.clear()
    header_includes = set(['platform/heap/Handle.h'])
    return {
        'header_includes': header_includes,
        'cpp_class': v8_utilities.cpp_name(dictionary),
        'members': [member_impl_context(member, interfaces_info,
                                        header_includes)
                    for member in dictionary.members],
    }


def member_impl_context(member, interfaces_info, header_includes):
    idl_type = unwrap_nullable_if_needed(member.idl_type)
    is_object = idl_type.name == 'Object'
    cpp_name = v8_utilities.cpp_name(member)

    def getter_expression():
        if idl_type.impl_should_use_nullable_container:
            return 'm_%s.get()' % cpp_name
        return 'm_%s' % cpp_name

    def has_method_expression():
        if idl_type.impl_should_use_nullable_container or idl_type.is_enum or idl_type.is_string_type:
            return '!m_%s.isNull()' % cpp_name
        elif is_object:
            return '!(m_{0}.isEmpty() || m_{0}.isNull() || m_{0}.isUndefined())'.format(cpp_name)
        else:
            return 'm_%s' % cpp_name

    def member_cpp_type():
        member_cpp_type = idl_type.cpp_type_args(used_in_cpp_sequence=True)
        if idl_type.impl_should_use_nullable_container:
            return v8_types.cpp_template_type('Nullable', member_cpp_type)
        return member_cpp_type

    cpp_default_value = None
    if member.default_value and not member.default_value.is_null:
        cpp_default_value = str(member.default_value)

    header_includes.update(idl_type.impl_includes_for_type(interfaces_info))
    return {
        'cpp_default_value': cpp_default_value,
        'cpp_name': cpp_name,
        'getter_expression': getter_expression(),
        'has_method_expression': has_method_expression(),
        'has_method_name': has_method_name_for_dictionary_member(member),
        'is_object': is_object,
        'is_traceable': idl_type.is_traceable,
        'member_cpp_type': member_cpp_type(),
        'rvalue_cpp_type': idl_type.cpp_type_args(used_as_rvalue_type=True),
        'setter_name': setter_name_for_dictionary_member(member),
    }
