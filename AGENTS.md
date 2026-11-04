# Strata (ka-hgis) — Antigravity Workspace Rules (앱 전체 지침)

사용자는 고고학 현장 전문가다. 모든 안내와 보고는 간결한 한국어로 작성하고, 코드·파일·심볼은 클릭 가능한 링크로 제시한다.

## 1. 앱 아키텍처 및 데이터 흐름
- **기술 스택**: C++20 / Qt 6 Widgets / OSGeo4W `qgis-dev` (`qgis_core`, `qgis_gui`) 기반 독립 데스크톱 앱. QGIS를 포크하거나 PROJ·GDAL·렌더링·좌표변환을 직접 재구현하지 않는다.
- **계층 분리**:
  - `src/core` → 정적 라이브러리 `ka_core` (GIS·저장·검수·내보내기·다운로드 서비스, UI 창 코드 금지).
  - `src/app` → 실행 파일 `ka-hgis` (`MainWindow`는 `MainWindow.cpp` + `MainWindow{Align,Cadastral,Chrome,ContextMenus,Downloads,Editing,Erase,Export,FeatureCard,Files,Offline,Overlay,Ribbon,Session,SurveyContour,Topographic,Undo}.cpp`로 분할, `KaDrawingStudio` 도면 조판, `KaTerrain3dStudio` 3D 지형, `Ka*` 위젯·지도 도구).
  - 테스트 실행 파일은 `ka_core`를 링크하고 필요한 `src/app/*.cpp`만 `CMakeLists.txt`의 `add_executable`에 명시적으로 포함한다(GLOB 미사용). `data/` 수정 시 실행 파일 옆으로 복사되도록 `ka-hgis` 재빌드가 필요하다.
- **핵심 데이터 흐름**:
  1. `KaApplication` 부팅 → `MainWindow` 첫 화면(홈만 표시, 자동 복원·자동 저장 없음; 저장 안 된 변경은 제목에 ` *` 표시).
  2. 새 조사(`SurveyProjectFactory`, 기본 폴더 `preferredSurveyDir()`, 바탕화면 기본값 금지) 또는 열기(`SurveyStorage::readEmbedded` + `LayerOps` 복구·재연결 + `reloadSurveyGpkgReaders`).
  3. 그리기(`KaCaptureMapTool`) → `LayerOps::ensureDomainLayer` (도메인 레이어를 추가하는 유일한 경로) → `KaUndoGroup`(도형 생성과 속성 입력을 단일 Undo 단계로 묶음).
  4. 저장(`Ctrl+S` → `SurveyStorage::persistWorkspace`): 세대 복사본 생성·검증·원자적 교체. 다른 이름으로 저장(`SurveySaveAs::moveToCopy`)은 원본에 커밋하지 않고 사본에만 기록한다.
  5. 검수·제출(`ProjectStateBuilder` → `ChecklistEngine` + `data/rules/drawing_checklist.v1.json` → `LayoutService` `user_sheet` + `ExportService::exportSubmissionPackage`): `EPSG:5179` SHP + `조사도면.pdf` + `MANIFEST.sha256` + `README_submit.txt` 생성. 주변유적 현황 엑셀(`.xlsx`)은 `NearbyHeritageExport::exportToXlsx`로 내보낸다.

## 2. 도메인 레이어·범례·좌표계(CRS)·공간 연산 불변 규칙
- **7개 도메인 레이어** (`data/schemas/ka_hgis_layers.yaml`):
  `survey_area`(조사구역), `feature_poly`(유구 면), `feature_line`(유구 선), `section_line`(단면선), `control_points`(기준점), `artifact_point`(유물 점), `trial_trench`(트렌치).
  - 레이어 정체성은 항상 `ka_hgis/layer_key` 커스텀 속성으로 판별한다(한국어 레이어 이름은 표시용 라벨).
  - 빈 도메인 레이어를 자동 추가하거나 GPKG 테이블이 있다고 해서 무조건 레이어 트리에 올리지 않는다. `loadSurveyLayers`는 절대 `removeAllMapLayers()`를 호출하지 않는다.
  - **숨겨진 도메인 레이어 가시성 복구**: 기존 조사 파일에서 `survey_area` 등 도메인 레이어가 이미 존재하지만 레이어 트리 체크가 꺼져(`!node->isVisible()`) 있을 수 있다. 자동 이어그리기 대상(`KaSurveyAreaDialog::layerToContinue`)은 보이는 레이어만 고르고, 편집·도형 추가 시에는 반드시 `setItemVisibilityChecked(true)`로 레이어와 상위 그룹 표시를 켠다.
