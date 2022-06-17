// Copyright (c) Thorsten Beier
// Copyright (c) JupyterLite Contributors
// Distributed under the terms of the Modified BSD License.

declare function createXeusModule(options: any): any;

globalThis.Module = {};

// when a toplevel cell uses an await, the cell is implicitly
// wrapped in a async function. Since the webloop - eventloop
// implementation does not support `eventloop.run_until_complete(f)`
// we need to convert the toplevel future in a javascript Promise
// this `toplevel` promise is then awaited before we
// execute the next cell. After the promise is awaited we need
// to do some cleanup and delete the python proxy
// (ie a js-wrapped python object) to avoid memory leaks
globalThis.toplevel_promise = null;
globalThis.toplevel_promise_py_proxy = null;

// We alias self to ctx and give it our newly created type
const ctx: Worker = self as any;
let raw_xkernel: any;
let raw_xserver: any;

async function waitRunDependency() {
  const promise = new Promise((r: any) => {
    globalThis.Module.monitorRunDependencies = (n: number) => {
      if (n === 0) {
        console.log('all `RunDependencies` loaded');
        r();
      }
    };
  });
  // If there are no pending dependencies left, monitorRunDependencies will
  // never be called. Since we can't check the number of dependencies,
  // manually trigger a call.
  globalThis.Module.addRunDependency('dummy');
  globalThis.Module.removeRunDependency('dummy');
  return promise;
}

async function get_stdin() {
  const replyPromise = new Promise(resolve => {
    resolveInputReply = resolve;
  });
  return replyPromise;
}

// eslint-disable-next-line
// @ts-ignore: breaks typedoc
ctx.get_stdin = get_stdin;

let resolveInputReply: any;

async function load() {
  const options: any = {};

  importScripts('./xpython_wasm.js');

  globalThis.Module = await createXeusModule(options);

  importScripts('./python_data.js');

  await waitRunDependency();
  raw_xkernel = new globalThis.Module.xkernel();
  raw_xserver = raw_xkernel.get_server();
  raw_xkernel!.start();
}

const loadCppModulePromise = load();

ctx.onmessage = async (event: MessageEvent): Promise<void> => {
  await loadCppModulePromise;

  if (
    globalThis.toplevel_promise !== null &&
    globalThis.toplevel_promise_py_proxy !== null
  ) {
    await globalThis.toplevel_promise;
    globalThis.toplevel_promise_py_proxy.delete();
    globalThis.toplevel_promise_py_proxy = null;
    globalThis.toplevel_promise = null;
  }

  const data = event.data;
  const msg = data.msg;
  const msg_type = msg.header.msg_type;

  if (msg_type === 'input_reply') {
    resolveInputReply(msg);
  } else {
    raw_xserver!.notify_listener(msg);
  }
};
