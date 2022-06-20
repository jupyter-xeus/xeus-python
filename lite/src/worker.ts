// Copyright (c) Thorsten Beier
// Copyright (c) JupyterLite Contributors
// Distributed under the terms of the Modified BSD License.

import { expose } from 'comlink';

import { DriveFS, DriveFSEmscriptenNodeOps, IEmscriptenFSNode, IStats } from '@jupyterlite/contents';

declare function createXeusModule(options: any): any;

globalThis.Module = {};

// TODO Remove this. This is to ensure we always perform node ops on Nodes and
// not Streams, but why is it needed??? Why do we get Streams and not Nodes from
// emscripten in the case of xeus-python???
class StreamNodeOps extends DriveFSEmscriptenNodeOps {

  private getNode(nodeOrStream: any) {
    if (nodeOrStream["node"]) {
      return nodeOrStream["node"];
    }
    return nodeOrStream;
  };

  lookup(parent: IEmscriptenFSNode, name: string): IEmscriptenFSNode {
    return super.lookup(this.getNode(parent), name);
  };

  getattr(node: IEmscriptenFSNode): IStats {
    return super.getattr(this.getNode(node));
  };

  setattr(node: IEmscriptenFSNode, attr: IStats): void {
    super.setattr(this.getNode(node), attr);
  };

  mknod(
    parent: IEmscriptenFSNode,
    name: string,
    mode: number,
    dev: any
  ): IEmscriptenFSNode {
    return super.mknod(this.getNode(parent), name, mode, dev);
  };

  rename(oldNode: IEmscriptenFSNode, newDir: IEmscriptenFSNode, newName: string): void {
    super.rename(this.getNode(oldNode), this.getNode(newDir), newName);
  };

  rmdir(parent: IEmscriptenFSNode, name: string): void {
    super.rmdir(this.getNode(parent), name);
  };

  readdir(node: IEmscriptenFSNode): string[] {
    return super.readdir(this.getNode(node));
  };

}

// TODO Remove this when we don't need StreamNodeOps anymore
class LoggingDrive extends DriveFS {
  constructor(options: DriveFS.IOptions) {
    super(options);

    this.node_ops = new StreamNodeOps(this);
  }
}

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

    this._drive = new LoggingDrive({
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
    if (!path) {
      return;
    }

    globalThis.Module.FS.chdir(path);
  }

  async processMessage(event: any): Promise<void> {
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

    const msg_type = event.msg.header.msg_type;

    if (msg_type === 'input_reply') {
      resolveInputReply(event.msg);
    } else {
      this._raw_xserver.notify_listener(event.msg);
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
    const promise = new Promise<void>(resolve => {
      globalThis.Module.monitorRunDependencies = (n: number) => {
        if (n === 0) {
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
