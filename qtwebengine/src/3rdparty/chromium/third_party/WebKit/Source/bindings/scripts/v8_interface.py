# Copyright (C) 2013 Google Inc. All rights reserved.
# coding=utf-8
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

"""Generate template values for an interface.

Design doc: http://www.chromium.org/developers/design-documents/idl-compiler
"""

from collections import defaultdict
import itertools
from operator import itemgetter

import idl_types
from idl_types import IdlType, inherits_interface
import v8_attributes
from v8_globals import includes
import v8_methods
import v8_types
from v8_types import cpp_ptr_type, cpp_template_type
import v8_utilities
from v8_utilities import capitalize, conditional_string, cpp_name, gc_type, has_extended_attribute_value, runtime_enabled_function_name


INTERFACE_H_INCLUDES = frozenset([
    'bindings/v8/V8Binding.h',
    'bindings/v8/V8DOMWrapper.h',
    'bindings/v8/WrapperTypeInfo.h',
    'platform/heap/Handle.h',
])
INTERFACE_CPP_INCLUDES = frozenset([
    'bindings/v8/ExceptionState.h',
    'bindings/v8/V8DOMConfiguration.h',
    'bindings/v8/V8HiddenValue.h',
    'bindings/v8/V8ObjectConstructor.h',
    'core/dom/ContextFeatures.h',
    'core/dom/Document.h',
    'platform/RuntimeEnabledFeatures.h',
    'platform/TraceEvent.h',
    'wtf/GetPtr.h',
    'wtf/RefPtr.h',
])


