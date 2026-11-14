struct Frame { camera:array<vec4f,4>, size:vec4u, params:vec4f, sampling:vec4f }
@group(0) @binding(0) var<uniform> frame:Frame;
@group(0) @binding(1) var<storage,read> accumulation:array<vec4f>;
@vertex fn vertex(@builtin(vertex_index) index:u32)->@builtin(position) vec4f {
  let p=array<vec2f,3>(vec2f(-1,-1),vec2f(3,-1),vec2f(-1,3));return vec4f(p[index],0,1);
}
@fragment fn fragment(@builtin(position) position:vec4f)->@location(0) vec4f {
  let x=min(u32(position.x),frame.size.x-1u);let y=min(u32(position.y),frame.size.y-1u);let c=accumulation[y*frame.size.x+x];
  let linear=max(c.xyz/max(c.w,1),vec3f(0))*frame.params.y;
  return vec4f(pow(vec3f(1)-exp(-linear),vec3f(1/2.2)),1);
}
