// Chatou settings UI — front-end logic + native bridge
//
// Native bridge functions (exposed by C++ via webview.bind):
//   nativeReadSchema()                      → JSON {api_key, model, host, path}
//   nativeWriteSchema(api_key, model, host, path)  → bool
//   nativeReadLang()                        → string
//   nativeWriteLang(text)                   → bool
//   nativeOpenUrl(url)                      → void
//   nativeClose()                           → void
//
// All bind() calls return Promises.

const PROVIDER_PRESETS = {
  deepseek: { model: "deepseek-chat",    host: "api.deepseek.com",  path: "/v1/chat/completions" },
  moonshot: { model: "moonshot-v1-8k",   host: "api.moonshot.cn",   path: "/v1/chat/completions" },
  openai:   { model: "gpt-4o",           host: "api.openai.com",    path: "/v1/chat/completions" },
  ollama:   { model: "qwen2.5:7b",       host: "localhost:11434",   path: "/v1/chat/completions" },
  custom:   { model: "",                 host: "",                  path: "" },
};

// Read URL hash / query for which page to show: ?page=lang | ?page=model
function activePage() {
  const url = new URL(window.location.href);
  return url.searchParams.get("page") || "lang";
}

function showPage(name) {
  document.querySelectorAll(".page").forEach(el => el.hidden = true);
  const el = document.getElementById("page-" + name);
  if (el) el.hidden = false;
  // Tab visual state.
  document.querySelectorAll(".page-tab").forEach(b => {
    b.classList.toggle("active", b.dataset.page === name);
  });
}

// Initialize model page lazily — only when user clicks the tab the first time,
// to avoid two nativeRead* calls at startup.
let _modelInited = false;
async function ensureModelInit() {
  if (_modelInited) return;
  _modelInited = true;
  try { await initModelPage(); } catch (e) { /* surface via toast inside */ }
}

// Map a raw classify-error tail (the text after the "error:" prefix returned
// by nativeClassifyLang) into a human-readable, user-actionable Chinese
// string. Ordered matching: most specific first. Falls back to a generic
// "保存失败:" with the original message.
function humanizeClassifyError(raw) {
  const s = String(raw || "").trim();
  const low = s.toLowerCase();
  // API key missing — checked before generic auth so it wins on overlap.
  if (low.indexOf("api key") >= 0 || low.indexOf("api_key") >= 0 ||
      s.indexOf("未配置") >= 0) {
    return "请先在『模型配置』填 API key";
  }
  if (low.indexOf("401") >= 0 || low.indexOf("403") >= 0) {
    return "API key 无效或没有权限，请检查";
  }
  if (low.indexOf("429") >= 0) {
    return "调用过于频繁，请稍后再试";
  }
  if (low.indexOf("500") >= 0 || low.indexOf("502") >= 0 ||
      low.indexOf("503") >= 0 || low.indexOf("5xx") >= 0) {
    return "AI 服务端故障，请稍后再试";
  }
  if (s.indexOf("无法解析") >= 0 || low.indexOf("parse") >= 0) {
    return "模型回复格式不兼容，建议换 DeepSeek / OpenAI";
  }
  if (s.indexOf("网络") >= 0 || low.indexOf("network") >= 0) {
    return "网络异常，请检查代理 / VPN 后重试";
  }
  return "保存失败：" + s;
}

function toast(msg, kind = "ok", opts = {}) {
  const el = document.getElementById("toast");
  // Reset content. `opts.action` adds a clickable link to the right of the
  // message; useful for "view log" affordances on error toasts.
  el.textContent = "";
  const msgSpan = document.createElement("span");
  msgSpan.textContent = msg;
  el.appendChild(msgSpan);
  if (opts.action && opts.actionLabel) {
    const btn = document.createElement("a");
    btn.href = "#";
    btn.textContent = opts.actionLabel;
    btn.className = "toast-action";
    btn.style.marginLeft = "12px";
    btn.style.textDecoration = "underline";
    btn.style.cursor = "pointer";
    btn.addEventListener("click", (e) => {
      e.preventDefault();
      try { opts.action(); } catch (_) {}
    });
    el.appendChild(btn);
  }
  el.className = "toast " + kind;
  el.hidden = false;
  requestAnimationFrame(() => el.classList.add("show"));
  // Errors with an action stay visible long enough for the user to click it.
  const ms = opts.action ? 6000 : 1800;
  clearTimeout(toast._t);
  toast._t = setTimeout(() => {
    el.classList.remove("show");
    setTimeout(() => { el.hidden = true; }, 250);
  }, ms);
}

