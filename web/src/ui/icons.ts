// Small, original stroke icons. No icon font, external request, or runtime dependency.
const paths:Record<string,string>={
 layers:'<path d="m12 3 9 5-9 5-9-5 9-5Zm-9 9 9 5 9-5M3 16l9 5 9-5"/>',
 inspect:'<circle cx="10" cy="10" r="6"/><path d="m15 15 6 6M7 10h6m-3-3v6"/>',
 settings:'<path d="M4 6h16M4 12h16M4 18h16"/><path d="M8 3v6m8 0v6M10 15v6"/>',
 reset:'<path d="M3 10a9 9 0 1 1 2 8M3 4v6h6"/>',
 pause:'<path d="M8 5v14M16 5v14"/>',play:'<path d="m8 4 12 8-12 8V4Z"/>',
 expand:'<path d="M9 3H3v6m12-6h6v6M3 15v6h6m12-6v6h-6"/>',
 sun:'<circle cx="12" cy="12" r="4"/><path d="M12 2v2m0 16v2M2 12h2m16 0h2M5 5l1 1m12 12 1 1M5 19l1-1M18 6l1-1"/>',
 close:'<path d="m6 6 12 12M6 18 18 6"/>',
 chevron:'<path d="m9 6 6 6-6 6"/>',
 code:'<path d="m8 6-6 6 6 6m8-12 6 6-6 6M14 3l-4 18"/>',
 download:'<path d="M12 3v12m-5-5 5 5 5-5M4 16v5h16v-5"/>',
 help:'<circle cx="12" cy="12" r="9"/><path d="M9 9a3 3 0 1 1 4 3c-1 .5-1 1-1 2m0 3h.01"/>',
 panel:'<rect x="3" y="4" width="18" height="16" rx="2"/><path d="M9 4v16"/>',
 activity:'<path d="M2 12h5l3-8 4 16 3-8h5"/>',
 github:'<path d="M9 19c-4 1-4-2-6-2m12 5v-4a4 4 0 0 0-1-3c3 0 6-1 6-6a5 5 0 0 0-1-3 5 5 0 0 0 0-4s-2 0-4 2a15 15 0 0 0-6 0C7 2 5 2 5 2a5 5 0 0 0 0 4 5 5 0 0 0-1 3c0 5 3 6 6 6a4 4 0 0 0-1 3v4"/>',
 search:'<circle cx="10" cy="10" r="6"/><path d="m15 15 6 6"/>',
 box:'<path d="m12 3 9 5v9l-9 5-9-5V8l9-5Zm0 9v10M3 8l9 4 9-4M8 5l9 5"/>',
 camera:'<rect x="3" y="6" width="13" height="13" rx="2"/><path d="m16 10 5-3v11l-5-3M6 3h5"/>'
};
export function icon(name:string){return `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">${paths[name]??paths.box}</svg>`;}
export function tool(id:string,name:string,glyph:string,shortcut=''){return `<button id="${id}" class="icon-button" aria-label="${name}" title="${name}${shortcut?' ('+shortcut+')':''}">${icon(glyph)}</button>`;}
