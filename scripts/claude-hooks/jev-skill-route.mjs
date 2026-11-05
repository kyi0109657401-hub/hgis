#!/usr/bin/env node
// UserPromptSubmit hook, every project on this PC: Jev (TypeSafe, development only) picks the working
// stage for the message, and a confident pick is passed to Claude as one line naming the skill.
// Only the masked end of the message is sent (code, links, mail, secrets, paths, file names, places,
// key-like strings and numbers hidden). No key, no network, a slow answer or any error adds nothing,
// the process ends within 2.4 s, and the exit code is always 0.
// Each pick is logged next to this script without the message text, with the transcript path so the
// skills Claude really called can be counted later.
// User decision 2026-10-03 「skills선택에 jev활용」「전역으로 설정진행」, docs/intent/2026-10-03-jev-stage-hint.md.
import crypto from 'node:crypto';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

const THRESHOLD = 0.6;
const TIMEOUT_MS = 2000;
const WATCHDOG_MS = 2400;
const SENT_CHARS = 200;
const LOG_LIMIT = 1024 * 1024;
const STAGES = {
  debugging: { name: '원인 찾기', skill: 'superpowers:systematic-debugging' },
  new_feature: { name: '요청 기록·설계', skill: 'capture-intent, superpowers:brainstorming' },
  planning: { name: '구현 계획', skill: 'superpowers:writing-plans' },
  code_change: { name: '시험 먼저', skill: 'superpowers:test-driven-development' },
  verification: { name: '검증·코드 리뷰', skill: 'superpowers:verification-before-completion, superpowers:requesting-code-review' },
};
const CRITERIA = {
  debugging: "A bug report or symptom: something that used to work does not, an error, a crash, a screenshot of a problem, or 'it went back to how it was'. Start by finding the root cause.",
  new_feature: 'A request for a new feature, UI/UX design, mockup, or to change how something works or looks, or a change to development settings. Start by capturing intent and brainstorming.',
  planning: 'A request to write an implementation plan, specification, step-by-step architecture, or break down a multi-step task before coding.',
  code_change: 'A small, already-clear code edit, or the go-ahead to build something already agreed. Start by writing a failing test (TDD).',
  verification: 'A request to verify, test, review code, check before completion, or commit/finish a branch.',
  question: 'A question, an explanation request, a link to read, or a status check. Answer directly; no workflow skill is needed.',
};
const CONTEXT7_INSTRUCTIONS =
  'Does fulfilling this developer request benefit from looking up external library, framework, SDK, or API documentation (such as Qt6, QGIS, GDAL, CMake, Node.js, React, Web APIs, or third-party packages) via Context7 MCP before answering or writing code?';

