import './style.css';
import { Renderer } from './renderer/renderer';
import type { Entity,Loaded,Metadata } from './model';
document.querySelector<HTMLDivElement>('#app')!.innerHTML=`
<header><div class="brand"><span class="mark">P</span><strong>PBRT <span>/ SCENE LAB</span></strong><small>WebGPU · SDK consumer</small></div><a href="https://github.com/mmp/pbrt-v4">PBRT v4 ↗</a></header>
<main><aside class="left"><div class="eyebrow">WORKSPACE</div><h1>场景检查器</h1><label for="scene">示例场景</label><select id="scene"><option value="cornell-box">Cornell Box</option><option value="spaceship">Spaceship · 加载检查</option><option value="bathroom">Bathroom · 加载检查</option></select><p class="hint">同一套 C++ Parser，通过 WASM 加载原始 PBRT。</p><div class="divider"></div><div class="row"><h2>场景层次</h2><span id="shape-count" class="tag">—</span></div><nav id="tree"></nav></aside>
<section class="viewport"><div class="toolbar"><span><i id="indicator"></i><span id="status">正在初始化 WebGPU…</span></span><button id="reset">重置相机</button></div><div class="canvas-wrap"><canvas id="canvas" aria-label="WebGPU 路径追踪预览"></canvas><div class="overlay" id="overlay">正在加载 Cornell Box</div></div><div class="render-controls"><div><span class="eyebrow">PROGRESSIVE PATH TRACING</span><div class="metric"><b id="spp">0</b><span>spp · 累积样本</span></div></div><div class="control"><label for="exposure">曝光 <output id="exposure-value">1.0</output></label><input id="exposure" type="range" min="0.2" max="3" step="0.1" value="1"></div><button id="pause">暂停</button><button id="restart">重新累积</button><div class="control"><label for="resolution">预览最长边</label><select id="resolution"><option>128</option><option>256</option><option selected>384</option><option>512</option></select></div><label class="clamp"><input id="clamp" type="checkbox" checked>采样钳制 64/8</label></div><div class="footer-note">拖动画布旋转相机 · 相机变化会重置累积 <span id="load-time"></span></div></section>
<aside class="right"><div class="eyebrow">INSPECTOR</div><h2 id="selection">相机</h2><div id="summary" class="summary"></div><div class="divider"></div><div class="row"><h2>原始 PBRT 参数</h2><span class="tag">Scene IR</span></div><pre id="parameters"></pre><details><summary>Transform</summary><pre id="transform"></pre></details><details id="diagnostic-panel"><summary>诊断 <span id="diagnostic-count">0</span></summary><pre id="diagnostics"></pre></details><p class="hint">当前渲染支持 Cornell 的 diffuse 与面积光。复杂场景保留完整参数并显示尚未实现的特性。</p></aside></main>`;
const $=<T extends HTMLElement=HTMLElement>(id:string)=>document.getElementById(id) as T;
let generation=0,metadata:Metadata|undefined,selection:Entity|undefined;
const renderer=new Renderer($<HTMLCanvasElement>('canvas'),(s,error=false)=>{$('status').textContent=s;$('indicator').className=error?'error':'ready';if(error){$('overlay').textContent=s;$('overlay').hidden=false;}},s=>{$('spp').textContent=s.toString();});
const worker=new Worker(new URL('./worker/load.ts',import.meta.url),{type:'module'});
function inspect(entity:Entity,label:string){selection=entity;$('selection').textContent=label;$('parameters').textContent=JSON.stringify({type:entity.type,parameters:entity.parameters,attributes:entity.inheritedParameters,references:entity.references,material:entity.material,areaLight:entity.areaLight,reverseOrientation:entity.reverseOrientation},null,2);$('transform').textContent=JSON.stringify({start:entity.transform,end:entity.endTransform},null,2);}
function group(title:string,items:Entity[]){const details=document.createElement('details');details.open=title==='Geometry';const summary=document.createElement('summary');summary.textContent=title+' · '+items.length;details.append(summary);items.forEach((e,i)=>{const b=document.createElement('button');b.className='tree-item';b.textContent=e.name||`${e.type} ${i}`;b.onclick=()=>{document.querySelectorAll('.tree-item').forEach(x=>x.classList.remove('active'));b.classList.add('active');inspect(e,b.textContent!);};details.append(b);});$('tree').append(details);}
function show(meta:Metadata){
 metadata=meta;$('tree').replaceChildren();group('Camera',[meta.camera]);group('Render settings',[meta.film,meta.sampler,meta.integrator,meta.pixelFilter,meta.accelerator]);group('Geometry',meta.shapes);group('Materials',meta.materials);group('Textures',meta.textures);group('Lights',[...meta.lights,...meta.areaLights]);
 $('shape-count').textContent=String(meta.counts.shapes);$('summary').textContent=`${meta.counts.triangles.toLocaleString()} triangles\n${meta.counts.placements} placements · ${meta.counts.nodes} BVH nodes\n${meta.materials.length} materials · ${meta.textures.length} textures`;
 $('diagnostic-count').textContent=String(meta.diagnostics.length);$('diagnostics').textContent=meta.diagnostics.map(d=>`${d.severity} [${d.code}] ${d.message}\n${d.source}:${d.line}`).join('\n\n')||'无解析或适配诊断';inspect(meta.camera,'Camera · '+meta.camera.type);
}
let gpuError:unknown;
const ready=renderer.init().catch(e=>{gpuError=e;$('status').textContent=String(e);$('overlay').textContent=String(e);$('indicator').className='error';});
worker.onmessage=async ({data}:{data:Loaded|{type:string;generation:number;message:string}})=>{
 if(data.generation!==generation)return;
 if(data.type==='progress'){$('overlay').textContent=(data as {message:string}).message;return;}
 if(data.type==='error'){$('status').textContent=(data as {message:string}).message;$('overlay').textContent=(data as {message:string}).message;return;}
 const loaded=data as Loaded;show(loaded.metadata);$('load-time').textContent=`WASM 加载 ${(loaded.milliseconds/1000).toFixed(2)} s`;
 try{await ready;if(gpuError)throw gpuError;if(loaded.generation!==generation)return;await renderer.load(loaded.metadata,loaded.buffers);$('overlay').hidden=loaded.metadata.renderable;}catch(e){renderer.disposeScene();$('status').textContent=String(e);$('indicator').className='error';$('overlay').textContent=String(e);$('overlay').hidden=false;}
};
function load(){generation++;renderer.disposeScene();$('overlay').hidden=false;$('overlay').textContent='正在读取场景包…';worker.postMessage({generation,scene:$<HTMLSelectElement>('scene').value,base:new URL(import.meta.env.BASE_URL,location.origin).href});}
$<HTMLSelectElement>('scene').onchange=load;
$('reset').onclick=()=>{if(metadata){renderer.camera=[...metadata.camera.transform];renderer.reset();}};
$('restart').onclick=()=>renderer.reset();$('pause').onclick=()=>{renderer.paused=!renderer.paused;$('pause').textContent=renderer.paused?'继续':'暂停';};
const validate=document.createElement('button');validate.textContent='GPU 自检';document.querySelector('.toolbar')!.append(validate);
validate.onclick=async()=>{validate.disabled=true;try{const n=await renderer.validate();$('status').textContent=`GPU / CPU hit 自检 ${n}/${n} 通过`;}catch(e){$('status').textContent=String(e);$('indicator').className='error';}finally{validate.disabled=false;}};
const retry=document.createElement('button');retry.textContent='重新初始化';retry.title='重新加载页面，重建 Worker 和 GPU 资源';retry.onclick=()=>location.reload();document.querySelector('.toolbar')!.append(retry);
$<HTMLInputElement>('exposure').oninput=e=>{renderer.exposure=Number((e.target as HTMLInputElement).value);$('exposure-value').textContent=renderer.exposure.toFixed(1);};
$<HTMLSelectElement>('resolution').onchange=e=>renderer.resize(Number((e.target as HTMLSelectElement).value));
$<HTMLInputElement>('clamp').onchange=e=>{renderer.clamp=(e.target as HTMLInputElement).checked;renderer.reset();};
let drag:{x:number;y:number}|undefined;const canvas=$<HTMLCanvasElement>('canvas');canvas.onpointerdown=e=>{drag={x:e.clientX,y:e.clientY};canvas.setPointerCapture(e.pointerId);};canvas.onpointerup=()=>{drag=undefined;};canvas.onpointermove=e=>{if(drag){renderer.orbit(e.clientX-drag.x,e.clientY-drag.y);drag={x:e.clientX,y:e.clientY};}};
window.addEventListener('pagehide',()=>{worker.terminate();renderer.destroy();});
window.addEventListener('pageshow',e=>{if(e.persisted)location.reload();});
// Read-only QA snapshots; no renderer side effects are triggered through this object.
Object.defineProperty(window,'pbrtLab',{get:()=>({generation,metadata,selection,spp:renderer.sample,failed:renderer.failed,paused:renderer.paused})});
load();
