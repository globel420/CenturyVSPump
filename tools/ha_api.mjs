#!/usr/bin/env node
// Small HA REST/WebSocket client. Credentials stay in the supplied private file.
import { readFile, writeFile } from "node:fs/promises";
import { pathToFileURL } from "node:url";

export async function credentialFromFile(path) {
  const input = await readFile(path, "utf8");
  const row = input.split(/\r?\n/).find(line => line.startsWith("HOME_ASSISTANT_MCP_TOKEN="));
  let value = row?.slice(row.indexOf("=") + 1).trim();
  if (value?.startsWith('"') && value.endsWith('"')) value = value.slice(1, -1);
  if (value?.startsWith("'") && value.endsWith("'")) value = value.slice(1, -1);
  if (!value || /[\r\n]/.test(value)) throw new Error("Missing or invalid HA credential");
  return value;
}

export class HAClient {
  constructor(baseUrl, token) {
    const url = new URL(baseUrl);
    if (!["http:", "https:"].includes(url.protocol) || url.username || url.password || url.search || url.hash)
      throw new Error("Use an explicit HTTP(S) Home Assistant origin without embedded credentials");
    this.origin = url.origin;
    this.token = token;
    this.sequence = 0;
    this.pending = new Map();
  }

  async http(method, path, body) {
    if (!path.startsWith("/api/") || path.startsWith("//")) throw new Error("Only HA API paths are allowed");
    const response = await fetch(this.origin + path, {
      method, redirect: "error", signal: AbortSignal.timeout(30000),
      headers: { Authorization: `Bearer ${this.token}`, "Content-Type": "application/json" },
      body: body === undefined ? undefined : JSON.stringify(body),
    });
    if (!response.ok) throw new Error(`HA ${method} ${path.split("?")[0]}: HTTP ${response.status}`);
    const text = await response.text();
    if (!text) return null;
    try { return JSON.parse(text); } catch { return text; }
  }

  async connect() {
    if (this.socket) return;
    const socket = new WebSocket(this.origin.replace(/^http/, "ws") + "/api/websocket");
    this.socket = socket;
    await new Promise((resolve, reject) => {
      const timer = setTimeout(() => { socket.close(); reject(new Error("HA WebSocket authentication timed out")); }, 15000);
      socket.addEventListener("message", event => {
        const message = JSON.parse(event.data);
        if (message.type === "auth_required") socket.send(JSON.stringify({ type: "auth", access_token: this.token }));
        else if (message.type === "auth_ok") { clearTimeout(timer); resolve(); }
        else if (message.type === "auth_invalid") { clearTimeout(timer); reject(new Error("HA rejected the credential")); }
        else if (message.type === "result") {
          const pending = this.pending.get(message.id);
          if (!pending) return;
          this.pending.delete(message.id);
          clearTimeout(pending.timer);
          if (message.success) pending.resolve(message.result);
          else pending.reject(new Error(`HA ${pending.type}: ${message.error?.code || "request failed"} ${message.error?.message || ""}`));
        }
      });
      socket.addEventListener("error", () => { clearTimeout(timer); reject(new Error("HA WebSocket connection failed")); });
      socket.addEventListener("close", () => {
        clearTimeout(timer);
        reject(new Error("HA WebSocket closed before authentication completed"));
        for (const pending of this.pending.values()) {
          clearTimeout(pending.timer); pending.reject(new Error("HA WebSocket closed"));
        }
        this.pending.clear();
      });
    });
  }

  async ws(message) {
    await this.connect();
    const id = ++this.sequence;
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => { this.pending.delete(id); reject(new Error(`HA ${message.type}: response timed out`)); }, 30000);
      this.pending.set(id, { resolve, reject, timer, type: message.type });
      this.socket.send(JSON.stringify({ ...message, id }));
    });
  }
  close() { this.socket?.close(); }
}

async function main() {
  const args = process.argv.slice(2);
  const value = key => { const i = args.indexOf(key); return i < 0 ? undefined : args[i + 1]; };
  const client = new HAClient(value("--url"), await credentialFromFile(value("--credential-file")));
  const body = value("--json-file") ? JSON.parse(await readFile(value("--json-file"), "utf8")) :
    value("--json") ? JSON.parse(value("--json")) : undefined;
  const method = value("--method") || "GET";
  const websocket = args.includes("--ws");
  const readOnlyWs = /^(auth\/current_user|config\/entity_registry\/list|config\/device_registry\/list|lovelace\/dashboards\/list|lovelace\/config|lovelace\/resources|input_datetime\/list|input_boolean\/list|get_services)$/;
  const readOnlyHttp = method === "GET" || (method === "POST" && value("--path") === "/api/template");
  if ((websocket ? !readOnlyWs.test(body?.type || "") : !readOnlyHttp) && !args.includes("--apply"))
    throw new Error("Mutation requires --apply");
  try {
    const result = websocket ? await client.ws(body) : await client.http(method, value("--path"), body);
    const output = JSON.stringify(result, null, 2) + "\n";
    if (value("--out")) {
      await writeFile(value("--out"), output, { mode: 0o600, flag: "wx" });
      console.log(JSON.stringify({ saved: value("--out") }));
    } else console.log(output);
  } finally { client.close(); }
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href)
  main().catch(error => { console.error(error.message); process.exitCode = 1; });
