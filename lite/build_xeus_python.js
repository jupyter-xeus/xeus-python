const { existsSync, readFileSync, writeFileSync } = require('fs');
const crypto = require('crypto');
const path = require('path');
const { execSync } = require('child_process');

const VERSION_FILE_NAME = 'xpython_wasm.hash';
const VERSION_FILE_PATH = path.join('src', VERSION_FILE_NAME);

const hashSum = crypto.createHash('sha256');

hashSum.update(readFileSync('Dockerfile'));
const buildHash = hashSum.digest('hex');

let needsRebuild = true;

if (existsSync(VERSION_FILE_PATH)) {
  const currentHash = readFileSync(VERSION_FILE_PATH, {
    encoding: 'utf8',
    flag: 'r'
  });

  if (currentHash === buildHash) {
    needsRebuild = false;
  }
}

if (needsRebuild) {
  execSync('docker build -t jupyterlite-xeus-kernel .');

  execSync(
    'docker run --rm -v $(pwd):/src -u $(id -u):$(id -g) jupyterlite-xeus-kernel copy_output.sh'
  );

  writeFileSync(VERSION_FILE_PATH, buildHash);
}