def generate_interface(interface):
    includes.clear()
    includes.update(INTERFACE_CPP_INCLUDES)
    header_includes = set(INTERFACE_H_INCLUDES)

    parent_interface = interface.parent
    if parent_interface:
        header_includes.update(v8_types.includes_for_interface(parent_interface))
    extended_attributes = interface.extended_attributes

    is_audio_buffer = inherits_interface(interface.name, 'AudioBuffer')
    if is_audio_buffer:
        includes.add('modules/webaudio/AudioBuffer.h')

    is_document = inherits_interface(interface.name, 'Document')
    if is_document:
        includes.update(['bindings/v8/ScriptController.h',
                         'bindings/v8/V8WindowShell.h',
                         'core/frame/LocalFrame.h'])

    # [ActiveDOMObject]
    is_active_dom_object = 'ActiveDOMObject' in extended_attributes

    # [CheckSecurity]
    is_check_security = 'CheckSecurity' in extended_attributes
    if is_check_security:
        includes.add('bindings/v8/BindingSecurity.h')

    # [DependentLifetime]
    is_dependent_lifetime = 'DependentLifetime' in extended_attributes

    # [MeasureAs]
    is_measure_as = 'MeasureAs' in extended_attributes
    if is_measure_as:
        includes.add('core/frame/UseCounter.h')

    # [SetWrapperReferenceFrom]
    reachable_node_function = extended_attributes.get('SetWrapperReferenceFrom')
    if reachable_node_function:
        includes.update(['bindings/v8/V8GCController.h',
                         'core/dom/Element.h'])

    # [SetWrapperReferenceTo]
    set_wrapper_reference_to_list = [{
        'name': argument.name,
        # FIXME: properly should be:
        # 'cpp_type': argument.idl_type.cpp_type_args(used_as_argument=True),
        # (if type is non-wrapper type like NodeFilter, normally RefPtr)
        # Raw pointers faster though, and NodeFilter hacky anyway.
        'cpp_type': argument.idl_type.implemented_as + '*',
        'idl_type': argument.idl_type,
        'v8_type': v8_types.v8_type(argument.idl_type.name),
    } for argument in extended_attributes.get('SetWrapperReferenceTo', [])]
    for set_wrapper_reference_to in set_wrapper_reference_to_list:
        set_wrapper_reference_to['idl_type'].add_includes_for_type()

    # [SpecialWrapFor]
    if 'SpecialWrapFor' in extended_attributes:
        special_wrap_for = extended_attributes['SpecialWrapFor'].split('|')
    else:
        special_wrap_for = []
    for special_wrap_interface in special_wrap_for:
        v8_types.add_includes_for_interface(special_wrap_interface)

    # [Custom=Wrap], [SetWrapperReferenceFrom]
    has_visit_dom_wrapper = (
        has_extended_attribute_value(interface, 'Custom', 'VisitDOMWrapper') or
        reachable_node_function or
        set_wrapper_reference_to_list)

    this_gc_type = gc_type(interface)

    template_contents = {
        'conditional_string': conditional_string(interface),  # [Conditional]
        'cpp_class': cpp_name(interface),
        'gc_type': this_gc_type,
        'has_custom_legacy_call_as_function': has_extended_attribute_value(interface, 'Custom', 'LegacyCallAsFunction'),  # [Custom=LegacyCallAsFunction]
        'has_custom_to_v8': has_extended_attribute_value(interface, 'Custom', 'ToV8'),  # [Custom=ToV8]
        'has_custom_wrap': has_extended_attribute_value(interface, 'Custom', 'Wrap'),  # [Custom=Wrap]
        'has_visit_dom_wrapper': has_visit_dom_wrapper,
        'header_includes': header_includes,
        'interface_name': interface.name,
        'is_active_dom_object': is_active_dom_object,
        'is_audio_buffer': is_audio_buffer,
        'is_check_security': is_check_security,
        'is_dependent_lifetime': is_dependent_lifetime,
        'is_document': is_document,
        'is_event_target': inherits_interface(interface.name, 'EventTarget'),
        'is_exception': interface.is_exception,
        'is_node': inherits_interface(interface.name, 'Node'),
        'measure_as': v8_utilities.measure_as(interface),  # [MeasureAs]
        'parent_interface': parent_interface,
        'pass_cpp_type': cpp_template_type(
            cpp_ptr_type('PassRefPtr', 'RawPtr', this_gc_type),
            cpp_name(interface)),
        'reachable_node_function': reachable_node_function,
        'runtime_enabled_function': runtime_enabled_function_name(interface),  # [RuntimeEnabled]
        'set_wrapper_reference_to_list': set_wrapper_reference_to_list,
        'special_wrap_for': special_wrap_for,
        'v8_class': v8_utilities.v8_class_name(interface),
        'wrapper_configuration': 'WrapperConfiguration::Dependent'
            if (has_visit_dom_wrapper or
                is_active_dom_object or
                is_dependent_lifetime)
            else 'WrapperConfiguration::Independent',
    }

    # Constructors
    constructors = [generate_constructor(interface, constructor)
                    for constructor in interface.constructors
                    # FIXME: shouldn't put named constructors with constructors
                    # (currently needed for Perl compatibility)
                    # Handle named constructors separately
                    if constructor.name == 'Constructor']
    if len(constructors) > 1:
        template_contents['constructor_overloads'] = generate_overloads(constructors)

    # [CustomConstructor]
    custom_constructors = [{  # Only needed for computing interface length
        'number_of_required_arguments':
            number_of_required_arguments(constructor),
    } for constructor in interface.custom_constructors]

    # [EventConstructor]
    has_event_constructor = 'EventConstructor' in extended_attributes
    any_type_attributes = [attribute for attribute in interface.attributes
                           if attribute.idl_type.name == 'Any']
    if has_event_constructor:
        includes.add('bindings/v8/Dictionary.h')
        if any_type_attributes:
            includes.add('bindings/v8/SerializedScriptValue.h')

    # [NamedConstructor]
    named_constructor = generate_named_constructor(interface)

    if (constructors or custom_constructors or has_event_constructor or
        named_constructor):
        includes.add('bindings/v8/V8ObjectConstructor.h')
        includes.add('core/frame/LocalDOMWindow.h')

    template_contents.update({
        'any_type_attributes': any_type_attributes,
        'constructors': constructors,
        'has_custom_constructor': bool(custom_constructors),
        'has_event_constructor': has_event_constructor,
        'interface_length':
            interface_length(interface, constructors + custom_constructors),
        'is_constructor_call_with_document': has_extended_attribute_value(
            interface, 'ConstructorCallWith', 'Document'),  # [ConstructorCallWith=Document]
        'is_constructor_call_with_execution_context': has_extended_attribute_value(
            interface, 'ConstructorCallWith', 'ExecutionContext'),  # [ConstructorCallWith=ExeuctionContext]
        'is_constructor_raises_exception': extended_attributes.get('RaisesException') == 'Constructor',  # [RaisesException=Constructor]
        'named_constructor': named_constructor,
    })

    # Constants
    template_contents.update({
        'constants': [generate_constant(constant) for constant in interface.constants],
        'do_not_check_constants': 'DoNotCheckConstants' in extended_attributes,
    })

    # Attributes
    attributes = [v8_attributes.generate_attribute(interface, attribute)
                  for attribute in interface.attributes]
    template_contents.update({
        'attributes': attributes,
        'has_accessors': any(attribute['is_expose_js_accessors'] for attribute in attributes),
        'has_attribute_configuration': any(
             not (attribute['is_expose_js_accessors'] or
                  attribute['is_static'] or
                  attribute['runtime_enabled_function'] or
                  attribute['per_context_enabled_function'])
             for attribute in attributes),
        'has_constructor_attributes': any(attribute['constructor_type'] for attribute in attributes),
        'has_per_context_enabled_attributes': any(attribute['per_context_enabled_function'] for attribute in attributes),
        'has_replaceable_attributes': any(attribute['is_replaceable'] for attribute in attributes),
    })

    # Methods
    methods = [v8_methods.generate_method(interface, method)
               for method in interface.operations
               if method.name]  # Skip anonymous special operations (methods)
    generate_method_overloads(methods)

    per_context_enabled_methods = []
    custom_registration_methods = []
    method_configuration_methods = []

    for method in methods:
        # Skip all but one method in each set of overloaded methods.
        if 'overload_index' in method and 'overloads' not in method:
            continue

        if 'overloads' in method:
            overloads = method['overloads']
            per_context_enabled_function = overloads['per_context_enabled_function_all']
            runtime_enabled_function = overloads['runtime_enabled_function_all']
            has_custom_registration = overloads['has_custom_registration_all']
        else:
            per_context_enabled_function = method['per_context_enabled_function']
            runtime_enabled_function = method['runtime_enabled_function']
            has_custom_registration = method['has_custom_registration']

        if per_context_enabled_function:
            per_context_enabled_methods.append(method)
            continue
        if runtime_enabled_function or has_custom_registration:
            custom_registration_methods.append(method)
            continue
        method_configuration_methods.append(method)

    for method in methods:
        # The value of the Function object’s “length” property is a Number
        # determined as follows:
        # 1. Let S be the effective overload set for regular operations (if the
        # operation is a regular operation) or for static operations (if the
        # operation is a static operation) with identifier id on interface I and
        # with argument count 0.
        # 2. Return the length of the shortest argument list of the entries in S.
        # FIXME: This calculation doesn't take into account whether runtime
        # enabled overloads are actually enabled, so length may be incorrect.
        # E.g., [RuntimeEnabled=Foo] void f(); void f(long x);
        # should have length 1 if Foo is not enabled, but length 0 if it is.
        method['length'] = (method['overloads']['minarg'] if 'overloads' in method else
                            method['number_of_required_arguments'])

    template_contents.update({
        'custom_registration_methods': custom_registration_methods,
        'has_origin_safe_method_setter': any(
            method['is_check_security_for_frame'] and not method['is_read_only']
            for method in methods),
        'method_configuration_methods': method_configuration_methods,
        'per_context_enabled_methods': per_context_enabled_methods,
        'methods': methods,
    })

    template_contents.update({
        'indexed_property_getter': indexed_property_getter(interface),
        'indexed_property_setter': indexed_property_setter(interface),
        'indexed_property_deleter': indexed_property_deleter(interface),
        'is_override_builtins': 'OverrideBuiltins' in extended_attributes,
        'named_property_getter': named_property_getter(interface),
        'named_property_setter': named_property_setter(interface),
        'named_property_deleter': named_property_deleter(interface),
    })

    return template_contents


