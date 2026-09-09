(function (global) {
  const pending = new Map();
  let callCounter = 0;

  function invoke(command, args) {
    args = args || {};
    const callId = "call_" + (++callCounter);
    return new Promise((resolve, reject) => {
      pending.set(callId, { resolve, reject });
      // W środowisku Silver ta funkcja jest podmieniana przez shim
      // natywnym `__silver_native_invoke(callId, command, JSON.stringify(args))`,
      // które wrzuca wpis do kolejki odbieranej przez js::poll_invoke.
      if (typeof __silver_native_invoke === "function") {
        __silver_native_invoke(callId, command, JSON.stringify(args));
      } else {
        reject(new Error("Silver: __silver_native_invoke niedostępne — czy backend wywołał app::load_js(...)?"));
      }
    });
  }

  // Wołane przez natywny shim, gdy backend odpowie (js::resolve).
  function __silver_resolve(callId, resultJson) {
    const entry = pending.get(callId);
    if (!entry) return;
    pending.delete(callId);
    try {
      entry.resolve(JSON.parse(resultJson));
    } catch (e) {
      entry.reject(e);
    }
  }

  const listeners = new Map();

  function listen(eventName, callback) {
    if (!listeners.has(eventName)) listeners.set(eventName, []);
    listeners.get(eventName).push(callback);
    return () => {
      const arr = listeners.get(eventName) || [];
      const idx = arr.indexOf(callback);
      if (idx >= 0) arr.splice(idx, 1);
    };
  }

  // Wołane przez natywny shim, gdy backend wyemituje zdarzenie (js::emit).
  function __silver_dispatch_event(eventName, payloadJson) {
    const cbs = listeners.get(eventName) || [];
    const payload = JSON.parse(payloadJson);
    for (const cb of cbs) cb(payload);
  }

  global.silver = { invoke, listen };
  global.__silver_resolve = __silver_resolve;
  global.__silver_dispatch_event = __silver_dispatch_event;
})(globalThis);
