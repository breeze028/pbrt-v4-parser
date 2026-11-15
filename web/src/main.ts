import './style.css';
import { Renderer } from './renderer/renderer';
import type { Entity,Loaded,Metadata } from './model';
import { icon,tool } from './ui/icons';
import { Explorer } from './ui/explorer';
import { Properties } from './ui/properties';

const $=<T extends HTMLElement=HTMLElement>(id:string)=>document.getElementById(id) as T;
document.querySelector<HTMLDivElement>('#app')!.innerHTML=`
<header class="topbar">
 <a class="brand" href="https://github.com/breeze028/pbrt-v4-parser" aria-label="PBRT Scene Lab on GitHub"><span class="brand-mark">${icon('layers')}</span><strong>PBRT <span>Scene Lab</span></strong><span class="version">0.1</span></a>
 <div class="scene-picker"><label for="scene">SCENE</label><select id="scene"><option value="spaceship">Spaceship</option><option value="bathroom">Bathroom</option><option value="cornell-box">Cornell Box</option></select></div>
 <nav class="mode-switch" aria-label="Workspace mode"><button id="showcase" aria-pressed="true">Showcase</button><button id="inspect" aria-pressed="false">Inspect</button></nav>
 <a class="github-link" href="https://github.com/breeze028/pbrt-v4-parser" title="View source on GitHub">${icon('github')}<span>Source</span></a>
</header>
<main id="workspace" data-mode="showcase">
 <aside id="explorer-panel" class="panel explorer-panel" aria-label="Scene Explorer" inert>
  <div class="panel-heading"><span>${icon('layers')}Scene Explorer</span>${tool('close-explorer','Close Scene Explorer','close')}</div>
  <div class="explorer-filters"><label class="search-field">${icon('search')}<input id="search" type="search" placeholder="Search nodes…" aria-label="Search scene nodes" autocomplete="off"><kbd>/</kbd></label>
   <select id="category" aria-label="Filter node category"><option>All</option><option>Camera</option><option>Geometry</option><option>Materials</option><option>Textures</option><option>Lights</option><option>Instances</option><option>Objects</option><option>Media</option><option>Render settings</option></select></div>
  <div id="tree" class="tree-viewport"></div><div class="panel-footer"><span id="node-count">Loading structure…</span><span>Scene IR</span></div>
  <div id="resize-left" class="resize-handle" role="separator" aria-orientation="vertical" aria-label="Resize Scene Explorer" tabindex="0"></div>
 </aside>
 <section id="viewport" class="viewport" aria-label="Path tracing viewport">
  <div class="scene-caption"><div class="eyebrow">PBRT V4 / PATH TRACING</div><h1 id="scene-title">Spaceship</h1><p id="scene-credit">Original scene · thecali · CC0</p></div>
  <div class="viewport-actions"><button id="toggle-explorer" class="text-button inspect-only" title="Toggle Scene Explorer" aria-expanded="true">${icon('layers')}<span>Explorer</span></button><button id="toggle-properties" class="text-button inspect-only" title="Toggle Properties" aria-expanded="true">${icon('panel')}<span>Properties</span></button></div>
  <div id="film-area" class="film-area"><canvas id="canvas" aria-label="WebGPU path tracing preview"></canvas></div>
  <div id="overlay" class="loading-overlay" role="status"><div class="loading-mark">${icon('layers')}</div><strong id="loading-title">Preparing your scene</strong><p id="loading-message">Initializing WebGPU…</p><button id="retry" hidden>Reload application</button></div>
  <div class="floating-toolbar" role="toolbar" aria-label="Viewport controls">
   ${tool('reset','Reset camera','reset','R')}${tool('pause','Pause rendering','pause','Space')}${tool('restart','Restart accumulation','activity')}
   <span class="toolbar-divider"></span>${tool('exposure-toggle','Exposure','sun')}${tool('settings-toggle','Render settings','settings')}
   <span class="toolbar-divider"></span>${tool('fullscreen','Fullscreen','expand','F')}${tool('help-toggle','Camera controls & help','help','?')}
  </div>
  <section id="exposure-panel" class="popover exposure-popover" aria-label="Exposure" hidden><div class="popover-heading"><h2>Exposure</h2>${tool('close-exposure','Close exposure','close')}</div><label class="field" for="exposure"><span>Display exposure <output id="exposure-value">1.0</output></span><input id="exposure" type="range" min="0.2" max="3" step="0.1" value="1"></label><p class="muted">Adjusts the display. Linear HDR data is unchanged.</p></section>
  <section id="settings-panel" class="popover settings-popover" aria-label="Render settings" hidden>
   <div class="popover-heading"><div><div class="eyebrow">PATH TRACER</div><h2>Render settings</h2></div>${tool('close-settings','Close render settings','close')}</div>
   <div class="settings-grid">
    <label class="field">Preview resolution<select id="resolution"><option>128</option><option>256</option><option selected>384</option><option>512</option></select></label>
    <label class="field">Target samples<input id="target-spp" type="number" min="0" max="100000" step="1" value="0"><small>0 = continuous</small></label>
    <label class="check wide"><input id="manual-depth" type="checkbox" disabled>Override scene path depth</label>
    <label class="field">Max depth<input id="depth" type="number" min="1" max="1024" step="1" value="8" disabled></label>
    <label class="field">Random seed<input id="seed" type="number" min="0" max="4294967295" step="1" value="1"></label>
    <p id="depth-info" class="muted wide">Reading scene defaults…</p>
    <label class="check"><input id="clamp" type="checkbox" checked>Sample clamp 64/8</label><label class="check"><input id="lod0" type="checkbox">Force LOD 0</label>
    <label class="check"><input id="enable-alpha" type="checkbox" checked>Alpha masks</label><label class="check"><input id="enable-bump" type="checkbox" checked>Bump mapping</label>
   </div><div class="popover-footer"><button id="export-hdr" class="text-button">${icon('download')}Export HDR <span class="badge">PFM</span></button></div>
  </section>
  <section id="help-panel" class="popover" aria-label="Camera controls and help" hidden><div class="popover-heading"><h2>Explore the scene</h2>${tool('close-help','Close help','close')}</div><dl class="shortcuts"><dt>Orbit</dt><dd>Drag the image</dd><dt>Move</dt><dd><kbd>W A S D</kbd> · <kbd>Q E</kbd></dd><dt>Look</dt><dd>Arrow keys</dd><dt>Dolly</dt><dd>Scroll the image</dd><dt>Reset camera</dt><dd><kbd>R</kbd></dd><dt>Pause / resume</dt><dd><kbd>Space</kbd></dd><dt>Inspect / showcase</dt><dd><kbd>I</kbd></dd><dt>Fullscreen</dt><dd><kbd>F</kbd></dd></dl><p class="muted">Focus the image to move. Hold Shift for larger steps. Select scene nodes in the Explorer; viewport picking is not available.</p><a id="license-link">Open-source licenses ↗</a></section>
 </section>
 <aside id="properties-panel" class="panel properties-panel" aria-label="Properties Inspector" inert>
  <div class="panel-heading"><span>Properties</span>${tool('close-properties','Close Properties','close')}</div>
  <div class="selection-heading"><span id="selection-kind" class="eyebrow">CAMERA</span><h2 id="selection">No selection</h2></div>
  <div class="property-tabs" role="tablist" aria-label="Property view"><button id="properties-tab" role="tab" aria-selected="true" aria-controls="property-content">Properties</button><button id="raw-tab" role="tab" aria-selected="false" aria-controls="raw-data">${icon('code')}Raw Data</button></div>
  <div class="property-scroll"><figure id="texture-card" hidden><canvas id="texture-preview" aria-label="Texture preview"></canvas><figcaption id="texture-info"></figcaption></figure><div id="property-content" role="tabpanel" aria-labelledby="properties-tab"></div><pre id="raw-data" role="tabpanel" aria-labelledby="raw-tab" hidden></pre></div>
  <div class="panel-footer">Read-only · Original PBRT values</div><div id="resize-right" class="resize-handle" role="separator" aria-orientation="vertical" aria-label="Resize Properties" tabindex="0"></div>
 </aside>
 <section id="diagnostic-drawer" class="diagnostic-drawer" aria-label="Diagnostics" hidden><div class="drawer-heading"><div class="diagnostic-tabs" role="tablist" aria-label="Diagnostic view"><button id="scene-log-tab" role="tab" aria-selected="true">Scene <span id="diagnostic-count" class="badge">0</span></button><button id="gpu-log-tab" role="tab" aria-selected="false">GPU checks</button><button id="activity-log-tab" role="tab" aria-selected="false">Activity</button></div><button id="validate" class="text-button">${icon('activity')}Run GPU self-test</button>${tool('close-diagnostics','Close diagnostics','close')}</div><pre id="diagnostics" role="tabpanel"></pre><pre id="gpu-checks" role="tabpanel" hidden>Not run. Run the self-test to read back actual GPU results.</pre><pre id="activity-log" role="tabpanel" hidden></pre></section>
</main>
<footer class="statusbar"><div class="status-primary"><i id="indicator" class="busy"></i><span id="status" role="status">Initializing WebGPU…</span></div><div class="status-metrics"><span><b id="spp">0</b> spp</span><span id="triangle-count">— triangles</span><span id="buffer-size">—</span><span id="gpu-adapter">GPU pending</span></div><button id="diagnostics-toggle" title="Toggle diagnostics" aria-expanded="false">${icon('activity')}<span>Diagnostics</span><span id="diagnostic-badge" class="badge">0</span></button></footer>`;

