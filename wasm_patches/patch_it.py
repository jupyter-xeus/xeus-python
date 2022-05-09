with open('./build/xpython_wasm.js', 'r') as f:
    content = f.read()


query = 'Module["preloadPlugins"].push(audioPlugin);'
content = content.replace(query, '')

with open('./build/xpython_wasm.js', 'w') as f:
    f.write(content)