# [DeprecateAs], [Reflect], [RuntimeEnabled]
def generate_constant(constant):
    # (Blink-only) string literals are unquoted in tokenizer, must be re-quoted
    # in C++.
    if constant.idl_type.name == 'String':
        value = '"%s"' % constant.value
    else:
        value = constant.value

    extended_attributes = constant.extended_attributes
    return {
        'cpp_class': extended_attributes.get('PartialInterfaceImplementedAs'),
        'name': constant.name,
        # FIXME: use 'reflected_name' as correct 'name'
        'reflected_name': extended_attributes.get('Reflect', constant.name),
        'runtime_enabled_function': runtime_enabled_function_name(constant),
        'value': value,
    }


################################################################################
# Overloads
################################################################################

def generate_method_overloads(methods):
    # Regular methods
    generate_overloads_by_type([method for method in methods
                                if not method['is_static']])
    # Static methods
    generate_overloads_by_type([method for method in methods
                                if method['is_static']])


def generate_overloads_by_type(methods):
    """Generates |method.overload*| template values.

    Called separately for static and non-static (regular) methods,
    as these are overloaded separately.
    Modifies |method| in place for |method| in |methods|.
    Doesn't change the |methods| list itself (only the values, i.e. individual
    methods), so ok to treat these separately.
    """
    # Add overload information only to overloaded methods, so template code can
    # easily verify if a function is overloaded
    for name, overloads in method_overloads_by_name(methods):
        # Resolution function is generated after last overloaded function;
        # package necessary information into |method.overloads| for that method.
        overloads[-1]['overloads'] = generate_overloads(overloads)
        overloads[-1]['overloads']['name'] = name


def method_overloads_by_name(methods):
    """Returns generator of overloaded methods by name: [name, [method]]"""
    # Filter to only methods that are actually overloaded
    method_counts = Counter(method['name'] for method in methods)
    overloaded_method_names = set(name
                                  for name, count in method_counts.iteritems()
                                  if count > 1)
    overloaded_methods = [method for method in methods
                          if method['name'] in overloaded_method_names]

    # Group by name (generally will be defined together, but not necessarily)
    return sort_and_groupby(overloaded_methods, itemgetter('name'))


