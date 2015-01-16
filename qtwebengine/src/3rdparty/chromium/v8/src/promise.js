// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";

// This file relies on the fact that the following declaration has been made
// in runtime.js:
// var $Object = global.Object
// var $WeakMap = global.WeakMap

// For bootstrapper.

var IsPromise;
var PromiseCreate;
var PromiseResolve;
var PromiseReject;
var PromiseChain;
var PromiseCatch;
var PromiseThen;

// mirror-debugger.js currently uses builtins.promiseStatus. It would be nice
// if we could move these property names into the closure below.
// TODO(jkummerow/rossberg/yangguo): Find a better solution.

// Status values: 0 = pending, +1 = resolved, -1 = rejected
var promiseStatus = GLOBAL_PRIVATE("Promise#status");
var promiseValue = GLOBAL_PRIVATE("Promise#value");
var promiseOnResolve = GLOBAL_PRIVATE("Promise#onResolve");
var promiseOnReject = GLOBAL_PRIVATE("Promise#onReject");
var promiseRaw = GLOBAL_PRIVATE("Promise#raw");

(function() {

  var $Promise = function Promise(resolver) {
    if (resolver === promiseRaw) return;
    if (!%_IsConstructCall()) throw MakeTypeError('not_a_promise', [this]);
    if (!IS_SPEC_FUNCTION(resolver))
      throw MakeTypeError('resolver_not_a_function', [resolver]);
    var promise = PromiseInit(this);
    try {
      %DebugPromiseHandlePrologue(function() { return promise });
      resolver(function(x) { PromiseResolve(promise, x) },
               function(r) { PromiseReject(promise, r) });
    } catch (e) {
      PromiseReject(promise, e);
    } finally {
      %DebugPromiseHandleEpilogue();
    }
  }

  // Core functionality.

  function PromiseSet(promise, status, value, onResolve, onReject) {
    SET_PRIVATE(promise, promiseStatus, status);
    SET_PRIVATE(promise, promiseValue, value);
    SET_PRIVATE(promise, promiseOnResolve, onResolve);
    SET_PRIVATE(promise, promiseOnReject, onReject);
    return promise;
  }

  function PromiseInit(promise) {
    return PromiseSet(
        promise, 0, UNDEFINED, new InternalArray, new InternalArray)
  }

  function PromiseDone(promise, status, value, promiseQueue) {
    if (GET_PRIVATE(promise, promiseStatus) === 0) {
      PromiseEnqueue(value, GET_PRIVATE(promise, promiseQueue));
      PromiseSet(promise, status, value);
    }
  }

  function PromiseCoerce(constructor, x) {
    if (!IsPromise(x) && IS_SPEC_OBJECT(x)) {
      var then;
      try {
        then = x.then;
      } catch(r) {
        return %_CallFunction(constructor, r, PromiseRejected);
      }
      if (IS_SPEC_FUNCTION(then)) {
        var deferred = %_CallFunction(constructor, PromiseDeferred);
        try {
          %_CallFunction(x, deferred.resolve, deferred.reject, then);
        } catch(r) {
          deferred.reject(r);
        }
        return deferred.promise;
      }
    }
    return x;
  }

  function PromiseHandle(value, handler, deferred) {
    try {
      %DebugPromiseHandlePrologue(
          function() {
            var queue = GET_PRIVATE(deferred.promise, promiseOnReject);
            return (queue && queue.length == 0) ? deferred.promise : UNDEFINED;
          });
      var result = handler(value);
      if (result === deferred.promise)
        throw MakeTypeError('promise_cyclic', [result]);
      else if (IsPromise(result))
        %_CallFunction(result, deferred.resolve, deferred.reject, PromiseChain);
      else
        deferred.resolve(result);
    } catch (exception) {
      try {
        %DebugPromiseHandlePrologue(function() { return deferred.promise });
        deferred.reject(exception);
      } catch (e) { } finally {
        %DebugPromiseHandleEpilogue();
      }
    } finally {
      %DebugPromiseHandleEpilogue();
    }
  }

  function PromiseEnqueue(value, tasks) {
    %EnqueueMicrotask(function() {
      for (var i = 0; i < tasks.length; i += 2) {
        PromiseHandle(value, tasks[i], tasks[i + 1])
      }
    });
  }

  function PromiseIdResolveHandler(x) { return x }
  function PromiseIdRejectHandler(r) { throw r }

  function PromiseNopResolver() {}

  // -------------------------------------------------------------------
  // Define exported functions.

  // For bootstrapper.

  IsPromise = function IsPromise(x) {
    return IS_SPEC_OBJECT(x) && HAS_PRIVATE(x, promiseStatus);
  }

  PromiseCreate = function PromiseCreate() {
    return new $Promise(PromiseNopResolver)
  }

  PromiseResolve = function PromiseResolve(promise, x) {
    PromiseDone(promise, +1, x, promiseOnResolve)
  }

  PromiseReject = function PromiseReject(promise, r) {
    PromiseDone(promise, -1, r, promiseOnReject)
  }

  // Convenience.

  function PromiseDeferred() {
    if (this === $Promise) {
      // Optimized case, avoid extra closure.
      var promise = PromiseInit(new $Promise(promiseRaw));
      return {
        promise: promise,
        resolve: function(x) { PromiseResolve(promise, x) },
        reject: function(r) { PromiseReject(promise, r) }
      };
    } else {
      var result = {};
      result.promise = new this(function(resolve, reject) {
        result.resolve = resolve;
        result.reject = reject;
      })
      return result;
    }
  }

  function PromiseResolved(x) {
    if (this === $Promise) {
      // Optimized case, avoid extra closure.
      return PromiseSet(new $Promise(promiseRaw), +1, x);
    } else {
      return new this(function(resolve, reject) { resolve(x) });
    }
  }

  function PromiseRejected(r) {
    if (this === $Promise) {
      // Optimized case, avoid extra closure.
      return PromiseSet(new $Promise(promiseRaw), -1, r);
    } else {
      return new this(function(resolve, reject) { reject(r) });
    }
  }

  // Simple chaining.

  PromiseChain = function PromiseChain(onResolve, onReject) {  // a.k.a.
                                                                // flatMap
    onResolve = IS_UNDEFINED(onResolve) ? PromiseIdResolveHandler : onResolve;
    onReject = IS_UNDEFINED(onReject) ? PromiseIdRejectHandler : onReject;
    var deferred = %_CallFunction(this.constructor, PromiseDeferred);
    switch (GET_PRIVATE(this, promiseStatus)) {
      case UNDEFINED:
        throw MakeTypeError('not_a_promise', [this]);
      case 0:  // Pending
        GET_PRIVATE(this, promiseOnResolve).push(onResolve, deferred);
        GET_PRIVATE(this, promiseOnReject).push(onReject, deferred);
        break;
      case +1:  // Resolved
        PromiseEnqueue(GET_PRIVATE(this, promiseValue), [onResolve, deferred]);
        break;
      case -1:  // Rejected
        PromiseEnqueue(GET_PRIVATE(this, promiseValue), [onReject, deferred]);
        break;
    }
    return deferred.promise;
  }

  PromiseCatch = function PromiseCatch(onReject) {
    return this.then(UNDEFINED, onReject);
  }

  // Multi-unwrapped chaining with thenable coercion.

  PromiseThen = function PromiseThen(onResolve, onReject) {
    onResolve = IS_SPEC_FUNCTION(onResolve) ? onResolve
                                            : PromiseIdResolveHandler;
    onReject = IS_SPEC_FUNCTION(onReject) ? onReject
                                          : PromiseIdRejectHandler;
    var that = this;
    var constructor = this.constructor;
    return %_CallFunction(
      this,
      function(x) {
        x = PromiseCoerce(constructor, x);
        return x === that ? onReject(MakeTypeError('promise_cyclic', [x])) :
               IsPromise(x) ? x.then(onResolve, onReject) : onResolve(x);
      },
      onReject,
      PromiseChain
    );
  }

  // Combinators.

  function PromiseCast(x) {
    // TODO(rossberg): cannot do better until we support @@create.
    return IsPromise(x) ? x : new this(function(resolve) { resolve(x) });
  }

  function PromiseAll(values) {
    var deferred = %_CallFunction(this, PromiseDeferred);
    var resolutions = [];
    if (!%_IsArray(values)) {
      deferred.reject(MakeTypeError('invalid_argument'));
      return deferred.promise;
    }
    try {
      var count = values.length;
      if (count === 0) {
        deferred.resolve(resolutions);
      } else {
        for (var i = 0; i < values.length; ++i) {
          this.resolve(values[i]).then(
            function(i, x) {
              resolutions[i] = x;
              if (--count === 0) deferred.resolve(resolutions);
            }.bind(UNDEFINED, i),  // TODO(rossberg): use let loop once
                                    // available
            function(r) { deferred.reject(r) }
          );
        }
      }
    } catch (e) {
      deferred.reject(e)
    }
    return deferred.promise;
  }

  function PromiseOne(values) {
    var deferred = %_CallFunction(this, PromiseDeferred);
    if (!%_IsArray(values)) {
      deferred.reject(MakeTypeError('invalid_argument'));
      return deferred.promise;
    }
    try {
      for (var i = 0; i < values.length; ++i) {
        this.resolve(values[i]).then(
          function(x) { deferred.resolve(x) },
          function(r) { deferred.reject(r) }
        );
      }
    } catch (e) {
      deferred.reject(e)
    }
    return deferred.promise;
  }

  // -------------------------------------------------------------------
  // Install exported functions.

  %CheckIsBootstrapping();
  %SetProperty(global, 'Promise', $Promise, DONT_ENUM);
  InstallFunctions($Promise, DONT_ENUM, [
    "defer", PromiseDeferred,
    "accept", PromiseResolved,
    "reject", PromiseRejected,
    "all", PromiseAll,
    "race", PromiseOne,
    "resolve", PromiseCast
  ]);
  InstallFunctions($Promise.prototype, DONT_ENUM, [
    "chain", PromiseChain,
    "then", PromiseThen,
    "catch", PromiseCatch
  ]);

})();
