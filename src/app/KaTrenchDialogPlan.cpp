// KaTrenchDialog preview: debounced, cached, computed off the UI thread (F097), and the
// prompt before a re-generation replaces a hand-adjusted grid (F156).
#include "KaTrenchDialog.h"
#include "KaAzimuthDial.h"
#include "core/TrenchPlanCache.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QRadioButton>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QMetaObject>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QThreadPool>
#include <QTimer>

#include <utility>

void KaTrenchDialog::addShapeRows(QFormLayout* form) {
  // ㄱ·ㄴ자 트렌치 (사용자 요청 2026-10-19): 한 모서리에서 직각으로 꺾인 두 팔 15 m + 15 m, 폭 2 m, 꺾는 방향 네 가지.
  m_shape = new QComboBox(this);
  m_shape->setObjectName(QStringLiteral("trenchShape"));
  m_shape->addItem(QStringLiteral("곧은 트렌치"), 0);  // data: 0 곧은, 1 + Spec::armBend for the Ls
  for (const QString& bend : {QStringLiteral("ㄴ자"), QStringLiteral("ㄱ자"), QStringLiteral("ㄴ자 좌우 반대"),
                              QStringLiteral("ㄱ자 좌우 반대")})
    m_shape->addItem(bend + QStringLiteral(" 트렌치 (15 m + 15 m)"), m_shape->count());
  m_shape->setToolTip(QStringLiteral("ㄱ·ㄴ자는 팔 15 m 그대로 구역 안에 온전히 들어가는 것만 놓아, 비율이 목표를 조금 넘습니다."));
  form->addRow(QStringLiteral("모양"), m_shape);
  connect(m_shape, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { schedulePlan(); });

  // 트렌치 길이 (사용자 요청 2026-10-28): 고른 길이 하나로만 깐다. 섞거나 잘라 넣거나 평균 길이로 줄이지 않는다.
  m_lengthRow = new QWidget(this);
  m_lengthRow->setObjectName(QStringLiteral("trenchLength"));
  auto* lengthLay = new QHBoxLayout(m_lengthRow);
  lengthLay->setContentsMargins(0, 0, 0, 0);
  m_length = new QButtonGroup(m_lengthRow);
  for (int metres : {20, 15, 10}) {
    auto* button = new QRadioButton(QStringLiteral("%1 m").arg(metres), m_lengthRow);
    button->setToolTip(QStringLiteral("폭 2 m · 길이 %1 m 트렌치로만 조사구역 안에 깝니다.").arg(metres));
    button->setChecked(metres == 20);
    m_length->addButton(button, metres);
    lengthLay->addWidget(button);
  }
  lengthLay->addStretch(1);
  form->addRow(QStringLiteral("트렌치 길이"), m_lengthRow);
  connect(m_length, &QButtonGroup::idToggled, this, [this](int, bool on) { if (on) schedulePlan(); });

  // 「좌표격자에 맞추기」 (사용자 요청 2026-10-20): 바깥 모서리를 좌표격자 교점에, 방향을 좌표격자 방향에 둔다.
  m_gridSnap = new QCheckBox(QStringLiteral("좌표격자에 맞추기"), this);
  m_gridSnap->setObjectName(QStringLiteral("trenchGridLock"));
  form->addRow(m_gridSnap);
  connect(m_gridSnap, &QCheckBox::toggled, this, [this](bool) { schedulePlan(); });
}

void KaTrenchDialog::setCoordinateGrid(bool available, const TrenchGridSnap::Grid& grid) {
  m_gridAvailable = available && grid.step > 0.0;
  m_grid = grid;
  schedulePlan();
}

const TrenchGridSnap::Grid* KaTrenchDialog::gridLock() const {
  return m_gridSnap && m_gridSnap->isEnabled() && m_gridSnap->isChecked() ? &m_grid : nullptr;
}

void KaTrenchDialog::schedulePlan() {
  refreshRows();
  ++m_planSeq;  // an answer for the previous input is stale from now on
  m_planPending = true;
  m_planTimer->start();
}

