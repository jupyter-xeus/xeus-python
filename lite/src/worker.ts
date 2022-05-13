// Copyright (c) Thorsten Beier
// Copyright (c) JupyterLite Contributors
// Distributed under the terms of the Modified BSD License.

declare function createXeusModule(options: any): any;

globalThis.Module = {};

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

  const data = event.data;
  const msg = data.msg;
  const msg_type = msg.header.msg_type;

  if (msg_type === 'input_reply') {
    resolveInputReply(msg);
  } else {
    raw_xserver!.notify_listener(msg);
  }
};
