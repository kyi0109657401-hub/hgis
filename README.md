# 고고학 전용 HGIS (ka-hgis, 앱 이름 Strata) v2

C++20/Qt6 독립 실행형 필드고고학 HGIS. **OSGeo4W qgis-dev (QGIS 4.x) 라이브러리 링크**. 소스 포크 아님.
바탕화면 바로가기 이름은 「고고학 전용 HGIS」, 앱 창·시작 안내는 「Strata · 필드고고학 GIS」다(같은 앱).

작업 좌표계: **EPSG:5186(중부원점) / EPSG:5187(동부원점)**, 새 조사 기본 **5187**. **EPSG:5179 는 제출 SHP 전용**이다. 측량 폴리곤 중심 작성(제26조급 규칙 검수).

**처음 쓰는 사람:** [docs/user/quick-start.md](docs/user/quick-start.md) (새 조사 → 배경 → 그리기 → 검수·제출, 단축키, 용어, 문제 해결)

**저장소:** https://github.com/kyi0109657401-hub/hgis · 브랜치 `main`

## 다른 PC에서 바로 개발

상세: [`docs/other-pc-setup.md`](docs/other-pc-setup.md)

```powershell
git clone https://github.com/kyi0109657401-hub/hgis.git
cd hgis
# 기준 PC와 같은 OSGeo4W 판을 복사해 온다 (install-deps 는 설치한 날의 qgis-dev 를 받는다)
.\scripts\osgeo4w-bundle.ps1 -Import E:\ka-hgis-sdk

.\scripts\bootstrap-dev-pc.ps1   # dev-env.lock.json 비교 + build + ctest + smoke
.\scripts\run-ka-hgis.ps1
```

