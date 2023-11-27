// Emscripten doesn't make UTF8ToString or wasmTable available on Module by default...
Module.UTF8ToString = UTF8ToString;
Module.wasmTable = wasmTable;
// Emscripten has a bug where it accidentally exposes an empty object as Module.ERRNO_CODES
Module.ERRNO_CODES = ERRNO_CODES;



Module['async_init'] = async function(
    kernel_root_url, 
    pkg_root_url,
    verbose) {
    Module['bootstrap_from_empack_packed_environment']
    return Module['bootstrap_from_empack_packed_environment'](
    `${kernel_root_url}/empack_env_meta.json`, /* packages_json_url */
    pkg_root_url,               /* package_tarballs_root_url */
    verbose                 /* verbose */
    );
}