// node --test scripts/claude-hooks/jev-skill-route.test.mjs
// Jev is never called here: route() takes the ask function, and the script test feeds broken input.
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { mask, route } from './jev-skill-route.mjs';

const HOOK = path.join(path.dirname(fileURLToPath(import.meta.url)), 'jev-skill-route.mjs');

const answer = (choice, confidence, context7Noul = 0.1) => async () => ({
  ok: true, costUsd: 0.00002,
  answers: {
    stage: { type: 'choice', choice, confidence, probabilities: { [choice]: confidence } },
    context7: { type: 'noul', noul: context7Noul },
  },
});
const tempLog = (t) => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'jev-route-'));
  t.after(() => fs.rmSync(dir, { recursive: true, force: true }));
  return path.join(dir, 'route.log');
};
const input = (prompt) => ({ prompt, session_id: 's1', cwd: 'A:/proj', transcript_path: 'A:/t.jsonl' });

test('masking hides coordinates, paths, links, mail and key-like strings but keeps the words', () => {
  const text = mask('좌표 127.0312, 37.5123 에서 A:\\조사\\2026\\현장.gpkg 가 안 열려요 https://x.kr/a?k=1 a@b.kr sk-ABCDEF0123456789abcdef0123');
  for (const leak of ['127', '37.5', '조사\\2026', 'https://', 'a@b.kr', 'ABCDEF0123456789']) assert.ok(!text.includes(leak), `${leak} leaked: ${text}`);
  assert.match(text, /좌표/);
  assert.match(text, /안 열려요/);
});

// Review 2026-10-03: each of these went out unmasked in the first version.
test('masking also hides spaced, UNC and relative paths, file names, secrets, full-width digits and places', () => {
  const probes = {
    unc: '\\\\nas01\\조사\\화성상리\\현장.gpkg 가 안 열려요',
    spaced: 'A:\\조사 자료\\화성 상리 유적\\현장.gpkg 가 안 열려요',
    home: '~/조사자료/상리 를 못 찾아요',
    relative: 'data\\상리\\a.dxf 오류',
    bare: '화성상리유적.gpkg 가 깨졌어요',
    fileUrl: 'file:///C:/조사/a.pdf 열기',
    dotted: 'key=abc.def.ghi.jkl 넣었는데 안 돼요',
    password: 'password = "hunter2!x" 맞나요',
    short: 'ABCD1234EFGH 키가 안 먹어요',
    fullWidth: '좌표 １２７．０３ 가 틀려요',
    address: '화성시 봉담읍 상리 산12-3번지 지도가 안 떠요',
    site: '상리유적 도면이 비어요',
  };
  const leaks = { unc: 'nas01', spaced: '화성 상리 유적', home: '조사자료', relative: '상리', bare: '화성상리유적', fileUrl: '조사',
    dotted: 'abc.def', password: 'hunter2', short: 'ABCD', fullWidth: '１２７', address: '봉담읍', site: '상리유적' };
  for (const [k, text] of Object.entries(probes)) assert.ok(!mask(text).includes(leaks[k]), `${k}: ${mask(text)}`);
  assert.match(mask(probes.address), /안 떠요/);
});

test('only the end of a long message is sent', () => {
  assert.ok(mask(`${'가'.repeat(500)} 지도가 안 떠요`).length <= 200);
  assert.match(mask(`${'가'.repeat(500)} 지도가 안 떠요`), /안 떠요$/);
});

test('a confident debugging pick becomes one line naming the stage and its skill', async (t) => {
  const line = await route(input('지도가 안 떠요'), { ask: answer('debugging', 0.86), logFile: tempLog(t) });
  assert.match(line, /원인 찾기/);
  assert.match(line, /superpowers:systematic-debugging/);
  assert.match(line, /0\.86/);
  assert.match(line, /\(Jev 추천\)/);
});

test('new features point to the request record and clear edits to the test-first skill', async (t) => {
  const nf = await route(input('버튼 하나 넣어 줘'), { ask: answer('new_feature', 0.9), logFile: tempLog(t) });
  assert.match(nf, /capture-intent/);
  assert.match(nf, /superpowers:brainstorming/);
  assert.match(await route(input('계획 세워 줘'), { ask: answer('planning', 0.85), logFile: tempLog(t) }), /superpowers:writing-plans/);
  assert.match(await route(input('그 오타 고쳐'), { ask: answer('code_change', 0.7), logFile: tempLog(t) }), /superpowers:test-driven-development/);
  assert.match(await route(input('검증하고 커밋해'), { ask: answer('verification', 0.88), logFile: tempLog(t) }), /superpowers:verification-before-completion/);
});

test('context7 noul judgment recommends context7-mcp alongside superpowers or standalone', async (t) => {
  const both = await route(input('QGIS QgsRasterLayer API로 렌더러 만들어 줘'), {
    ask: answer('code_change', 0.84, 0.89),
    logFile: tempLog(t),
  });
  assert.match(both, /superpowers:test-driven-development/);
  assert.match(both, /context7-mcp/);
  assert.match(both, /0\.89/);

  const docOnly = await route(input('Qt6 QQuickWidget 최신 API 시그니처 알려줘'), {
    ask: answer('question', 0.92, 0.81),
    logFile: tempLog(t),
  });
  assert.match(docOnly, /context7-mcp/);
  assert.match(docOnly, /\(Jev 추천\)/);
});

