import type { Entity,Metadata } from '../model';
import type { Item } from './explorer';
function text(tag:string,value:string,className=''){const e=document.createElement(tag);e.textContent=value;e.className=className;return e;}
function valueNode(value:unknown):HTMLElement{
 if(Array.isArray(value)&&value.length>12){const d=document.createElement('details');d.append(text('summary',`${value.length.toLocaleString()} values`),text('pre',JSON.stringify(value,null,2)));return d;}
 return text('code',typeof value==='object'?JSON.stringify(value):String(value??'—'));
}
function section(title:string,open=true){const d=document.createElement('details');d.className='property-section';d.open=open;d.append(text('summary',title));return d;}
function field(host:HTMLElement,key:string,value:unknown){const row=document.createElement('div');row.className='property-row';row.append(text('span',key),valueNode(value));host.append(row);}
export class Properties {
 private item?:Item;private raw=false;
 constructor(private host:HTMLElement,private rawHost:HTMLElement,private title:HTMLElement,private kind:HTMLElement,private texture:HTMLCanvasElement,private caption:HTMLElement){}
 select(item:Item,meta:Metadata,records?:Uint32Array,pixels?:ArrayBuffer){
  this.item=item;this.title.textContent=item.label;this.kind.textContent=item.category;this.host.replaceChildren();this.rawHost.textContent='';this.caption.textContent='';this.texture.parentElement!.hidden=true;
  const data=item.data as Record<string,unknown>,e=item.entity;
  const identity=section('Identity');
  for(const key of ['name','type','valueType','colorSpace','source','line','scope'])if(data[key]!==undefined)field(identity,key==='source'?'Source':key==='line'?'Line':key,data[key]);
  this.host.append(identity);
  if(e){
   const params=section(`Parameters · ${e.parameters.length}`);if(!e.parameters.length)params.append(text('p','No local parameters.','muted'));
   for(const p of e.parameters){const block=document.createElement('div');block.className='parameter';block.append(text('strong',p.name),text('small',`${p.type} · ${p.colorSpace}`),valueNode(p.values));params.append(block);}this.host.append(params);
   if(e.inheritedParameters.length){const inherited=section(`Inherited parameters · ${e.inheritedParameters.length}`,false);for(const p of e.inheritedParameters){field(inherited,p.name,p.values);field(inherited,p.name+' type',p.type);}this.host.append(inherited);}
  }
  for(const key of ['transform','endTransform'])if(Array.isArray(data[key])){
   const d=section(key==='transform'?'Transform · start':'Transform · end',false),matrix=text('div','','matrix');
   (data[key] as number[]).forEach(v=>matrix.append(text('code',String(v))));d.append(matrix);this.host.append(d);
  }
  for(const key of ['references','resources'])if(Array.isArray(data[key])){
   const list=data[key] as Record<string,unknown>[],d=section(`${key==='references'?'References':'Resources'} · ${list.length}`,list.length>0);
   for(const entry of list){const block=text('div','','reference');for(const [k,v] of Object.entries(entry))field(block,k,v);d.append(block);}if(!list.length)d.append(text('p','None','muted'));this.host.append(d);
  }
  const extra=section('Associations & attributes');let count=0;
  for(const [key,value] of Object.entries(data))if(!['name','type','valueType','colorSpace','source','line','scope','parameters','inheritedParameters','transform','endTransform','references','resources'].includes(key)){field(extra,key,value);count++;}
  if(count)this.host.append(extra);
  const index=e?meta.textures.indexOf(e):-1;
  if(index>=0&&records&&pixels){const slot=records[records[8]+index*8],image=meta.images[slot];if(image){const level=image.levels[0];this.texture.width=level.width;this.texture.height=level.height;this.texture.getContext('2d')!.putImageData(new ImageData(new Uint8ClampedArray(pixels.slice(level.offset,level.offset+level.bytes)),level.width,level.height),0,0);this.caption.textContent=`${image.path}\n${level.width} × ${level.height} · ${image.levels.length} mip levels\n${image.role} · ${image.srgb?'sRGB color':'Linear data'}`;this.texture.parentElement!.hidden=false;}}
  this.setRaw(this.raw);
 }
 setRaw(raw:boolean){this.raw=raw;this.host.hidden=raw;this.rawHost.hidden=!raw;this.texture.parentElement!.hidden=raw||!this.caption.textContent;if(raw)this.rawHost.textContent=this.item?JSON.stringify(this.item.data,null,2):'Select a scene node to inspect its original data.';}
 clear(){this.item=undefined;this.host.replaceChildren(text('p','Load a scene and select a node.','empty-state'));this.rawHost.textContent='';this.title.textContent='No selection';this.kind.textContent='Properties';this.caption.textContent='';this.texture.parentElement!.hidden=true;}
}
