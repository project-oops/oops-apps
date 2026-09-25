/*
 * OOPSy-DAISY's on-device UI: the oops-apps index page, rendered by the SDK webview.
 *
 * This is the same catalogue page the website serves (project-oops.github.io/oops-apps), adapted
 * to the on-device renderer and bridged to the native install queue:
 *
 *   - litehtml has no CSS grid, so the card container is `display:flex; flex-wrap:wrap` rather
 *     than the website's `display:grid`. Everything else keeps the index look: the dark panel,
 *     the daisy-green accent, the rounded cards, the size badge.
 *   - The page has no mouse. The native controller (oopsy-daisy_main.c) reads the pad and drives
 *     the page by calling its functions through the JS engine: `oopsySelect(i)` highlights a card,
 *     `oopsyScreen('browse'|'queue')` switches view, `oopsyRefresh()` repaints the queue, and
 *     `oopsyError(msg)` shows a banner. Install itself is native, on X - the page never touches
 *     the network or the filesystem.
 *   - The two pieces of dynamic state come back from the native side through zero-argument bridge
 *     functions the payload registers: `__oopsy_catalog()` and `__oopsy_queue()`, each returning
 *     one of the JSON strings oopsy_catalog_json()/oopsy_queue_json() build.
 *
 * The markup is written entirely with single-quoted HTML attributes and single-quoted JavaScript
 * strings, so the C string literal below carries no escaped double-quotes and stays readable. It
 * uses only the DOM surface the webview documents: getElementById, innerHTML, textContent,
 * setAttribute. Show/hide is a `style` attribute written through setAttribute rather than the
 * `.style` property, and progress-bar widths are inline `style=` in generated markup - both are
 * plain attributes litehtml parses, not DOM-property setters the bridge might not carry.
 */

