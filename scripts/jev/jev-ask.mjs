// Jev(TypeSafe System One) 판정을 개발 도구에서 부른다. 앱에는 쓰지 않는다(docs/intent/2026-10-02-jev-dev-only.md).
// 사용: node scripts/jev/jev-ask.mjs <요청.json>
//   요청 = {"state": <판정할 글·코드 조각>, "questions": {"<id>": {"type": "noul"|"choice"|"score", ...}}}
//   출력 = {"ok": true, "model", "answers", "usage", "costUsd"} 또는 {"ok": false, "reason", "message"}
//   끝 코드: 0 성공, 2 키 없음, 3 그 밖의 실패.
// 키는 사용자 환경 변수 TYPESAFE_API_KEY 에서만 읽고 출력·로그에 쓰지 않는다. 조사 자료·좌표·도면 원본은 보내지 않는다.
import { readFileSync } from 'node:fs';
import { execFileSync } from 'node:child_process';
import https from 'node:https';
import { pathToFileURL } from 'node:url';

export const MODEL = 'jev-1.13.0';  // 판정 기준을 이 판으로 맞춘다(별칭 jev-latest 는 예고 없이 바뀐다)
const ENDPOINT = 'https://api.typesafe.ai/v1/systemone';
const USD_PER_INPUT_TOKEN = 0.042e-6;

// 키를 넣은 뒤 Claude 앱을 다시 켜기 전에는 이 프로세스 환경에 없다: Windows 는 사용자 환경 변수를 한 번 더 읽는다.
function readWindowsUserEnv(name) {
  if (process.platform !== 'win32') return '';
  try {
    const out = execFileSync('reg', ['query', 'HKCU\\Environment', '/v', name],
                             { encoding: 'utf8', stdio: ['ignore', 'pipe', 'ignore'], timeout: 500 });
    const m = out.match(new RegExp(`${name}\\s+REG_(?:EXPAND_)?SZ\\s+(.*)`));
    return m ? m[1] : '';
  } catch {
    return '';
  }
}

export function loadKey(env = process.env, readUserEnv = readWindowsUserEnv) {
  return (env.TYPESAFE_API_KEY || readUserEnv('TYPESAFE_API_KEY') || '').trim();
}

const sleep = (ms) => new Promise((done) => setTimeout(done, ms));

// Node v24 on Windows crashes in libuv (UV_HANDLE_CLOSING) or hangs 4 s in undici keep-alive pool when
// global fetch() + AbortSignal.timeout() is used in short-lived CLI hooks. node:https with agent:false exits cleanly in <0.3 s.
function httpsFetch(url, { method = 'POST', headers = {}, body = '', timeoutMs = 15000 } = {}) {
  return new Promise((resolve, reject) => {
    const req = https.request(url, {
      method,
      headers: { ...headers, connection: 'close', 'content-length': Buffer.byteLength(body) },
      agent: false,
      timeout: timeoutMs,
    }, (res) => {
      const chunks = [];
      res.on('data', (c) => chunks.push(c));
      res.on('end', () => {
        const text = Buffer.concat(chunks).toString('utf8');
        const status = res.statusCode ?? 0;
        resolve({ ok: status >= 200 && status < 300, status, json: async () => JSON.parse(text) });
      });
    });
    req.on('timeout', () => req.destroy(new Error('timeout')));
    req.on('error', reject);
    req.end(body);
  });
}

export async function ask(request, { key = loadKey(), fetchImpl = httpsFetch, retries = 2, backoffMs = 500,
                                     timeoutMs = 15000 } = {}) {
  if (!key) return { ok: false, reason: 'no-key', message: 'TYPESAFE_API_KEY 가 설정되지 않았다' };
  const body = JSON.stringify({ model: MODEL, state: request.state ?? null, questions: request.questions });
  for (let attempt = 0; ; ++attempt) {
    let res;
    try {
      res = await fetchImpl(ENDPOINT, {
        method: 'POST',
        headers: { authorization: 'Bearer ' + key, 'content-type': 'application/json' },
        body,
        timeoutMs,
        signal: fetchImpl === httpsFetch ? undefined : AbortSignal.timeout(timeoutMs),
      });
    } catch (error) {
      if (attempt < retries) {
        await sleep(backoffMs * 2 ** attempt);
        continue;
      }
      return { ok: false, reason: 'network', message: String(error?.message ?? error) };
    }
    if (res.ok) {
      const json = await res.json();
      return { ok: true, model: json.model, answers: json.answers, usage: json.usage,
               costUsd: (json.usage?.input_tokens ?? 0) * USD_PER_INPUT_TOKEN };
    }
    if ((res.status === 408 || res.status === 429 || res.status >= 500) && attempt < retries) {
      await sleep(backoffMs * 2 ** attempt);
      continue;
    }
    let message = '';
    try {
      message = (await res.json())?.detail?.message ?? '';
    } catch {}
    return { ok: false, reason: `http-${res.status}`, message };
  }
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) {
  const request = JSON.parse(readFileSync(process.argv[2] ?? 0, 'utf8'));
  const result = await ask(request);
  console.log(JSON.stringify(result, null, 1));
  process.exitCode = result.ok ? 0 : result.reason === 'no-key' ? 2 : 3;
}
