// Copyright (c) Thorsten Beier
// Copyright (c) JupyterLite Contributors
// Distributed under the terms of the Modified BSD License.

import { wrap } from 'comlink';
import type { Remote } from 'comlink';

import { ISignal, Signal } from '@lumino/signaling';
import { PromiseDelegate } from '@lumino/coreutils';

import { PageConfig } from '@jupyterlab/coreutils';
import { KernelMessage } from '@jupyterlab/services';

import { IKernel } from '@jupyterlite/kernel';

interface IXeusKernel {
  ready(): Promise<void>;

  mount(driveName: string, mountpoint: string, baseUrl: string): Promise<void>;

  cd(path: string): Promise<void>;

  processMessage(msg: any): Promise<void>;
}

export class XeusServerKernel implements IKernel {
  /**
   * Instantiate a new XeusServerKernel
   *
   * @param options The instantiation options for a new XeusServerKernel
   */
  constructor(options: XeusServerKernel.IOptions) {
    const { id, name, sendMessage, location } = options;
    this._id = id;
    this._name = name;
    this._location = location;
    this._sendMessage = sendMessage;
    this._worker = new Worker(new URL('./worker.js', import.meta.url), {
      type: 'module'
    });
    this._worker.onmessage = e => {
      this._processWorkerMessage(e.data);
    };
    this._remote = wrap(this._worker);
    this.initFileSytem(options);
  }

  async handleMessage(msg: KernelMessage.IMessage): Promise<void> {
    this._parent = msg;
    this._parentHeader = msg.header;
    await this._sendMessageToWorker(msg);
  }

  private async _sendMessageToWorker(msg: any): Promise<void> {
    // TODO Remove this??
    if (msg.header.msg_type !== 'input_reply') {
      this._executeDelegate = new PromiseDelegate<void>();
    }
    await this._remote.processMessage({ msg, parent: this.parent });
    if (msg.header.msg_type !== 'input_reply') {
      return await this._executeDelegate.promise;
    }
  }

  /**
   * Get the last parent header
   */
  get parentHeader():
    | KernelMessage.IHeader<KernelMessage.MessageType>
    | undefined {
    return this._parentHeader;
  }

  /**
   * Get the last parent message (mimick ipykernel's get_parent)
   */
  get parent(): KernelMessage.IMessage | undefined {
    return this._parent;
  }

  /**
   * Get the kernel location
   */
  get location(): string {
    return this._location;
  }

  /**
   * Process a message coming from the pyodide web worker.
   *
   * @param msg The worker message to process.
   */
  private _processWorkerMessage(msg: any): void {
    if (!msg.header) {
      return;
    }

    msg.header.session = this._parentHeader?.session ?? '';
    msg.session = this._parentHeader?.session ?? '';
    this._sendMessage(msg);

    // resolve promise
    if (
      msg.header.msg_type === 'status' &&
      msg.content.execution_state === 'idle'
    ) {
      this._executeDelegate.resolve();
    }
  }

  /**
   * A promise that is fulfilled when the kernel is ready.
   */
  get ready(): Promise<void> {
    return Promise.resolve();
  }

  /**
   * Return whether the kernel is disposed.
   */
  get isDisposed(): boolean {
    return this._isDisposed;
  }

  /**
   * A signal emitted when the kernel is disposed.
   */
  get disposed(): ISignal<this, void> {
    return this._disposed;
  }

  /**
   * Dispose the kernel.
   */
  dispose(): void {
    if (this.isDisposed) {
      return;
    }
    this._isDisposed = true;
    this._disposed.emit(void 0);
  }

  /**
   * Get the kernel id
   */
  get id(): string {
    return this._id;
  }

  /**
   * Get the name of the kernel
   */
  get name(): string {
    return this._name;
  }

  private async initFileSytem(options: XeusServerKernel.IOptions) {
    let driveName: string;
    let localPath: string;

    if (options.location.includes(':')) {
      const parts = options.location.split(':');
      driveName = parts[0];
      localPath = parts[1];
    } else {
      driveName = '';
      localPath = options.location;
    }

    await this._remote.ready();

    if (options.mountDrive) {
      await this._remote.mount(driveName, '/drive', PageConfig.getBaseUrl());
      await this._remote.cd(localPath);
    }
  }

  private _id: string;
  private _name: string;
  private _location: string;
  private _remote: Remote<IXeusKernel>;
  private _isDisposed = false;
  private _disposed = new Signal<this, void>(this);
  private _worker: Worker;
  private _sendMessage: IKernel.SendMessage;
  private _executeDelegate = new PromiseDelegate<void>();
  private _parentHeader:
    | KernelMessage.IHeader<KernelMessage.MessageType>
    | undefined = undefined;
  private _parent: KernelMessage.IMessage | undefined = undefined;
}

/**
 * A namespace for XeusServerKernel statics.
 */
export namespace XeusServerKernel {
  /**
   * The instantiation options for a Pyodide kernel
   */
  export interface IOptions extends IKernel.IOptions {
    mountDrive: boolean;
  }
}
