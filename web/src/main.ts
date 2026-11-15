import './style.css';
import { Renderer } from './renderer/renderer';
import type { Entity,Loaded,Metadata } from './model';
document.querySelector<HTMLDivElement>('#app')!.innerHTML=`
<header><div class="brand"><span class="mark">P</span><strong>PBRT <span>/ SCENE LAB</span></strong><small>WebGPU · SDK consumer</small></div><a href="https://github.com/mmp/pbrt-v4">PBRT v4 ↗</a></header>
<main><aside class="left"><div class="eyebrow">WORKSPACE</div><h1>场景检查器</h1><label for="scene">示例场景</label><select id="scene"><option value="cornell-box">Cornell Box</option><option value="spaceship">Spaceship</option><option value="bathroom">Bathroom</option></select><p class="hint">同一套 C++ Parser，通过 WASM 加载原始 PBRT。</p><div class="divider"></div><div class="row"><h2>场景层次</h2><span id="shape-count" class="tag">—</span></div><nav id="tree"></nav></aside>
<section class="viewport"><div class="toolbar"><span><i id="indicator"></i><span id="status">正在初始化 WebGPU…</span></span><button id="reset">重置相机</button></div><div class="canvas-wrap"><canvas id="canvas" aria-label="WebGPU 路径追踪预览"></canvas><div class="overlay" id="overlay">正在加载 Cornell Box</div></div><div class="render-controls"><div><span class="eyebrow">PROGRESSIVE PATH TRACING</span><div class="metric"><b id="spp">0</b><span>spp · 累积样本</span></div></div><div class="control"><label for="exposure">曝光 <output id="exposure-value">1.0</output></label><input id="exposure" type="range" min="0.2" max="3" step="0.1" value="1"></div><button id="pause">暂停</button><button id="restart">重新累积</button><div class="control"><label for="resolution">预览最长边</label><select id="resolution"><option>128</option><option>256</option><option selected>384</option><option>512</option></select></div><label class="clamp"><input id="clamp" type="checkbox" checked>采样钳制 64/8</label></div><div class="footer-note">拖动旋转 · 滚轮缩放 · 聚焦画布后 WASD/QE 移动、方向键转头 <span id="load-time"></span></div></section>
<aside class="right"><div class="eyebrow">INSPECTOR</div><h2 id="selection">相机</h2><div id="summary" class="summary"></div><div class="divider"></div><div class="row"><h2>原始 PBRT 参数</h2><span class="tag">Scene IR</span></div><pre id="parameters"></pre><figure id="texture-card" hidden><canvas id="texture-preview" aria-label="贴图预览"></canvas><figcaption id="texture-info"></figcaption></figure><details><summary>Transform</summary><pre id="transform"></pre></details><details id="diagnostic-panel"><summary>诊断 <span id="diagnostic-count">0</span></summary><pre id="diagnostics"></pre></details><p class="hint">LightTransport 风格的 RGB 路径追踪。涂层采用 Principled + coat 近似；银光谱使用 RGB 近似。原始 PBRT 参数保留在此。</p></aside></main>`;
const $=<T extends HTMLElement=HTMLElement>(id:string)=>document.getElementById(id) as T;
const licenseLink=document.createElement('a');licenseLink.href=import.meta.env.BASE_URL+'licenses/NOTICE.txt';licenseLink.textContent='开源许可';document.querySelector('header a')!.before(licenseLink);
let generation=0,metadata:Metadata|undefined,selection:Entity|undefined,imageBytes:ArrayBuffer|undefined,packedRecords:Uint32Array|undefined;
const renderer=new Renderer($<HTMLCanvasElement>('canvas'),(s,error=false)=>{$('status').textContent=s;$('indicator').className=error?'error':'ready';if(error){$('overlay').textContent=s;$('overlay').hidden=false;}},s=>{$('spp').textContent=s.toString();$('pause').textContent=renderer.paused?'继续':'暂停';});
const worker=new Worker(new URL('./worker/load.ts',import.meta.url),{type:'module'});
function inspect(entity:Entity,label:string){
 selection=entity;$('selection').textContent=label;$('parameters').textContent=JSON.stringify({type:entity.type,source:entity.source,line:entity.line,parameters:entity.parameters,attributes:entity.inheritedParameters,references:entity.references,resources:entity.resources,material:entity.material,areaLight:entity.areaLight,reverseOrientation:entity.reverseOrientation},null,2);$('transform').textContent=JSON.stringify({start:entity.transform,end:entity.endTransform},null,2);$('texture-card').hidden=true;
 const index=metadata?.textures.indexOf(entity)??-1;
 if(index>=0 && packedRecords && imageBytes){const slot=packedRecords[packedRecords[8]+index*8],image=metadata!.images[slot];if(image){
  const level=image.levels[0],canvas=$<HTMLCanvasElement>('texture-preview');canvas.width=level.width;canvas.height=level.height;
  canvas.getContext('2d')!.putImageData(new ImageData(new Uint8ClampedArray(imageBytes.slice(level.offset,level.offset+level.bytes)),level.width,level.height),0,0);
  $('texture-info').textContent=`${image.path}\n${level.width} × ${level.height} · ${image.levels.length} mip levels\n${image.role} · ${image.srgb?'sRGB color':'linear data'}`;$('texture-card').hidden=false;
 }}
}
function group(title:string,items:Entity[]){const details=document.createElement('details');details.open=title==='Geometry';const summary=document.createElement('summary');summary.textContent=title+' · '+items.length;details.append(summary);items.forEach((e,i)=>{const b=document.createElement('button');b.className='tree-item';b.textContent=e.name||`${e.type} ${i}`;b.onclick=()=>{document.querySelectorAll('.tree-item').forEach(x=>x.classList.remove('active'));b.classList.add('active');inspect(e,b.textContent!);};details.append(b);});$('tree').append(details);}
function hierarchy(title:string,items:unknown[]){const details=document.createElement('details'),summary=document.createElement('summary');summary.textContent=title+' · '+items.length;details.append(summary);items.forEach((item,i)=>{const b=document.createElement('button');b.className='tree-item';b.textContent=String((item as {name?:string}).name||i);b.onclick=()=>{$('selection').textContent=title+' · '+b.textContent;$('parameters').textContent=JSON.stringify(item,null,2);$('transform').textContent=JSON.stringify((item as {transform?:number[]}).transform??{},null,2);$('texture-card').hidden=true;};details.append(b);});$('tree').append(details);}
function show(meta:Metadata){
 metadata=meta;$('tree').replaceChildren();group('Camera',[meta.camera]);group('Render settings',[meta.film,meta.sampler,meta.integrator,meta.pixelFilter,meta.accelerator]);group('Geometry',meta.shapes);hierarchy('Objects',meta.objects);hierarchy('Instances',meta.instances);group('Materials',meta.materials);group('Textures',meta.textures);group('Lights',[...meta.lights,...meta.areaLights]);group('Media',meta.media);
 $('shape-count').textContent=String(meta.counts.shapes);$('summary').textContent=`${meta.counts.triangles.toLocaleString()} triangles\n${meta.counts.placements} placements · ${meta.counts.nodes} BVH nodes\n${meta.materials.length} materials · ${meta.textures.length} textures`;
 $('diagnostic-count').textContent=String(meta.diagnostics.length);$('diagnostics').textContent=meta.diagnostics.map(d=>`${d.severity} [${d.code}] ${d.message}\n${d.source}:${d.line}`).join('\n\n')||'无解析或适配诊断';inspect(meta.camera,'Camera · '+meta.camera.type);
}
let gpuError:unknown;
const ready=renderer.init().then(()=>{$('gpu-adapter').textContent=renderer.gpuName;}).catch(e=>{gpuError=e;$('status').textContent=String(e);$('overlay').textContent=String(e);$('indicator').className='error';});
worker.onmessage=async ({data}:{data:Loaded|{type:string;generation:number;message:string}})=>{
 if(data.generation!==generation)return;
 if(data.type==='progress'){$('overlay').textContent=(data as {message:string}).message;return;}
 if(data.type==='error'){$('status').textContent=(data as {message:string}).message;$('overlay').textContent=(data as {message:string}).message;return;}
 const loaded=data as Loaded;imageBytes=loaded.buffers[4];packedRecords=new Uint32Array(loaded.buffers[3]);show(loaded.metadata);$('load-time').textContent=`WASM 加载 ${(loaded.milliseconds/1000).toFixed(2)} s`;
 try{await ready;if(gpuError)throw gpuError;if(loaded.generation!==generation)return;await renderer.load(loaded.metadata,loaded.buffers);applyDepth(false);$('overlay').hidden=loaded.metadata.renderable;}catch(e){renderer.disposeScene();$('status').textContent=String(e);$('indicator').className='error';$('overlay').textContent=String(e);$('overlay').hidden=false;}
};
function load(){$<HTMLInputElement>('manual-depth').disabled=true;$<HTMLInputElement>('depth').disabled=true;$('depth-info').textContent='正在读取场景深度…';generation++;renderer.disposeScene();imageBytes=undefined;packedRecords=undefined;$('texture-card').hidden=true;$('gpu-checks').textContent='尚未运行';$('overlay').hidden=false;$('overlay').textContent='正在读取场景包…';worker.postMessage({generation,scene:$<HTMLSelectElement>('scene').value,base:new URL(import.meta.env.BASE_URL,location.origin).href});}
$<HTMLSelectElement>('scene').onchange=load;
$('reset').onclick=()=>{if(metadata){renderer.camera=[...metadata.camera.transform];renderer.reset();}};
$('restart').onclick=()=>{renderer.paused=false;renderer.reset();};$('pause').onclick=()=>{renderer.paused=!renderer.paused;if(!renderer.paused && renderer.maxSpp && renderer.sample>=renderer.maxSpp){renderer.maxSpp=0;$<HTMLInputElement>('target-spp').value='0';}$('pause').textContent=renderer.paused?'继续':'暂停';};
const validate=document.createElement('button');validate.textContent='GPU 自检';document.querySelector('.toolbar')!.append(validate);
validate.onclick=async()=>{validate.disabled=true;const current=generation;try{const n=await renderer.validate();if(current!==generation)return;$('status').textContent=`GPU 自检通过 · ray ${n.rays}/${n.rays} · BSDF ${n.bsdf.checked} · tex ${n.textures.texels}`;$('gpu-checks').textContent=JSON.stringify(n,null,2);console.info('GPU checks',n);}catch(e){if(current===generation){$('status').textContent=String(e);$('indicator').className='error';}}finally{validate.disabled=false;}};
const checks=document.createElement('details');checks.innerHTML='<summary>GPU 自检结果</summary><pre id="gpu-checks">尚未运行</pre>';document.querySelector('.right')!.append(checks);
const settings=document.createElement('details');settings.className='settings';settings.innerHTML='<summary>渲染设置与 HDR 导出</summary><div class="settings-grid"><label>目标 spp（0 = 持续）<input id="target-spp" type="number" min="0" max="100000" value="0"></label><label>随机种子<input id="seed" type="number" min="0" max="4294967295" value="1"></label><label class="clamp"><input id="lod0" type="checkbox">强制 LOD 0</label><button id="export-hdr">导出线性 HDR（PFM）</button></div>';document.querySelector('.footer-note')!.after(settings);
const adapterInfo=document.createElement('p');adapterInfo.id='gpu-adapter';adapterInfo.className='hint';settings.after(adapterInfo);
$<HTMLInputElement>('target-spp').oninput=e=>{renderer.maxSpp=Math.max(0,Math.min(100000,Math.floor(Number((e.target as HTMLInputElement).value)||0)));};
const depthControls=document.createElement('div');depthControls.className='depth-controls';depthControls.innerHTML='<label class="clamp"><input id="manual-depth" type="checkbox" disabled>手动控制路径深度</label><label for="depth">最大深度（max_depth）</label><input id="depth" type="number" min="1" max="1024" step="1" value="8" disabled><span id="depth-info" class="hint" aria-live="polite">正在读取场景深度…</span>';document.querySelector('.render-controls')!.after(depthControls);
let manualDepth:number|undefined;
function applyDepth(reset=true){
 if(!metadata)return;
 const manual=$<HTMLInputElement>('manual-depth');manual.disabled=false;
 if(manual.checked && manualDepth===undefined)manualDepth=metadata.maxDepth;
 renderer.depth=manual.checked?manualDepth!:metadata.maxDepth;
 const input=$<HTMLInputElement>('depth');input.disabled=!manual.checked;input.value=String(renderer.depth);
 $('depth-info').textContent=`场景默认：${metadata.maxDepth} · 当前：${renderer.depth}${manual.checked?'（手动）':'（跟随场景）'}`;
 if(reset)renderer.reset();
}
$<HTMLInputElement>('manual-depth').onchange=()=>applyDepth();
$<HTMLInputElement>('depth').oninput=e=>{const value=(e.target as HTMLInputElement).valueAsNumber;if(!Number.isFinite(value))return;manualDepth=Math.max(1,Math.min(1024,Math.floor(value)));applyDepth();};
$<HTMLInputElement>('depth').onchange=()=>applyDepth(false);
$<HTMLInputElement>('seed').oninput=e=>{renderer.seed=Number((e.target as HTMLInputElement).value)>>>0;renderer.reset();};
$<HTMLInputElement>('lod0').onchange=e=>{renderer.lod0=(e.target as HTMLInputElement).checked;renderer.reset();};
for(const [id,label] of [['enable-alpha','透明遮罩'],['enable-bump','Bump']]){const wrap=document.createElement('label');wrap.className='clamp';wrap.innerHTML=`<input id="${id}" type="checkbox" checked>${label}`;document.querySelector('.settings-grid')!.append(wrap);}
$<HTMLInputElement>('enable-alpha').onchange=e=>{renderer.alpha=(e.target as HTMLInputElement).checked;renderer.reset();};
$<HTMLInputElement>('enable-bump').onchange=e=>{renderer.bump=(e.target as HTMLInputElement).checked;renderer.reset();};
$('export-hdr').onclick=async()=>{try{const hdr=await renderer.readHdr();const header=new TextEncoder().encode(`PF\n${hdr.width} ${hdr.height}\n-1.0\n`);const url=URL.createObjectURL(new Blob([header,hdr.rgb.buffer],{type:'application/octet-stream'}));const a=document.createElement('a');a.href=url;a.download=$<HTMLSelectElement>('scene').value+`-${hdr.spp}spp.pfm`;a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);}catch(e){$('status').textContent=String(e);}};
const retry=document.createElement('button');retry.textContent='重新初始化';retry.title='重新加载页面，重建 Worker 和 GPU 资源';retry.onclick=()=>location.reload();document.querySelector('.toolbar')!.append(retry);
$<HTMLInputElement>('exposure').oninput=e=>{renderer.exposure=Number((e.target as HTMLInputElement).value);$('exposure-value').textContent=renderer.exposure.toFixed(1);};
$<HTMLSelectElement>('resolution').onchange=e=>renderer.resize(Number((e.target as HTMLSelectElement).value));
$<HTMLInputElement>('clamp').onchange=e=>{renderer.clamp=(e.target as HTMLInputElement).checked;renderer.reset();};
let drag:{x:number;y:number}|undefined;const canvas=$<HTMLCanvasElement>('canvas');canvas.tabIndex=0;canvas.onpointerdown=e=>{canvas.focus();drag={x:e.clientX,y:e.clientY};canvas.setPointerCapture(e.pointerId);};canvas.onpointerup=()=>{drag=undefined;};canvas.onpointercancel=()=>{drag=undefined;};canvas.onpointermove=e=>{if(drag){renderer.orbit(e.clientX-drag.x,e.clientY-drag.y);drag={x:e.clientX,y:e.clientY};}};
canvas.onwheel=e=>{e.preventDefault();renderer.move(0,0,Math.max(-.5,Math.min(.5,-e.deltaY*.002)));};
canvas.onkeydown=e=>{const step=e.shiftKey?.45:.15;const action:Record<string,()=>void>={w:()=>renderer.move(0,0,step),s:()=>renderer.move(0,0,-step),a:()=>renderer.move(-step,0,0),d:()=>renderer.move(step,0,0),q:()=>renderer.move(0,-step,0),e:()=>renderer.move(0,step,0),arrowleft:()=>renderer.look(-.035,0),arrowright:()=>renderer.look(.035,0),arrowup:()=>renderer.look(0,.035),arrowdown:()=>renderer.look(0,-.035)};const run=action[e.key.toLowerCase()];if(run){e.preventDefault();run();}};
window.addEventListener('pagehide',()=>{worker.terminate();renderer.destroy();});
window.addEventListener('pageshow',e=>{if(e.persisted)location.reload();});
// Read-only QA snapshots; no renderer side effects are triggered through this object.
Object.defineProperty(window,'pbrtLab',{get:()=>({generation,metadata,selection,spp:renderer.sample,failed:renderer.failed,paused:renderer.paused})});
load();
