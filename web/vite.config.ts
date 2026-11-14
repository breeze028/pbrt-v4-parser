import { defineConfig } from 'vite';
// Same metadata/base policy as webgpu-pages-starter (MIT, see THIRD_PARTY).
export default defineConfig({base: process.env.PAGES_BASE_PATH ?? '/', build:{target:'es2022'},worker:{format:'es'}});