test('Antigravity PreInvocation input uses lastUserInput and deduplicates repeated invocations in the same turn', async (t) => {
  const logFile = tempLog(t);
  let calls = 0;
  const ask = async () => {
    calls++;
    return answer('debugging', 0.91, 0.75)();
  };
  const agyInput = {
    conversationId: 'conv-123',
    lastUserInput: 'QGIS 지도가 안 떠요',
    workspacePaths: ['D:/hgis'],
    transcriptPath: 'D:/t.jsonl',
  };
  const first = await route(agyInput, { ask, logFile });
  assert.match(first, /superpowers:systematic-debugging/);
  assert.match(first, /context7-mcp/);
  const second = await route(agyInput, { ask, logFile });
  assert.equal(second, null);
  assert.equal(calls, 1);
});

test('an unsure pick or a plain question adds nothing', async (t) => {
  assert.equal(await route(input('음'), { ask: answer('debugging', 0.59), logFile: tempLog(t) }), null);
  assert.equal(await route(input('이게 뭐야?'), { ask: answer('question', 0.95), logFile: tempLog(t) }), null);
});

test('Jev receives only the masked message', async (t) => {
  let sent = '';
  const ask = async (request) => {
    sent = JSON.stringify(request);
    return answer('debugging', 0.9)();
  };
  await route(input('좌표 127.0312 가 틀려요'), { ask, logFile: tempLog(t) });
  assert.ok(sent.includes('좌표'));
  assert.ok(!sent.includes('127.0312'));
});

test('slash commands, background notices and empty prompts never reach Jev', async (t) => {
  let calls = 0;
  const ask = async () => {
    calls++;
    return answer('debugging', 0.9)();
  };
  for (const p of ['/clear', '<task-notification>\n<status>completed</status>', '   ']) {
    assert.equal(await route(input(p), { ask, logFile: tempLog(t) }), null);
  }
  assert.equal(calls, 0);
});

test('no key, a network error or a thrown error adds nothing', async (t) => {
  assert.equal(await route(input('안 돼요'), { ask: async () => ({ ok: false, reason: 'no-key' }), logFile: tempLog(t) }), null);
  assert.equal(await route(input('안 돼요'), { ask: async () => { throw new Error('boom'); }, logFile: tempLog(t) }), null);
});

test('the log keeps the pick and the transcript but not the message', async (t) => {
  const logFile = tempLog(t);
  await route(input('비밀 좌표 지도가 안 떠요'), { ask: answer('debugging', 0.86), logFile });
  const entry = JSON.parse(fs.readFileSync(logFile, 'utf8').trim());
  assert.equal(entry.choice, 'debugging');
  assert.equal(entry.shown, true);
  assert.equal(entry.transcript, 'A:/t.jsonl');
  assert.ok(!JSON.stringify(entry).includes('비밀'));
});

test('the hint states the estimate without quoting a user decision', async (t) => {
  const line = await route(input('지도가 안 떠요'), { ask: answer('debugging', 0.86), logFile: tempLog(t) });
  assert.doesNotMatch(line, /사용자 2026/);
});

test('the log is started over once it passes its size limit', async (t) => {
  const logFile = tempLog(t);
  fs.writeFileSync(logFile, 'x'.repeat(1024 * 1024 + 10));
  await route(input('지도가 안 떠요'), { ask: answer('debugging', 0.86), logFile });
  assert.ok(fs.statSync(logFile).size < 10000);
  assert.ok(fs.existsSync(`${logFile}.1`));
});

// A pending DNS lookup kept the process alive after the 2 s abort (review 2026-10-03).
test('the script exits within 3 s even if the helper never answers', (t) => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'jev-route-hang-'));
  t.after(() => fs.rmSync(dir, { recursive: true, force: true }));
  fs.copyFileSync(HOOK, path.join(dir, 'jev-skill-route.mjs'));
  fs.writeFileSync(path.join(dir, 'jev-ask.mjs'), 'export const ask = () => new Promise(() => setTimeout(() => {}, 20000));\n');
  const started = Date.now();
  const r = spawnSync(process.execPath, [path.join(dir, 'jev-skill-route.mjs')], { input: JSON.stringify(input('지도가 안 떠요')), encoding: 'utf8' });
  assert.equal(r.status, 0, r.stderr);
  assert.equal(r.stdout.trim(), '');
  assert.ok(Date.now() - started < 3000, `took ${Date.now() - started} ms`);
});

test('the script never blocks a message on broken input', () => {
  const r = spawnSync(process.execPath, [HOOK], { input: 'not json', encoding: 'utf8' });
  assert.equal(r.status, 0, r.stderr);
  assert.equal(r.stdout.trim(), '');
});