export function mask(text) {
  const s = String(text)
    .replace(/```[\s\S]*?(?:```|$)/g, '[코드]')
    .replace(/\b(?:https?|file|ftp):\/\/\S+/gi, '[링크]')
    .replace(/[\w.+-]+@[\w-]+(?:\.[\w-]+)+/g, '[메일]')
    .replace(/\b(?:api[_-]?key|key|token|password|passwd|pwd|secret)\s*[=:]\s*(?:"[^"]*"|'[^']*'|\S+)/gi, '[가림]')
    // a drive, UNC, home or relative path runs to the last separator on its line, spaces included
    .replace(/(?:[A-Za-z]:[\\/]|\\\\|~[\\/]|\.{1,2}[\\/])(?:[^\n]*[\\/])?[^\s\\/]*/g, '[경로]')
    .replace(/\S*[\\/]\S*/g, '[경로]')
    .replace(/[^\s"'`]+\.(?:gpkg|shp|shx|dbf|prj|dxf|dwg|qgz|qgs|tiff?|csv|xlsx?|pdf|las|laz|kml|geojson|json|zip|png|jpe?g)\b/gi, '[파일]')
    // addresses (two or more administrative units in a row) and site names
    .replace(/[가-힣]+(?:특별자치시|특별자치도|특별시|광역시|도|시|군|구)(?:\s+[가-힣]+(?:읍|면|동|가|리)(?=[\s,.)]|$))+/g, '[지명]')
    .replace(/[가-힣\p{Nd}-]*(?:유적지?|고분군?|산성|패총|번지)/gu, '[지명]')
    .replace(/[A-Za-z0-9_+=.-]{12,}/g, (m) => (/\d/.test(m) || m.length >= 24 ? '[가림]' : m))
    .replace(/\p{Nd}[\p{Nd}.,:．，°'′″-]*/gu, '[수]')
    .trim();
  return s.length > SENT_CHARS ? s.slice(-SENT_CHARS) : s;
}

function log(logFile, entry) {
  try {
    if (fs.existsSync(logFile) && fs.statSync(logFile).size > LOG_LIMIT) fs.renameSync(logFile, `${logFile}.1`);
    fs.appendFileSync(logFile, `${JSON.stringify({ t: new Date().toISOString(), ...entry })}\n`);
  } catch {}
}

function lastLogEntry(logFile) {
  try {
    if (!fs.existsSync(logFile)) return null;
    const lines = fs.readFileSync(logFile, 'utf8').trim().split('\n');
    return lines.length && lines[lines.length - 1] ? JSON.parse(lines[lines.length - 1]) : null;
  } catch {
    return null;
  }
}

// Returns the line to add for Claude / Antigravity (development only), or null.
export async function route(input, { ask, logFile }) {
  const rawPrompt = typeof input?.prompt === 'string' ? input.prompt
    : typeof input?.lastUserInput === 'string' ? input.lastUserInput : '';
  const prompt = rawPrompt.trim();
  if (!prompt || prompt.startsWith('/') || prompt.startsWith('<task-notification>')) return null;
  const masked = mask(prompt);
  const session = input?.session_id ?? input?.conversationId ?? null;
  const promptHash = crypto.createHash('sha1').update(masked).digest('hex').slice(0, 12);
  if (input?.lastUserInput !== undefined && session) {
    const prev = lastLogEntry(logFile);
    if (prev && prev.session === session && prev.promptHash === promptHash) return null;
  }
  const started = Date.now();
  let result;
  try {
    result = await ask({
      state: `A software developer's chat message to a coding assistant, end only, with numbers, paths, places and keys masked: ${JSON.stringify(masked)}`,
      questions: {
        stage: { type: 'choice', instructions: 'Which working stage should the coding assistant start with for this message?', criteria: CRITERIA },
        context7: { type: 'noul', instructions: CONTEXT7_INSTRUCTIONS },
      },
    });
  } catch {
    result = { ok: false, reason: 'error' };
  }
  const pick = result?.ok ? result.answers?.stage : null;
  const stage = pick && STAGES[pick.choice];
  const confidence = Number(pick?.confidence ?? 0);
  const c7Noul = Number(result?.ok ? (result.answers?.context7?.noul ?? 0) : 0);
  const stageShown = Boolean(stage) && confidence >= THRESHOLD;
  const c7Shown = c7Noul >= THRESHOLD;
  const shown = stageShown || c7Shown;
  log(logFile, {
    session, promptHash, cwd: input.cwd ?? input.workspacePaths?.[0],
    transcript: input.transcript_path ?? input.transcriptPath,
    choice: pick?.choice ?? null, confidence, context7: c7Noul, shown, ms: Date.now() - started,
    ...(result?.ok ? { costUsd: result.costUsd } : { failed: result?.reason }),
  });
  if (!shown) return null;
  const parts = [];
  if (stageShown) {
    parts.push(`이 메시지는 「${stage.name}」 단계로 보인다 — ${stage.skill}, 확률 ${confidence.toFixed(2)}`);
  }
  if (c7Shown) {
    parts.push(`외부 라이브러리·API 공식 문서 확인 권장 — context7-mcp(resolve-library-id → query-docs), 확률 ${c7Noul.toFixed(2)}`);
  }
  return `Jev 단계 추정(개발 전용 단계·Context7 판단 장치): ${parts.join(' · ')}. ` +
    '이 추정을 따를 때는 진행 안내의 단계 이름 뒤에 「(Jev 추천)」을 붙이는 것이 이 PC의 설정이다. ' +
    '메시지와 맞지 않으면 따르지 않아도 된다.';
}

async function main() {
  // A pending DNS lookup can outlive the request's abort; never hold the message longer than this.
  setTimeout(() => process.exit(0), WATCHDOG_MS).unref();
  const input = JSON.parse(fs.readFileSync(0, 'utf8'));
  const isAntigravity = input && input.lastUserInput !== undefined && input.prompt === undefined;
  const here = path.dirname(fileURLToPath(import.meta.url));
  const helper = [path.join(here, 'jev-ask.mjs'), path.join(here, '..', 'jev', 'jev-ask.mjs')].find((p) => fs.existsSync(p));
  if (!helper) {
    if (isAntigravity) process.stdout.write('{}');
    return;
  }
  const { ask } = await import(pathToFileURL(helper).href);
  const line = await route(input, {
    ask: (request) => ask(request, { retries: 0, timeoutMs: TIMEOUT_MS }),
    logFile: path.join(here, 'jev-skill-route.log'),
  });
  if (isAntigravity) {
    const payload = line ? { injectSteps: [{ ephemeralMessage: line }] } : {};
    process.stdout.write(JSON.stringify(payload));
  } else if (line) {
    process.stdout.write(JSON.stringify({ hookSpecificOutput: { hookEventName: 'UserPromptSubmit', additionalContext: line } }));
  }
  process.exitCode = 0;
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) {
  try {
    await main();
  } catch {
    // Never block or delay a message because this hint failed.
    process.exitCode = 0;
  }
}
