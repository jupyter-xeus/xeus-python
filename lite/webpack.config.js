const CopyPlugin = require('copy-webpack-plugin');

module.exports = {
  plugins: [
    new CopyPlugin({
      patterns: [
        {
          from: 'src/xeus_kernel.wasm',
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
