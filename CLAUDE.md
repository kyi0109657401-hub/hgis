# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

# Strata (ka-hgis) 작업 지침

사용자는 개발자가 아닌 고고학 현장 전문가다. 요청은 짧은 한국어, 증상("안 돼요"), 스크린샷으로 온다. 전역 `~/.claude/CLAUDE.md`를 따른다. 가끔 필요한 절차·경위·경로: docs/agent/strata-reference.md.

## 일하는 방식 (스킬 지침보다 우선)

1. 스킬은 해당 단계에서 적극 사용한다(2026-10-03). 단계마다 Skill 도구로 실제로 부르고, 앞에서나 요약 전에 부른 것으로 대신하지 않는다: 증상·버그·스크린샷·「예전으로 돌아갔다」 → superpowers:systematic-debugging, 코드 수정 → superpowers:test-driven-development, 화면이 바뀌는 수정 → run-strata, 끝났다고 말하기 전 → superpowers:verification-before-completion. brainstorming은 현장 쓰임새를 물어야 하는 새 기능에만. 과제마다 작업자·검토자를 따로 돌리지 않는다(2026-10-02 「빠르게 진행」, subagent-driven-development 금지): 이 대화에서 차례로 만들고(executing-plans), 기능 하나가 끝나면 requesting-code-review로 한 번 검토하고, 진짜 지적만 이번 변경 안에서 고치고 나머지는 보고에 한 줄.
2. "해줘/고쳐줘"가 진행 승인이다. 기술 결정(설계, 접근법, 계획, 실행 방식, 브랜치 마무리)은 묻지 말고 가장 안전한 쪽으로 끝까지 하고 보고에 한 줄. 묻는 것은 앱이 현장에서 어떻게 보이고 동작할지가 정말 불분명할 때만, 한 번에 묶어서.
3. 새 기능·동작 변경: 전역 `capture-intent` 기록 → 쓰임새가 갈리는 질문(4개 이하, 추천 답)과 함께 보이고 「진행」에서 한 번 멈춤 → 기록만 커밋 → 경로(spike/bounded/architectural)와 설계를 한국어 몇 문장으로 쓰고 같은 턴에 계속. 「추천대로」면 남은 질문 모두 추천 답. 사실(코드 동작, 파일 위치, 좌표계)은 직접 찾는다. 버그·작은 변경은 묻지 않는다.
4. 스펙·계획(docs/superpowers, 한국어, 첫머리에 기록 경로)은 여러 단계짜리 큰 기능에서만. brainstorming의 HARD-GATE·한 메시지 한 질문·절마다 승인·스펙 검토 대기·시각 도우미, 계획 검토·실행 방식 질문은 하지 않는다.
5. 원인 찾기의 "도움 요청" 단계 = 한 일을 보고하고 가장 안전한 쪽으로 계속. 고치기 세 번 실패하면 말하고 다음 요청까지 코드를 바꾸지 않는다.
6. 지금 단계를 밝히고, 안내와 보고는 단계 이름까지 한국어로('시험 먼저', '끝나기 전 확인'). 영어는 사용자가 입력하거나 누를 경로·명령에만.
7. 원래 있던 시험의 기댓값을 바꾸거나(검사문 안의 이름만 바꿔도) 지우거나 건너뛰게(QSKIP·QEXPECT_FAIL) 했으면 커밋 메시지에 `시험 변경: <이유>` 줄을 쓰고 보고 맨 위에 「바꾼 시험」으로 알린다(2026-10-03). 커밋 전 검사가 이 줄 없이는 막는다.

## 코드 고치는 방식

- 주변 코드의 이름·주석·모양에 맞춘다. 바뀐 줄은 모두 요청에서 바로 이어져야 하고, 상관없는 옆 코드·죽은 코드는 고치지 말고 보고에 한 줄(내 변경으로 안 쓰이게 된 것만 지운다).
- 요청하지 않은 설정·확장성·추상화, 일어날 수 없는 경우의 처리는 넣지 않는다.
- Architecture: `src/core` → static lib `ka_core` (services, no window code); `src/app` → `ka-hgis` exe (MainWindow split over `MainWindow*.cpp`, Ka* widgets and map tools). Test exes link `ka_core` and compile the `src/app` files they need by listing them in their own `add_executable` in CMakeLists.txt, so a new app file used by a test goes into that list too. Critical path diagram: docs/architecture/data-flow.md (its "error blocks" edge predates 2026-10-10); code map: docs/agent/strata-reference.md.
- Hotspots, keep edits narrow: KaDrawingStudio.cpp, MainWindow.cpp, LayerOps.cpp, BasemapOps.cpp. CMake source lists are explicit (no GLOB). data/ is copied next to the exe: a data/ edit needs a ka-hgis rebuild.
- Before writing or changing a Qt, QGIS, GDAL, PROJ or CMake API call, check that version's docs with context7 (resolve-library-id → query-docs) (2026-10-11 사용자 지시). Without the tool, say so; do not guess.
- C++ definitions, references, errors: LSP (clangd) before grep. No build/compile_commands.json → `cmake -S . -B build` (no build/ at all: the first-build command), then build.
- 보안 검토 결과는 이 앱(Windows 데스크톱, 서버·웹 없음)에 해당하는 것만 이번 변경 안에서 고치고 나머지는 보고에 한 줄.

