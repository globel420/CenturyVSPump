#!/usr/bin/env node
// Prepare reviewed Lovelace payloads from a fresh export. Never contacts HA.
import { readFile, writeFile } from "node:fs/promises";

const [page, input, output] = process.argv.slice(2);
if (!["pool-pump", "home-operations"].includes(page) || !input || !output)
  throw new Error("Usage: dashboard_update.mjs pool-pump|home-operations INPUT.json OUTPUT.json");
const config = JSON.parse(await readFile(input, "utf8"));
const spec = JSON.parse(await readFile(new URL("controls.json", import.meta.url), "utf8"));
const retired = new Set([
  "input_boolean.smud_plan", "input_text.smud_plan_status", "input_datetime.smud_plan_last_aligned",
  "automation.smud_plan_start_b1_2245", "automation.smud_plan_peak_guard_1655",
]);
const restart = "button.backyard_vgreen_pool_pump_restart_schedule_at_hour_one";
const pauseNotice = "Pause holds until you resume or the M5 restarts. Resuming primes as required and returns to the current schedule hour. Power restoration starts Hour 1 automatically; Home Assistant restores the active timing after reconnecting.";
const phaseNotice = "**Selected schedule:** {{ states('select.backyard_vgreen_pool_pump_stock_schedule_set') }}{{ states('select.backyard_vgreen_pool_pump_stock_schedule_profile') }}\n\n**Current schedule hour:** {{ states('sensor.backyard_vgreen_pool_pump_schedule_hour') }} / 24\n\n**Controller mode:** {{ states('sensor.backyard_vgreen_pool_pump_controller_mode') }}\n\nThe M5 runs its selected schedule locally. Home Assistant only aligns its timing; it does not force B1.";
let removed = 0;
function transform(value) {
  if (Array.isArray(value)) return value.filter(item => {
    const drop = typeof item === "string" ? retired.has(item) : retired.has(item?.entity) ||
      (item?.type === "markdown" && item.content?.startsWith("**Arming control**"));
    if (drop) ++removed;
    return !drop;
  }).map(transform);
  if (!value || typeof value !== "object") return value;
  const result = Object.fromEntries(Object.entries(value).map(([key, child]) => [key, transform(child)]));
  if (result.type === "heading" && result.heading === "Pool & SMUD") result.heading = "Pool Pump";
  if (result.type === "markdown" && result.content?.startsWith("Pause Pump stays active indefinitely."))
    result.content = pauseNotice;
  if (result.type === "markdown" && result.title === "Expected current B1 phase") {
    result.title = "Current schedule";
    result.content = phaseNotice;
  }
  if (result.type === "markdown" && result.content?.startsWith("Pump actions stay separate from monitoring."))
    result.content = "Pump actions stay separate from monitoring. Time alignment preserves Pause and Override. Power restoration starts the selected schedule automatically. Confirm the physical situation before any manual action.";
  if (result.type === "entities" && result.title === "Home Assistant checks" &&
      !result.entities.some(e => e.entity === "automation.pool_pump_synchronize_schedule_timing"))
    result.entities.push({ entity: "automation.pool_pump_synchronize_schedule_timing", name: "Schedule timing sync" });
  if (result.type === "button" && result.entity === restart) {
    result.tap_action = {
      action: "perform-action", perform_action: "script.pool_pump_timing",
      data: { operation: "set_hour", hour: 1 },
      confirmation: { text: "Set Hour 1 and keep this timing? Pause and Override remain in effect." },
    };
  }
  return result;
}
const updated = transform(config);
const view = updated.views.find(v => v.path === (page === "pool-pump" ? "overview" : "operations"));
const section = view?.sections?.find(s => s.cards?.some(c => c.type === "heading" &&
  c.heading === (page === "pool-pump" ? "Pump Controls" : "Pool Pump")));
if (!section) throw new Error("Expected pump section was not found; no output written");
if (!section.cards.some(c => c.entity === "select.pool_pump_schedule_hour")) {
  section.cards.push({ type: "heading", heading: "Schedule Timing", icon: "mdi:clock-outline" },
    ...structuredClone(spec.dashboard_cards));
}
const serialized = JSON.stringify(updated, null, 2) + "\n";
for (const entity of retired) if (serialized.includes(entity))
  throw new Error(`Unresolved legacy reference: ${entity}`);
await writeFile(output, serialized, { flag: "wx", mode: 0o600 });
console.log(JSON.stringify({ page, removedLegacyEntries: removed, output }));
