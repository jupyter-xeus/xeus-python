// Copyright (c) Thorsten Beier
// Copyright (c) JupyterLite Contributors
// Distributed under the terms of the Modified BSD License.

import {
  IServiceWorkerManager,
  JupyterLiteServer,
  JupyterLiteServerPlugin
} from '@jupyterlite/server';
import { IBroadcastChannelWrapper } from '@jupyterlite/contents';
import { IKernel, IKernelSpecs } from '@jupyterlite/kernel';

import { WebWorkerKernel } from './web_worker_kernel';

import logo32 from '!!file-loader?context=.!../style/logos/python-logo-32x32.png';
import logo64 from '!!file-loader?context=.!../style/logos/python-logo-64x64.png';

const server_kernel: JupyterLiteServerPlugin<void> = {
  id: '@jupyterlite/xeus-python-kernel-extension:kernel',
  autoStart: true,
  requires: [IKernelSpecs],
  optional: [IServiceWorkerManager, IBroadcastChannelWrapper],
  activate: (
    app: JupyterLiteServer,
    kernelspecs: IKernelSpecs,
    serviceWorker?: IServiceWorkerManager,
    broadcastChannel?: IBroadcastChannelWrapper
  ) => {
    kernelspecs.register({
      spec: {
        name: 'xeus-python',
        display_name: 'Python (XPython)',
        language: 'python',
        argv: [],
        resources: {
          'logo-32x32': logo32,
          'logo-64x64': logo64
        }
      },
      create: async (options: IKernel.IOptions): Promise<IKernel> => {
        const mountDrive = !!(
          serviceWorker?.enabled && broadcastChannel?.enabled
        );

        if (mountDrive) {
          console.info(
            'xeus-python contents will be synced with Jupyter Contents'
          );
        } else {
          console.warn(
            'xeus-python contents will NOT be synced with Jupyter Contents'
          );
        }

        return new WebWorkerKernel({
          ...options,
          mountDrive
        });
      }
    });
  }
};

const plugins: JupyterLiteServerPlugin<any>[] = [server_kernel];

export default plugins;
