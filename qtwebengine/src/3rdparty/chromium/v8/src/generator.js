// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";

// This file relies on the fact that the following declarations have been made
// in runtime.js:
// var $Function = global.Function;

// ----------------------------------------------------------------------------


// Generator functions and objects are specified by ES6, sections 15.19.3 and
// 15.19.4.

function GeneratorObjectNext(value) {
  if (!IS_GENERATOR(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['[Generator].prototype.next', this]);
  }

  return %_GeneratorNext(this, value);
}

function GeneratorObjectThrow(exn) {
  if (!IS_GENERATOR(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['[Generator].prototype.throw', this]);
  }

  return %_GeneratorThrow(this, exn);
}

function GeneratorObjectIterator() {
  return this;
}

function GeneratorFunctionPrototypeConstructor(x) {
  if (%_IsConstructCall()) {
    throw MakeTypeError('not_constructor', ['GeneratorFunctionPrototype']);
  }
}

function GeneratorFunctionConstructor(arg1) {  // length == 1
  var source = NewFunctionString(arguments, 'function*');
  var global_receiver = %GlobalReceiver(global);
  // Compile the string in the constructor and not a helper so that errors
  // appear to come from here.
  var f = %CompileString(source, true);
  if (!IS_FUNCTION(f)) return f;
  f = %_CallFunction(global_receiver, f);
  %FunctionMarkNameShouldPrintAsAnonymous(f);
  return f;
}


function SetUpGenerators() {
  %CheckIsBootstrapping();
  var GeneratorObjectPrototype = GeneratorFunctionPrototype.prototype;
  InstallFunctions(GeneratorObjectPrototype,
                   DONT_ENUM | DONT_DELETE | READ_ONLY,
                   ["next", GeneratorObjectNext,
                    "throw", GeneratorObjectThrow]);
  %FunctionSetName(GeneratorObjectIterator, '[Symbol.iterator]');
  %SetProperty(GeneratorObjectPrototype, symbolIterator, GeneratorObjectIterator,
      DONT_ENUM | DONT_DELETE | READ_ONLY);
  %SetProperty(GeneratorObjectPrototype, "constructor",
               GeneratorFunctionPrototype, DONT_ENUM | DONT_DELETE | READ_ONLY);
  %SetPrototype(GeneratorFunctionPrototype, $Function.prototype);
  %SetCode(GeneratorFunctionPrototype, GeneratorFunctionPrototypeConstructor);
  %SetProperty(GeneratorFunctionPrototype, "constructor",
               GeneratorFunction, DONT_ENUM | DONT_DELETE | READ_ONLY);
  %SetPrototype(GeneratorFunction, $Function);
  %SetCode(GeneratorFunction, GeneratorFunctionConstructor);
}

SetUpGenerators();