def generate_overloads(overloads):
    """Returns |overloads| template values for a single name.

    Sets |method.overload_index| in place for |method| in |overloads|
    and returns dict of overall overload template values.
    """
    assert len(overloads) > 1  # only apply to overloaded names
    for index, method in enumerate(overloads, 1):
        method['overload_index'] = index

    effective_overloads_by_length = effective_overload_set_by_length(overloads)
    lengths = [length for length, _ in effective_overloads_by_length]
    name = overloads[0].get('name', '<constructor>')

    # Check and fail if all overloads with the shortest acceptable arguments
    # list are runtime enabled, since we would otherwise set 'length' on the
    # function object to an incorrect value when none of those overloads were
    # actually enabled at runtime. The exception is if all overloads are
    # controlled by the same runtime enabled feature, in which case there would
    # be no function object at all if it is not enabled.
    shortest_overloads = effective_overloads_by_length[0][1]
    if (all(method.get('runtime_enabled_function')
            for method, _, _ in shortest_overloads) and
        not common_value(overloads, 'runtime_enabled_function')):
        raise ValueError('Function.length of %s depends on runtime enabled features' % name)

    # Check and fail if overloads disagree on any of the extended attributes
    # that affect how the method should be registered.
    # Skip the check for overloaded constructors, since they don't support any
    # of the extended attributes in question.
    if not overloads[0].get('is_constructor'):
        overload_extended_attributes = [
            method['custom_registration_extended_attributes']
            for method in overloads]
        for extended_attribute in v8_methods.CUSTOM_REGISTRATION_EXTENDED_ATTRIBUTES:
            if common_key(overload_extended_attributes, extended_attribute) is None:
                raise ValueError('Overloads of %s have conflicting extended attribute %s'
                                 % (name, extended_attribute))

    return {
        'deprecate_all_as': common_value(overloads, 'deprecate_as'),  # [DeprecateAs]
        'length_tests_methods': length_tests_methods(effective_overloads_by_length),
        'minarg': lengths[0],
        # 1. Let maxarg be the length of the longest type list of the
        # entries in S.
        'maxarg': lengths[-1],
        'measure_all_as': common_value(overloads, 'measure_as'),  # [MeasureAs]
        'has_custom_registration_all': common_value(overloads, 'has_custom_registration'),
        'per_context_enabled_function_all': common_value(overloads, 'per_context_enabled_function'),  # [PerContextEnabled]
        'runtime_enabled_function_all': common_value(overloads, 'runtime_enabled_function'),  # [RuntimeEnabled]
        'valid_arities': lengths
            # Only need to report valid arities if there is a gap in the
            # sequence of possible lengths, otherwise invalid length means
            # "not enough arguments".
            if lengths[-1] - lengths[0] != len(lengths) - 1 else None,
    }


def effective_overload_set(F):
    """Returns the effective overload set of an overloaded function.

    An effective overload set is the set of overloaded functions + signatures
    (type list of arguments, with optional and variadic arguments included or
    not), and is used in the overload resolution algorithm.

    For example, given input [f1(optional long x), f2(DOMString s)], the output
    is informally [f1(), f1(long), f2(DOMString)], and formally
    [(f1, [], []), (f1, [long], [optional]), (f2, [DOMString], [required])].

    Currently the optionality list is a list of |is_optional| booleans (True
    means optional, False means required); to support variadics this needs to
    be tri-valued as required, optional, or variadic.

    Formally:
    An effective overload set represents the allowable invocations for a
    particular operation, constructor (specified with [Constructor] or
    [NamedConstructor]), legacy caller or callback function.

    An additional argument N (argument count) is needed when overloading
    variadics, but we don't use that currently.

    Spec: http://heycam.github.io/webidl/#dfn-effective-overload-set

    Formally the input and output lists are sets, but methods are stored
    internally as dicts, which can't be stored in a set because they are not
    hashable, so we use lists instead.

    Arguments:
        F: list of overloads for a given callable name.

    Returns:
        S: list of tuples of the form (callable, type list, optionality list).
    """
    # Code closely follows the algorithm in the spec, for clarity and
    # correctness, and hence is not very Pythonic.

    # 1. Initialize S to ∅.
    # (We use a list because we can't use a set, as noted above.)
    S = []

    # 2. Let F be a set with elements as follows, according to the kind of
    # effective overload set:
    # (Passed as argument, nothing to do.)

    # 3. & 4. (maxarg, m) are only needed for variadics, not used.

    # 5. For each operation, extended attribute or callback function X in F:
    for X in F:  # X is the "callable", F is the overloads.
        arguments = X['arguments']
        # 1. Let n be the number of arguments X is declared to take.
        n = len(arguments)
        # 2. Let t0..n−1 be a list of types, where ti is the type of X’s
        # argument at index i.
        # (“type list”)
        t = tuple(argument['idl_type_object'] for argument in arguments)
        # 3. Let o0..n−1 be a list of optionality values, where oi is “variadic”
        # if X’s argument at index i is a final, variadic argument, “optional”
        # if the argument is optional, and “required” otherwise.
        # (“optionality list”)
        # (We’re just using a boolean for optional vs. required.)
        o = tuple(argument['is_optional'] for argument in arguments)
        # 4. Add to S the tuple <X, t0..n−1, o0..n−1>.
        S.append((X, t, o))
        # 5. If X is declared to be variadic, then:
        # (Not used, so not implemented.)
        # 6. Initialize i to n−1.
        i = n - 1
        # 7. While i ≥ 0:
        # Spec bug (fencepost error); should be “While i > 0:”
        # https://www.w3.org/Bugs/Public/show_bug.cgi?id=25590
        while i > 0:
            # 1. If argument i of X is not optional, then break this loop.
            if not o[i]:
                break
            # 2. Otherwise, add to S the tuple <X, t0..i−1, o0..i−1>.
            S.append((X, t[:i], o[:i]))
            # 3. Set i to i−1.
            i = i - 1
        # 8. If n > 0 and all arguments of X are optional, then add to S the
        # tuple <X, (), ()> (where “()” represents the empty list).
        if n > 0 and all(oi for oi in o):
            S.append((X, [], []))
    # 6. The effective overload set is S.
    return S