// ─── Language page ────────────────────────────────────────────────
async function initLangPage() {
  const input = document.getElementById("langInput");
  const saveBtn = document.getElementById("langSave");
  const recentSection = document.getElementById("recentSection");
  const recentChips = document.getElementById("recentChips");
  const recentEmpty = document.getElementById("recentEmpty");
  const clearRecent = document.getElementById("clearRecent");
  const HISTORY_LIMIT = 8;
  let styleHistory = [];

  function normalizeHistory(raw) {
    if (!Array.isArray(raw)) return [];
    const seen = new Set();
    const clean = [];
    for (const item of raw) {
      const label = String(item && item.label || "").trim();
      const category = String(item && item.category || "").toUpperCase();
      if (!label || !"ABCD".includes(category) || seen.has(label.toLowerCase())) continue;
      seen.add(label.toLowerCase());
      clean.push({ label, category, source: item.source === "preset" ? "preset" : "custom" });
      if (clean.length >= HISTORY_LIMIT) break;
    }
    return clean;
  }

  async function persistHistory() {
    const ok = await window.nativeWriteStyleHistory(JSON.stringify(styleHistory));
    if (!ok) throw new Error("无法写入最近使用记录");
  }

  async function writeLang(value) {
    const ok = await window.nativeWriteLang(value);
    if (!ok) throw new Error("无法写入风格设置");
  }

  function selectStyle(label, category, chip) {
    input.value = label;
    input.dataset.category = category;
    input.focus();
    input.setSelectionRange(label.length, label.length);
    document.querySelectorAll(".chip.active").forEach(x => x.classList.remove("active"));
    if (chip) chip.classList.add("active");
  }

  function renderHistory() {
    recentChips.textContent = "";
    recentSection.hidden = false;
    recentEmpty.hidden = styleHistory.length > 0;
    recentChips.hidden = styleHistory.length === 0;
    for (const item of styleHistory) {
      const chip = document.createElement("button");
      chip.type = "button";
      chip.className = "chip recent-chip";
      chip.textContent = item.label;
      chip.dataset.category = item.category;
      chip.addEventListener("click", () => selectStyle(item.label, item.category, chip));
      recentChips.appendChild(chip);
    }
  }

  async function rememberStyle(label, category, source) {
    const key = label.toLowerCase();
    styleHistory = styleHistory.filter(x => x.label.toLowerCase() !== key);
    styleHistory.unshift({ label, category, source });
    styleHistory = styleHistory.slice(0, HISTORY_LIMIT);
    renderHistory();
    await persistHistory();
  }

  try {
    const raw = await window.nativeReadStyleHistory();
    styleHistory = normalizeHistory(JSON.parse(raw || "[]"));
  } catch (_) {
    styleHistory = [];
  }
  renderHistory();

  clearRecent.addEventListener("click", async () => {
    styleHistory = [];
    renderHistory();
    try {
      await persistHistory();
      toast("已清空最近使用", "ok");
    } catch (e) {
      toast("清空失败：" + (e && e.message || e), "error");
    }
  });

  // Pre-fill only if user has a non-default custom value. The install
  // script seeds "en" (legacy 2-letter code that maps to English), and
  // "English" is also the implicit default, so both should leave the
  // field blank so the placeholder "English" shows in muted tone.
  // Clicking puts the cursor at the leftmost position — nothing to delete.
  const DEFAULT_ALIASES = new Set(["en", "english", "english "]);
  let currentLangValue = "";
  let currentCategory = "";
  try {
    const cur = await window.nativeReadLang();
    currentLangValue = (cur || "").trim();
    // Strip the "X:" category prefix written by save() (X = A/B/C/D) so the
    // input shows only the human label, but remember the category so an
    // unmodified save() can re-emit it without another classify call.
    const m = /^([ABCD]):(.*)$/.exec(currentLangValue);
    if (m) {
      currentCategory = m[1];
      currentLangValue = m[2].trim();
    }
    if (currentLangValue && !DEFAULT_ALIASES.has(currentLangValue.toLowerCase())) {
      // Show "无" instead of "off" — friendlier UI label.
      input.value = currentLangValue.toLowerCase() === "off"
                      ? "无"          // 无
                      : currentLangValue;
      if (currentCategory) input.dataset.category = currentCategory;
    }
  } catch (e) {}

  // 纯净模式 toggle: lives in the titlebar (top right). When ON, write
  // "off" to the lang file and grey out the rest of the panel so it's
  // visually obvious that translation is disabled. When OFF, restore the
  // previously typed value (or "English") and unlock the UI.
  const pureToggle = document.getElementById("pureToggle");
  const mainEl = document.querySelector("main");
  const runtimeStatus = document.getElementById("runtimeStatus");
  const runtimeStatusText = document.getElementById("runtimeStatusText");
  let lastLang = "";  // remembers value before pure mode flip-on
  function applyPureLock(on) {
    if (mainEl) {
      mainEl.classList.toggle("pure-locked", !!on);
    }
    // Disable form controls in BOTH pages (lang chips/save + model
    // form/preset/save) so keyboard tabbing can't reach them either.
    document.querySelectorAll(
      "#page-lang input, #page-lang button.chip, #page-lang .btn.primary, " +
      "#page-model input, #page-model button"
    ).forEach(el => { el.disabled = !!on; });
    if (runtimeStatus) runtimeStatus.classList.toggle("paused", !!on);
    if (runtimeStatusText) {
      runtimeStatusText.textContent = on ? "翻译已暂停" : "翻译已就绪";
    }
  }
  if (pureToggle) {
    // Initial state: reflect current lang file.
    try {
      const cur = (typeof currentLangValue === "string" ? currentLangValue : "");
      if (cur.toLowerCase() === "off") {
        pureToggle.checked = true;
        applyPureLock(true);
      }
    } catch (e) {}
    pureToggle.addEventListener("change", async () => {
      if (pureToggle.checked) {
        const cur = input.value.trim();
        if (cur && cur !== "无") lastLang = cur;
        input.value = "无";  // 无 — friendly display
        applyPureLock(true);
        try { await writeLang("off"); } catch (e) {}
      } else {
        const restore = lastLang || input.placeholder || "English";
        input.value = restore;
        applyPureLock(false);
        try { await writeLang(restore); } catch (e) {}
      }
    });
  }

  // Each chip carries (or inherits from its .chips group) data-category =
  // A|B|C|D. When the user clicks a chip we stash that letter on the input
  // element so save() can skip the classify LLM call entirely — which lets
  // a freshly-installed user pick a chip preset and save it BEFORE they've
  // entered their API key. Free-form typed input still has to go through
  // classify (we have no way to know the category up front).
  function chipCategory(chip) {
    return chip.dataset.category ||
           (chip.closest(".chips") || {}).dataset?.category || "";
  }
  function knownCategory(label) {
    const wanted = String(label || "").trim().toLowerCase();
    let category = "";
    document.querySelectorAll(".style-grid .chip, #recentChips .chip").forEach(c => {
      const value = (c.dataset.value || c.textContent || "").trim().toLowerCase();
      if (!category && value === wanted) category = chipCategory(c);
    });
    return category;
  }
  function clearChipActive() {
    document.querySelectorAll(".chip.active").forEach(x => x.classList.remove("active"));
    delete input.dataset.category;
  }
  document.querySelectorAll(".chip").forEach(c => {
    if (c.classList.contains("recent-chip")) return;
    c.addEventListener("click", () => {
      clearChipActive();
      const cat = chipCategory(c);
      selectStyle(c.dataset.value || c.textContent, cat, c);
    });
  });
  // Any manual edit invalidates the chip "selected" state — including the
  // pre-classified category — so the next save() falls back to classify.
  input.addEventListener("input", clearChipActive);
  input.addEventListener("focus", () => {
    const v = input.value.trim();
    let match = null;
    document.querySelectorAll(".chip").forEach(c => {
      const cv = (c.dataset.value || c.textContent || "").trim();
      if (cv === v) match = c;
    });
    if (!match) clearChipActive();
  });

  // Save: write lang.txt + close. Empty input falls back to the placeholder
  // default ("English"), so user can just press Save without typing anything.
  // Display "无" for the "off" sentinel; keep the file's stored value
  // ASCII so the plugin's compare stays codepage-safe.
  const PURE_DISPLAY = "无";          // 无
  function toStoredLang(v) {
    return (v && v.trim() === PURE_DISPLAY) ? "off" : v;
  }
  function fromStoredLang(v) {
    return (v && v.trim().toLowerCase() === "off") ? PURE_DISPLAY : v;
  }

  async function save() {
    let v = input.value.trim();
    if (!v) v = input.placeholder || "English";
    v = toStoredLang(v);
    saveBtn.disabled = true;
    try {
      // Pure mode ("off") skips classification — write the bare sentinel so
      // the plugin's codepage-safe compare still matches.
      if (v.toLowerCase() === "off") {
        await writeLang("off");
        toast("已保存", "ok");
        return;
      }
      // Chip path — input.dataset.category is set when the current value
      // came from a preset chip click (cleared on any manual edit). Use
      // that letter directly and skip the LLM call so users without an
      // API key can still save preset styles.
      // Exact matches typed by hand (for example "English") inherit the
      // category already declared by the built-in/history chip. They should
      // not make an unnecessary classifier request.
      const presetCat = input.dataset.category || knownCategory(v);
      if (presetCat && "ABCD".includes(presetCat)) {
        await writeLang(presetCat + ":" + v);
        const isPreset = Array.from(document.querySelectorAll(".style-grid .chip"))
          .some(c => (c.dataset.value || c.textContent || "").trim() === v);
        await rememberStyle(v, presetCat, isPreset ? "preset" : "custom");
        toast("已保存", "ok");
        return;
      }
      // Free-form input — classify via one LLM call, store as "X:name" so
      // the plugin picks the right category prompt when F8 is pressed. On failure
      // we keep the old lang.txt; we don't silently default to a guess.
      const orig = saveBtn.textContent;
      saveBtn.textContent = "分类中…";
      const r = await window.nativeClassifyLang(v);
      saveBtn.textContent = orig;
      if (!r || r.startsWith("error:")) {
        const msg = r ? r.slice(6) : "未知错误";
        toast(humanizeClassifyError(msg), "error");
        // No API key / auth failure → jump to 模型配置 so user can fix it.
        const low = msg.toLowerCase();
        if (low.indexOf("api key") >= 0 || low.indexOf("api_key") >= 0 ||
            msg.indexOf("未配置") >= 0 ||
            low.indexOf("401") >= 0 || low.indexOf("403") >= 0) {
          setTimeout(() => showPage("model"), 600);
        }
        return;  // keep old lang.txt, don't write a bad value
      }
      const cat = r.trim().toUpperCase();
      if (!"ABCD".includes(cat) || cat.length !== 1) {
        toast("保存失败：分类返回非法值 " + cat, "error");
        return;
      }
      await writeLang(cat + ":" + v);
      await rememberStyle(v, cat, "custom");
      toast("已保存", "ok");
      // Stay open — user may want to make more changes. They close with X.
    } catch (e) {
      toast("保存失败：" + (e && e.message || e), "error");
    } finally {
      saveBtn.disabled = false;
    }
  }
  saveBtn.addEventListener("click", save);
  input.addEventListener("keydown", e => {
    if (e.key === "Enter") save();
  });
}

