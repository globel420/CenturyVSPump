/* VGreen Pump Control 1.0.1 — local Home Assistant dashboard card.
 * No credentials, external JavaScript, firmware writes or automatic motor actions.
 * Load /local/vgreen-pump-card.js as a JavaScript module.
 */
export const VERSION = '1.0.1';
export const PROFILES = {
  A1:[[0,2,3450],[2,4,2750],[4,10,1750],[10,12,1150],[12,24,0]],
  A2:[[0,2,3450],[2,4,2850],[4,10,1850],[10,12,1250],[12,24,0]],
  A3:[[0,4,3450],[4,8,1750],[8,12,1150],[12,24,0]],
  A4:[[0,2,3250],[2,10,1150],[10,12,3250],[12,24,0]],
  A5:[[0,24,1725]], A6:[[0,24,1100]], A7:[[0,24,1725]], A8:[[0,24,3450]],
  B1:[[0,2,3450],[2,6,2850],[6,10,2250],[10,18,1550],[18,24,0]],
  B2:[[0,2,3350],[2,6,2750],[6,10,2150],[10,18,1450],[18,24,0]],
  B3:[[0,2,3250],[2,6,2650],[6,10,1950],[10,18,1350],[18,24,0]],
  B4:[[0,4,3150],[4,8,2550],[8,12,1850],[12,18,1250],[18,24,0]],
  B5:[[0,4,3050],[4,8,2450],[8,12,1750],[12,18,1150],[18,24,0]],
  B6:[[0,4,2950],[4,8,2350],[8,12,1650],[12,18,1050],[18,24,0]],
  B7:[[0,2,3450],[2,6,2850],[6,10,2250],[10,24,1550]],
  B8:[[0,4,2950],[4,10,2350],[10,18,1650],[18,24,1050]],
};
const prefix = 'backyard_vgreen_pool_pump';
export const DEFAULT_ENTITIES = {
  actual:`sensor.${prefix}_actual_pump_speed`, target:`sensor.${prefix}_controller_target_speed`,
  mode:`sensor.${prefix}_controller_mode`, phase:`sensor.${prefix}_schedule_position_seconds`,
  hour:`sensor.${prefix}_schedule_hour`, healthy:`binary_sensor.${prefix}_pump_communication_healthy`,
  fault:`binary_sensor.${prefix}_motor_fault_latched`, pause:`switch.${prefix}_maintenance_pause`,
  override:`button.${prefix}_start_24_hour_override`, resume:`button.${prefix}_resume_current_schedule`,
  family:`select.${prefix}_stock_schedule_set`, profile:`select.${prefix}_stock_schedule_profile`,
  hour_select:'select.pool_pump_schedule_hour', timing:'script.pool_pump_timing',
  anchor:'input_datetime.pool_pump_hour_one_start', uptime:`sensor.${prefix}_controller_uptime`,
  reset_reason:`sensor.${prefix}_controller_reset_reason`,
  power:'sensor.pool_pump_estimated_power', energy:'sensor.pool_pump_estimated_energy_current_smud_bill',
  energy_quality:'input_text.pool_pump_energy_estimate_quality',
  energy_coverage:'input_number.pool_pump_energy_history_coverage',
};
const modeLabels = {
  'schedule running':'Running', 'schedule stopped':'Scheduled Off', 'maintenance pause':'Paused',
  '24-hour override':'Override', priming:'Priming', 'freeze protection':'Freeze Protection',
};
const mod = (n,m) => ((n%m)+m)%m;
export function numeric(value) {
  return value !== null && value !== undefined && String(value).trim() !== '' && Number.isFinite(Number(value)) ? Number(value) : null;
}
export function nextChange(segments, phase) {
  if (!segments || !Number.isFinite(phase) || phase < 0 || phase >= 86400) return null;
  const index = segments.findIndex(s=>phase>=s[0]*3600 && phase<s[1]*3600);
  if (index<0) return null;
  for (let n=1;n<segments.length;n++) {
    const s=segments[(index+n)%segments.length];
    if(s[2]!==segments[index][2]) return {rpm:s[2],seconds:mod(s[0]*3600-phase,86400)};
  }
  return null;
}
export function pumpModel(states, entities=DEFAULT_ENTITIES, now=Date.now(), connected=true) {
  const state = key => states[entities[key]]?.state;
  const rawPhase=numeric(state('phase'));
  const age=(now-Date.parse(states[entities.phase]?.last_updated))/1000;
  const profile=`${state('family')||''}${state('profile')||''}`;
  const validPhase=rawPhase!==null && rawPhase>=0 && rawPhase<86400;
  const modeKnown=!!state('mode')&&!['unknown','unavailable'].includes(state('mode'));
  const online=connected && state('healthy')==='on' && validPhase && age>=-5 && age<=90 && !!PROFILES[profile] && modeKnown;
  const fault=state('fault')!=='off'; // Unknown fault status must not enable remote operation.
  const phase=validPhase ? mod(rawPhase+(online?Math.max(0,age):0),86400) : null;
  const segments=PROFILES[profile];
  const planned=phase===null?null:segments?.find(s=>phase>=s[0]*3600&&phase<s[1]*3600)?.[2];
  const paused=state('pause')==='on'||state('mode')==='maintenance pause';
  return {online,fault,phase,profile,segments,planned,paused,mode:state('mode'),
    actual:online&&!fault?numeric(state('actual')):null,
    next:phase===null?null:nextChange(segments,phase),
    label:!online?'Disconnected':state('fault')==='on'?'Motor Fault':fault?'Status Unavailable':modeLabels[state('mode')]||state('mode'),
    controls:online&&!fault&&!!modeLabels[state('mode')]&&['on','off'].includes(state('pause')),
    reportedAt:states[entities.phase]?.last_updated,
  };
}
// Command descriptions are pure data. Only a confirmed user interaction sends them.
export function commandPlan(action, value, states, e=DEFAULT_ENTITIES) {
  const call=(domain,service,data)=>({domain,service,data});
  if(action==='pause') return [call('switch','turn_on',{entity_id:e.pause})];
  if(action==='override'||action==='resume') return [call('button','press',{entity_id:e[action]})];
  if(action==='reset') return [call('script',e.timing.slice(7),{operation:'reset'})];
  if(action==='hour' && /^(?:[1-9]|1[0-9]|2[0-4])$/.test(String(value)))
    return [call('select','select_option',{entity_id:e.hour_select,option:String(value)})];
  if(action==='profile' && Object.hasOwn(PROFILES,value)) {
    const calls=[];
    if(states[e.family]?.state!==value[0]) calls.push(call('select','select_option',{entity_id:e.family,option:value[0]}));
    if(states[e.profile]?.state!==value[1]) calls.push(call('select','select_option',{entity_id:e.profile,option:value[1]}));
    return calls;
  }
  throw new Error('Invalid pump action');
}
export function historyPoints(rows=[]) {
  return rows.map(r=>({t:Date.parse(r.last_changed||r.last_updated),value:r.state})).filter(p=>Number.isFinite(p.t)).sort((a,b)=>a.t-b.t);
}
// Merge connectivity with RPM so disconnections are gaps, never false zeroes or run time.
export function observedSeries(speed, health, start, end) {
  const events=[...speed.map(p=>({...p,kind:'rpm'})),...health.map(p=>({...p,kind:'health'}))].sort((a,b)=>a.t-b.t);
  let rpm=null, healthy=false;
  const points=[];
  for(const event of events) {
    if(event.t>end) break;
    if(event.kind==='rpm') rpm=numeric(event.value); else healthy=event.value==='on';
    const point={t:Math.max(start,event.t),value:healthy&&rpm!==null&&rpm>=0?rpm:null};
    if(points.at(-1)?.t===point.t) points[points.length-1]=point; else points.push(point);
  }
  if(!points.length||points[0].t>start) points.unshift({t:start,value:null});
  return points;
}
export function observedDuration(points,start,end) {
  let known=0,running=0;
  for(let i=0;i<points.length;i++) {
    const seconds=Math.max(0,Math.min(end,points[i+1]?.t??end)-Math.max(start,points[i].t))/1000;
    if(points[i].value!==null) {known+=seconds;if(points[i].value>0)running+=seconds;}
  }
  return {known,running,coverage:known/((end-start)/1000)*100};
}
export function stepPath(points,start,end,width=640,height=160) {
  const path=[];
  for(let i=0;i<points.length;i++) {
    const p=points[i],a=Math.max(start,p.t),b=Math.min(end,points[i+1]?.t??end);
    if(p.value===null||b<=a)continue;
    const x=t=>((t-start)/(end-start)*width).toFixed(2);
    const y=(height-2-Math.min(3500,p.value)/3500*(height-4)).toFixed(2);
    path.push(`M${x(a)},${y}H${x(b)}`);
    const next=points[i+1];
    if(next?.value!==null&&next?.t===b&&b<end)path.push(`V${(height-2-Math.min(3500,next.value)/3500*(height-4)).toFixed(2)}`);
  }
  return path.join(' ');
}
const esc = value => String(value??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const duration = seconds => `${Math.floor(seconds/3600)}h ${Math.floor(seconds%3600/60)}m`;
const speed = rpm => rpm===null||rpm===undefined?'—':rpm===0?'Off':`${Math.round(rpm).toLocaleString('en-US')} RPM`;
const icon = name => `<ha-icon icon="mdi:${name}" aria-hidden="true"></ha-icon>`;

const CSS = `
:host{display:block;color:#172521;font-family:var(--paper-font-body1_-_font-family,system-ui,sans-serif);font-size:16px;line-height:1.5;color-scheme:light;--accent:#087f72;--muted:#52655f;--line:#d5deda}
*{box-sizing:border-box} [hidden]{display:none!important} .shell{background:#f2f5f3;min-height:100%;padding:24px 16px 40px}.wrap{max-width:1160px;margin:auto}h1,h2,h3,p{margin:0}h1{font-size:28px;line-height:1.25;letter-spacing:-.6px}h2{font-size:20px;letter-spacing:-.3px}h3{font-size:16px}header{display:flex;align-items:center;justify-content:space-between;gap:12px;margin-bottom:22px}header p{color:var(--muted);font-size:13px;margin-top:7px}.chip{display:inline-flex;border-radius:30px;background:#e9efed;padding:5px 12px;font-size:14px;font-weight:600;color:var(--muted)}.status-live{font-size:12px;white-space:nowrap}.grid{display:grid;grid-template-columns:minmax(0,2fr) minmax(0,1fr);gap:24px;align-items:start}.card{background:#fff;border-radius:22px;padding:24px;box-shadow:0 5px 22px #17252105;min-width:0}.dial-card{text-align:center}.dial{width:180px;height:180px;margin:16px auto 0;position:relative}.dial>svg{position:absolute;inset:0;width:100%;height:100%}.rpm{position:absolute;top:43px;inset-inline:0;font-size:44px;font-weight:750;line-height:1;letter-spacing:-1px;font-variant-numeric:tabular-nums}.rpm-unit{position:absolute;top:89px;inset-inline:0;font-size:14px;color:var(--muted)}.spark{position:absolute;bottom:31px;width:90px;height:22px;left:45px}.dial-min,.dial-max,.spark-label{position:absolute;font-size:12px;color:var(--muted);bottom:3px}.dial-min{left:10px}.dial-max{right:0}.spark-label{bottom:10px;inset-inline:0}.next{margin-top:20px;font-size:14px;color:var(--muted);font-weight:500}.next-value{font-size:19px;font-weight:700;color:#172521;margin-top:4px;font-variant-numeric:tabular-nums}.sub{font-size:14px;color:var(--muted);margin-top:5px}.mode-note{font-size:14px;color:var(--muted);margin-top:8px}.controls{padding:16px}.control{width:100%;display:flex;text-align:left;align-items:center;gap:16px;border:1px solid var(--line);border-radius:16px;padding:14px 16px;min-height:71px;background:#e9efed;color:#172521;cursor:pointer;font:inherit;margin-bottom:8px}.control:last-of-type{margin-bottom:0}.control strong{display:block;font-size:16px}.control small{display:block;font-size:14px;color:var(--muted)}.control .symbol{display:flex;align-items:center;justify-content:center;width:40px;height:40px;border-radius:50%;background:#fff;flex-shrink:0}ha-icon{--mdc-icon-size:22px}.control.override{background:#fff2de;border-color:#ffc674;color:#9a5708}.override small{color:#9a5708}.control.resume{background:#e4f4f0;border-color:#b5dbd4;color:#087f72}.resume small{color:#087f72}button:disabled,select:disabled{cursor:not-allowed;opacity:.58}.notice{border-radius:14px;background:#e9efed;color:var(--muted);padding:13px;font-size:12px;line-height:1.6;margin-top:16px}.schedule{grid-column:1}.schedule h2 span{font-size:14px;font-weight:400;color:var(--muted)}.schedule-track{position:relative;margin-top:24px;height:48px}.segments{display:flex;overflow:hidden;border-radius:14px;height:100%;border:1px solid var(--line)}.segment{border:0;border-right:1px solid #ffffff45;min-width:0;padding:0;cursor:pointer}.marker{position:absolute;top:0;bottom:0;width:3px;background:#a75f09;pointer-events:none;transform:translateX(-1.5px)}.time-labels{position:relative;height:28px;margin-top:5px;font-size:12px;color:var(--muted)}.time-labels span{position:absolute;transform:translateX(-50%);white-space:nowrap}.time-labels span:first-child{transform:none}.time-labels span:last-child{transform:translateX(-100%)}details{margin-top:18px}summary{min-height:44px;cursor:pointer;color:var(--muted);font-size:14px;display:flex;align-items:center;gap:8px}summary:before{content:'▸'}details[open]>summary:before{content:'▾'}table{border-collapse:collapse;width:100%;font-size:14px}td,th{text-align:left;padding:9px 4px;border-bottom:1px solid #e9efed}td:last-child,th:last-child{text-align:right}tr.current{background:#e4f4f0}.settings{grid-column:1;display:grid;grid-template-columns:1fr 1fr;gap:24px}.settings .card{padding:20px}label{display:block;font-size:14px;color:var(--muted);margin:18px 0 6px}select{width:100%;border:1px solid var(--line);border-radius:12px;padding:11px;background:white;color:#172521;font:inherit;min-height:46px}.small{font-size:13px;color:var(--muted);margin-top:12px}.outline{border:1px solid var(--line);background:#fff;border-radius:12px;min-height:44px;padding:10px 14px;font:inherit;font-size:14px;cursor:pointer;color:#172521}.reset{width:100%;margin-top:16px}.analytics{grid-column:1}.analytics-heading{display:flex;justify-content:space-between;gap:12px;align-items:center}.legend{display:flex;gap:16px;font-size:12px;margin-top:16px;color:var(--muted)}.legend span:before{content:'';display:inline-block;width:20px;border-top:2px solid #087f72;vertical-align:middle;margin-right:6px}.legend span:last-child:before{border-top:2px dashed #52655f}.chart{display:grid;grid-template-columns:34px minmax(0,1fr);margin-top:10px;gap:4px}.y-labels{display:flex;flex-direction:column;justify-content:space-between;font-size:11px;color:var(--muted);height:160px}.chart svg{width:100%;height:160px}.x-labels{display:flex;justify-content:space-between;font-size:12px;color:var(--muted);margin-left:38px}.metrics{display:grid;grid-template-columns:1fr 1fr;gap:12px;margin-top:22px}.metric{background:#f2f5f3;border-radius:14px;padding:14px}.metric span{display:block;font-size:12px;color:var(--muted)}.metric strong{display:block;font-size:25px;font-variant-numeric:tabular-nums;margin-top:5px}.quality{font-size:12px;color:var(--muted);margin-top:12px}.runtime{margin-top:16px}.events{list-style:none;padding:0;margin:12px 0 0;font-size:14px}.events li{display:flex;justify-content:space-between;gap:16px;padding:7px 0;border-bottom:1px solid #e9efed}.events time{color:var(--muted);white-space:nowrap}.references{grid-column:1}.charts{display:grid;grid-template-columns:1fr 1fr;gap:16px;margin-top:18px}.charts a{display:block;color:var(--accent);font-size:14px;text-decoration:none}.charts img{display:block;width:100%;height:auto;object-fit:contain;border:1px solid var(--line);border-radius:12px;background:#fff;margin-bottom:8px}.support{grid-column:2;grid-row:2 / span 3}.support details:first-child{margin-top:0}.support li{margin:10px 0;font-size:14px}.support ul{padding-left:18px}.support dl{font-size:14px;display:grid;grid-template-columns:1fr 1fr;gap:10px}.support dt{color:var(--muted)}.support dd{text-align:right;margin:0;overflow-wrap:anywhere}a{color:var(--accent)}.message{font-size:14px;border-radius:12px;padding:12px 14px;margin-bottom:16px;background:#e4f4f0}.message.error{background:#fbecef;color:#af303d}.status-error{background:#fbecef;color:#af303d}.status-override{background:#fff2de;color:#9a5708}dialog{width:min(440px,calc(100% - 32px));border:0;border-radius:20px;padding:24px;color:#172521;background:#fff;box-shadow:0 12px 80px #0003}dialog::backdrop{background:#0b201b66}dialog h2{font-size:21px}dialog p{margin:14px 0 22px;color:var(--muted);font-size:15px}.dialog-actions{display:flex;justify-content:flex-end;gap:10px}.confirm{background:#087f72;color:white;border-color:#087f72}button:focus-visible,select:focus-visible,summary:focus-visible,a:focus-visible{outline:3px solid #087f72;outline-offset:3px}.footer{font-size:12px;color:var(--muted);margin-top:24px}
.dial>svg.spark{inset:auto;bottom:31px;left:45px;width:90px;height:22px}
@media(max-width:900px){.grid{grid-template-columns:minmax(0,1fr)}.dial-card{order:1}.controls{order:2}.schedule{order:3}.settings{order:4}.analytics{order:5}.references{order:6}.support{order:7;grid-column:auto;grid-row:auto}.card{padding:20px}.controls{padding:16px}.shell{padding-top:18px}.settings{gap:16px}.charts{grid-template-columns:1fr}header{align-items:flex-start}.status-live{margin-top:4px}}
@media(max-width:540px){.settings{grid-template-columns:1fr}.grid{gap:20px}.shell{padding-left:16px;padding-right:16px}h1{font-size:27px}.status-live{max-width:130px;white-space:normal;text-align:right}.control{padding:12px 14px}.next-value{font-size:18px}}
`;

const TEMPLATE = `<style>${CSS}</style><div class="shell"><div class="wrap">
<header><div><h1>Pool Pump</h1><p>M5Stack local schedule control</p></div><span id="connection" class="chip status-live">Connecting…</span></header>
<div id="message" class="message" role="status" hidden></div><main class="grid">
<section class="card dial-card" aria-label="Pump status"><span id="mode" class="chip" aria-live="polite">Connecting…</span>
<div class="dial"><svg viewBox="0 0 100 100" role="img" aria-label="Pump speed, zero to 3450 RPM"><path d="M20 80 A45 45 0 1 1 80 80" fill="none" stroke="#e9efed" stroke-width="6" stroke-linecap="round"/><path id="arc" d="M20 80 A45 45 0 1 1 80 80" fill="none" stroke="#087f72" stroke-width="6" stroke-linecap="round" pathLength="100" stroke-dasharray="0 100"/></svg><div id="rpm" class="rpm">—</div><div class="rpm-unit">RPM</div><svg class="spark" id="spark" viewBox="0 0 100 24" role="img" aria-label="Actual RPM history, last hour"><path id="spark-path" fill="none" stroke="#087f72" stroke-width="2"/></svg><span class="dial-min">0</span><span class="spark-label">Last hour</span><span class="dial-max">3,450</span></div>
<div class="next" id="next-label">Next scheduled speed change</div><div class="next-value" id="next-value">—</div><p id="mode-note" class="mode-note" hidden></p><p id="profile-hour" class="sub">Waiting for controller</p></section>
<section class="card controls" aria-label="Main controls">
<button id="pause" class="control" data-action="pause" disabled><span class="symbol">${icon('pause')}</span><span><strong>Pause</strong><small id="pause-caption">Hold motor stopped</small></span></button>
<button id="override" class="control override" data-action="override" disabled><span class="symbol">${icon('lightning-bolt-outline')}</span><span><strong>Override</strong><small id="override-caption">3,450 RPM · up to 24h</small></span></button>
<button id="resume" class="control resume" data-action="resume" disabled><span class="symbol">${icon('play-outline')}</span><span><strong>Resume Schedule</strong><small id="resume-caption">Return to the current schedule hour</small></span></button>
<p class="notice"><strong>Note:</strong> Resuming during an off hour keeps the pump off until its next scheduled run. It is not “start now”.</p></section>
<section class="card schedule"><h2>Today's schedule <span id="schedule-name"></span></h2><div class="schedule-track"><div id="segments" class="segments"></div><div id="marker" class="marker" aria-label="Current schedule position" hidden></div></div><div id="time-labels" class="time-labels"></div><p id="position" class="sub"></p><p id="step" class="sub"></p><details><summary>View speed steps</summary><table><thead><tr><th>Time</th><th>Scheduled speed</th></tr></thead><tbody id="steps"></tbody></table></details></section>
<section class="settings" aria-label="Schedule settings"><div class="card"><h3>${icon('clock-outline')} Schedule timing</h3><label for="hour">Current Schedule Hour</label><select id="hour" data-change="hour" disabled>${Array.from({length:24},(_,i)=>`<option value="${i+1}">Hour ${i+1}</option>`).join('')}</select><p class="small">Selecting an hour keeps the new timing.<br>Active Hour 1 start: <strong id="anchor">—</strong></p><button id="reset" class="outline reset" data-action="reset" disabled>Reset to Default Timing</button><p class="small">Default Hour 1: 11 PM–midnight Pacific. Reset restores the current default phase, not always Hour 1.</p></div><div class="card"><h3>${icon('calendar-clock')} Schedule settings</h3><label for="profile">Family &amp; Profile</label><select id="profile" data-change="profile" disabled>${Object.keys(PROFILES).map(p=>`<option value="${p}">Profile ${p}</option>`).join('')}</select><p class="small">All 16 stock profiles. Changes require confirmation and may change pump speed.</p><p class="small">Pause and Override remain in effect when you change schedule timing.</p></div></section>
<section class="card analytics"><div class="analytics-heading"><h2>RPM history</h2><button class="outline" data-action="refresh" aria-label="Refresh pump history">${icon('refresh')}</button></div><p class="small" id="history-status">Loading controller history…</p><div class="legend"><span>Actual RPM</span><span>Commanded RPM</span></div><div class="chart"><div class="y-labels"><span>3,500</span><span>1,750</span><span>0</span></div><svg viewBox="0 0 640 160" preserveAspectRatio="none" role="img" aria-label="Actual and commanded pump RPM during the last 24 hours"><path d="M0 2H640 M0 80H640 M0 158H640" stroke="#d5deda" stroke-width="1" fill="none"/><path id="target-history" fill="none" stroke="#52655f" stroke-width="1.5" stroke-dasharray="5 4" vector-effect="non-scaling-stroke"/><path id="actual-history" fill="none" stroke="#087f72" stroke-width="2" vector-effect="non-scaling-stroke"/></svg></div><div class="x-labels"><span id="history-start">—</span><span id="history-middle">—</span><span id="history-end">—</span></div><p class="runtime"><strong id="runtime">Observed run time: —</strong></p><p class="small" id="history-coverage">Last 24 hours. Gaps are unavailable data; RPM does not measure water flow.</p><div class="metrics"><div class="metric"><span>Estimated power</span><strong id="power">—</strong><span>RPM-based estimate</span></div><div class="metric"><span>Billing-period estimate</span><strong id="energy">—</strong><span>Not utility metered</span></div></div><p id="energy-quality" class="quality"></p><h3 style="margin-top:24px">Recent controller activity</h3><ul id="events" class="events"><li>Loading…</li></ul></section>
<section class="card references"><h2>All schedule charts</h2><div class="charts"><a href="https://harrybartos.com/orlando/Schedule-A.jpg" target="_blank" rel="noopener"><img src="https://harrybartos.com/orlando/Schedule-A.jpg" alt="Century V-Green EVO Schedule Set A, all eight profiles" loading="lazy">Schedule Set A · Open full size</a><a href="https://harrybartos.com/orlando/Schedule-B.jpg" target="_blank" rel="noopener"><img src="https://harrybartos.com/orlando/Schedule-B.jpg" alt="Century V-Green EVO Schedule Set B, all eight profiles" loading="lazy">Schedule Set B · Open full size</a></div><p class="small">Manual charts use elapsed hours 0–24. Controller Hour 1 starts at elapsed hour 0. Open either image to zoom.</p></section>
<aside class="card support"><details><summary>${icon('gesture-tap-button')} M5 Button Guide</summary><ul><li><strong>1 press:</strong> maximum-speed Override, up to 24 hours.</li><li><strong>2 presses:</strong> Pause while schedule time continues.</li><li><strong>3 presses:</strong> report the selected profile with LED blinks.</li><li><strong>Hold 3 seconds, then release:</strong> exit Pause or Override and resume the schedule. During normal operation, this enters Schedule Set Mode.</li><li>In Schedule Set Mode, press 1–8 times for the profile, then hold to save. Use the EVO manual as the reference.</li></ul><p class="small">Use the M5Stack's front button, not its side reset. Power restoration starts Hour 1; HA restores the active timing after reconnecting. Pause is not electrical isolation.</p></details><details><summary>${icon('heart-pulse')} Diagnostics</summary><dl><dt>Communication</dt><dd id="diag-connection">—</dd><dt>Motor fault</dt><dd id="diag-fault">—</dd><dt>Controller uptime</dt><dd id="diag-uptime">—</dd><dt>Last reset</dt><dd id="diag-reset">—</dd><dt>Last phase report</dt><dd id="diag-report">—</dd><dt>Card version</dt><dd>${VERSION}</dd></dl><p class="small">The M5Stack runs independently of this page and Home Assistant.</p></details></aside>
</main><p class="footer">Times use the Home Assistant time zone. Schedule forecasts do not end Pause or Override.</p>
<dialog id="confirm-dialog" aria-labelledby="confirm-title"><h2 id="confirm-title">Confirm pump action</h2><p id="confirm-text"></p><div class="dialog-actions"><button class="outline" data-dialog="cancel">Cancel</button><button class="outline confirm" data-dialog="confirm">Confirm</button></div></dialog></div></div>`;

export class VGreenPumpCard extends (globalThis.HTMLElement || class {}) {
  setConfig(config) {
    const entities={...DEFAULT_ENTITIES,...config.entities};
    for(const [key,id] of Object.entries(entities)) {
      if(!/^[a-z_]+\.[a-z0-9_]+$/.test(id)||id.split('.')[0]!==DEFAULT_ENTITIES[key]?.split('.')[0])throw new Error(`Invalid entity: ${key}`);
    }
    this.config={...config,entities};
    if(!this.shadowRoot) {
      this.attachShadow({mode:'open'}).innerHTML=TEMPLATE;
      this.shadowRoot.addEventListener('click',event=>{
        const dialog=event.target.closest('[data-dialog]');
        if(dialog){this.$('confirm-dialog').close(dialog.dataset.dialog);return;}
        const button=event.target.closest('[data-action]');
        if(button&&!button.disabled)void this.act(button.dataset.action);
        const segment=event.target.closest('[data-step]');
        if(segment)this.text('step',segment.title);
      });
      this.shadowRoot.addEventListener('change',event=>{
        const action=event.target.dataset.change;
        if(action)void this.act(action,event.target.value);
      });
    }
    this.render();
  }
  $(id){return this.shadowRoot?.getElementById(id);}
  text(id,value){const node=this.$(id);if(node&&node.textContent!==String(value))node.textContent=value;}
  set hass(value){this._hass=value;this.render();if(this.isConnected&&!this._historyStarted)void this.loadHistory();}
  connectedCallback(){if(!this.timer)this.timer=setInterval(()=>{this.render();if(!this.historyAt||Date.now()-this.historyAt>60000)void this.loadHistory();},5000);if(this._hass)void this.loadHistory();}
  disconnectedCallback(){clearInterval(this.timer);this.timer=null;this.$('confirm-dialog')?.close('cancel');}
  getCardSize(){return 18;}
  getGridOptions(){return {columns:'full',rows:'auto'};}
  time(stamp,seconds=false){return new Intl.DateTimeFormat('en-US',{timeZone:this._hass?.config?.time_zone||'America/Los_Angeles',hour:'numeric',minute:'2-digit',...(seconds?{second:'2-digit'}:{})}).format(new Date(stamp));}
  model(){return pumpModel(this._hass?.states||{},this.config?.entities,Date.now(),this._hass?.connected!==false);}
  render(){
    if(!this.config||!this._hass||!this.shadowRoot)return;
    const e=this.config.entities,states=this._hass.states,state=k=>states[e[k]]?.state;
    const now=Date.now(),m=this.model();this.current=m;
    if(m.online)this.lastGood={...m,at:now};
    const schedule=m.online?{...m,at:now}:this.lastGood||{...m,at:Date.parse(m.reportedAt)};
    this.text('mode',m.label);this.$('mode').className=`chip ${m.fault||!m.online?'status-error':m.mode==='24-hour override'?'status-override':''}`;
    this.text('connection',m.online?'● Controller connected':'● Controller unavailable');
    this.text('rpm',m.actual===null?'—':Math.round(m.actual).toLocaleString('en-US'));
    this.$('arc').setAttribute('stroke-dasharray',`${m.actual===null?0:Math.max(0,Math.min(100,m.actual/3450*100))} 100`);
    this.text('next-label',m.online?'Next scheduled speed change':'Next scheduled speed change · last known');
    this.text('next-value',schedule.next&&Number.isFinite(schedule.at)?`${this.time(schedule.at+schedule.next.seconds*1000)} → ${speed(schedule.next.rpm)}`:schedule.segments?.length===1?`No changes · ${speed(schedule.segments[0][2])} continuous`:'—');
    const note=!m.online?'Controller disconnected or data is stale. The pump may still operate locally.':m.fault?'Motor fault or unknown fault status. Remote controls are unavailable.':{
      'maintenance pause':'Paused until resumed or the controller restarts. Schedule time continues.',
      '24-hour override':'Maximum-speed override, up to 24 hours. Resume Schedule ends it.',
      priming:'Three-minute priming cycle, then the current schedule.',
    }[m.mode]||'';
    this.text('mode-note',note);this.$('mode-note').hidden=!note;
    const hour=schedule.phase===null?null:Math.floor(schedule.phase/3600)+1;
    this.text('profile-hour',`${m.online?'Profile':'Last known profile'}: ${schedule.segments?schedule.profile:'—'} · Hour ${hour??'—'} of 24`);
    const enabled=m.controls&&!this.busy;
    const available=k=>!!states[e[k]]&&state(k)!=='unavailable'; // Button state "unknown" means never pressed, not offline.
    this.$('pause').disabled=!enabled||m.paused||!available('pause');
    this.$('override').disabled=!enabled||m.paused||m.mode==='24-hour override'||!available('override');
    this.$('resume').disabled=!enabled||(!m.paused&&m.mode!=='24-hour override')||!available('resume');
    this.$('hour').disabled=!enabled||!available('hour_select');
    this.$('profile').disabled=!enabled||!available('family')||!available('profile');
    this.$('reset').disabled=!enabled||!available('timing');
    this.text('pause-caption',m.paused?'Currently paused':!m.controls?'Remote control unavailable':'Hold motor stopped');
    this.text('override-caption',m.mode==='24-hour override'?'Override active':m.paused?'Resume the schedule first':!m.controls?'Remote control unavailable':'3,450 RPM · up to 24h');
    this.text('resume-caption',!m.controls?'Remote control unavailable':m.paused||m.mode==='24-hour override'?(m.planned?'Return to this schedule hour':'Return to scheduled off period'):'Schedule already active');
    if(this.shadowRoot.activeElement!==this.$('hour'))this.$('hour').value=String(Math.floor((m.phase??0)/3600)+1);
    if(this.shadowRoot.activeElement!==this.$('profile'))this.$('profile').value=m.profile;
    const reference=state('anchor');
    if(/^\d{2}:\d{2}:\d{2}$/.test(reference||'')){const [h,min]=reference.split(':').map(Number);this.text('anchor',`${h%12||12}:${String(min).padStart(2,'0')} ${h>=12?'PM':'AM'}`);}else this.text('anchor','Unavailable');
    this.renderSchedule(schedule,m.online);
    const power=numeric(state('power')),energy=numeric(state('energy')),coverage=numeric(state('energy_coverage'));
    this.text('power',m.online&&power!==null?`${Math.round(power).toLocaleString('en-US')} W`:'—');
    this.text('energy',energy!==null?`${energy.toLocaleString('en-US',{maximumFractionDigits:2})} kWh`:'—');
    this.text('energy-quality',`${state('energy_quality')||'Estimate quality unavailable'}${coverage!==null?` · ${coverage.toFixed(1)}% history coverage`:''}. Estimated from RPM; not SMUD-metered usage.`);
    this.text('diag-connection',m.online?'Connected':'Unavailable / stale');this.text('diag-fault',state('fault')==='off'?'None reported':state('fault')==='on'?'Fault reported':'Unknown');
    const uptime=numeric(state('uptime'));this.text('diag-uptime',m.online&&uptime!==null?duration(uptime):'—');this.text('diag-reset',state('reset_reason')||'—');
    this.text('diag-report',Number.isFinite(Date.parse(m.reportedAt))?this.time(m.reportedAt,true):'—');
  }
  renderSchedule(m,online){
    this.text('schedule-name',m.segments?`(Profile ${m.profile})`:'');
    const valid=m.segments&&m.phase!==null&&Number.isFinite(m.at);
    this.$('marker').hidden=!valid;
    if(!valid){this.$('segments').innerHTML='';this.$('steps').innerHTML='';this.text('position','Schedule timing unavailable');return;}
    const start=m.at-m.phase*1000;
    const key=`${m.profile}:${Math.floor(start/60000)}:${Math.floor(m.phase/3600)}`;
    if(key!==this.scheduleKey){
      this.scheduleKey=key;
      const colors=['#087f72','#348f82','#539e91','#74b0a4','#a6cbc0'];
      this.$('segments').innerHTML=m.segments.map((s,i)=>`<button class="segment" data-step="${i}" style="width:${(s[1]-s[0])/24*100}%;background:${s[2]===0?'#e9efed':colors[Math.min(i,4)]}" title="${esc(`${this.time(start+s[0]*3600000)}–${this.time(start+s[1]*3600000)} · ${speed(s[2])}`)}" aria-label="${esc(`${this.time(start+s[0]*3600000)} to ${this.time(start+s[1]*3600000)}, ${speed(s[2])}`)}"></button>`).join('');
      this.$('time-labels').innerHTML=[0,6,18,24].map(h=>`<span style="left:${h/24*100}%">${esc(this.time(start+h*3600000).replace(':00',''))}</span>`).join('');
      this.$('steps').innerHTML=m.segments.map(s=>`<tr class="${m.phase>=s[0]*3600&&m.phase<s[1]*3600?'current':''}"><td>${esc(this.time(start+s[0]*3600000))}–${esc(this.time(start+s[1]*3600000))}</td><td>${speed(s[2])}</td></tr>`).join('');
    }
    this.$('marker').style.left=`${m.phase/86400*100}%`;
    this.$('marker').setAttribute('aria-label',`${online?'Current':'Last known'} position: Hour ${Math.floor(m.phase/3600)+1}`);
    this.text('position',`${online?'Current-position line':'Last known position'} · Hour ${Math.floor(m.phase/3600)+1} · ${this.time(m.at)}`);
    this.text('step',`Current scheduled step: ${speed(m.planned)}`);
  }
  async loadHistory(){
    if(!this._hass||!this.config||this.loading||!this.isConnected)return;
    this.loading=true;this._historyStarted=true;const end=Date.now(),start=end-86400000,e=this.config.entities;
    const ids=[e.actual,e.target,e.healthy,e.mode];
    try{
      const params=new URLSearchParams({filter_entity_id:ids.join(','),end_time:new Date(end).toISOString(),minimal_response:'',no_attributes:''});
      const result=await this._hass.callApi('GET',`history/period/${encodeURIComponent(new Date(start).toISOString())}?${params}`);
      const groups=new Map(result.map(rows=>[rows[0]?.entity_id,historyPoints(rows)]));
      const health=groups.get(e.healthy)||[];
      const actual=observedSeries(groups.get(e.actual)||[],health,start,end),target=observedSeries(groups.get(e.target)||[],health,start,end);
      this.$('actual-history').setAttribute('d',stepPath(actual,start,end));this.$('target-history').setAttribute('d',stepPath(target,start,end));
      this.$('spark-path').setAttribute('d',stepPath(actual,end-3600000,end,100,24));
      const stats=observedDuration(actual,start,end);
      this.text('history-status',`Last 24 hours · Updated ${this.time(end)} · Controller telemetry`);
      this.text('runtime',`Observed run time: ${duration(stats.running)}`);
      this.text('history-coverage',`${stats.coverage.toFixed(1)}% of this 24-hour window has connected RPM data. Gaps are unavailable data; RPM does not measure water flow.`);
      this.text('history-start',this.time(start));this.text('history-middle',this.time(start+43200000));this.text('history-end',this.time(end));
      const events=(groups.get(e.mode)||[]).filter(p=>p.t>start+1000&&p.t<=end).filter((p,i,all)=>i===0||p.value!==all[i-1].value).slice(-6).reverse();
      this.$('events').innerHTML=events.length?events.map(p=>`<li><span>${esc(modeLabels[p.value]||(['unknown','unavailable'].includes(p.value)?'Controller unavailable':p.value))}</span><time>${esc(this.time(p.t))}</time></li>`).join(''):'<li>No recorded mode changes in this window.</li>';
      this.historyAt=end;
    }catch{this.text('history-status','History unavailable. Existing graph, if shown, is stale. Live controls are separate.');this.historyAt=Date.now();}
    finally{this.loading=false;}
  }
  notify(message,error=false){this.text('message',message);this.$('message').className=`message${error?' error':''}`;this.$('message').hidden=!message;}
  async confirm(message){
    const dialog=this.$('confirm-dialog');this.text('confirm-text',message);dialog.returnValue='cancel';
    return new Promise(resolve=>{dialog.addEventListener('close',()=>resolve(dialog.returnValue==='confirm'),{once:true});dialog.showModal();});
  }
  async act(action,value){
    if(action==='refresh'){await this.loadHistory();return;}
    if(this.busy||!this.model().controls)return;
    const messages={pause:'Pause the pump? It stays paused until you resume it or the controller restarts. Schedule time continues.',override:'Run the pump at 3,450 RPM for up to 24 hours? This overrides the schedule immediately. Confirm the pump is primed and water flow is safe.',resume:'Resume the current schedule? Priming may run as required. During a scheduled off hour, the pump stays off.',reset:'Restore the 11 PM default timing? This may change scheduled speed. Pause, Override and the selected profile remain unchanged.',hour:`Set schedule Hour ${value} and keep this timing? This may change pump speed. Pause and Override remain unchanged.`,profile:`Change to profile ${value}? Pump speed may change. When both family and profile differ, the two existing controller selectors are updated in sequence.`};
    if(!messages[action])return;
    this.busy=true;this.render();
    let sent=0;
    try{
      if(!await this.confirm(messages[action]))return;
      if(!this.model().controls)throw new Error('Controller is no longer available. No further command was sent.');
      const plan=commandPlan(action,value,this._hass.states,this.config.entities);
      for(const c of plan){
        if(!this.model().controls)throw new Error('Controller became unavailable. Check its actual state before retrying.');
        await this._hass.callService(c.domain,c.service,c.data);sent++;
      }
      this.notify(sent?'Request sent. The readings on this page reflect the controller response.':'That profile is already selected.');
    }catch(error){this.notify(`${sent?'Some changes were sent; check the current controller state. ':''}${error?.message||'Request failed. No successful action is assumed.'}`,true);}
    finally{this.busy=false;this.$('hour').blur();this.$('profile').blur();this.render();}
  }
}
if(globalThis.customElements&&!customElements.get('vgreen-pump-card')){
  customElements.define('vgreen-pump-card',VGreenPumpCard);
  window.customCards=window.customCards||[];
  window.customCards.push({type:'vgreen-pump-card',name:'VGreen Pump Control',description:'Local schedules, manual controls and real pump telemetry.'});
}