def effective_overload_set_by_length(overloads):
    def type_list_length(entry):
        # Entries in the effective overload set are 3-tuples:
        # (callable, type list, optionality list)
        return len(entry[1])

    effective_overloads = effective_overload_set(overloads)
    return list(sort_and_groupby(effective_overloads, type_list_length))


def distinguishing_argument_index(entries):
    """Returns the distinguishing argument index for a sequence of entries.

    Entries are elements of the effective overload set with the same number
    of arguments (formally, same type list length), each a 3-tuple of the form
    (callable, type list, optionality list).

    Spec: http://heycam.github.io/webidl/#dfn-distinguishing-argument-index

    If there is more than one entry in an effective overload set that has a
    given type list length, then for those entries there must be an index i
    such that for each pair of entries the types at index i are
    distinguishable.
    The lowest such index is termed the distinguishing argument index for the
    entries of the effective overload set with the given type list length.
    """
    # Only applicable “If there is more than one entry”
    assert len(entries) > 1
    type_lists = [tuple(idl_type.name for idl_type in entry[1])
                  for entry in entries]
    type_list_length = len(type_lists[0])
    # Only applicable for entries that “[have] a given type list length”
    assert all(len(type_list) == type_list_length for type_list in type_lists)
    name = entries[0][0].get('name', 'Constructor')  # for error reporting

    # The spec defines the distinguishing argument index by conditions it must
    # satisfy, but does not give an algorithm.
    #
    # We compute the distinguishing argument index by first computing the
    # minimum index where not all types are the same, and then checking that
    # all types in this position are distinguishable (and the optionality lists
    # up to this point are identical), since "minimum index where not all types
    # are the same" is a *necessary* condition, and more direct to check than
    # distinguishability.
    types_by_index = (set(types) for types in zip(*type_lists))
    try:
        # “In addition, for each index j, where j is less than the
        #  distinguishing argument index for a given type list length, the types
        #  at index j in all of the entries’ type lists must be the same”
        index = next(i for i, types in enumerate(types_by_index)
                     if len(types) > 1)
    except StopIteration:
        raise ValueError('No distinguishing index found for %s, length %s:\n'
                         'All entries have the same type list:\n'
                         '%s' % (name, type_list_length, type_lists[0]))
    # Check optionality
    # “and the booleans in the corresponding list indicating argument
    #  optionality must be the same.”
    # FIXME: spec typo: optionality value is no longer a boolean
    # https://www.w3.org/Bugs/Public/show_bug.cgi?id=25628
    initial_optionality_lists = set(entry[2][:index] for entry in entries)
    if len(initial_optionality_lists) > 1:
        raise ValueError(
            'Invalid optionality lists for %s, length %s:\n'
            'Optionality lists differ below distinguishing argument index %s:\n'
            '%s'
            % (name, type_list_length, index, set(initial_optionality_lists)))

    # Check distinguishability
    # http://heycam.github.io/webidl/#dfn-distinguishable
    # Use names to check for distinct types, since objects are distinct
    # FIXME: check distinguishability more precisely, for validation
    distinguishing_argument_type_names = [type_list[index]
                                          for type_list in type_lists]
    if (len(set(distinguishing_argument_type_names)) !=
        len(distinguishing_argument_type_names)):
        raise ValueError('Types in distinguishing argument are not distinct:\n'
                         '%s' % distinguishing_argument_type_names)

    return index


def length_tests_methods(effective_overloads_by_length):
    """Returns sorted list of resolution tests and associated methods, by length.

    This builds the main data structure for the overload resolution loop.
    For a given argument length, bindings test argument at distinguishing
    argument index, in order given by spec: if it is compatible with
    (optionality or) type required by an overloaded method, resolve to that
    method.

    Returns:
        [(length, [(test, method)])]
    """
    return [(length, list(resolution_tests_methods(effective_overloads)))
            for length, effective_overloads in effective_overloads_by_length]


