
with open('./bld_ems/xeus_kernel.js', 'r') as f:
    content = f.read()


query = 'Module["preloadPlugins"].push(audioPlugin);'
content = content.replace(query,'')

with open('./bld_ems/xeus_kernel.js', 'w')  as f:
    f.write(content)