void KaTrenchDialog::refreshRows() {
  const bool fill = autoFill();
  const bool ratio = fill && ratioMode();
  // 지금 쓰지 않는 칸은 회색으로 두지 않고 숨긴다. 자동 배치에서 행·열 「2」가 보이면
  // 트렌치 22개와 어긋나 보였다. 비율 모드의 길이·둑은 아래 결과 칸에 적힌다.
  // 비율 모드의 이름은 조사 종류가 정한다(시굴 Tr-, 표본 Sp-). 접두 칸은 쓰이지 않아 숨긴다.
  if (auto* form = qobject_cast<QFormLayout*>(layout())) {
    form->setRowVisible(m_rows, !fill);
    form->setRowVisible(m_cols, !fill);
    form->setRowVisible(m_lengthRow, m_shape->currentData().toInt() == 0);
    form->setRowVisible(m_balk, !ratio);
    form->setRowVisible(m_prefix, !ratio);
  }
  // Only the ㄱ·ㄴ자 10%/2% plan is laid on the coordinate grid.
  m_gridSnap->setEnabled(m_gridAvailable && ratio && m_shape->currentData().toInt() > 0);
  m_gridSnap->setToolTip(m_gridAvailable
      ? QStringLiteral("ㄱ·ㄴ자 시굴·표본에서 꺾인 바깥 모서리를 좌표격자 %1 m 교점에, 방향을 좌표격자 방향에 맞춥니다. "
                       "트렌치끼리는 1 m 이상 띄웁니다.").arg(QLocale().toString(m_grid.step, 'g', 6))
      : QStringLiteral("지도에 미터 좌표격자를 켜면 쓸 수 있습니다."));
  m_az->setEnabled(!useTerrainAzimuth() && !gridLock());
  if (m_dial) {  // the dial shows the direction the grid will be laid in (the terrain's when that is on)
    m_dial->setAzimuth(effectiveAzimuth());
    m_dialNote->setText(KaAzimuthDial::note(effectiveAzimuth()));
  }
}

void KaTrenchDialog::refreshPlan() {
  m_planTimer->stop();
  refreshRows();
  const quint64 seq = ++m_planSeq;
  if (!autoFill()) {
    m_planPending = false;
    const TrenchGridGenerator::Spec sp = spec();
    const double one = sp.armLength > 0.0 ? sp.trenchWidth * (2.0 * sp.armLength - sp.trenchWidth)
                                          : sp.trenchWidth * sp.trenchLength;
    const double t = sp.rows * sp.cols * one;
    setSummaryTone("plain");
    showMeter(-1.0, 0.0);
    m_ratio->setText(QStringLiteral("수동 배치: %1개 · 총 %2㎡ — 「맵에 찍기」로 맵에서 위치를 정하세요.")
                         .arg(sp.rows * sp.cols)
                         .arg(QLocale().toString(t, 'f', 0)));
    return;
  }
  const QByteArray area = m_areaWkb;
  if (ratioMode()) {
    const double target = targetPct();
    const double az = effectiveAzimuth();
    const double arm = spec().armLength;
    const int bend = spec().armBend;
    const double length = spec().trenchLength;
    const bool onGrid = gridLock() != nullptr;
    const TrenchGridSnap::Grid grid = m_grid;
    TrenchGridGenerator::RatioFill known;
    if (TrenchPlanCache::lookup(area, target, 2.0, az, &known, arm, bend, onGrid ? &grid : nullptr, length)) {
      m_planPending = false;
      showRatioPlan(known, az);
      return;
    }
    m_planPending = true;
    setSummaryTone("plain");
    m_ratio->setText(QStringLiteral("배치를 계산하는 중입니다…"));
    // The pool is drained in the destructor, so `this` outlives the task.
    m_planPool->start([this, seq, area, target, az, arm, bend, onGrid, grid, length]() {
      const TrenchGridGenerator::RatioFill plan =
          TrenchPlanCache::ratioPlan(area, target, 2.0, az, arm, bend, onGrid ? &grid : nullptr, length);
      QMetaObject::invokeMethod(this, [this, seq, plan, az]() {
        if (seq != m_planSeq) return;
        m_planPending = false;
        showRatioPlan(plan, az);
      }, Qt::QueuedConnection);
    });
    return;
  }
  const TrenchGridGenerator::Spec sp = spec();
  m_planPending = true;
  setSummaryTone("plain");
  m_ratio->setText(QStringLiteral("배치를 계산하는 중입니다…"));
  m_planPool->start([this, seq, area, sp]() {
    QString reason;
    std::vector<TrenchGridGenerator::Cell> cells = TrenchGridGenerator::buildInArea(sp, area, &reason);
    QMetaObject::invokeMethod(this, [this, seq, cells = std::move(cells), reason]() {
      if (seq != m_planSeq) return;
      m_planPending = false;
      showFillPlan(cells, reason);
    }, Qt::QueuedConnection);
  });
}