def resolution_tests_methods(effective_overloads):
    """Yields resolution test and associated method, in resolution order, for effective overloads of a given length.

    This is the heart of the resolution algorithm.
    http://heycam.github.io/webidl/#dfn-overload-resolution-algorithm

    Note that a given method can be listed multiple times, with different tests!
    This is to handle implicit type conversion.

    Returns:
        [(test, method)]
    """
    methods = [effective_overload[0]
               for effective_overload in effective_overloads]
    if len(methods) == 1:
        # If only one method with a given length, no test needed
        yield 'true', methods[0]
        return

    # 6. If there is more than one entry in S, then set d to be the
    # distinguishing argument index for the entries of S.
    index = distinguishing_argument_index(effective_overloads)
    # (7-9 are for handling |undefined| values for optional arguments before
    # the distinguishing argument (as “missing”), so you can specify only some
    # optional arguments. We don’t support this, so we skip these steps.)
    # 10. If i = d, then:
    # (d is the distinguishing argument index)
    # 1. Let V be argi.
    #     Note: This is the argument that will be used to resolve which
    #           overload is selected.
    cpp_value = 'info[%s]' % index

    # Extract argument and IDL type to simplify accessing these in each loop.
    arguments = [method['arguments'][index] for method in methods]
    arguments_methods = zip(arguments, methods)
    idl_types = [argument['idl_type_object'] for argument in arguments]
    idl_types_methods = zip(idl_types, methods)

    # We can’t do a single loop through all methods or simply sort them, because
    # a method may be listed in multiple steps of the resolution algorithm, and
    # which test to apply differs depending on the step.
    #
    # Instead, we need to go through all methods at each step, either finding
    # first match (if only one test is allowed) or filtering to matches (if
    # multiple tests are allowed), and generating an appropriate tests.

    # 2. If V is undefined, and there is an entry in S whose list of
    # optionality values has “optional” at index i, then remove from S all
    # other entries.
    try:
        method = next(method for argument, method in arguments_methods
                      if argument['is_optional'])
        test = '%s->IsUndefined()' % cpp_value
        yield test, method
    except StopIteration:
        pass

    # 3. Otherwise: if V is null or undefined, and there is an entry in S that
    # has one of the following types at position i of its type list,
    # • a nullable type
    try:
        method = next(method for idl_type, method in idl_types_methods
                      if idl_type.is_nullable)
        test = 'isUndefinedOrNull(%s)' % cpp_value
        yield test, method
    except StopIteration:
        pass

    # 4. Otherwise: if V is a platform object – but not a platform array
    # object – and there is an entry in S that has one of the following
    # types at position i of its type list,
    # • an interface type that V implements
    # (Unlike most of these tests, this can return multiple methods, since we
    #  test if it implements an interface. Thus we need a for loop, not a next.)
    # (We distinguish wrapper types from built-in interface types.)
    for idl_type, method in ((idl_type, method)
                             for idl_type, method in idl_types_methods
                             if idl_type.is_wrapper_type):
        test = 'V8{idl_type}::hasInstance({cpp_value}, isolate)'.format(idl_type=idl_type.base_type, cpp_value=cpp_value)
        yield test, method

    # 8. Otherwise: if V is any kind of object except for a native Date object,
    # a native RegExp object, and there is an entry in S that has one of the
    # following types at position i of its type list,
    # • an array type
    # • a sequence type
    # ...
    # • a dictionary
    try:
        # FIXME: IDL dictionary not implemented, so use Blink Dictionary
        # http://crbug.com/321462
        idl_type, method = next((idl_type, method)
                                for idl_type, method in idl_types_methods
                                if (idl_type.array_or_sequence_type or
                                    idl_type.name == 'Dictionary'))
        if idl_type.array_or_sequence_type:
            # (We test for Array instead of generic Object to type-check.)
            # FIXME: test for Object during resolution, then have type check for
            # Array in overloaded method: http://crbug.com/262383
            test = '%s->IsArray()' % cpp_value
        else:
            # FIXME: should be '{1}->IsObject() && !{1}->IsDate() && !{1}->IsRegExp()'.format(cpp_value)
            # FIXME: the IsDate and IsRegExp checks can be skipped if we've
            # already generated tests for them.
            test = '%s->IsObject()' % cpp_value
        yield test, method
    except StopIteration:
        pass

    # (Check for exact type matches before performing automatic type conversion;
    # only needed if distinguishing between primitive types.)
    if len([idl_type.is_primitive_type for idl_type in idl_types]) > 1:
        # (Only needed if match in step 11, otherwise redundant.)
        if any(idl_type.name == 'String' or idl_type.is_enum
               for idl_type in idl_types):
            # 10. Otherwise: if V is a Number value, and there is an entry in S
            # that has one of the following types at position i of its type
            # list,
            # • a numeric type
            try:
                method = next(method for idl_type, method in idl_types_methods
                              if idl_type.is_numeric_type)
                test = '%s->IsNumber()' % cpp_value
                yield test, method
            except StopIteration:
                pass

    # (Perform automatic type conversion, in order. If any of these match,
    # that’s the end, and no other tests are needed.) To keep this code simple,
    # we rely on the C++ compiler's dead code elimination to deal with the
    # redundancy if both cases below trigger.

    # 11. Otherwise: if there is an entry in S that has one of the following
    # types at position i of its type list,
    # • DOMString
    # • an enumeration type
    # * ByteString
    # Blink: ScalarValueString is a pending Web IDL addition
    try:
        method = next(method for idl_type, method in idl_types_methods
                      if idl_type.name in ('String',
                                           'ByteString',
                                           'ScalarValueString') or
                      idl_type.is_enum)
        yield 'true', method
    except StopIteration:
        pass

    # 12. Otherwise: if there is an entry in S that has one of the following
    # types at position i of its type list,
    # • a numeric type
    try:
        method = next(method for idl_type, method in idl_types_methods
                      if idl_type.is_numeric_type)
        yield 'true', method
    except StopIteration:
        pass