let generation=0,metadata:Metadata|undefined,selection:Entity|undefined,imageBytes:ArrayBuffer|undefined,packedRecords:Uint32Array|undefined,loading=true;
let mode:'showcase'|'inspect'='showcase',leftOpen=true,rightOpen=true,manualDepth:number|undefined;
const sceneNames:Record<string,[string,string]>={'cornell-box':['Cornell Box','Original PBRT scene · CC0'],spaceship:['Spaceship','Original scene · thecali · CC0'],bathroom:['Bathroom','Original scene · Mareck · CC0']};
const activity:string[]=[];
function log(message:string){activity.push(message);if(activity.length>80)activity.shift();$('activity-log').textContent=activity.join('\n');}
function status(message:string,error=false){$('status').textContent=message;$('status').title=message;$('indicator').className=error?'error':loading?'busy':'ready';if(error){$('loading-title').textContent='Unable to render this scene';$('loading-message').textContent=message;$('overlay').hidden=false;$('retry').hidden=false;log(message);}}
function pauseLabel(paused:boolean){if($('pause').getAttribute('aria-label')===(paused?'Resume rendering':'Pause rendering'))return;$('pause').innerHTML=icon(paused?'play':'pause');$('pause').setAttribute('aria-label',paused?'Resume rendering':'Pause rendering');$('pause').title=(paused?'Resume rendering':'Pause rendering')+' (Space)';}
const renderer=new Renderer($<HTMLCanvasElement>('canvas'),status,s=>{$('spp').textContent=s.toLocaleString();pauseLabel(renderer.paused);if(renderer.paused&&renderer.maxSpp>0&&s>=renderer.maxSpp)status(`Target reached · ${renderer.maxSpp} spp`);});
const worker=new Worker(new URL('./worker/load.ts',import.meta.url),{type:'module'});
const properties=new Properties($('property-content'),$('raw-data'),$('selection'),$('selection-kind'),$<HTMLCanvasElement>('texture-preview'),$('texture-info'));
const explorer=new Explorer($('tree'),$('node-count'),(item,fromUser)=>{if(!metadata)return;selection=item.entity;properties.select(item,metadata,packedRecords,imageBytes);if(fromUser&&mode==='inspect'&&window.innerWidth<1440){rightOpen=true;leftOpen=false;updatePanels();}});
$('license-link').setAttribute('href',import.meta.env.BASE_URL+'licenses/NOTICE.txt');