## Product rules (do not break)

- Standalone C++20/Qt 6 Widgets app on OSGeo4W qgis-dev (`qgis_core`, `qgis_gui`). Never fork QGIS or reimplement PROJ, GDAL, rendering or CRS transforms. Keep the GPLv2+ About notices.
- Domain layers `survey_area`, `feature_poly`, `feature_line`, `section_line`, `control_points`, `artifact_point`, `trial_trench` (schema `data/schemas/ka_hgis_layers.yaml`): identity is the `ka_hgis/layer_key` property (Korean titles are labels). Only `LayerOps::ensureDomainLayer` adds one; never auto-add empty ones or add one because a GPKG table exists. `loadSurveyLayers` never calls `removeAllMapLayers()`.
- Legend groups 조사 데이터 / 참조 지도. Basemaps, WMS/XYZ, soil, geology, aligned rasters are reference maps, never survey data. Cadastral layers (5 km download, VWorld picture) stay at the tree root, outside 참조 지도; the VWorld picture cannot be snapped to.
- CRS: work CRS EPSG:5187 (default) or 5186; never assume 5179. Submission = EPSG:5179 SHP + PDF + MANIFEST.sha256 via `ExportService`, files only, not added to the map. Checklist errors do not block (2026-10-10, docs/intent/2026-10-10-submit-despite-checklist.md): 「제출 꾸러미 만들기」 asks once and README_submit.txt records them; without a composed user_sheet the package has no 조사도면.pdf; no SHP and no sheet = no package. DXF is not a submit path.
- Keys: never hardcode a VWorld or personal key. `loadApiKey` order: `%LOCALAPPDATA%\ka-hgis\ka-hgis-vworld.ini` → `HKCU\Software\ka-hgis` → env `VWORLD_API_KEY` → gitignored `config/secrets.ini`. Satellite: keyed api.vworld.kr WMTS first, xdworld only without a key. Cadastral WMS: `crs=EPSG:3857` + KEY, never send DOMAIN, CRS candidates 4326→3857→900913 only. An expired key returns HTTP 200 + XML error: blank tiles, nothing logged.
- Crash rules (each caused 0xc0000005): keep `qgis/parallel_rendering=false`. Never stopRendering/clearCache or re-refresh while WMS/XYZ tiles load (skip when `isDrawing()`); no blocking GetMap or `canvas->refresh()` during a draw, use `LayerOps::refreshCanvasIfIdle`. Layout: no QGIS rubber band, no `setLayers({})`, no `refresh()`; call `m_toolSelect->setLayout`.
- Decided, do not reopen: startup shows the home screen only (no auto-restore); no autosave (save on 저장/Ctrl+S or the close prompt, unsaved shows " *"); New/Save-As start in `preferredSurveyDir()`, never the Desktop; never modify the user's original survey data; name Strata, one-row ribbon, Malgun Gothic (bundled IBM Plex only with `KA_HGIS_BUNDLED_FONTS=1`).
- QGIS edit buffer: unsaved features have negative ids; a multipolygon in a single-POLYGON GPKG table silently becomes a zero-area ring (store pieces as separate features); undo stacks are per layer.
- Regressions: for a familiar symptom read docs/ERROR_REGRESSION.md first; after fixing a new bug add a row there and a regression test. For map, CRS, layer-order or layout bugs collect project/layer CRS, canvas layer order, scale and provider URI before editing.
- Jev is for Claude's development ONLY, never in the app (2026-10-02): `node scripts/jev/jev-ask.mjs <request.json>` judges review findings and ranks debugging hypotheses; report 「Jev 판정 n건 · 진짜로 본 것 m건 · 비용」. Send only code, test output and review text, never survey data, coordinates, drawings or keys. Build and tests decide; report disagreements with the Claude reviewers. Never echo, store or commit TYPESAFE_API_KEY; without it use the Claude reviewers and say so.

