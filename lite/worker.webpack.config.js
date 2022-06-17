const path = require('path');
const rules = [
  {
    test: /\.ts$/,
    loader: 'ts-loader',
    options: {
      configFile: path.resolve('./tsconfig.json')
    }
  },
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
  extensions: [".ts", ".js"],
};

module.exports = [
  {
    entry: './src/worker.ts',
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
