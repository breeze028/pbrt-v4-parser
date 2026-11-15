import type { Entity,Metadata } from '../model';
import { icon } from './icons';
export interface Item {id:string;category:string;label:string;data:unknown;entity?:Entity}
interface Row {group?:string;item?:Item;count?:number}
const HEIGHT=34,OVERSCAN=5;
export class Explorer {
 private items:Item[]=[];private rows:Row[]=[];private expanded=new Set(['Camera','Geometry']);private selected='';private active=0;
 private query='';private category='All';private frame=0;
 constructor(private host:HTMLElement,private count:HTMLElement,private onSelect:(item:Item,fromUser:boolean)=>void){
  host.tabIndex=0;host.setAttribute('role','tree');host.setAttribute('aria-label','Scene nodes');
  host.addEventListener('scroll',()=>{if(!this.frame)this.frame=requestAnimationFrame(()=>{this.frame=0;this.render();});});
  new ResizeObserver(()=>this.render()).observe(host);
  host.onkeydown=e=>{
   if(!['ArrowDown','ArrowUp','Home','End','Enter','ArrowRight','ArrowLeft'].includes(e.key)||!this.rows.length)return;e.preventDefault();
   if(e.key==='ArrowDown')this.active=Math.min(this.rows.length-1,this.active+1);
   if(e.key==='ArrowUp')this.active=Math.max(0,this.active-1);
   if(e.key==='Home')this.active=0;if(e.key==='End')this.active=this.rows.length-1;
   const row=this.rows[this.active];
   if(e.key==='Enter'){this.activate(row);return;}
   if(row.group&&(e.key==='ArrowRight'||e.key==='ArrowLeft')){if(e.key==='ArrowRight')this.expanded.add(row.group);else this.expanded.delete(row.group);this.rebuild();return;}
   const top=this.active*HEIGHT;if(top<host.scrollTop)host.scrollTop=top;else if(top+HEIGHT>host.scrollTop+host.clientHeight)host.scrollTop=top+HEIGHT-host.clientHeight;
   this.render();
  };
 }
 clear(){this.items=[];this.rows=[];this.selected='';this.count.textContent='Loading structure…';this.render();}
 load(meta:Metadata){
  this.items=[];
  const add=(category:string,data:unknown[],entities=false)=>data.forEach((value,i)=>{
   const e=value as Entity;const resource=(e.resources?.[0] as {literal?:string}|undefined)?.literal;
   this.items.push({id:category+'-'+i,category,label:e.name||resource?.split('/').pop()||`${e.type||category.slice(0,-1)} ${i}`,data:value,entity:entities?e:undefined});
  });
  add('Camera',[meta.camera],true);add('Render settings',[meta.film,meta.sampler,meta.integrator,meta.pixelFilter,meta.accelerator],true);
  add('Geometry',meta.shapes,true);add('Objects',meta.objects);add('Instances',meta.instances);add('Materials',meta.materials,true);add('Textures',meta.textures,true);add('Lights',[...meta.lights,...meta.areaLights],true);add('Media',meta.media,true);
  this.selected='Camera-0';this.host.scrollTop=0;this.rebuild();this.onSelect(this.items[0],false);
 }
 filter(query:string,category:string){this.query=query.toLowerCase();this.category=category;this.host.scrollTop=0;this.active=0;this.rebuild();}
 private rebuild(){
  const matches=this.items.filter(i=>(this.category==='All'||i.category===this.category)&&(!this.query||`${i.label} ${i.category} ${i.entity?.type??''} ${i.entity?.source??''} ${JSON.stringify(i.entity?.parameters??[])}`.toLowerCase().includes(this.query)));
  this.count.textContent=`${matches.length.toLocaleString()} / ${this.items.length.toLocaleString()} nodes`;
  this.rows=[];for(const category of [...new Set(matches.map(i=>i.category))]){
   const children=matches.filter(i=>i.category===category);this.rows.push({group:category,count:children.length});
   if(this.expanded.has(category)||this.query||this.category!=='All')children.forEach(item=>this.rows.push({item}));
  }
  this.active=Math.min(this.active,Math.max(0,this.rows.length-1));this.render();
 }
 private activate(row:Row){
  if(row.group){if(this.expanded.has(row.group))this.expanded.delete(row.group);else this.expanded.add(row.group);this.rebuild();}
  else if(row.item){this.selected=row.item.id;this.onSelect(row.item,true);this.render();}
 }
 private render(){
  const start=Math.max(0,Math.floor(this.host.scrollTop/HEIGHT)-OVERSCAN),end=Math.min(this.rows.length,Math.ceil((this.host.scrollTop+this.host.clientHeight)/HEIGHT)+OVERSCAN);
  const content=document.createElement('div');content.className='tree-space';content.style.height=this.rows.length*HEIGHT+'px';
  if(!this.rows.length){content.className='empty-state';content.textContent=this.items.length?'No matching nodes. Try another search.':'Loading scene structure…';content.style.height='auto';}
  for(let i=start;i<end;i++){
   const row=this.rows[i],button=document.createElement('button');button.className='tree-row'+(row.group?' tree-group':'')+(row.item?.id===this.selected?' selected':'')+(i===this.active?' keyboard-active':'');
   button.style.top=i*HEIGHT+'px';button.tabIndex=-1;button.id='node-'+(row.item?.id??row.group?.replaceAll(' ','-'));button.setAttribute('role','treeitem');button.setAttribute('aria-level',row.group?'1':'2');
   if(row.group){button.innerHTML=icon('chevron');button.classList.toggle('expanded',this.expanded.has(row.group)||!!this.query||this.category!=='All');button.setAttribute('aria-expanded',String(this.expanded.has(row.group)||!!this.query||this.category!=='All'));}
   else{button.innerHTML=icon(row.item?.category==='Camera'?'camera':row.item?.category==='Geometry'?'box':'layers');button.setAttribute('aria-selected',String(row.item?.id===this.selected));}
   const label=document.createElement('span');label.textContent=row.group??row.item!.label;button.append(label);button.title=row.item?`${row.item.category} · ${row.item.label}`:row.group!;
   if(row.group){const n=document.createElement('small');n.textContent=String(row.count);button.append(n);}
   button.onclick=()=>{this.active=i;this.activate(row);this.host.focus({preventScroll:true});};content.append(button);
  }
  this.host.replaceChildren(content);const active=content.children[this.active-start] as HTMLElement|undefined;
  if(active?.id)this.host.setAttribute('aria-activedescendant',active.id);else this.host.removeAttribute('aria-activedescendant');
 }
}
