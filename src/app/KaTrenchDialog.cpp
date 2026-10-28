#include "KaTrenchDialog.h"
#include "KaAzimuthDial.h"
#include "KaTargetMeter.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QSpinBox>
#include <QStyle>
#include <QThreadPool>
#include <QTimer>
#include <QVBoxLayout>

namespace {
// Spin-box steps and typing settle before the 10%/2% search runs.
constexpr int kPlanSettleMs = 200;
}  // namespace

KaTrenchDialog::KaTrenchDialog(QWidget* parent) : QDialog(parent) {
  setWindowTitle(QStringLiteral("시굴격자 속성"));
  setModal(false);

  auto* form = new QFormLayout(this);

  m_auto = new QCheckBox(QStringLiteral("지금 그린(또는 선택한) 조사구역에 자동 배치"), this);
  form->addRow(m_auto);

  // 조사 종류가 목표 비율을 정한다. 길이·둑은 그 비율에 맞춰 자동으로 잡힌다.
  m_kind = new QComboBox(this);
  m_kind->addItem(QStringLiteral("시굴조사 — 전체 면적의 10%"),
                  static_cast<int>(SurveyKind::Trial));
  m_kind->addItem(QStringLiteral("표본조사 — 전체 면적의 2%"),
                  static_cast<int>(SurveyKind::Sample));
  m_kind->addItem(QStringLiteral("직접 지정 — 규격·둑을 내가 정함"),
                  static_cast<int>(SurveyKind::Manual));
  m_kind->setToolTip(QStringLiteral(
      "시굴은 조사구역의 10%, 표본은 2%로 배치합니다. 폭 2 m·길이 20 m 이내에서 "
      "길이와 둑 간격을 맞추며, 모든 격자가 구역 안에 들어갑니다."));
  form->addRow(QStringLiteral("조사 종류"), m_kind);

  m_terrain = new QCheckBox(QStringLiteral("지형 사면 방향으로 넣기(등고선 직교)"), this);
  m_terrain->setToolTip(QStringLiteral(
      "트렌치 장축을 오르막 쪽으로 돌립니다. 등고선과 나란히 넣으면 같은 층만 "
      "따라가 층서를 못 읽습니다. DEM을 올려야 씁니다."));
  form->addRow(m_terrain);
  m_terrainInfo = new QLabel(this);
  m_terrainInfo->setObjectName(QStringLiteral("dialogHint"));
  m_terrainInfo->setWordWrap(true);
  form->addRow(m_terrainInfo);

  addShapeRows(form);  // 모양 (곧은 / ㄱ·ㄴ자) and 좌표격자에 맞추기, in KaTrenchDialogPlan.cpp


  m_balk = new QDoubleSpinBox(this);
  m_balk->setRange(0, 200);
  m_balk->setValue(10.0);
  m_balk->setSuffix(QStringLiteral(" m"));
  m_balk->setToolTip(QStringLiteral("트렌치 사이 간격입니다. 간격을 키우면 시굴 비율이 내려갑니다."));
  form->addRow(QStringLiteral("둑(간격)"), m_balk);

  m_rows = new QSpinBox(this);
  m_rows->setRange(1, 200);
  m_rows->setValue(2);
  form->addRow(QStringLiteral("행"), m_rows);

  m_cols = new QSpinBox(this);
  m_cols->setRange(1, 200);
  m_cols->setValue(2);
  form->addRow(QStringLiteral("열"), m_cols);

  m_az = new QDoubleSpinBox(this);
  m_az->setRange(0, 360);
  m_az->setValue(0);
  m_az->setSingleStep(1.0);
  m_az->setSuffix(QStringLiteral(" °"));
  m_az->setToolTip(QStringLiteral("격자 회전(북 기준 시계 방향). 「구역에 깔기」를 누르면 회전된 격자로 다시 배치합니다."));
  const bool paper = qApp && qApp->property("kaPaperLook").toBool();
  if (paper) {  // 2차 보드 판 5: a dial beside the box and one plain sentence on where the grid points
    auto* turn = new QHBoxLayout;
    turn->setSpacing(8);
    m_dial = new KaAzimuthDial(this);
    m_dialNote = new QLabel(KaAzimuthDial::note(0.0), this);
    m_dialNote->setObjectName(QStringLiteral("trenchDialNote"));
    turn->addWidget(m_az);
    turn->addWidget(m_dial);
    turn->addWidget(m_dialNote, 1);
    form->addRow(QStringLiteral("회전(방위)"), turn);
  } else {
    form->addRow(QStringLiteral("회전(방위)"), m_az);
  }

  m_prefix = new QLineEdit(QStringLiteral("Tr-"), this);
  form->addRow(QStringLiteral("이름 접두"), m_prefix);

  // 계산 결과는 창 안의 결과 칸에 보인다. 초록 굵은 한 줄로 흘려 쓰지 않는다.
  m_ratio = new QLabel(this);
  m_ratio->setObjectName(QStringLiteral("trenchSummary"));
  m_ratio->setWordWrap(true);
  form->addRow(m_ratio);
  if (paper) {  // 2차 보드 판 5: the 시굴 비율 on a bar with the 목표 marked
    m_meter = new KaTargetMeter(this);
    m_meter->hide();
    form->addRow(m_meter);
  }

  // 깐 뒤에 쓰는 도구는 격자가 생긴 다음에만 보인다. 한 줄에 단추 다섯 개는 좁았다.
  m_afterPlace = new QWidget(this);
  auto* afterLay = new QVBoxLayout(m_afterPlace);
  afterLay->setContentsMargins(0, 0, 0, 0);
  afterLay->setSpacing(6);
  auto* hint = new QLabel(
      QStringLiteral("깐 격자는 맵에서 끌어 옮깁니다. 개별 편집은 트렌치 하나 이동·삭제입니다."),
      m_afterPlace);
  hint->setObjectName(QStringLiteral("dialogHint"));
  hint->setWordWrap(true);
  afterLay->addWidget(hint);
  auto* afterRow = new QHBoxLayout();
  auto* editOne = new QPushButton(QStringLiteral("개별 편집"), m_afterPlace);
  editOne->setToolTip(QStringLiteral("그래픽처럼 트렌치를 하나씩 선택·이동·삭제합니다."));
  auto* move = new QPushButton(QStringLiteral("전체 이동"), m_afterPlace);
  move->setToolTip(QStringLiteral("격자를 끌어다 놓아 전체를 옮깁니다. Ctrl+Z로 되돌립니다."));
  afterRow->addWidget(editOne);
  afterRow->addWidget(move);
  afterRow->addStretch(1);
  afterLay->addLayout(afterRow);
  form->addRow(m_afterPlace);
  m_afterPlace->setVisible(false);

  auto* btnRow = new QHBoxLayout();
  auto* apply = new QPushButton(QStringLiteral("구역에 깔기"), this);
  apply->setDefault(true);
  apply->setToolTip(QStringLiteral("조사구역 위에 격자를 다시 깝니다. 깐 뒤 맵에서 끌어 옮기세요."));
  auto* manual = new QPushButton(QStringLiteral("맵에 찍기"), this);
  manual->setToolTip(QStringLiteral("맵을 한 번 찍어 그 점을 원점으로 놓습니다."));
  auto* close = new QPushButton(QStringLiteral("닫기"), this);
  btnRow->addWidget(apply);
  btnRow->addWidget(manual);
  btnRow->addStretch(1);
  btnRow->addWidget(close);
  form->addRow(btnRow);

  connect(apply, &QPushButton::clicked, this, &KaTrenchDialog::applyRequested);
  connect(manual, &QPushButton::clicked, this, &KaTrenchDialog::manualPlaceRequested);
  connect(editOne, &QPushButton::clicked, this, &KaTrenchDialog::editSingleRequested);
  connect(move, &QPushButton::clicked, this, &KaTrenchDialog::moveRequested);
  connect(close, &QPushButton::clicked, this, &QDialog::hide);

  // One background search at a time; a newer request makes older answers stale.
  m_planPool = new QThreadPool(this);
  m_planPool->setMaxThreadCount(1);
  m_planTimer = new QTimer(this);
  m_planTimer->setSingleShot(true);
  m_planTimer->setInterval(kPlanSettleMs);
  connect(m_planTimer, &QTimer::timeout, this, &KaTrenchDialog::refreshPlan);

  connect(m_balk, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          [this](double) { schedulePlan(); });
  connect(m_az, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          [this](double) { schedulePlan(); });
  connect(m_rows, QOverload<int>::of(&QSpinBox::valueChanged), this,
          [this](int) { schedulePlan(); });
  connect(m_cols, QOverload<int>::of(&QSpinBox::valueChanged), this,
          [this](int) { schedulePlan(); });
  connect(m_auto, &QCheckBox::toggled, this, [this](bool) { schedulePlan(); });
  connect(m_kind, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this](int) { schedulePlan(); });
  connect(m_terrain, &QCheckBox::toggled, this, [this](bool) { schedulePlan(); });
  refreshPlan();
}

