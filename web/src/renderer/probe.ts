// Independent brute-force CPU reference. It does not walk GPU BVH nodes.
type V=[number,number,number];
const sub=(a:V,b:V):V=>[a[0]-b[0],a[1]-b[1],a[2]-b[2]];
const dot=(a:V,b:V)=>a[0]*b[0]+a[1]*b[1]+a[2]*b[2];
const cross=(a:V,b:V):V=>[a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]];
const normalize=(v:V):V=>v.map(x=>x/Math.hypot(...v)) as V;
export function probes(buffers:ArrayBuffer[],camera:number[]){
 const vertices=new Float32Array(buffers[0]),primitives=new Uint32Array(buffers[1]),records=new Uint32Array(buffers[3]),floats=new Float32Array(buffers[3]);
 const byShape=new Map<number,number[]>();for(let p=0;p<primitives.length/4;p++){const shape=primitives[p*4+3];let ids=byShape.get(shape);if(!ids){ids=[];byShape.set(shape,ids);}ids.push(p);}
 const rays=[[.123,-.071,1],[-.11,.10,1],[.09,.13,1],[.07,-.09,1],[-.08,-.05,1],[0,0,-1]].map(local=>{
   const d:V=[0,1,2].map(j=>camera[j*4]*local[0]+camera[j*4+1]*local[1]+camera[j*4+2]*local[2]) as V;const len=Math.hypot(...d);
   return {origin:[camera[3],camera[7],camera[11]] as V,direction:d.map(v=>v/len) as V};
 });
 const expected=rays.map(ray=>{
   let hit={valid:false,distance:1e30,u:0,v:0,primitive:0,instance:0,geometric:[0,0,0] as V,normal:[0,0,0] as V};
   for(let i=0;i<records[5];i++){
     const b=records[4]+i*40,shape=records[b+38];
     const point=(index:number):V=>{const p=index*12,x=vertices[p],y=vertices[p+1],z=vertices[p+2];return [0,1,2].map(j=>floats[b+j*4]*x+floats[b+j*4+1]*y+floats[b+j*4+2]*z+floats[b+j*4+3]) as V;};
     for(const j of byShape.get(shape)??[]){
       const a=point(primitives[j*4]),e1=sub(point(primitives[j*4+1]),a),e2=sub(point(primitives[j*4+2]),a),q=cross(ray.direction,e2),det=dot(e1,q);if(Math.abs(det)<1e-10)continue;
       const s=sub(ray.origin,a),u=dot(s,q)/det,r=cross(s,e1),v=dot(ray.direction,r)/det,t=dot(e2,r)/det;
       if(u>=0&&v>=0&&u+v<=1&&t>1e-5&&t<hit.distance){
         let geometric=normalize(cross(e1,e2));const flags=records[b+39];
         if(Boolean(flags&1)!==Boolean(flags&2))geometric=geometric.map(x=>-x) as V;
         let normal=geometric;
         const ids=[primitives[j*4],primitives[j*4+1],primitives[j*4+2]];
         if(ids.every(k=>vertices[k*12+7]>0)){
           const n=[0,1,2].map(k=>ids.reduce((sum,id,index)=>sum+vertices[id*12+4+k]*[1-u-v,u,v][index],0));
           normal=normalize([0,1,2].map(k=>floats[b+16+k]*n[0]+floats[b+20+k]*n[1]+floats[b+24+k]*n[2]) as V);
           if(flags&1)normal=normal.map(x=>-x) as V;
           if(dot(normal,geometric)<0)normal=normal.map(x=>-x) as V;
         }
         if(dot(normal,ray.direction)>0)normal=normal.map(x=>-x) as V;
         hit={valid:true,distance:t,u,v,primitive:j,instance:i,geometric,normal};
       }
     }
   }return hit;
 });
 const input=new Float32Array(rays.length*16);rays.forEach((ray,i)=>{input.set(ray.origin,i*16);input.set(ray.direction,i*16+4);});return {input,expected};
}
