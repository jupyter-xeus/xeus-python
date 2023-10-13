// Copyright (c) JupyterLite Contributors
// Distributed under the terms of the Modified BSD License.

import { test } from '@jupyterlab/galata';

import { expect } from '@playwright/test';

import { firefoxWaitForApplication } from './utils';

test.use({ waitForApplication: firefoxWaitForApplication });

test.describe('General', () => {
  test.beforeEach(async ({ page }) => {
    await page.goto('lab/index.html');
  });

  test('Kernel is registered', async ({ page }) => {
    const imageName = 'launcher.png';
    const launcherElement = await page.$('.jp-Launcher-body');
    if (!launcherElement) {
      throw new Error('Launcher not found');
    }
    expect(await launcherElement.screenshot()).toMatchSnapshot(imageName);
  });
});