KaTrenchDialog::~KaTrenchDialog() {
  // A search still running posts its answer to this dialog: let it finish first.
  if (m_planPool) {
    m_planPool->clear();
    m_planPool->waitForDone();
  }
}

void KaTrenchDialog::setArea(const QByteArray& wkb, double areaM2) {
  const bool hadArea = !m_areaWkb.isEmpty();
  m_areaWkb = wkb;
  m_areaM2 = areaM2;
  const bool has = !wkb.isEmpty();
  m_auto->setEnabled(has);
  if (!has)
    m_auto->setChecked(false);
  else if (!hadArea)
    m_auto->setChecked(true);
  m_auto->setToolTip(has
                         ? QStringLiteral(
                               "조사구역이 여러 개면 선택한 곳만, 선택이 없으면 마지막에 그린 곳만 깝니다.")
                         : QStringLiteral("조사구역을 먼저 그리면 그 구역 안에 자동 배치를 쓸 수 있습니다."));
  schedulePlan();
}

KaTrenchDialog::SurveyKind KaTrenchDialog::surveyKind() const {
  return static_cast<SurveyKind>(m_kind->currentData().toInt());
}

double KaTrenchDialog::targetPct() const {
  switch (surveyKind()) {
    case SurveyKind::Trial: return 10.0;
    case SurveyKind::Sample: return 2.0;
    case SurveyKind::Manual: break;
  }
  return 0.0;
}

