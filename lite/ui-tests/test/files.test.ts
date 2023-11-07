// Copyright (c) JupyterLite Contributors
// Distributed under the terms of the Modified BSD License.

import { test } from '@jupyterlab/galata';

import { expect } from '@playwright/test';

import { firefoxWaitForApplication } from './utils';

test.use({ waitForApplication: firefoxWaitForApplication });

test.describe('File system', () => {
  test.beforeEach(async ({ page }) => {
    await page.goto('lab/index.html');
  });

  // this test can take a while due to the time take to type the inputs
  test.setTimeout(60000 * 3);

  test('Create files from the notebook and open them in JupyterLab', async ({
    page
  }) => {
    const notebook = await page.notebook.createNew();
    if (!notebook) {
      throw new Error('Notebook could not be created');
    }

    // add cells for manipulating files
    const filename = 'test.txt';
    const content = 'Hello, world!';
    await page.notebook.setCell(0, 'code', 'import os\nos.listdir()');
    await page.notebook.addCell('code', 'from pathlib import Path');
    await page.notebook.addCell('code', `p = Path("${filename}")`);
    await page.notebook.addCell('code', `p.write_text("${content}")`);
    await page.notebook.addCell('code', 'p.exists()');
    await page.notebook.addCell('code', 'os.listdir()');

    // execute the cells
    await page.notebook.run();

    // the first cell output should contain the name of the notebook
    const output = await page.notebook.getCellTextOutput(0);
    expect(output).toBeTruthy();
    expect(output![0]).toContain(notebook);

    // the last cell output should contain the name of the created file
    const output2 = await page.notebook.getCellTextOutput(5);
    expect(output2).toBeTruthy();
    expect(output2![0]).toContain(filename);

    await page.notebook.close();

    // open the created file from the file browser
    await page.waitForSelector(`.jp-DirListing-content >> text="${filename}"`);
    await page.filebrowser.open(filename);

    // check the file contents
    const line = await page
      .locator(`.jp-FileEditor .cm-line >> text="${content}"`)
      .innerText();
    expect(line).toBe(content);
  });
});
