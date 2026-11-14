export interface Parameter { name:string;type:string;colorSpace:string;values:unknown[] }
export interface Entity { name:string;type:string;source:string;line:number;parameters:Parameter[];inheritedParameters:Parameter[];transform:number[];endTransform:number[];references:unknown[];material?:number;areaLight?:number;reverseOrientation?:boolean;insideMedium?:string;outsideMedium?:string }
export interface Metadata {
  layoutVersion:number;renderable:boolean;tlasRoot:number;maxBvhDepth:number;
  counts:Record<string,number>;camera:Entity;film:Entity;sampler:Entity;integrator:Entity;pixelFilter:Entity;accelerator:Entity;fov:number;width:number;height:number;maxDepth:number;filter:string;filterRadius:number[];
  shapes:Entity[];materials:Entity[];textures:Entity[];lights:Entity[];areaLights:Entity[];media:Entity[];
  objects:{name:string;shapes:number[]}[];instances:{object:number;transform:number[];endTransform:number[]}[];
  diagnostics:{severity:string;code:string;message:string;source:string;line:number}[];
}
export interface Loaded {type:'loaded';generation:number;metadata:Metadata;buffers:ArrayBuffer[];milliseconds:number}
