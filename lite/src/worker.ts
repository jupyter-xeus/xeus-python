// Copyright (c) Thorsten Beier
// Copyright (c) JupyterLite Contributors
// Distributed under the terms of the Modified BSD License.

import { expose } from 'comlink';

import { DriveFS } from '@jupyterlite/contents';

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

let resolveInputReply: any;

async function get_stdin() {
  const replyPromise = new Promise(resolve => {
    resolveInputReply = resolve;
  });
  return replyPromise;
}

(self as any).get_stdin = get_stdin;

class XeusPythonKernel {
  constructor() {
    this._ready = new Promise(resolve => {
      this.initialize(resolve);
    });
  }

  async ready(): Promise<void> {
    return await this._ready;
  }

  mount(driveName: string, mountpoint: string, baseUrl: string): void {
    const { FS, PATH, ERRNO_CODES } = globalThis.Module;

    this._drive = new DriveFS({
      FS,
      PATH,
      ERRNO_CODES,
      baseUrl,
      driveName,
      mountpoint
    });

    FS.mkdir(mountpoint);
    FS.mount(this._drive, {}, mountpoint);
    FS.chdir(mountpoint);
  }

  cd(path: string) {
    const { FS } = globalThis.Module;

    FS.chdir(path);
  }

  async processMessage(msg: any): Promise<void> {
    await this._ready;

    if (
      globalThis.toplevel_promise !== null &&
      globalThis.toplevel_promise_py_proxy !== null
    ) {
      await globalThis.toplevel_promise;
      globalThis.toplevel_promise_py_proxy.delete();
      globalThis.toplevel_promise_py_proxy = null;
      globalThis.toplevel_promise = null;
    }

    const msg_type = msg.header.msg_type;

    if (msg_type === 'input_reply') {
      resolveInputReply(msg);
    } else {
      this._raw_xserver.notify_listener(msg);
    }
  }

  private async initialize(resolve: () => void) {
    importScripts('./xpython_wasm.js');

    globalThis.Module = await createXeusModule({});

    importScripts('./python_data.js');

    await this.waitRunDependency();

    this._raw_xkernel = new globalThis.Module.xkernel();
    this._raw_xserver = this._raw_xkernel.get_server();

    if (!this._raw_xkernel) {
      console.error('Failed to start kernel!');
    }

    this._raw_xkernel.start();

    resolve();
  }

  private async waitRunDependency() {
    const promise = new Promise((resolve: any) => {
      globalThis.Module.monitorRunDependencies = (n: number) => {
        if (n === 0) {
          console.log('all `RunDependencies` loaded');
          resolve();
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

  private _raw_xkernel: any;
  private _raw_xserver: any;
  private _drive: DriveFS | null = null;
  private _ready: PromiseLike<void>;
}

expose(new XeusPythonKernel());