void KaTrenchDialog::showRatioPlan(const TrenchGridGenerator::RatioFill& plan, double azimuth) {
  if (plan.cells.empty()) {
    setSummaryTone("warn");
    showMeter(-1.0, 0.0);
    m_ratio->setText(plan.error);
    return;
  }
  showMeter(plan.ratioPct, targetPct());
  {
    const QSignalBlocker blocker(m_balk);
    m_balk->setValue(plan.balk);
  }
  setSummaryTone("ok");
  const QString shape = plan.arm > 0.0
                            ? QStringLiteral("ㄱ·ㄴ자 트렌치 %2개 · 폭 2 m · 팔 %3 m + %3 m · 둑 %4 m 이상")
                            : QStringLiteral("%3 m 트렌치 %2개 · 폭 2 m · 둑 %4 m");
  m_ratio->setText(
      (QStringLiteral("%1 · ") + shape + QStringLiteral(" · 총 %5㎡ · 비율 %6% (목표 %7%) · 방위 %8°"))
          .arg(surveyKind() == SurveyKind::Trial ? QStringLiteral("시굴조사") : QStringLiteral("표본조사"))
          .arg(plan.cells.size())
          .arg(QLocale().toString(plan.arm > 0.0 ? plan.arm : plan.length, 'g', 4))
          .arg(QLocale().toString(plan.balk, 'f', 0))
          .arg(QLocale().toString(TrenchGridGenerator::totalArea(plan.cells), 'f', 2))
          .arg(QLocale().toString(plan.ratioPct, 'f', 1))
          .arg(QLocale().toString(targetPct(), 'f', 0))
          .arg(QLocale().toString(azimuth, 'f', 0)) +
      (gridLock() ? QStringLiteral(" · 좌표격자 %1 m에 맞춤").arg(QLocale().toString(m_grid.step, 'g', 6)) : QString()));
}

void KaTrenchDialog::showFillPlan(const std::vector<TrenchGridGenerator::Cell>& cells,
                                  const QString& reason) {
  if (cells.empty()) {
    // Say why (invalid boundary, spec, nothing fits) instead of 「트렌치 0개」.
    setSummaryTone("warn");
    showMeter(-1.0, 0.0);
    m_ratio->setText(reason.isEmpty()
                         ? QStringLiteral("현재 규격과 방향으로는 구역 안에 들어가는 트렌치가 없습니다.")
                         : reason);
    return;
  }
  const double t = TrenchGridGenerator::totalArea(cells);
  const double pct = m_areaM2 > 0.0 ? t / m_areaM2 * 100.0 : 0.0;
  const bool over = pct > 10.0;
  setSummaryTone(over ? "warn" : "ok");
  showMeter(pct, 10.0);
  m_ratio->setText(QStringLiteral("트렌치 %1개 · 총 %2㎡ · 시굴 비율 %3% (기준: 시굴 10% · 표본 2%)%4")
                       .arg(cells.size())
                       .arg(QLocale().toString(t, 'f', 0))
                       .arg(QLocale().toString(pct, 'f', 1))
                       .arg(over ? QStringLiteral(" — 간격을 키우세요") : QString()));
}

bool KaTrenchDialog::confirmReplaceAdjusted(QWidget* parent, qint64 trenchCount) {
  QMessageBox box(parent);
  box.setIcon(QMessageBox::Question);
  box.setWindowTitle(QStringLiteral("시굴격자"));
  box.setText(QStringLiteral("이미 깔린 시굴격자를 새 격자로 바꿀까요?"));
  box.setInformativeText(
      QStringLiteral("지금 격자(트렌치 %1개)는 손으로 옮기거나 지운 배치까지 새 배치로 바뀝니다.\n"
                     "바꾼 뒤에도 Ctrl+Z로 지금 격자를 되살릴 수 있습니다.")
          .arg(trenchCount));
  QPushButton* replace = box.addButton(QStringLiteral("새 격자로 바꾸기"), QMessageBox::AcceptRole);
  QPushButton* keep = box.addButton(QStringLiteral("그대로 두기"), QMessageBox::RejectRole);
  box.setDefaultButton(keep);
  box.setEscapeButton(keep);
  box.exec();
  return box.clickedButton() == replace;
}