function updatePanels(){
 const workspace=$('workspace');workspace.dataset.mode=mode;workspace.classList.toggle('left-closed',!leftOpen);workspace.classList.toggle('right-closed',!rightOpen);
 $('explorer-panel').inert=mode!=='inspect'||!leftOpen;$('properties-panel').inert=mode!=='inspect'||!rightOpen;
 $('toggle-explorer').setAttribute('aria-expanded',String(leftOpen));$('toggle-properties').setAttribute('aria-expanded',String(rightOpen));
 $('showcase').setAttribute('aria-pressed',String(mode==='showcase'));$('inspect').setAttribute('aria-pressed',String(mode==='inspect'));
}
function setMode(value:'showcase'|'inspect'){mode=value;closePopovers();if(value==='inspect'){leftOpen=true;rightOpen=window.innerWidth>=1440;}updatePanels();}
$('showcase').onclick=()=>setMode('showcase');$('inspect').onclick=()=>setMode('inspect');
$('toggle-explorer').onclick=()=>{leftOpen=!leftOpen;if(leftOpen&&window.innerWidth<1100)rightOpen=false;updatePanels();};
$('toggle-properties').onclick=()=>{rightOpen=!rightOpen;if(rightOpen&&window.innerWidth<1100)leftOpen=false;updatePanels();};
$('close-explorer').onclick=()=>{leftOpen=false;updatePanels();$('toggle-explorer').focus();};$('close-properties').onclick=()=>{rightOpen=false;updatePanels();$('toggle-properties').focus();};
function filter(){explorer.filter($<HTMLInputElement>('search').value,$<HTMLSelectElement>('category').value);}
$('search').oninput=filter;$('category').onchange=filter;
for(const raw of [false,true]){const id=raw?'raw-tab':'properties-tab';$(id).onclick=()=>{properties.setRaw(raw);$('raw-tab').setAttribute('aria-selected',String(raw));$('properties-tab').setAttribute('aria-selected',String(!raw));};}
for(const ids of [['properties-tab','raw-tab'],['scene-log-tab','gpu-log-tab','activity-log-tab']])for(const [index,id] of ids.entries())$(id).onkeydown=e=>{if(e.key==='ArrowRight'||e.key==='ArrowLeft'){e.preventDefault();const next=ids[(index+(e.key==='ArrowRight'?1:ids.length-1))%ids.length];$(next).click();$(next).focus();}};
function closePopovers(){for(const id of ['settings','exposure','help']){$(id+'-panel').hidden=true;$(id+'-toggle').setAttribute('aria-expanded','false');}}
for(const id of ['settings','exposure','help']){
 $(id+'-toggle').setAttribute('aria-expanded','false');$(id+'-toggle').setAttribute('aria-controls',id+'-panel');
 $(id+'-toggle').onclick=()=>{const opening=$(id+'-panel').hidden;closePopovers();$(id+'-panel').hidden=!opening;$(id+'-toggle').setAttribute('aria-expanded',String(opening));if(opening){if(window.innerWidth<1440){rightOpen=false;if(window.innerWidth<1100)leftOpen=false;updatePanels();}const h=$(id+'-panel').querySelector('h2')!;h.tabIndex=-1;h.focus();}};
 $('close-'+id).onclick=()=>{closePopovers();$(id+'-toggle').focus();};
}
document.addEventListener('pointerdown',e=>{if(!(e.target as HTMLElement).closest('.popover,.floating-toolbar'))closePopovers();});
function drawer(open:boolean){$('diagnostic-drawer').hidden=!open;$('workspace').classList.toggle('drawer-open',open);$('diagnostics-toggle').setAttribute('aria-expanded',String(open));}
$('diagnostics-toggle').onclick=()=>drawer(Boolean($('diagnostic-drawer').hidden));$('close-diagnostics').onclick=()=>{drawer(false);$('diagnostics-toggle').focus();};
for(const [index,id] of ['scene-log-tab','gpu-log-tab','activity-log-tab'].entries())$(id).onclick=()=>{['diagnostics','gpu-checks','activity-log'].forEach((panel,i)=>{$(panel).hidden=i!==index;});['scene-log-tab','gpu-log-tab','activity-log-tab'].forEach((tab,i)=>$(tab).setAttribute('aria-selected',String(i===index)));};