- **범례 구역 및 스냅 정책**:
  - 레이어 목록 구역: `조사 데이터` / `지적도` / `주변·참조` / `배경 지도`.
  - 배경지도(위성·일반·역사지도), 수치지형도, 토양도, 지질도, 주변유적, 정합 도면(DXF/DWG), 정합 래스터는 모두 참조 자료(`참조 지도`)이며 절대 `조사 데이터`에 넣지 않는다. 참조·지적 레이어는 폴리곤 나누기·묶기 등 원본 변형 명령에서 차단(`MainWindow::refuseReadOnlyLayer`)한다.
  - 지적 레이어(5km 다운로드 지적도, VWorld 지적 본번·부번)는 레이어 트리 최상위(루트, `참조 지도` 그룹 밖)에 두며, VWorld 지적 그림(WMS)에는 자석(스냅)이 붙지 않는다.
- **좌표계(CRS)**:
  - 작업 좌표계는 `EPSG:5187`(기본, 동부원점) 또는 `EPSG:5186`(중부원점)이며 절대 `EPSG:5179`로 가정하지 않는다.
  - 제출 꾸러미(`ExportService`)만 `EPSG:5179` SHP로 변환해 파일로 내보내며 지도에 다시 추가하지 않는다.
- **주변 거리 버퍼(`500m`/`1000m`) 및 반경 내 유적 추출**:
  - `BufferAnalysis::addDistanceRing` 및 `NearbyHeritageExport::collect`는 직각 모서리가 $\sqrt{2}$배(`707~710m`)로 튀어나가고 미세 꺾임이 왜곡되는 `JoinStyle::Miter`를 쓰지 않는다.
  - 항상 모든 방향(대각선 모서리 포함)에서 실제 거리가 정확히 맞도록 **`Qgis::JoinStyle::Round`(36분할)** 및 유클리드 최단거리(`sitePoly.distance(g) <= radius`)를 사용한다.

## 3. 크래시(`0xc0000005`)·GPKG·API 키 안전 규칙
- **렌더링 크래시 방지**:
  - `qgis/parallel_rendering=false` 유지.
  - WMS/XYZ 타일 로딩·렌더링 중(`canvas->isDrawing()`)에는 절대 `stopRendering()`, `clearCache()`, 블로킹 WMS `GetMap`, 동기 `canvas->refresh()`를 호출하지 않는다. 캔버스 갱신은 `LayerOps::refreshCanvasIfIdle`을 사용한다.
- **도면 조판(`KaDrawingStudio` / `LayoutService`) 크래시 방지**:
  - QGIS 기본 러버밴드 금지, 빈 레이어 목록 `setLayers({})` 금지, `refresh()` 직접 호출 금지.
  - 항상 `m_toolSelect->setLayout(ly)`를 호출하고, 범례는 사용자가 용지 밖에 놓아도(`ka_hgis/legend_kept_on_page`) 강제로 용지 안으로 되밀지 않는다.
- **QGIS 편집 버퍼 및 GPKG 무결성**:
  - 저장 전 피처는 음수 ID를 가진다.
  - 단일 `POLYGON` GPKG 테이블에 멀티폴리곤을 넣으면 면적 0인 깨진 링이 되므로 반드시 조각별로 분리해 개별 피처로 저장한다(`PolygonPieces`). 떨어진 두 구역은 단일 폴리곤 레이어에서 하나로 묶지 않는다.
  - QGIS OGR GPKG `featureCount()`가 일시적으로 실패하거나 낡은 값을 줄 수 있으므로, 제출·검수·도형 유무 확인에는 항상 `LayerFeatures::any`와 `LayerFeatures::count`를 사용한다.
  - 레이어 삭제 후 저장 시 파일 잠금(`MoveFileEx` 거부)이 일어나지 않도록 삭제된 조사 레이어는 `SurveyHandles::release`로 핸들을 놓는다.