const char oopsy_page_html[] =
"<!doctype html>\n"
"<html lang='en'><head><meta charset='utf-8'>\n"
"<style>\n"
":root{--accent:#3fb950;--bg:#08090b;--panel:#111317;--panel2:#171a1f;--fg:#e9e9ea;--muted:#9aa0a6;--rule:#24272d}\n"
"*{box-sizing:border-box}\n"
"html,body{margin:0}\n"
"body{background:#08090b;color:#e9e9ea;font-family:sans-serif;line-height:1.5}\n"
".wrap{max-width:1232px;margin:0 auto;padding:0 24px}\n"
".site{padding:26px 0 16px;border-bottom:1px solid #24272d}\n"
".brand{font-size:30px;margin:0 0 8px;font-weight:700}\n"
".brand span{color:#3fb950}\n"
".tagline{color:#9aa0a6;margin:0;font-size:15px}\n"
".tagline b{color:#3fb950}\n"
".err{background:#2a1618;border:1px solid #f85149;color:#ffb4ab;border-radius:10px;padding:12px 14px;margin:16px 0}\n"
"main{padding:18px 0}\n"
".grid{font-size:0}\n"
".card{background:#111317;border:1px solid #24272d;border-radius:12px;padding:12px;width:360px;margin:0 12px 12px 0;display:inline-block;vertical-align:top;font-size:14px}\n"
".card.sel{border-color:#3fb950;background:#171a1f}\n"
".tile{width:48px;height:48px;border-radius:10px;font-size:22px;font-weight:700;color:#04120a;background:#3fb950;display:inline-block;vertical-align:top;text-align:center;line-height:48px}\n"
".body{display:inline-block;vertical-align:top;margin-left:12px;width:274px}\n"
".card h3{margin:0 0 4px;font-size:15px;font-weight:600}\n"
".meta{margin-top:6px}\n"
".badge{font-size:11px;color:#9aa0a6;border:1px solid #24272d;border-radius:6px;padding:2px 7px}\n"
".empty{color:#9aa0a6;padding:24px 0}\n"
".section{font-size:16px;margin:4px 0 14px}\n"
".qlist{display:flex;flex-direction:column;gap:12px}\n"
".qrow{background:#111317;border:1px solid #24272d;border-radius:10px;padding:12px 14px}\n"
".qhead{display:flex;justify-content:space-between;margin-bottom:8px}\n"
".qname{font-size:14px}\n"
".qstate{font-size:12px;color:#9aa0a6}\n"
".qstate.downloading,.qstate.installing{color:#e9e9ea}\n"
".qstate.done{color:#3fb950}\n"
".qstate.failed{color:#f85149}\n"
".bar{height:10px;background:#20242a;border-radius:6px;overflow:hidden}\n"
".fill{height:100%;background:#3fb950}\n"
".fill.failed{background:#f85149}\n"
".foot{border-top:1px solid #24272d;padding:14px 0;color:#9aa0a6;font-size:14px}\n"
"</style></head>\n"
"<body><div class='wrap'>\n"
"<header class='site'>\n"
"<h1 class='brand'>oops-apps <span>index</span></h1>\n"
"<p class='tagline'>Install homebrew straight onto this console - highlight a title and press <b>X</b>.</p>\n"
"</header>\n"
"<div id='err' class='err' style='display:none'></div>\n"
"<main id='browse'>\n"
"<div id='grid' class='grid'></div>\n"
"<p id='empty' class='empty' style='display:none'>No installable titles in the latest build.</p>\n"
"</main>\n"
"<main id='queue' style='display:none'>\n"
"<h2 class='section'>Downloads <span id='qcount'></span></h2>\n"
"<div id='qlist' class='qlist'></div>\n"
"<p id='qempty' class='empty'>Nothing queued yet. Highlight a title and press X to add it.</p>\n"
"</main>\n"
"<footer class='foot'><span id='hint'>X install | [ ] downloads | O exit</span></footer>\n"
"</div>\n"
"<script>\n"
"'use strict';\n"
"function DBG(m){try{console.log('PAGEDBG '+m);}catch(e){}}\n"
"function oesc(s){return String(s==null?'':s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');}\n"
"function obytes(n){if(!n&&n!==0)return '';var u=['B','KB','MB','GB'],i=0;while(n>=1024&&i<u.length-1){n/=1024;i++;}return (i===0?n:n.toFixed(1))+' '+u[i];}\n"
"function oshow(id,on){var e=document.getElementById(id);if(e)e.setAttribute('style',on?'':'display:none');}\n"
"var __cat=[],__sel=0;\n"
"function oCatalog(){var s;try{s=String(__oopsy_catalog());}catch(e){DBG('oCatalog call threw:'+e);return [];}DBG('oCatalog raw len='+s.length+' head='+s.substring(0,90));var a;try{a=JSON.parse(s||'[]');}catch(e){DBG('oCatalog parse threw:'+e);return [];}DBG('oCatalog parsed n='+(a&&a.length));return a;}\n"
"function oQueue(){try{return JSON.parse(__oopsy_queue()||'[]');}catch(e){return [];}}\n"
"function renderCards(){\n"
"  DBG('renderCards enter n='+__cat.length);\n"
"  var h='';\n"
"  try{\n"
"  for(var i=0;i<__cat.length;i++){var a=__cat[i];\n"
"    var letter=oesc((a.name||'?').charAt(0).toUpperCase());\n"
"    var tag=a.size?obytes(a.size):'title';\n"
"    h+='<div class=\\'card\\' id=\\'card-'+i+'\\'><div class=\\'tile\\'>'+letter+'</div><div class=\\'body\\'><h3>'+oesc(a.name)+'</h3><div class=\\'meta\\'><span class=\\'badge\\'>'+oesc(tag)+'</span></div></div></div>';\n"
"  }\n"
"  }catch(e){DBG('renderCards build threw:'+e);}\n"
"  DBG('renderCards html.len='+h.length);\n"
"  var g=document.getElementById('grid');\n"
"  DBG('renderCards grid='+(g?'found':'MISSING'));\n"
"  if(g){g.innerHTML=h;var cc=0;for(var k=0;k<__cat.length;k++){if(document.getElementById('card-'+k))cc++;}DBG('renderCards cards_in_dom='+cc+'/'+__cat.length);}\n"
"  oshow('empty',__cat.length===0);\n"
"}\n"
"function oLayoutDump(){try{var G=document.getElementById('grid');var C=document.getElementById('card-0');function R(e){if(!e||!e.getBoundingClientRect)return 'na';var r=e.getBoundingClientRect();return r.x+','+r.y+' '+r.width+'x'+r.height;}DBG('LAYOUT grid='+R(G)+' card0='+R(C)+' body='+R(document.body));}catch(e){DBG('LAYOUT threw:'+e);}}\n"
"function oopsySelect(i){\n"
"  if(i<0)i=0;if(i>=__cat.length)i=__cat.length-1;if(i<0)i=0;__sel=i;\n"
"  for(var k=0;k<__cat.length;k++){var c=document.getElementById('card-'+k);if(c)c.setAttribute('class',k===i?'card sel':'card');}\n"
"}\n"
"function oopsyScreen(name){\n"
"  var q=(name==='queue');\n"
"  oshow('browse',!q);oshow('queue',q);\n"
"  document.getElementById('hint').innerHTML=q?'O back':'X install | [ ] downloads | O exit';\n"
"  if(q)oopsyRefresh();\n"
"}\n"
"function oopsyRefresh(){\n"
"  var jobs=oQueue(),h='';\n"
"  for(var i=0;i<jobs.length;i++){var j=jobs[i];\n"
"    var label=j.state;if(j.state==='failed'&&j.error)label=j.error;else if(j.state==='done')label='installed';\n"
"    var pct=(j.pct|0);if(pct<0)pct=0;if(pct>100)pct=100;\n"
"    var ff=(j.state==='failed')?'fill failed':'fill';\n"
"    h+='<div class=\\'qrow\\'><div class=\\'qhead\\'><span class=\\'qname\\'>'+oesc(j.name)+'</span><span class=\\'qstate '+oesc(j.state)+'\\'>'+oesc(label)+'</span></div><div class=\\'bar\\'><div class=\\''+ff+'\\' style=\\'width:'+pct+'%\\'></div></div></div>';\n"
"  }\n"
"  document.getElementById('qlist').innerHTML=h;\n"
"  document.getElementById('qcount').textContent=jobs.length?('('+jobs.length+')'):'';\n"
"  oshow('qempty',jobs.length===0);\n"
"}\n"
"function oopsyError(msg){var e=document.getElementById('err');e.textContent=msg||'';oshow('err',!!msg);}\n"
"function oopsyInit(){DBG('oopsyInit catfn='+(typeof __oopsy_catalog)+' qfn='+(typeof __oopsy_queue));try{__cat=oCatalog();DBG('oopsyInit cat.n='+(__cat&&__cat.length));renderCards();DBG('oopsyInit after renderCards');oopsySelect(0);DBG('oopsyInit after select');oopsyRefresh();DBG('oopsyInit done');}catch(e){DBG('oopsyInit THREW:'+e+' '+(e&&e.stack));}}\n"
"window.oopsySelect=oopsySelect;window.oopsyScreen=oopsyScreen;window.oopsyRefresh=oopsyRefresh;window.oopsyError=oopsyError;\n"
"oopsyInit();\n"
"setInterval(oopsyRefresh,700);\n"
"</script>\n"
"</body></html>\n";