// Display fit is independent of sampling resolution. UI-only resizing keeps the
// Film aspect and the existing GPU backbuffer; only the resolution control allocates pixels.
function fitFilm(){const area=$('film-area');if(!metadata||!area.clientWidth||!area.clientHeight)return;const aspect=metadata.width/metadata.height;const width=Math.min(area.clientWidth,area.clientHeight*aspect),height=width/aspect;canvas.style.width=width+'px';canvas.style.height=height+'px';$('buffer-size').textContent=`${renderer.width} × ${renderer.height}`;}
const canvas=$<HTMLCanvasElement>('canvas');canvas.tabIndex=0;new ResizeObserver(fitFilm).observe($('film-area'));
function resizable(id:string,side:'left'|'right'){
 const handle=$(id),variable=side==='left'?'--explorer-width':'--properties-width';
 const set=(width:number)=>{const clamped=Math.max(220,Math.min(380,window.innerWidth*.33,width));$('workspace').style.setProperty(variable,clamped+'px');handle.setAttribute('aria-valuenow',String(Math.round(clamped)));};
 handle.setAttribute('aria-valuemin','220');handle.setAttribute('aria-valuemax','380');handle.setAttribute('aria-valuenow',side==='left'?'248':'300');
 handle.onpointerdown=e=>{const x=e.clientX,width=$(side==='left'?'explorer-panel':'properties-panel').getBoundingClientRect().width;handle.setPointerCapture(e.pointerId);handle.onpointermove=move=>set(width+(move.clientX-x)*(side==='left'?1:-1));};handle.onpointerup=handle.onpointercancel=()=>{handle.onpointermove=null;};
 handle.onkeydown=e=>{if(e.key==='ArrowLeft'||e.key==='ArrowRight'){e.preventDefault();set($(side==='left'?'explorer-panel':'properties-panel').getBoundingClientRect().width+(e.key==='ArrowRight'?16:-16)*(side==='left'?1:-1));}};
}
resizable('resize-left','left');resizable('resize-right','right');
window.addEventListener('resize',()=>{if(window.innerWidth<1100&&leftOpen&&rightOpen)rightOpen=false;updatePanels();fitFilm();});