bool KaTrenchDialog::useTerrainAzimuth() const {
  return m_terrain->isChecked() && m_aspect.valid;
}

void KaTrenchDialog::setTerrainAspect(const TrenchGridGenerator::SlopeAspect& aspect) {
  m_aspect = aspect;
  m_terrain->setEnabled(aspect.valid);
  if (!aspect.valid) {
    m_terrain->setChecked(false);
    // 사용자 요청 2026-10-05: no terrain data and flat ground are told apart; both lay north-south (방위 칸 0°).
    m_terrainInfo->setText(
        (aspect.samples < 3 ? QStringLiteral("지형 자료가 없어 남북으로 깝니다(DEM·수치지형도 등고선 없음).")
                            : QStringLiteral("평지(평균 경사 %1%)라 남북으로 깝니다.")
                                  .arg(QLocale().toString(aspect.slopePct, 'f', 1))) +
        QStringLiteral(" 방위 칸에 다른 값을 넣으면 그 방향으로 깝니다."));
  } else {
    // 사면이 있으면 기본으로 켠다 — 층서를 자르는 쪽이 조사 기본이다.
    m_terrain->setChecked(true);
    m_terrainInfo->setText(QStringLiteral("지형: 오르막 %1° · 경사 %2% — 트렌치 장축을 이 방향으로 넣습니다.")
                               .arg(QLocale().toString(aspect.azimuthDeg, 'f', 0))
                               .arg(QLocale().toString(aspect.slopePct, 'f', 1)));
  }
  schedulePlan();
}

double KaTrenchDialog::effectiveAzimuth() const {
  return gridLock() ? m_grid.rotationDeg : useTerrainAzimuth() ? m_aspect.azimuthDeg : m_az->value();
}

TrenchGridGenerator::Spec KaTrenchDialog::spec() const {
  TrenchGridGenerator::Spec sp;
  sp.trenchWidth = 2.0;
  sp.trenchLength = m_length->checkedId();
  sp.armLength = m_shape->currentData().toInt() > 0 ? 15.0 : 0.0;
  sp.armBend = qMax(0, m_shape->currentData().toInt() - 1);
  sp.balkWidth = m_balk->value();
  sp.rows = m_rows->value();
  sp.cols = m_cols->value();
  sp.azimuthDeg = effectiveAzimuth();
  sp.namePrefix = m_prefix->text().trimmed();
  if (sp.namePrefix.isEmpty())
    sp.namePrefix = QStringLiteral("Tr-");
  return sp;
}

bool KaTrenchDialog::autoFill() const {
  return m_auto->isChecked() && !m_areaWkb.isEmpty();
}

void KaTrenchDialog::setGridPlaced(bool placed) {
  if (m_afterPlace)
    m_afterPlace->setVisible(placed);
  adjustSize();
}

void KaTrenchDialog::showMeter(double percent, double target) {
  if (!m_meter) return;
  m_meter->setVisible(percent >= 0.0);
  if (percent < 0.0) return;
  m_meter->setTarget(target);
  m_meter->setValue(percent);
}

void KaTrenchDialog::setSummaryTone(const char* tone) {
  m_ratio->setProperty("tone", QString::fromLatin1(tone));
  m_ratio->style()->unpolish(m_ratio);
  m_ratio->style()->polish(m_ratio);
}

// The preview (schedulePlan / refreshPlan) lives in KaTrenchDialogPlan.cpp.