################################################################################
# Utility functions
################################################################################

def Counter(iterable):
    # Once using Python 2.7, using collections.Counter
    counter = defaultdict(lambda: 0)
    for item in iterable:
        counter[item] += 1
    return counter


def common(dicts, f):
    """Returns common result of f across an iterable of dicts, or None.

    Call f for each dict and return its result if the same across all dicts.
    """
    values = (f(d) for d in dicts)
    first_value = next(values)
    if all(value == first_value for value in values):
        return first_value
    return None


def common_key(dicts, key):
    """Returns common presence of a key across an iterable of dicts, or None.

    True if all dicts have the key, False if none of the dicts have the key,
    and None if some but not all dicts have the key.
    """
    return common(dicts, lambda d: key in d)


def common_value(dicts, key):
    """Returns common value of a key across an iterable of dicts, or None.

    Auxiliary function for overloads, so can consolidate an extended attribute
    that appears with the same value on all items in an overload set.
    """
    return common(dicts, lambda d: d.get(key))


def sort_and_groupby(l, key=None):
    """Returns a generator of (key, list), sorting and grouping list by key."""
    l.sort(key=key)
    return ((k, list(g)) for k, g in itertools.groupby(l, key))


################################################################################
# Constructors
################################################################################

# [Constructor]
def generate_constructor(interface, constructor):
    arguments_need_try_catch = any(v8_methods.argument_needs_try_catch(argument)
                                   for argument in constructor.arguments)

    return {
        'arguments': [v8_methods.generate_argument(interface, constructor, argument, index)
                      for index, argument in enumerate(constructor.arguments)],
        'arguments_need_try_catch': arguments_need_try_catch,
        'cpp_type': cpp_template_type(
            cpp_ptr_type('RefPtr', 'RawPtr', gc_type(interface)),
            cpp_name(interface)),
        'cpp_value': v8_methods.cpp_value(
            interface, constructor, len(constructor.arguments)),
        'has_exception_state':
            # [RaisesException=Constructor]
            interface.extended_attributes.get('RaisesException') == 'Constructor' or
            any(argument for argument in constructor.arguments
                if argument.idl_type.name == 'SerializedScriptValue' or
                   argument.idl_type.is_integer_type),
        'is_constructor': True,
        'is_named_constructor': False,
        'number_of_required_arguments':
            number_of_required_arguments(constructor),
    }


# [NamedConstructor]
def generate_named_constructor(interface):
    extended_attributes = interface.extended_attributes
    if 'NamedConstructor' not in extended_attributes:
        return None
    # FIXME: parser should return named constructor separately;
    # included in constructors (and only name stored in extended attribute)
    # for Perl compatibility
    idl_constructor = interface.constructors[-1]
    assert idl_constructor.name == 'NamedConstructor'
    constructor = generate_constructor(interface, idl_constructor)
    constructor.update({
        'name': extended_attributes['NamedConstructor'],
        'is_named_constructor': True,
    })
    return constructor


def number_of_required_arguments(constructor):
    return len([argument for argument in constructor.arguments
                if not argument.is_optional])


def interface_length(interface, constructors):
    # Docs: http://heycam.github.io/webidl/#es-interface-call
    if 'EventConstructor' in interface.extended_attributes:
        return 1
    if not constructors:
        return 0
    return min(constructor['number_of_required_arguments']
               for constructor in constructors)


################################################################################
# Special operations (methods)
# http://heycam.github.io/webidl/#idl-special-operations
################################################################################

def property_getter(getter, cpp_arguments):
    def is_null_expression(idl_type):
        if idl_type.is_union_type:
            return ' && '.join('!result%sEnabled' % i
                               for i, _ in enumerate(idl_type.member_types))
        if idl_type.name == 'String':
            return 'result.isNull()'
        if idl_type.is_interface_type:
            return '!result'
        return ''

    idl_type = getter.idl_type
    extended_attributes = getter.extended_attributes
    is_raises_exception = 'RaisesException' in extended_attributes

    # FIXME: make more generic, so can use v8_methods.cpp_value
    cpp_method_name = 'impl->%s' % cpp_name(getter)

    if is_raises_exception:
        cpp_arguments.append('exceptionState')
    union_arguments = idl_type.union_arguments
    if union_arguments:
        cpp_arguments.extend(union_arguments)

    cpp_value = '%s(%s)' % (cpp_method_name, ', '.join(cpp_arguments))

    return {
        'cpp_type': idl_type.cpp_type,
        'cpp_value': cpp_value,
        'is_custom':
            'Custom' in extended_attributes and
            (not extended_attributes['Custom'] or
             has_extended_attribute_value(getter, 'Custom', 'PropertyGetter')),
        'is_custom_property_enumerator': has_extended_attribute_value(
            getter, 'Custom', 'PropertyEnumerator'),
        'is_custom_property_query': has_extended_attribute_value(
            getter, 'Custom', 'PropertyQuery'),
        'is_enumerable': 'NotEnumerable' not in extended_attributes,
        'is_null_expression': is_null_expression(idl_type),
        'is_raises_exception': is_raises_exception,
        'name': cpp_name(getter),
        'union_arguments': union_arguments,
        'v8_set_return_value': idl_type.v8_set_return_value('result', extended_attributes=extended_attributes, script_wrappable='impl', release=idl_type.release),
    }