## Build, test, run (Bash tool, worktree root)

- First build (no build/CMakeCache.txt, ~8 min): `powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/build-now.ps1`
- Later builds: `powershell.exe -NoProfile -ExecutionPolicy Bypass -Command '. ./scripts/dev-env.ps1; $ErrorActionPreference="Continue"; cmake --build build --config Release --parallel --target ka-hgis <test-target>; exit $LASTEXITCODE'` (on C1060, MSB4018, MSB4166, 0x800705AF add `-- /m:1 /p:CL_MPCount=2`).
- Tests, only the groups you touched (docs/testing-map.md; logs in build/test-logs/): `powershell.exe -NoProfile -ExecutionPolicy Bypass -Command '. ./scripts/dev-env.ps1; $ErrorActionPreference="Continue"; ctest --test-dir build -C Release -j4 --output-on-failure -R "^(name1|name2)$"; exit $LASTEXITCODE'`. Never plain `ctest`; full suite only when asked. perf_engine, heritage_download_retry, topographic_browser, survey_contour, startup_splash flake under load: rerun alone first.
- No known failures since 2026-10-03 (the former four pass locally and in CI): every failing test is real.
- Tests go into existing QtTest files (`ka_add_qtest(<name> <target>)`; new test_save_open.cpp functions also need a `ka_add_qtest_filter(save_open_* …)` entry), write only to QTemporaryDir, use QStandardPaths test mode, never the app's org/app name. UI with no testable seam: keep the code, build plus the nearest test.
- Smoke (exit 0): `powershell.exe -NoProfile -ExecutionPolicy Bypass -Command '. ./scripts/dev-env.ps1; $p = Start-Process build/Release/ka-hgis.exe -ArgumentList "--smoke-quit" -WorkingDirectory build/Release -Wait -PassThru; exit $p.ExitCode'`. Never start ka-hgis.exe raw. If the user's app runs this exe, rename it before rebuilding; close it only if its title has no " *".
- After every successful app build: smoke, then repoint the 「Strata (개발)」 desktop shortcut (docs/agent/strata-reference.md). The desktop is `[Environment]::GetFolderPath('Desktop')`, which may be redirected (OneDrive\바탕 화면), not `%USERPROFILE%\Desktop`.

## Git (main = GitHub kyi0109657401-hub/hgis)

- `<main>` below = the main checkout, the first line of `git worktree list` (PCs: `C:\Users\권을\Documents\qgis` + `C:\OSGeo4W`; `D:\hgis` + `D:\OSGeo4W`; old `A:\qgis` + `A:\OSGeo4W`). `git remote -v` origin must be kyi0109657401-hub/hgis: `kwonyoungin11/hgis` (now http-www-dong-guk-or-kr/hgis) is a stale copy that stopped on 2026-10-01.
- Work in the session worktree (in `<main>` itself, in place); never create another. Message `type(scope): 한국어 요약`, Korean body with a test-result line, Co-Authored-By. Commit from the Bash tool.
- Every verified commit (2026-10-03): rebase onto main if it moved and rerun the touched tests, then `git -C <main> merge --ff-only <sha>` and `git -C <main> push origin main`. Never cancel, disable or wait on GitHub CI; check its last run once at the next task. PRs only when asked. If memory names a newer line the user calls the current app, say so in the first report.
- `.claude/hooks/commit-gate.mjs` blocks commits touching src/, tests/, cmake/, data/, CMakeLists.txt until the build and a later ctest prove them, test-weakening commits without `시험 변경:`, and C++ files past the CI line limit (raising docs/quality/line-limit-baseline.txt needs `길이 기준 변경:`). Follow its reason; no bypass.
- Worktree-only work is lost when a desktop session is deleted. Lost-work report: check `git -C <main> for-each-ref refs/snapshots` first.
- Never force-push, run scripts/auto-git-push.ps1 or scripts/commit.ps1, or commit secrets, *.gpkg/*.qgz or a portable's config.
- 「포터블」 only when asked: skill `portable-release`; say first it is ~1 GB and C: is nearly full.

## Windows pitfalls

- Other repo .ps1: same `powershell.exe -NoProfile -ExecutionPolicy Bypass` form, from Bash.
- A .ps1 with Korean needs a UTF-8 BOM. moc cannot parse `R"(...)"` in Q_OBJECT files (silent link failures). Git Bash: /dev/null, not `>nul`.