function applyDepth(reset=true){if(!metadata)return;const manual=$<HTMLInputElement>('manual-depth');manual.disabled=loading;if(manual.checked&&manualDepth===undefined)manualDepth=metadata.maxDepth;renderer.depth=manual.checked?manualDepth!:metadata.maxDepth;const input=$<HTMLInputElement>('depth');input.disabled=loading||!manual.checked;input.value=String(renderer.depth);$('depth-info').textContent=`Scene default: ${metadata.maxDepth} · Active: ${renderer.depth}${manual.checked?' (override)':''}`;if(reset)renderer.reset();}
let gpuError:unknown;
const ready=renderer.init().then(()=>{$('gpu-adapter').textContent=renderer.gpuName;$('gpu-adapter').title=renderer.gpuName;log('WebGPU adapter: '+renderer.gpuName);}).catch(e=>{gpuError=e;status(String(e),true);});
worker.onmessage=async({data}:{data:Loaded|{type:string;generation:number;message:string}})=>{
 if(data.generation!==generation)return;
 if(data.type==='progress'){$('loading-message').textContent=(data as {message:string}).message;return;}
 if(data.type==='error'){loading=false;status((data as {message:string}).message,true);return;}
 const loaded=data as Loaded;
 try{
  await ready;if(gpuError)throw gpuError;if(loaded.generation!==generation)return;
  metadata=loaded.metadata;imageBytes=loaded.buffers[4];packedRecords=new Uint32Array(loaded.buffers[3]);
  await renderer.load(metadata,loaded.buffers);if(loaded.generation!==generation)return;
  loading=false;applyDepth(false);fitFilm();explorer.load(metadata);
  $('triangle-count').textContent=`${metadata.counts.triangles.toLocaleString()} triangles`;
  const count=metadata.diagnostics.length;$('diagnostic-count').textContent=String(count);$('diagnostic-badge').textContent=String(count);
  $('diagnostics').textContent=metadata.diagnostics.map(d=>`${d.severity.toUpperCase()} [${d.code}] ${d.message}\n${d.source}:${d.line}`).join('\n\n')||'No parser or scene adaptation diagnostics.';
  log(`${sceneNames[$<HTMLSelectElement>('scene').value][0]} loaded in ${(loaded.milliseconds/1000).toFixed(2)} s · ${metadata.counts.triangles.toLocaleString()} triangles · ${metadata.counts.placements} placements`);
  if(metadata.renderable){$('overlay').hidden=true;status('Rendering');}else status('This scene requires unsupported rendering features. See Diagnostics.',true);
 }catch(e){loading=false;renderer.disposeScene();status(String(e),true);}
};
function load(){
 generation++;loading=true;renderer.disposeScene();metadata=undefined;selection=undefined;imageBytes=undefined;packedRecords=undefined;properties.clear();explorer.clear();
 const name=$<HTMLSelectElement>('scene').value;[$('scene-title').textContent,$('scene-credit').textContent]=sceneNames[name];
 $('overlay').hidden=false;$('loading-title').textContent='Preparing '+sceneNames[name][0];$('loading-message').textContent='Reading the original PBRT resources…';$('retry').hidden=true;
 $('gpu-checks').textContent='Not run for this scene.';$('diagnostics').textContent='Loading scene…';$('triangle-count').textContent='— triangles';$('diagnostic-count').textContent=$('diagnostic-badge').textContent='0';
 $<HTMLInputElement>('manual-depth').disabled=true;$<HTMLInputElement>('depth').disabled=true;$('depth-info').textContent='Reading scene defaults…';status('Loading scene');log('Loading '+sceneNames[name][0]);
 worker.postMessage({generation,scene:name,base:new URL(import.meta.env.BASE_URL,location.origin).href});
}
$('scene').onchange=load;$('retry').onclick=()=>location.reload();
$('reset').onclick=()=>{if(metadata){renderer.camera=[...metadata.camera.transform];renderer.reset();status('Camera reset');}};
$('restart').onclick=()=>{if(metadata){renderer.reset();status('Accumulation restarted');}};
$('pause').onclick=()=>{renderer.paused=!renderer.paused;if(!renderer.paused&&renderer.maxSpp&&renderer.sample>=renderer.maxSpp){renderer.maxSpp=0;$<HTMLInputElement>('target-spp').value='0';}pauseLabel(renderer.paused);status(renderer.paused?'Paused':'Rendering');};
$('fullscreen').onclick=async()=>{try{if(document.fullscreenElement)await document.exitFullscreen();else await $('app').requestFullscreen();}catch(e){log(String(e));status('Fullscreen is unavailable in this browser.');}};
document.addEventListener('fullscreenchange',()=>{$('fullscreen').setAttribute('aria-label',document.fullscreenElement?'Exit fullscreen':'Fullscreen');fitFilm();});
$('manual-depth').onchange=()=>applyDepth();$('depth').oninput=e=>{const value=(e.target as HTMLInputElement).valueAsNumber;if(!Number.isFinite(value))return;manualDepth=Math.max(1,Math.min(1024,Math.floor(value)));applyDepth();};$('depth').onchange=()=>applyDepth(false);
$('target-spp').oninput=e=>{renderer.maxSpp=Math.max(0,Math.min(100000,Math.floor(Number((e.target as HTMLInputElement).value)||0)));};
$('seed').oninput=e=>{renderer.seed=Number((e.target as HTMLInputElement).value)>>>0;renderer.reset();};
for(const [id,key] of [['clamp','clamp'],['lod0','lod0'],['enable-alpha','alpha'],['enable-bump','bump']] as const)$(id).onchange=e=>{renderer[key]=(e.target as HTMLInputElement).checked;renderer.reset();};
$('exposure').oninput=e=>{renderer.exposure=Number((e.target as HTMLInputElement).value);$('exposure-value').textContent=renderer.exposure.toFixed(1);};
$('resolution').onchange=e=>{renderer.resize(Number((e.target as HTMLSelectElement).value));fitFilm();};
$('export-hdr').onclick=async()=>{try{const hdr=await renderer.readHdr(),header=new TextEncoder().encode(`PF\n${hdr.width} ${hdr.height}\n-1.0\n`),url=URL.createObjectURL(new Blob([header,hdr.rgb.buffer],{type:'application/octet-stream'})),a=document.createElement('a');a.href=url;a.download=$<HTMLSelectElement>('scene').value+`-${hdr.spp}spp.pfm`;a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);log('Exported linear HDR · '+hdr.spp+' spp');}catch(e){log(String(e));status(String(e));}};
$('validate').onclick=async()=>{const button=$<HTMLButtonElement>('validate');button.disabled=true;const current=generation;try{const n=await renderer.validate();if(current!==generation)return;$('gpu-checks').textContent=JSON.stringify(n,null,2);status(`GPU self-test passed · ${n.rays} rays · ${n.bsdf.checked.toLocaleString()} BSDF · ${n.textures.texels} texture samples`);log('GPU self-test passed');console.info('GPU checks',n);}catch(e){if(current===generation){$('gpu-checks').textContent=String(e);status(String(e),true);}}finally{button.disabled=false;}};
let drag:{x:number;y:number}|undefined;
canvas.onpointerdown=e=>{canvas.focus();drag={x:e.clientX,y:e.clientY};canvas.setPointerCapture(e.pointerId);};canvas.onpointerup=canvas.onpointercancel=()=>{drag=undefined;};canvas.onpointermove=e=>{if(drag){renderer.orbit(e.clientX-drag.x,e.clientY-drag.y);drag={x:e.clientX,y:e.clientY};}};
canvas.onwheel=e=>{e.preventDefault();renderer.move(0,0,Math.max(-.5,Math.min(.5,-e.deltaY*.002)));};
canvas.onkeydown=e=>{const step=e.shiftKey?.45:.15;const actions:Record<string,()=>void>={w:()=>renderer.move(0,0,step),s:()=>renderer.move(0,0,-step),a:()=>renderer.move(-step,0,0),d:()=>renderer.move(step,0,0),q:()=>renderer.move(0,-step,0),e:()=>renderer.move(0,step,0),arrowleft:()=>renderer.look(-.035,0),arrowright:()=>renderer.look(.035,0),arrowup:()=>renderer.look(0,.035),arrowdown:()=>renderer.look(0,-.035)};const run=actions[e.key.toLowerCase()];if(run){e.preventDefault();run();}};
document.addEventListener('keydown',e=>{
 if(e.key==='Escape'){const open=['settings','exposure','help'].find(id=>!$(id+'-panel').hidden);closePopovers();if(open)$(open+'-toggle').focus();drawer(false);if(window.innerWidth<1440){rightOpen=false;if(window.innerWidth<1100)leftOpen=false;updatePanels();}return;}
 if(e.target instanceof HTMLInputElement||e.target instanceof HTMLSelectElement||e.target instanceof HTMLTextAreaElement||e.ctrlKey||e.metaKey||e.altKey)return;
 const key=e.key.toLowerCase();if(key==='i'){e.preventDefault();setMode(mode==='showcase'?'inspect':'showcase');}if(key==='r'){e.preventDefault();$('reset').click();}if(key==='f'){e.preventDefault();$('fullscreen').click();}if(key===' '&&!(e.target instanceof HTMLButtonElement)){e.preventDefault();$('pause').click();}if(key==='?')$('help-toggle').click();if(key==='/'){e.preventDefault();if(mode!=='inspect')setMode('inspect');leftOpen=true;updatePanels();$('search').focus();}
});
window.addEventListener('pagehide',()=>{worker.terminate();renderer.destroy();});window.addEventListener('pageshow',e=>{if(e.persisted)location.reload();});
Object.defineProperty(window,'pbrtLab',{get:()=>({generation,metadata,selection,spp:renderer.sample,failed:renderer.failed,paused:renderer.paused,depth:renderer.depth,mode})});
updatePanels();load();
