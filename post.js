// console.log("POST-JS HACK")
if ('wasmTable' in Module){
    // console.log("all good, no need for patching")
} else
{
    // console.log("needs patching")
    Module['wasmTable'] = wasmTable
}