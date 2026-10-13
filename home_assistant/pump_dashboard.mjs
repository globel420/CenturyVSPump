#!/usr/bin/env node
// Prepare a reversible overview-only replacement from a fresh HA API export.
// Does not contact Home Assistant, change entities or send pump actions.
import {readFile,writeFile} from 'node:fs/promises';
import {pathToFileURL} from 'node:url';

export function installPumpOverview(config) {
  const result=structuredClone(config);
  const matches=result.views?.filter(v=>v.path==='overview');
  if(matches?.length!==1)throw new Error('Expected exactly one overview view');
  const view=matches[0];
  for(const key of ['sections','max_columns','dense_section_placement','badges','header','footer'])delete view[key];
  view.type='panel';
  view.cards=[{type:'custom:vgreen-pump-card'}];
  return result;
}
if(process.argv[1]&&import.meta.url===pathToFileURL(process.argv[1]).href){
  const [input,output]=process.argv.slice(2);
  if(!input||!output)throw new Error('Usage: pump_dashboard.mjs BEFORE.json CANDIDATE.json');
  const before=JSON.parse(await readFile(input,'utf8'));
  await writeFile(output,JSON.stringify(installPumpOverview(before),null,2)+'\n',{flag:'wx',mode:0o600});
  console.log(JSON.stringify({saved:output,changedView:'overview',preservedViews:before.views.filter(v=>v.path!=='overview').map(v=>v.path)}));
}