// ─── Model page ───────────────────────────────────────────────────
async function initModelPage() {
  const f = {
    apiKey: document.getElementById("apiKey"),
    model:  document.getElementById("model"),
    host:   document.getElementById("host"),
    path:   document.getElementById("path"),
  };

  // Default placeholder hints — stored so we can restore after toggling 自定义.
  const defaultPlaceholders = {
    apiKey: f.apiKey.placeholder,
    model:  f.model.placeholder,
    host:   f.host.placeholder,
    path:   f.path.placeholder,
  };

  // Per-provider keyring (loaded from typeanything.keyring.json). Switching
  // preset stashes the current API Key under the active provider, then loads
  // the new provider's stored key (or empty if none).
  let keyring = {};
  let activeProvider = "deepseek";  // becomes the preset whose values match schema.yaml
  try {
    const krRaw = await window.nativeReadKeyring();
    keyring = JSON.parse(krRaw || "{}") || {};
  } catch (e) { keyring = {}; }

  // Detect current provider from the loaded schema host. The exact match
  // tells us which preset to highlight + which keyring slot to seed.
  function detectProviderFromHost(host) {
    if (!host) return "deepseek";
    for (const [name, preset] of Object.entries(PROVIDER_PRESETS)) {
      if (name === "custom") continue;
      if (preset.host && preset.host === host) return name;
    }
    return "custom";
  }

  // Load current values from schema.yaml.
  try {
    const cur = await window.nativeReadSchema();
    if (cur) {
      f.apiKey.value = cur.api_key || "";
      f.model.value  = cur.model   || "deepseek-chat";
      f.host.value   = cur.host    || "api.deepseek.com";
      f.path.value   = cur.path    || "/v1/chat/completions";
      activeProvider = detectProviderFromHost(cur.host);
      // Seed the keyring slot for the detected provider with the schema key
      // (so first-time users with only schema-based config see it persist).
      if (cur.api_key) keyring[activeProvider] = cur.api_key;
    }
  } catch (e) {}

  // Two distinct visual states for the preset row:
  //   .active  — what the user is *currently editing* (changes when they
  //              click a preset; not yet persisted).
  //   .current — what the runtime is actually using (the schema.yaml
  //              snapshot; only changes after a successful save).
  function setActivePreset(name) {
    document.querySelectorAll(".preset").forEach(x => {
      x.classList.toggle("active", x.dataset.preset === name);
    });
  }
  function setCurrentPreset(name) {
    document.querySelectorAll(".preset").forEach(x => {
      x.classList.toggle("current", x.dataset.preset === name);
    });
  }
  setActivePreset(activeProvider);
  setCurrentPreset(activeProvider);   // detected from schema.yaml at load
  let runtimeProvider = activeProvider;

  // Toggle placeholders. 自定义 preset hides the semi-transparent hints
  // (user types their own values from scratch); other presets restore them.
  function applyPlaceholders(presetName) {
    const blank = presetName === "custom";
    f.apiKey.placeholder = blank ? "" : defaultPlaceholders.apiKey;
    f.model.placeholder  = blank ? "" : defaultPlaceholders.model;
    f.host.placeholder   = blank ? "" : defaultPlaceholders.host;
    f.path.placeholder   = blank ? "" : defaultPlaceholders.path;
  }
  applyPlaceholders(activeProvider);

  // Live endpoint preview — show full URL the request will hit.
  const ep = document.getElementById("endpointPreview");
  function refreshPreview() {
    if (!ep) return;
    const h = (f.host.value || "").trim();
    const p = (f.path.value || "").trim();
    if (!h && !p) { ep.classList.add("empty"); ep.textContent = ""; return; }
    const scheme = /^localhost(:|$)|^127\.|^\d+\.\d+\.\d+\.\d+/.test(h) ? "http" : "https";
    const pp = p.startsWith("/") ? p : ("/" + p);
    ep.classList.remove("empty");
    ep.textContent = "POST  " + scheme + "://" + h + pp;
  }
  refreshPreview();
  f.host.addEventListener("input", refreshPreview);
  f.path.addEventListener("input", refreshPreview);

  // Preset buttons
  document.querySelectorAll(".preset").forEach(btn => {
    btn.addEventListener("click", () => {
      const newProvider = btn.dataset.preset;
      const p = PROVIDER_PRESETS[newProvider];
      if (!p) return;
      // Save current key under the previously active provider before switching.
      keyring[activeProvider] = f.apiKey.value;
      activeProvider = newProvider;
      // Load the new provider's stored key (or blank if never configured).
      f.apiKey.value = keyring[newProvider] || "";
      f.model.value  = p.model;
      f.host.value   = p.host;
      f.path.value   = p.path;
      applyPlaceholders(newProvider);
      refreshPreview();
      f.apiKey.focus();
      setActivePreset(newProvider);
    });
  });

  // Show / hide API key
  document.getElementById("toggleKey").addEventListener("click", () => {
    f.apiKey.type = f.apiKey.type === "password" ? "text" : "password";
  });

  // Cancel
  document.getElementById("btnCancelModel").addEventListener("click", () => {
    window.nativeClose();
  });

  // Save. API Key allowed to be empty — clearing it explicitly de-activates
  // that provider (runtime will get 401 / endpoint reject on next F8,
  // which is the user's intent if they just want to stop using that key).
  document.getElementById("btnSaveModel").addEventListener("click", async () => {
    const saveBtn = document.getElementById("btnSaveModel");
    const ak = f.apiKey.value;  // no .trim() — user may want literal empty
    saveBtn.disabled = true;
    try {
      // Persist per-provider keyring.
      keyring[activeProvider] = ak;
      try { await window.nativeWriteKeyring(JSON.stringify(keyring)); } catch (e) {}

      await window.nativeWriteSchema(ak.trim(), f.model.value.trim(),
                                     f.host.value.trim(), f.path.value.trim());
      // Mark this preset as the runtime-active one and stay open so the
      // user can keep editing other providers.
      runtimeProvider = activeProvider;
      setCurrentPreset(runtimeProvider);
      toast("已保存", "ok");
    } catch (e) {
      toast("保存失败：" + (e && e.message || e), "error");
    } finally {
      saveBtn.disabled = false;
    }
  });
}

