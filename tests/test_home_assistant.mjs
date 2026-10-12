import test from "node:test";
import assert from "node:assert/strict";
import { mkdtemp, readFile, writeFile, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";

const spec = JSON.parse(await readFile(new URL("../home_assistant/controls.json", import.meta.url)));
const migrator = fileURLToPath(new URL("../home_assistant/dashboard_update.mjs", import.meta.url));
const restart = "button.backyard_vgreen_pool_pump_restart_schedule_at_hour_one";

test("native timing controls cannot change the selected profile or release motor modes", () => {
  assert.equal(spec.reference_helper.initial_time, "23:00:00");
  assert.equal(spec.script.mode, "queued");
  assert.equal(spec.select.options, "{{ range(1, 25) | map('string') | list }}");
  assert.match(spec.select.state, /sensor\.backyard_vgreen_pool_pump_schedule_hour/);
  assert.equal(spec.automation.mode, "restart");
  const actions = [];
  function visit(value) {
    if (!value || typeof value !== "object") return;
    if (typeof value.action === "string") actions.push(value.action);
    Object.values(value).forEach(visit);
  }
  visit(spec.script);
  assert.deepEqual([...new Set(actions)].sort(), [
    "esphome.vgreen_pump_set_schedule_phase", "input_datetime.set_datetime",
  ]);
  assert.doesNotMatch(JSON.stringify(spec), /has_service\(/);
});

for (const page of ["pool-pump", "home-operations"]) {
  test(`${page}: migration preserves unrelated settings and is idempotent`, async () => {
    const dir = await mkdtemp(join(tmpdir(), "vgreen-ha-test-"));
    try {
      const keep = { type: "tile", entity: "sensor.smud_current_bill_start_date", name: "Billing" };
      const source = { title: "Preserved dashboard", views: [{
        path: page === "pool-pump" ? "overview" : "operations",
        sections: [{ type: "grid", cards: [
          { type: "heading", heading: page === "pool-pump" ? "Pump Controls" : "Pool & SMUD" },
          { type: "tile", entity: "input_boolean.smud_plan" },
          { type: "markdown", content: "**Arming control** obsolete timing" },
          { type: "markdown", title: "Expected current B1 phase", content: "old B1-only view" },
          { type: "markdown", content: "Pause Pump stays active indefinitely." },
          { type: "entities", title: "Home Assistant checks", entities: [
            "input_text.smud_plan_status",
            { entity: "input_datetime.smud_plan_last_aligned" },
            { entity: "automation.smud_plan_start_b1_2245" },
            { entity: "automation.smud_plan_peak_guard_1655" },
            { entity: "binary_sensor.pump_connection", name: "Keep this row" },
          ] },
          { type: "button", entity: restart, name: "Keep this label", hold_action: { action: "none" },
            tap_action: { action: "perform-action", perform_action: "button.press" } },
          keep,
        ] }],
      }], untouched: { custom: [1, "preserve", false] } };
      const input = join(dir, "before.json"), output = join(dir, "after.json"), second = join(dir, "again.json");
      await writeFile(input, JSON.stringify(source));
      function run(from, to) {
        const result = spawnSync(process.execPath, [migrator, page, from, to], { encoding: "utf8" });
        assert.equal(result.status, 0, result.stderr);
      }
      run(input, output);
      const actual = JSON.parse(await readFile(output));
      const cards = actual.views[0].sections[0].cards;
      assert.deepEqual(actual.untouched, source.untouched);
      assert.deepEqual(cards.find(c => c.entity === keep.entity), keep);
      assert.equal(cards.filter(c => c.entity === "select.pool_pump_schedule_hour").length, 1);
      const button = cards.find(c => c.entity === restart);
      assert.equal(button.name, "Keep this label");
      assert.deepEqual(button.hold_action, { action: "none" });
      assert.deepEqual(button.tap_action.data, { operation: "set_hour", hour: 1 });
      assert.equal(button.tap_action.perform_action, "script.pool_pump_timing");
      assert.equal(cards.find(c => c.title === "Current schedule").type, "markdown");
      assert.doesNotMatch(JSON.stringify(actual), /smud_plan|Arming control|Expected current B1 phase/);
      run(output, second);
      assert.deepEqual(JSON.parse(await readFile(second)), actual);
      const collision = spawnSync(process.execPath, [migrator, page, input, output], { encoding: "utf8" });
      assert.notEqual(collision.status, 0, "An existing backup/output must not be overwritten");
    } finally {
      // Remove only this test's concrete mkdtemp directory, never a user path.
      await rm(dir, { recursive: true });
    }
  });
}