- **API 키 및 보안**:
  - VWorld 키나 계정 비밀번호를 코드·로그·오류 메시지에 절대 노출하거나 커밋하지 않는다(`SecretMask` 사용).
  - 키 로드 순서: `%LOCALAPPDATA%\ka-hgis\ka-hgis-vworld.ini` → `HKCU\Software\ka-hgis` → 환경변수 `VWORLD_API_KEY` → `.gitignore` 처리된 `config/secrets.ini`.
  - 위성지도는 키가 있으면 공식 `api.vworld.kr` WMTS 우선, 없을 때만 `xdworld` 폴백. 지적 WMS는 `crs=EPSG:3857` + `KEY`(후보 `4326→3857→900913`만, `5186/5187/5179` 금지).
- **원본 데이터 보호**:
  - 사용자의 실제 조사 파일(`*.gpkg`, `*.qgz`)은 분석 시 읽기 전용으로만 열며 절대 수정·덮어쓰지 않는다. 테스트는 항상 `QTemporaryDir`와 `QStandardPaths::setTestModeEnabled(true)`만 사용한다.

## 4. 빌드·테스트·스모크·코드 수정 규칙 (`Qt6Xml.dll` 오류 및 300줄 제한 대응)
- **`build/Release/ka-hgis.exe` 무환경 단독 실행 절대 금지**:
  - OSGeo4W/Qt6 PATH 없이 `build/Release/ka-hgis.exe`를 호출하면 사용자 화면에 `Qt6Xml.dll이(가) 없어 코드 실행을 진행할 수 없습니다.` 시스템 모달 창이 뜨고 작업이 멈춘다.
  - 빌드(`cmake --build`, `AUTORCC`의 `rcc.exe` 실행 포함), 테스트(`ctest`), 스모크(`--smoke-quit`) 실행 시 반드시 같은 셸에서 `. ./scripts/dev-env.ps1` (또는 `D:\OSGeo4W\apps\Qt6\bin;D:\OSGeo4W\apps\qgis-dev\bin;D:\OSGeo4W\bin` PATH)을 먼저 적용한다.
- **CMake 빌드 및 테스트 명령**:
  - 빌드: `. ./scripts/dev-env.ps1; cmake --build build --config Release --target ka-hgis <test_target> --parallel 4`
  - 변경한 영역의 테스트만 실행: `. ./scripts/dev-env.ps1; ctest --test-dir build -C Release -R "^(name1|name2)$" --output-on-failure`
  - 스모크: `. ./scripts/dev-env.ps1; $p = Start-Process build/Release/ka-hgis.exe -ArgumentList "--smoke-quit" -WorkingDirectory build/Release -Wait -PassThru; exit $p.ExitCode` (종료 코드 `0` 필수).
- **전역 `clang-format` 훅 및 300줄 제한(`scorecard.ps1 -LineLimitOnly`)**:
  - 이 PC의 전역 PostToolUse 훅(`cpp_clang_format.py`)은 파일 편집 시 파일 전체에 `clang-format -i`를 실행한다.
  - 새 C++ 파일(`src/`, `tests/`)은 포맷 후에도 **300줄 이하**가 되도록 285줄 이하로 작성하고, 기존 300줄 초과 파일(`docs/quality/line-limit-baseline.txt` 등록 파일)은 수정한 줄만 남기고 무관한 포맷 diff를 정리한다.
  - 커밋 전 반드시 `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/scorecard.ps1 -LineLimitOnly` 통과를 확인한다.
- **예외 처리 및 테스트 변경 표시**:
  - `src/` 아래 모든 `catch (...)` 블록에는 반드시 `KA_LOG_EXCEPT();` (또는 `KaSessionLog::line`)를 넣는다(`catch_log` 테스트 검증).
  - 기존 테스트의 기댓값을 바꾸거나 검사 줄을 수정·제거할 때는 커밋 메시지에 반드시 `시험 변경: <이유>` 줄을 포함한다.

## 5. Git·워크트리·포터블 동기화
- 원격 저장소는 `https://github.com/kyi0109657401-hub/hgis.git` (`origin/main`)이며 로컬 `D:\hgis` 클론과 동일하다.
- 검증된 커밋은 `git push origin HEAD:main`으로 푸시하고 `git -C D:/hgis pull --ff-only origin main`으로 `D:\hgis`까지 동기화한다.
- 바탕화면에 개인용 포터블 폴더(`Strata-포터블-개인용-*`)가 있을 때 앱 변경 후 최신 `build/Release/ka-hgis.exe`를 포터블 폴더에도 복사하고 포터블 폴더 기준 `--smoke-quit`으로 검증한다.