- **같은 환경:** 기준 PC 판은 `dev-env.lock.json`에 잠근다. `.\scripts\dev-env-lock.ps1`로 이 PC가 같은지 본다.
- **개발:** 클론 + OSGeo4W(`qgis-dev`) + VS2022 + CMake → `bootstrap-dev-pc.ps1` 또는 `build-all.ps1`
- **실행만:** 개발 PC에서 `.\scripts\make-portable.ps1` 후 `dist\ka-hgis-portable\` 폴더 전체를 복사 → `start.bat` (OSGeo4W 설치 불필요)
- 조사 GPKG/SHP는 git에 없음 → 별도 복사
- 규칙: `CLAUDE.md` · `docs/HANDOFF.md`

## 환경 (검증된 구성)
- CMake는 `dev-env.lock.json`의 앞 두 자리 (`C:\Program Files\CMake\bin`, `C:\CMake\bin` 또는 PATH)
- VS 2022 BuildTools MSVC. 도구 모음 앞 두 자리는 `dev-env.lock.json`
- 체크아웃은 클론한 폴더. SDK는 `scripts/dev-env.ps1`이 `OSGEO4W_ROOT` → `C:\OSGeo4W` → `D:\OSGeo4W` → `A:\OSGeo4W` 순으로 찾는다
  - 패키지 판: `dev-env.lock.json` (`qgis-dev`, `qt6-devel`, `gdal-dev-devel`, `sqlite3-devel`, `pdal-dev`)
- 산출물: `build\Release\ka-hgis.exe`, `ka_hgis_tests.exe`, `ka_workflow_tests.exe`

## 원클릭 빌드·검증
```powershell
cd <클론한 폴더>
.\scripts\build-all.ps1
```
포함: cmake build → ctest → smoke-quit → e2e. 포터블 생성은 별도 요청 시에만 실행한다.
clangd용 컴파일 DB(`build\compile_commands.json`)는 빌드가 만든다(`scripts/compile-commands.mjs`).
clangd 탐색·Archify 구조도 설정과 사용 범위는 [`docs/developer-tools.md`](docs/developer-tools.md)를 따른다.

## 수동 빌드
```powershell
. .\scripts\dev-env.ps1
cmake --preset vs
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
.\scripts\run-ka-hgis.ps1 --smoke-quit
.\scripts\e2e-smoke.ps1
```
느린 PC 의 성능 예산, 호환 표본, 시험 환경은 [`TEST_INFRA.md`](TEST_INFRA.md)를 본다.

## 실행
```powershell
.\scripts\run-ka-hgis.ps1
# 또는 포터블 (형제 ka-hgis.exe 우선)
cd dist\ka-hgis-portable
.\run.bat
# 또는
.\run-ka-hgis.ps1
```

### 제품 축 (현재)

화면은 한 줄 리본(조사 · 기록 · 자료 받기 · 배경 지도 · 정합 · 내보내기 · 기타)이다. 메뉴 막대는 없다.

1. **새 조사** → 작업 좌표계 5186/5187 선택, GPKG 스키마 준비, **범례는 비움** (그릴 때/가져올 때 레이어 등장)
2. **배경 지도·자료 받기** → VWorld·지형·토양·지적·주변유적 등 **참조 지도** (조사 데이터와 분리)
3. **그리기** → 조사구역/유구 면·선/유물 · 우클릭·Enter 완료 · 그리는 동안 속성 팝업 없음
4. **저장** → Ctrl+S 로만 저장(자동 저장 없음). 저장 안 한 상태는 창 제목의 ` *`
5. **도면** → 조판 탭(기본은 지도 틀만)
6. **검수·제출**(Ctrl+E) → 검수 후 SHP **EPSG:5179** + 조사도면.pdf + MANIFEST (error 가 남아 있으면 한 번 묻고 만들며 README 에 적는다. 도면 용지가 없으면 PDF 만 빠진다)

### 기타 › 더보기 (가끔 쓰는 기능)
| 기능 | 설명 |
|---|---|
| 벡터 불러오기 / CSV 기준점 | 바깥 SHP·GPKG, 기준점 CSV 가져오기 |
| 맞추기 | 스캔 도면·영상 정합 |
| OSM 배경 · Google 위성 | 다른 배경 타일 |
| 지도 넓게 보기 · 왼쪽 패널 접기/펴기 | Ctrl+F11 · F9 |
| API 키 입력 · 계정 3종 | VWorld 키, VWorld 지적도·수치지형도·국가유산 인트라넷 아이디·비밀번호 |
| 웹 자료 · 정보 | 외부 사이트 바로가기, 저작권·버전 정보 |

## 문서
- **사용 안내:** `docs/user/quick-start.md`
- **다른 PC 셋업:** `docs/other-pc-setup.md`
- 시나리오: `docs/user/gui-scenario-checklist.md`
- ADR: `docs/adr/0001-standalone-cpp-qgis-libs.md`
- 데이터 모델: `docs/domain/data-model.md` (스키마 원본 `data/schemas/ka_hgis_layers.yaml`)
- 데이터 흐름: `docs/architecture/data-flow.md`
- 문서 목록: `docs/README.md` (옛 IA·잡카드·와이어프레임은 `docs/archive/`, 현재 UI 아님)
- 에이전트: `CLAUDE.md`, `docs/HANDOFF.md`, 프로젝트 스킬 `.claude/skills/`

## 런타임 주의
`PATH`에 `qgis-dev\bin`, `Qt6\bin`, `gdal-dev\bin`, **`pdal-dev\bin`** 필요 (`pdal-devcpp210.dll`).  
`scripts\dev-env.ps1` / `run-ka-hgis.ps1`이 설정합니다.

VWorld 배경지도는 **기타 › 더보기 › API 키 입력**에 키가 있을 때만 추가됩니다 (바이너리 기본 키 없음).

## 라이선스
GNU GPLv2 or later (QGIS 링크 파생물). 고지 `LICENSE`, 전문 `COPYING`, 제3자 `THIRD_PARTY_NOTICES.md`.

## 커밋 진행 상태 (항상 확인)

현재 브랜치/HEAD/원격 동기화/최근 커밋 목록:

- 파일: `docs/COMMIT_STATUS.md` (훅을 설치한 PC 에서만 갱신된다. 날짜가 오래됐으면 그 PC 에 훅이 없다는 뜻이다. 최신 상태는 `git log` 가 정본)
- 수동 갱신: `.\scripts\update-commit-status.ps1`
- 훅 설치(한 번): `.\scripts\install-git-hooks.ps1`  → 이후 그 PC 의 커밋마다 갱신
- 헬퍼 커밋: `.\scripts\commit.ps1 -Message "..." -Path path1,path2 -Push`
