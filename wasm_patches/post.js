// Emscripten doesn't make UTF8ToString or wasmTable available on Module by default...
Module.UTF8ToString = UTF8ToString;
Module.wasmTable = wasmTable;
// Emscripten has a bug where it accidentally exposes an empty object as Module.ERRNO_CODES
Module.ERRNO_CODES = ERRNO_CODES;



Module['async_init'] = async function(
    kernel_root_url, 
    pkg_root_url,
    verbose,
    empack_env_meta_link = '') {

    /* It should work with any kernel so we have to keep logic for this too
       by default it will work with local empack_env_meta.json
    */
    let packages_json_url = `${kernel_root_url}/empack_env_meta.json`;

    // if we host empack_env_meta somewhere else then we have to proceed it
    if (empack_env_meta_link) {
     packages_json_url = empack_env_meta_link;
    }
     
    Module['bootstrap_from_empack_packed_environment']
    return Module['bootstrap_from_empack_packed_environment'](
    packages_json_url,
    pkg_root_url,               /* package_tarballs_root_url */
    verbose,                    /* verbose */
    );
}
