const path = require('path');
const rules = [
  {
      test: /\.js$/,
      exclude: /node_modules/,
      loader: 'source-map-loader'
  }
];

const resolve = {
  fallback: {
    fs: false,
    child_process: false,
    crypto: false
  },
  extensions: [".js"],
};

module.exports = [
  {
    entry: './lib/worker.js',
    output: {
      filename: 'worker.js',
      path: path.resolve(__dirname, 'lib'),
      libraryTarget: 'amd'
    },
    module: {
      rules
    },
    devtool: 'source-map',
    resolve,
  }
];
