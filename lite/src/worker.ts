// Copyright (c) Thorsten Beier
// Copyright (c) JupyterLite Contributors
// Distributed under the terms of the Modified BSD License.


declare module globalThis {
    let Module:any;
}
globalThis.Module = {}
// // @ts-ignore
// var document: any = {}

// globalThis.Module = Module

// eslint-disable-next-line @typescript-eslint/ban-ts-comment
// @ts-ignore
import createXeusModule from './xeus_kernel.js';
// eslint-disable-next-line @typescript-eslint/ban-ts-comment
// @ts-ignore
// import populate from './python_data.js';
// // import populate from './python_data.js';
// // import populate from './python_data.js';
// // console.log("populate",populate)


// populate()

// We alias self to ctx and give it our newly created type
const ctx: Worker = self as any;
let raw_xkernel: any;
let raw_xserver: any;



async function waitRunDependency() {
  const promise = new Promise((r:any) => {
    globalThis.Module.monitorRunDependencies = (n:number) => {
      if (n === 0) {
        console.log("all `RunDependencies` loaded")
        r();
      }
    };
  });
  // If there are no pending dependencies left, monitorRunDependencies will
  // never be called. Since we can't check the number of dependencies,
  // manually trigger a call.
  globalThis.Module.addRunDependency("dummy");
  globalThis.Module.removeRunDependency("dummy");
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

// eslint-disable-next-line
// @ts-ignore: breaks typedoc
let resolveInputReply: any;


async function loadCppModule(moduleFactory: any): Promise<any> {
  const options: any = {};
  globalThis.Module = await moduleFactory(options);

  // eslint-disable-next-line @typescript-eslint/ban-ts-comment
  // @ts-ignore
  await import("./python_data");

  await waitRunDependency();
  raw_xkernel = new globalThis.Module.xkernel();
  raw_xserver = raw_xkernel.get_server();
  raw_xkernel!.start();
}

const loadCppModulePromise = loadCppModule(createXeusModule);

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
