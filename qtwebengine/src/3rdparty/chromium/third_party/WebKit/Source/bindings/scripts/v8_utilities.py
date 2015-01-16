# Copyright (C) 2013 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Functions shared by various parts of the code generator.

Extends IdlType and IdlUnion type with |enum_validation_expression| property.

Design doc: http://www.chromium.org/developers/design-documents/idl-compiler
"""

import re

from idl_types import IdlType, IdlUnionType
import idl_types
from v8_globals import includes
import v8_types

ACRONYMS = [
    'CSSOM',  # must come *before* CSS to match full acronym
    'CSS',
    'HTML',
    'IME',
    'JS',
    'SVG',
    'URL',
    'WOFF',
    'XML',
    'XSLT',
]


################################################################################
# Extended attribute parsing
################################################################################

def extended_attribute_value_contains(extended_attribute_value, value):
    return (extended_attribute_value and
            value in re.split('[|&]', extended_attribute_value))


def has_extended_attribute(definition_or_member, extended_attribute_list):
    return any(extended_attribute in definition_or_member.extended_attributes
               for extended_attribute in extended_attribute_list)


def has_extended_attribute_value(definition_or_member, name, value):
    extended_attributes = definition_or_member.extended_attributes
    return (name in extended_attributes and
            extended_attribute_value_contains(extended_attributes[name], value))


################################################################################
# String handling
################################################################################

def capitalize(name):
    """Capitalize first letter or initial acronym (used in setter names)."""
    for acronym in ACRONYMS:
        if name.startswith(acronym.lower()):
            return name.replace(acronym.lower(), acronym)
    return name[0].upper() + name[1:]


def strip_suffix(string, suffix):
    if not suffix or not string.endswith(suffix):
        return string
    return string[:-len(suffix)]


def uncapitalize(name):
    """Uncapitalizes first letter or initial acronym (used in method names).

    E.g., 'SetURL' becomes 'setURL', but 'URLFoo' becomes 'urlFoo'.
    """
    for acronym in ACRONYMS:
        if name.startswith(acronym):
            return name.replace(acronym, acronym.lower())
    return name[0].lower() + name[1:]


################################################################################
# C++
################################################################################

def enum_validation_expression(idl_type):
    # FIXME: Add IdlEnumType, move property to derived type, and remove this check
    if not idl_type.is_enum:
        return None
    return ' || '.join(['string == "%s"' % enum_value
                        for enum_value in idl_type.enum_values])
IdlType.enum_validation_expression = property(enum_validation_expression)


def scoped_name(interface, definition, base_name):
    # partial interfaces are implemented as separate classes, with their members
    # implemented as static member functions
    partial_interface_implemented_as = definition.extended_attributes.get('PartialInterfaceImplementedAs')
    if partial_interface_implemented_as:
        return '%s::%s' % (partial_interface_implemented_as, base_name)
    if (definition.is_static or
        definition.name in ('Constructor', 'NamedConstructor')):
        return '%s::%s' % (cpp_name(interface), base_name)
    return 'impl->%s' % base_name


def v8_class_name(interface):
    return v8_types.v8_type(interface.name)


################################################################################
# Specific extended attributes
################################################################################

# [ActivityLogging]
def activity_logging_world_list(member, access_type=''):
    """Returns a set of world suffixes for which a definition member has activity logging, for specified access type.

    access_type can be 'Getter' or 'Setter' if only checking getting or setting.
    """
    extended_attributes = member.extended_attributes
    if 'LogActivity' not in extended_attributes:
        return set()
    log_activity = extended_attributes['LogActivity']
    if log_activity and not log_activity.startswith(access_type):
        return set()

    includes.add('bindings/v8/V8DOMActivityLogger.h')
    if 'LogAllWorlds' in extended_attributes:
        return set(['', 'ForMainWorld'])
    return set([''])  # At minimum, include isolated worlds.


# [ActivityLogging]
def activity_logging_world_check(member):
    """Returns if an isolated world check is required when generating activity
    logging code.

    The check is required when there is no per-world binding code and logging is
    required only for isolated world.
    """
    extended_attributes = member.extended_attributes
    if 'LogActivity' not in extended_attributes:
        return False
    if ('PerWorldBindings' not in extended_attributes and
        'LogAllWorlds' not in extended_attributes):
        return True
    return False


# [CallWith]
CALL_WITH_ARGUMENTS = {
    'ScriptState': 'scriptState',
    'ExecutionContext': 'executionContext',
    'ScriptArguments': 'scriptArguments.release()',
    'ActiveWindow': 'callingDOMWindow(info.GetIsolate())',
    'FirstWindow': 'enteredDOMWindow(info.GetIsolate())',
    'Document': 'document',
}
# List because key order matters, as we want arguments in deterministic order
CALL_WITH_VALUES = [
    'ScriptState',
    'ExecutionContext',
    'ScriptArguments',
    'ActiveWindow',
    'FirstWindow',
    'Document',
]


def call_with_arguments(call_with_values):
    if not call_with_values:
        return []
    return [CALL_WITH_ARGUMENTS[value]
            for value in CALL_WITH_VALUES
            if extended_attribute_value_contains(call_with_values, value)]


# [Conditional]
def conditional_string(definition_or_member):
    extended_attributes = definition_or_member.extended_attributes
    if 'Conditional' not in extended_attributes:
        return None
    conditional = extended_attributes['Conditional']
    for operator in '&|':
        if operator in conditional:
            conditions = conditional.split(operator)
            operator_separator = ' %s%s ' % (operator, operator)
            return operator_separator.join('ENABLE(%s)' % expression for expression in sorted(conditions))
    return 'ENABLE(%s)' % conditional


# [DeprecateAs]
def deprecate_as(member):
    extended_attributes = member.extended_attributes
    if 'DeprecateAs' not in extended_attributes:
        return None
    includes.add('core/frame/UseCounter.h')
    return extended_attributes['DeprecateAs']


# [GarbageCollected], [WillBeGarbageCollected]
def gc_type(definition):
    extended_attributes = definition.extended_attributes
    if 'GarbageCollected' in extended_attributes:
        return 'GarbageCollectedObject'
    elif 'WillBeGarbageCollected' in extended_attributes:
        return 'WillBeGarbageCollectedObject'
    return 'RefCountedObject'


# [ImplementedAs]
def cpp_name(definition_or_member):
    extended_attributes = definition_or_member.extended_attributes
    if 'ImplementedAs' not in extended_attributes:
        return definition_or_member.name
    return extended_attributes['ImplementedAs']


# [MeasureAs]
def measure_as(definition_or_member):
    extended_attributes = definition_or_member.extended_attributes
    if 'MeasureAs' not in extended_attributes:
        return None
    includes.add('core/frame/UseCounter.h')
    return extended_attributes['MeasureAs']


# [PerContextEnabled]
def per_context_enabled_function_name(definition_or_member):
    extended_attributes = definition_or_member.extended_attributes
    if 'PerContextEnabled' not in extended_attributes:
        return None
    feature_name = extended_attributes['PerContextEnabled']
    return 'ContextFeatures::%sEnabled' % uncapitalize(feature_name)


# [RuntimeEnabled]
def runtime_enabled_function_name(definition_or_member):
    """Returns the name of the RuntimeEnabledFeatures function.

    The returned function checks if a method/attribute is enabled.
    Given extended attribute RuntimeEnabled=FeatureName, return:
        RuntimeEnabledFeatures::{featureName}Enabled
    """
    extended_attributes = definition_or_member.extended_attributes
    if 'RuntimeEnabled' not in extended_attributes:
        return None
    feature_name = extended_attributes['RuntimeEnabled']
    return 'RuntimeEnabledFeatures::%sEnabled' % uncapitalize(feature_name)
