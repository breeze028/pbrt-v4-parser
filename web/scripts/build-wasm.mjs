import { existsSync, readFileSync } from 'node:fs';
import { resolve, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';
import { spawnSync } from 'node:child_process';

const root=resolve(dirname(fileURLToPath(import.meta.url)),'../..');
const sdk=process.env.EMSDK ?? resolve(root,'.reference/emsdk');
const toolchain=resolve(sdk,'upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake');
if(!existsSync(toolchain))throw Error('Set EMSDK to an activated Emscripten 7.0.0 SDK (see web/README.md)');
const version=JSON.parse(readFileSync(resolve(sdk,'upstream/emscripten/emscripten-version.txt'),'utf8'));
if(version!=='7.0.0')throw Error(`Expected Emscripten 7.0.0, found ${version}; activate the pinned SDK first`);
function run(args){const result=spawnSync('cmake',args,{cwd:root,stdio:'inherit'});if(result.error)throw result.error;if(result.status!==0)process.exit(result.status??1);}
run(['-S','.', '-B','build-web-wasm','-G','Ninja',
  '-DCMAKE_TOOLCHAIN_FILE='+toolchain.replaceAll('\\','/'),'-DCMAKE_BUILD_TYPE=Release',
  '-DPBRT4_BUILD_WEB=ON','-DPBRT4_BUILD_TESTS=OFF','-DPBRT4_BUILD_TOOLS=OFF','-DPBRT4_BUILD_EXAMPLES=OFF']);
run(['--build','build-web-wasm','--parallel','4']);