// ─── Bootstrap ────────────────────────────────────────────────────
window.addEventListener("DOMContentLoaded", async () => {
  // Custom window controls (the OS title bar is stripped in MakeFrameless).
  const wcMin = document.getElementById("wcMin");
  const wcClose = document.getElementById("wcClose");
  if (wcMin)   wcMin.addEventListener("click",   () => window.nativeMinimize?.());
  if (wcClose) wcClose.addEventListener("click", () => window.nativeClose?.());

  // Window drag — WebView2's child window swallows WM_NCHITTEST, so we
  // explicitly hand off to the OS drag loop on mousedown in the titlebar
  // (excluding the buttons + the pure-mode toggle + any interactive ctl).
  const titlebar = document.querySelector(".titlebar");
  if (titlebar) {
    titlebar.addEventListener("mousedown", e => {
      if (e.button !== 0) return;
      if (e.target.closest(".window-controls, .pure-toggle, button, input, label, a")) return;
      window.nativeStartDrag?.();
    });
  }

  // Always init both pages so user can swap freely. Lang init is cheap;
  // model init makes one nativeReadSchema call.
  await initLangPage();
  await initModelPage();
  _modelInited = true;

  // Tab nav.
  document.querySelectorAll(".page-tab").forEach(b => {
    b.addEventListener("click", () => showPage(b.dataset.page));
  });

  const page = activePage();
  showPage(page);

  document.addEventListener("keydown", e => {
    if (e.key === "Escape") window.nativeClose();
  });
});
