// Only the output frame directory is exposed to the WASI guest.
const fs = require('node:fs');
const { WASI } = require('node:wasi');
(async () => {
  const wasi = new WASI({version:'preview1', args:['render_test','/frames'],
    preopens:{'/frames':process.argv[3]}, returnOnExit:true});
  const compiled = await WebAssembly.compile(fs.readFileSync(process.argv[2]));
  const instance = await WebAssembly.instantiate(compiled, {wasi_snapshot_preview1:wasi.wasiImport});
  process.exitCode = wasi.start(instance);
})().catch(error => { console.error(error); process.exitCode = 1; });
