import { build } from 'esbuild';
import vm from 'node:vm';
import assert from 'node:assert/strict';

const mocks = {
  '@nx.js/constants': 'export const AppletType={Application:0}; export const HidNpadButton={A:1,B:2,Plus:1024};',
  '@nx.js/install-title': 'export async function* installNsp(){ console.debug("internal title ID"); if(globalThis.failInstall) throw Error("synthetic failure"); globalThis.committed=true; yield {}; }',
  '@nx.js/ncm': 'export const NcmStorageId={SdCard:1};',
  '@tootallnate/nacp': 'export class NACP {buffer=new ArrayBuffer(16)}',
  'react': 'export function useState(initial){return [initial,value=>globalThis.states.push(value)]} export function useEffect(fn){fn()}',
  'react-tela': 'export const Rect="Rect", Text="Text";',
  'react-tela/render': 'export function render(element){element.type(element.props)}',
  'react/jsx-runtime': 'export const Fragment="Fragment"; export function jsx(type,props){return {type,props}} export const jsxs=jsx;',
  './prod-keys': 'export function loadForwarderKeys(){return new Uint8Array(2)}',
  './util': 'export async function deterministicTitleId(){return "0500123456789000"}',
  './hacbrewpack.js': `export default async function createModule(options){
    globalThis.silentPackager=!!options.print&&!!options.printErr;
    const module={FS:{mkdir(){},writeFile(){},readdir(){return ['title.nsp']},readFile(){return new Uint8Array(4)}},callMain(){return 0}};
    options.preRun(module); options.onRuntimeInitialized(); return module;
  }`,
};
const built = await build({entryPoints:['src/main.tsx'],bundle:true,write:false,format:'iife',platform:'neutral',jsx:'automatic',plugins:[{
  name:'installer-fixtures',setup(build){
    build.onResolve({filter:/.*/},args=>Object.hasOwn(mocks,args.path)?{path:args.path,namespace:'fixture'}:undefined);
    build.onLoad({filter:/.*/,namespace:'fixture'},args=>({contents:mocks[args.path],loader:'js'}));
  },
}]});
async function fixture({fail=false,held=false}={}) {
  let now=1000, buttons=held?1n:0n;
  const frames=[], results=[], launches=[], exits=[], debug=[];
  const request={version:1,name:'Synthetic game',author:'KazaR-RooT',displayVersion:'0.1',nroPath:'sdmc:/switch/NXNow/shortcuts/game.nro',args:'sdmc:/switch/NXNow/shortcuts/game.nro',language:'fr',returnPath:'sdmc:/switch/NxNow.nro'};
  const context=vm.createContext({
    console:{debug:message=>debug.push(message)},failInstall:fail,states:[],Blob,TextEncoder,TextDecoder,ArrayBuffer,Uint8Array,
    Date:{now:()=>now},screen:{},requestAnimationFrame:fn=>frames.push(fn),addEventListener(){},
    navigator:{getGamepads:()=>[{rawButtons:buttons}]},
    Switch:{appletType:()=>0,readFileSync:path=>path.endsWith('.json')?new TextEncoder().encode(JSON.stringify(request)).buffer:new ArrayBuffer(8),
      writeFileSync(path,data){const result=JSON.parse(data);assert.equal(result.success,context.committed===true);results.push(result)},removeSync(){},
      Application:class {constructor(path){this.path=path;this.icon=new ArrayBuffer(8)} launch(){launches.push(this.path)}},exit(){exits.push(true)},
    },
  });
  vm.runInContext(built.outputFiles[0].text,context);
  async function pump() { for(let i=0;i<20;++i){const callbacks=frames.splice(0);for(const frame of callbacks) frame(now);await Promise.resolve()} }
  await pump();
  assert.equal(context.silentPackager,true);
  assert.deepEqual(debug,[]);
  assert.equal(results.length,1);
  if(fail){assert(!context.states.includes('Raccourci créé'));now+=6000;await pump();assert.equal(launches.length,0)}
  else {assert(context.states.includes('Raccourci créé'));assert.equal(launches.length,0)}
  return {pump,launches,exits,setButtons:value=>{buttons=value},setTime:value=>{now=value}};
}
const timed=await fixture();timed.setTime(5999);await timed.pump();assert.equal(timed.launches.length,0);timed.setTime(6000);await timed.pump();assert(timed.launches.includes('sdmc:/switch/NxNow.nro'));
for(const button of [1n,2n]) {const f=await fixture();f.setButtons(button);await f.pump();assert(f.launches.includes('sdmc:/switch/NxNow.nro'))}
const held=await fixture({held:true});assert.equal(held.launches.length,0);held.setButtons(0n);await held.pump();held.setButtons(1n);await held.pump();assert.equal(held.launches[0],'sdmc:/switch/NxNow.nro');
const failed=await fixture({fail:true});failed.setButtons(2n);await failed.pump();assert.equal(failed.launches[0],'sdmc:/switch/NxNow.nro');
console.log('PASS installer: commit before success, silent generation, 5s/A/B dismissal, held-button guard, return to NxNow, failure stays visible');