def property_setter(setter):
    idl_type = setter.arguments[1].idl_type
    extended_attributes = setter.extended_attributes
    is_raises_exception = 'RaisesException' in extended_attributes
    return {
        'has_type_checking_interface':
            has_extended_attribute_value(setter, 'TypeChecking', 'Interface') and
            idl_type.is_wrapper_type,
        'idl_type': idl_type.base_type,
        'is_custom': 'Custom' in extended_attributes,
        'has_exception_state': is_raises_exception or
                               idl_type.is_integer_type,
        'is_raises_exception': is_raises_exception,
        'name': cpp_name(setter),
        'v8_value_to_local_cpp_value': idl_type.v8_value_to_local_cpp_value(
            extended_attributes, 'v8Value', 'propertyValue'),
    }


def property_deleter(deleter):
    idl_type = deleter.idl_type
    if str(idl_type) != 'boolean':
        raise Exception(
            'Only deleters with boolean type are allowed, but type is "%s"' %
            idl_type)
    extended_attributes = deleter.extended_attributes
    return {
        'is_custom': 'Custom' in extended_attributes,
        'is_raises_exception': 'RaisesException' in extended_attributes,
        'name': cpp_name(deleter),
    }


################################################################################
# Indexed properties
# http://heycam.github.io/webidl/#idl-indexed-properties
################################################################################

def indexed_property_getter(interface):
    try:
        # Find indexed property getter, if present; has form:
        # getter TYPE [OPTIONAL_IDENTIFIER](unsigned long ARG1)
        getter = next(
            method
            for method in interface.operations
            if ('getter' in method.specials and
                len(method.arguments) == 1 and
                str(method.arguments[0].idl_type) == 'unsigned long'))
    except StopIteration:
        return None

    return property_getter(getter, ['index'])


def indexed_property_setter(interface):
    try:
        # Find indexed property setter, if present; has form:
        # setter RETURN_TYPE [OPTIONAL_IDENTIFIER](unsigned long ARG1, ARG_TYPE ARG2)
        setter = next(
            method
            for method in interface.operations
            if ('setter' in method.specials and
                len(method.arguments) == 2 and
                str(method.arguments[0].idl_type) == 'unsigned long'))
    except StopIteration:
        return None

    return property_setter(setter)


def indexed_property_deleter(interface):
    try:
        # Find indexed property deleter, if present; has form:
        # deleter TYPE [OPTIONAL_IDENTIFIER](unsigned long ARG)
        deleter = next(
            method
            for method in interface.operations
            if ('deleter' in method.specials and
                len(method.arguments) == 1 and
                str(method.arguments[0].idl_type) == 'unsigned long'))
    except StopIteration:
        return None

    return property_deleter(deleter)


################################################################################
# Named properties
# http://heycam.github.io/webidl/#idl-named-properties
################################################################################

def named_property_getter(interface):
    try:
        # Find named property getter, if present; has form:
        # getter TYPE [OPTIONAL_IDENTIFIER](DOMString ARG1)
        getter = next(
            method
            for method in interface.operations
            if ('getter' in method.specials and
                len(method.arguments) == 1 and
                str(method.arguments[0].idl_type) == 'DOMString'))
    except StopIteration:
        return None

    getter.name = getter.name or 'anonymousNamedGetter'
    return property_getter(getter, ['propertyName'])


def named_property_setter(interface):
    try:
        # Find named property setter, if present; has form:
        # setter RETURN_TYPE [OPTIONAL_IDENTIFIER](DOMString ARG1, ARG_TYPE ARG2)
        setter = next(
            method
            for method in interface.operations
            if ('setter' in method.specials and
                len(method.arguments) == 2 and
                str(method.arguments[0].idl_type) == 'DOMString'))
    except StopIteration:
        return None

    return property_setter(setter)


def named_property_deleter(interface):
    try:
        # Find named property deleter, if present; has form:
        # deleter TYPE [OPTIONAL_IDENTIFIER](DOMString ARG)
        deleter = next(
            method
            for method in interface.operations
            if ('deleter' in method.specials and
                len(method.arguments) == 1 and
                str(method.arguments[0].idl_type) == 'DOMString'))
    except StopIteration:
        return None

    return property_deleter(deleter)
