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
          from: 'src/xpython_wasm.js',
          to: '.'
        },
        {
          from: 'src/*.gz',
          to: './[name].gz'
        },
        {
          from: 'src/*.js',
          to: './[name].js'
        },
        {
          from: 'src/empack_env_meta.json',
          to: '.'
        }
      ]
    })
  ]
};
