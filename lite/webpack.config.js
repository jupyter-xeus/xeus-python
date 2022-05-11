const CopyPlugin = require('copy-webpack-plugin');

module.exports = {
  plugins: [
    new CopyPlugin({
      patterns: [
        {
          from: 'src/xpython_wasm.wasm',
          to: '.'
        },
        {
          from: 'src/python_data.data',
          to: '.'
        },
      ]
    })
  ]
};
