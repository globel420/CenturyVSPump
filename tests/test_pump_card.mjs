import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {PROFILES,PALETTES,themeMode,DEFAULT_ENTITIES as E,numeric,nextChange,pumpModel,commandPlan,historyPoints,observedSeries,observedDuration,stepPath} from '../home_assistant/vgreen-pump-card.js';
import {installPumpOverview} from '../home_assistant/pump_dashboard.mjs';

test('all 16 dashboard profiles equal the firmware source of truth',async()=>{
  const cpp=await readFile(new URL('../components/vgreen_evo_controller/stock_schedule.h',import.meta.url),'utf8');
  const expected={};
  for(const m of cpp.matchAll(/constexpr ScheduleSegment k([AB][1-8])\[\]\s*=\s*\{([\s\S]*?)\};/g))
    expected[m[1]]=[...m[2].matchAll(/\{(\d+)U,\s*(\d+)U,\s*(\d+)U\}/g)].map(s=>s.slice(1).map(Number));
  assert.equal(Object.keys(expected).length,16);assert.deepEqual(PROFILES,expected);
});
test('next actual speed transition at every boundary, including midnight and continuous profiles',()=>{
  let checks=0;
  for(const segments of Object.values(PROFILES))for(const s of segments)for(const phase of [s[0]*3600,s[0]*3600+1,s[1]*3600-1]){
    const rpm=segments.find(p=>phase>=p[0]*3600&&phase<p[1]*3600)[2];
    const candidate=segments.filter(p=>p[2]!==rpm).map(p=>({rpm:p[2],seconds:(p[0]*3600-phase+86400)%86400})).sort((a,b)=>a.seconds-b.seconds)[0]??null;
    assert.deepEqual(nextChange(segments,phase),candidate);checks++;
  }
  assert.equal(checks,180);
  assert.deepEqual(nextChange(PROFILES.B1,18*3600),{rpm:3450,seconds:21600});
  assert.deepEqual(nextChange(PROFILES.B1,0),{rpm:2850,seconds:7200});
  assert.equal(nextChange(PROFILES.B1,null),null);
});
const now=Date.parse('2026-10-13T02:00:00Z');
function stateSet(){const s={};for(const [k,v] of Object.entries({phase:'72000',family:'B',profile:'1',healthy:'on',fault:'off',mode:'schedule stopped',actual:'0',pause:'off'}))s[E[k]]={state:v,last_updated:new Date(now).toISOString()};return s;}
test('real controller phase drives forecast; stale/disconnected/unknown/fault states fail closed',()=>{
  const s=stateSet();assert.equal(pumpModel(s,E,now).controls,true);assert.equal(pumpModel(s,E,now).actual,0);
  assert.equal(pumpModel(s,E,now+15000).phase,72015);
  assert.equal(pumpModel(s,E,now+91000).controls,false);assert.equal(pumpModel(s,E,now+91000).actual,null);
  assert.equal(pumpModel(s,E,now,false).online,false);
  for(const key of ['healthy','fault','phase','mode','pause']){const altered=structuredClone(s);altered[E[key]].state='unavailable';assert.equal(pumpModel(altered,E,now).controls,false,key);}
  s[E.fault].state='on';s[E.mode].state='fault';assert.equal(pumpModel(s,E,now).label,'Motor Fault');assert.equal(pumpModel(s,E,now).controls,false);
  assert.equal(numeric('unknown'),null);assert.equal(numeric(''),null);assert.equal(numeric('0.0'),0);
});
test('controls target existing native actions without raw motor/firmware writes',()=>{
  const s=stateSet();
  assert.deepEqual(commandPlan('pause',null,s),[{domain:'switch',service:'turn_on',data:{entity_id:E.pause}}]);
  assert.deepEqual(commandPlan('resume',null,s),[{domain:'button',service:'press',data:{entity_id:E.resume}}]);
  assert.deepEqual(commandPlan('override',null,s),[{domain:'button',service:'press',data:{entity_id:E.override}}]);
  assert.deepEqual(commandPlan('reset',null,s),[{domain:'script',service:'pool_pump_timing',data:{operation:'reset'}}]);
  for(let h=1;h<=24;h++)assert.deepEqual(commandPlan('hour',String(h),s),[{domain:'select',service:'select_option',data:{entity_id:E.hour_select,option:String(h)}}]);
  for(const h of [0,25,'1.5','01','unknown'])assert.throws(()=>commandPlan('hour',h,s));
  assert.deepEqual(commandPlan('profile','B1',s),[]);
  assert.equal(commandPlan('profile','A2',s).length,2);
  assert.throws(()=>commandPlan('profile','C1',s));
});
test('history keeps gaps and only counts observed, connected RPM',()=>{
  const speed=[{t:0,value:'1500'},{t:60000,value:'0'},{t:100000,value:'unknown'}];
  const health=[{t:0,value:'on'},{t:30000,value:'off'},{t:45000,value:'on'}];
  const points=observedSeries(speed,health,0,120000);
  assert.deepEqual(observedDuration(points,0,120000),{known:85,running:45,coverage:85/120*100});
  assert.match(stepPath(points,0,120000),/M/);assert.doesNotMatch(stepPath(points,0,120000),/NaN|undefined/);
  assert.equal(observedDuration(observedSeries(speed,[],0,120000),0,120000).known,0);
  assert.equal(observedDuration(observedSeries([],health,0,120000),0,120000).running,0);
  assert.equal(historyPoints([{state:'0',last_changed:'invalid'}]).length,0);
});
test('overview rollout preserves all other views and is idempotent',()=>{
  const input={title:'Pool Pump',extra:'preserve',views:[{path:'overview',title:'Pool Pump',type:'sections',sections:[{cards:[]}],max_columns:3},{path:'advanced',title:'Advanced',sections:[{cards:[{type:'entities',entities:['sensor.keep']}]}]}]};
  const output=installPumpOverview(input);
  assert.deepEqual(output.views[1],input.views[1]);assert.equal(output.extra,input.extra);
  assert.equal(input.views[0].type,'sections');assert.equal(output.views[0].type,'panel');
  assert.deepEqual(output.views[0].cards,[{type:'custom:vgreen-pump-card'}]);
  assert.deepEqual(installPumpOverview(output),output);
  assert.throws(()=>installPumpOverview({views:[]}));
});
test('card does not embed preview data, credentials, third-party scripts or automatic motor actions',async()=>{
  const source=await readFile(new URL('../home_assistant/vgreen-pump-card.js',import.meta.url),'utf8');
  assert.doesNotMatch(source,/Bearer |access_token|telemetry-snapshot|Preview:|supabase|https:\/\/.*\.js/);
  assert.equal((source.match(/\.callService\(/g)||[]).length,1);
  assert.ok(source.indexOf('if(!await this.confirm(messages[action]))return;')<source.indexOf('await this._hass.callService('));
});

test('theme follows the resolved HA mode, including explicit light and Auto results',()=>{
  assert.equal(themeMode({themes:{darkMode:true}}),'dark');
  assert.equal(themeMode({themes:{darkMode:false}}),'light');
  assert.equal(themeMode({themes:{darkMode:false},selectedTheme:{dark:true}}),'light');
  assert.equal(themeMode({themes:{darkMode:true},selectedTheme:{}}),'dark');
  assert.equal(themeMode({}),'light');
});
test('light and dark palettes cover identical tokens and retain readable text',()=>{
  assert.deepEqual(Object.keys(PALETTES.light).sort(),Object.keys(PALETTES.dark).sort());
  const luminance=hex=>{
    const rgb=hex.slice(1).match(/../g).map(v=>parseInt(v,16)/255).map(v=>v<=0.04045?v/12.92:((v+0.055)/1.055)**2.4);
    return rgb[0]*0.2126+rgb[1]*0.7152+rgb[2]*0.0722;
  };
  const ratio=(a,b)=>{const l=[luminance(a),luminance(b)].sort((a,b)=>b-a);return (l[0]+0.05)/(l[1]+0.05);};
  for(const [name,p] of Object.entries(PALETTES))for(const [fg,bg] of [['text','surface'],['muted','surface'],['muted','page'],['muted','soft'],['warning-text','warning-bg'],['danger-text','danger-bg'],['on-accent','accent']])
    assert.ok(ratio(p[fg],p[bg])>=4.5,`${name} ${fg}/${bg}: ${ratio(p[fg],p[bg])}`);
  assert.ok(ratio(PALETTES.dark.accent,PALETTES.dark['resume-bg'])>=4.5);
  assert.ok(ratio(PALETTES.dark.marker,PALETTES.dark.soft)>=3);
});
