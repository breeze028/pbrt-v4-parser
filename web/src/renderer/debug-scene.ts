// Display adapter for existing layout-v2 buffers. No new scene parser or loader.
export interface DebugMesh {
  placement: number;
  material: number;
  positions: Float32Array;
  color: [number, number, number];
  emission: [number, number, number];
}
export interface DebugScene {
  meshes: DebugMesh[];
  bounds: { min: number[]; max: number[] };
}
export function debugScene(buffers: ArrayBuffer[]): DebugScene {
  const vertices = new Float32Array(buffers[0]);
  const primitives = new Uint32Array(buffers[1]);
  const records = new Uint32Array(buffers[3]);
  const floats = new Float32Array(buffers[3]);
  if (records[0] !== 2) throw Error('Debug geometry requires GPU layout v2');
  const byShape = new Map<number, number[]>();
  for (let p = 0; p < primitives.length / 4; p++) {
    const shape = primitives[p * 4 + 3];
    if (!byShape.has(shape)) byShape.set(shape, []);
    byShape.get(shape)!.push(p);
  }
  const min = [Infinity, Infinity, Infinity], max = [-Infinity, -Infinity, -Infinity];
  const meshes: DebugMesh[] = [];
  for (let placement = 0; placement < records[5]; placement++) {
    const b = records[4] + placement * 40;
    const material = records[b + 37], shape = records[b + 38];
    const flags = records[b + 39];
    const flip = Boolean(flags & 1) !== Boolean(flags & 2);
    const positions: number[] = [];
    for (const p of byShape.get(shape) ?? []) {
      // Match geometric() in WGSL, including orientation and mirrored transforms.
      for (const corner of flip ? [0, 2, 1] : [0, 1, 2]) {
        const v = primitives[p * 4 + corner] * 12;
        for (let row = 0; row < 3; row++) {
          const k = b + row * 4;
          const value = floats[k] * vertices[v] + floats[k + 1] * vertices[v + 1]
            + floats[k + 2] * vertices[v + 2] + floats[k + 3];
          positions.push(value);
          min[row] = Math.min(min[row], value);
          max[row] = Math.max(max[row], value);
        }
      }
    }
    const m = records[2] + material * 16;
    meshes.push({ placement, material, positions: new Float32Array(positions),
      color: material < records[3] ? [floats[m], floats[m + 1], floats[m + 2]] : [.7, .7, .7],
      emission: [floats[b + 32], floats[b + 33], floats[b + 34]] });
  }
  return { meshes, bounds: { min, max } };
}
