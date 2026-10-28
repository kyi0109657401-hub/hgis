#include "KaPrintDialog.h"
#include "KaUserError.h"
#include "KaTilePreview.h"
#include "KaWindowSize.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QPageLayout>
#include <QPainter>
#include <QPdfWriter>
#include <QPrintDialog>
#include <QPrinter>
#include <QPrinterInfo>
#include <QProgressDialog>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QVBoxLayout>
#include <QVariantMap>

#include <algorithm>
#include <cmath>

namespace {
constexpr double kDefaultMarginMm = 5.0;  // 프린터를 모를 때의 가장자리
constexpr double kLabelBandMm = 6.0;      // 장 번호·5 cm 확인선 띠
const QString kSettingsGroup = QStringLiteral("print");

QPageSize outputPage(KaPrintDialog::Output output) {
  switch (output) {
    case KaPrintDialog::OutputA0: return QPageSize(QPageSize::A0);
    case KaPrintDialog::OutputA1: return QPageSize(QPageSize::A1);
    case KaPrintDialog::OutputA2: return QPageSize(QPageSize::A2);
    case KaPrintDialog::OutputA3: return QPageSize(QPageSize::A3);
    case KaPrintDialog::OutputDrawing: break;
  }
  return {};
}

QString percent(double ratio) {
  return QString::number(int(std::lround(ratio * 100.0)));
}

QString decimal(double value) {
  QString text = QLocale(QLocale::Korean).toString(value, 'f', 1);
  if (text.endsWith(QLatin1String(".0"))) text.chop(2);
  return text;
}

// A1 처럼 표준 용지면 그 이름, 아니면 59.4×84.1 cm.
QString sizeText(const QSizeF& mm) {
  const QString iso = TilePrint::isoName(mm);
  if (!iso.isEmpty()) return iso;
  return QStringLiteral("%1×%2 cm").arg(decimal(mm.width() / 10.0), decimal(mm.height() / 10.0));
}
}  // namespace

KaPrintDialog::KaPrintDialog(const QString& pdfPath, const QString& title, double drawingScale, QWidget* parent)
    : QDialog(parent), m_pdfPath(pdfPath), m_title(title), m_drawingScale(drawingScale > 0.0 ? drawingScale : 0.0),
      m_plate(qApp && qApp->property("kaPaperLook").toBool()) {
  setObjectName(QStringLiteral("printDialog"));
  setProperty("kaPlate", m_plate);
  setWindowTitle(QStringLiteral("도면 인쇄"));
  QString readError;
  // 도면 PDF 는 한 번만 연다: 크기와 미리보기 그림을 함께 읽는다.
  m_drawingMm = TilePrint::pdfPageSizeMm(pdfPath, &readError, &m_thumb, 560);

  auto* root = new QVBoxLayout(this);
  auto* form = new QFormLayout;
  m_form = form;
  m_printer = new QComboBox(this);
  m_printer->setObjectName(QStringLiteral("printPrinter"));
  const QStringList printers = QPrinterInfo::availablePrinterNames();
  for (const QString& name : printers) m_printer->addItem(name, name);
  if (printers.isEmpty()) {
    m_printer->addItem(QStringLiteral("(설치된 프린터 없음)"), QString());
    m_printer->setEnabled(false);
  } else {
    const int preferred = m_printer->findData(QPrinterInfo::defaultPrinterName());
    if (preferred >= 0) m_printer->setCurrentIndex(preferred);
  }
  auto* printerRow = new QWidget(this);
  auto* printerLayout = new QHBoxLayout(printerRow);
  printerLayout->setContentsMargins(0, 0, 0, 0);
  printerLayout->addWidget(m_printer, 1);
  m_properties = new QPushButton(QStringLiteral("프린터 속성…"), printerRow);
  m_properties->setObjectName(QStringLiteral("printProperties"));
  m_properties->setToolTip(QStringLiteral(
      "프린터 속성에서 단면 인쇄를 고르세요. 양면으로 나오면 기본 설정에서 단면을 고릅니다."));
  printerLayout->addWidget(m_properties);
  form->addRow(QStringLiteral("프린터"), printerRow);

  m_modeTiles = new QRadioButton(QStringLiteral("여러 장으로 나눠 크게 인쇄 (붙여서 큰 도면)"), this);
  m_modeFit = new QRadioButton(QStringLiteral("한 장에 인쇄 (용지에 들어가면 실제 크기로)"), this);
  m_modeTiles->setObjectName(QStringLiteral("printModeTiles"));
  m_modeFit->setObjectName(QStringLiteral("printModeFit"));
  auto* modes = new QButtonGroup(this);
  modes->addButton(m_modeTiles);
  modes->addButton(m_modeFit);
  auto* modeBox = new QVBoxLayout;
  modeBox->addWidget(m_modeTiles);
  modeBox->addWidget(m_modeFit);
  form->addRow(QStringLiteral("방법"), modeBox);

  m_output = new QComboBox(this);
  m_output->setObjectName(QStringLiteral("printOutput"));
  m_outputLabel = new QLabel(QStringLiteral("붙였을 때 크기"), this);
  form->addRow(m_outputLabel, m_output);

  m_sheet = new QComboBox(this);
  m_sheet->setObjectName(QStringLiteral("printSheet"));
  m_sheet->addItem(QStringLiteral("A4"));
  m_sheet->addItem(QStringLiteral("A3"));
  m_sheet->setCurrentIndex(printerSupports(QPageSize::A3) && !printers.isEmpty() ? SheetA3 : SheetA4);
  m_sheet->setMinimumWidth(110);
  m_overlap = new QDoubleSpinBox(this);
  m_overlap->setObjectName(QStringLiteral("printOverlap"));
  m_overlap->setRange(0.0, 30.0);
  m_overlap->setDecimals(0);
  m_overlap->setValue(10.0);
  m_overlap->setSuffix(QStringLiteral(" mm"));
  m_overlap->setToolTip(QStringLiteral("이웃 장과 같은 그림이 겹치는 폭입니다. 붙일 때 맞추기 쉽습니다."));
  auto* paperRow = new QHBoxLayout;
  paperRow->addWidget(m_sheet);
  paperRow->addSpacing(12);
  m_overlapLabel = new QLabel(QStringLiteral("겹침"), this);
  paperRow->addWidget(m_overlapLabel);
  paperRow->addWidget(m_overlap);
  paperRow->addStretch(1);
  form->addRow(QStringLiteral("용지"), paperRow);

  // 저장 PDF 와 같은 도면 PDF 를 그려 찍는다. 해상도 상한만 고른다.
  m_dpi = new QComboBox(this);
  m_dpi->setObjectName(QStringLiteral("printDpi"));
  for (int dpi : TilePrint::printDpiChoices()) {
    m_dpi->addItem(dpi <= int(TilePrint::kDefaultPrintDpi)
                       ? QStringLiteral("%1 DPI (보통)").arg(dpi)
                       : QStringLiteral("%1 DPI (가는 선이 많은 실측도)").arg(dpi),
                   dpi);
  }
  m_dpi->setToolTip(QStringLiteral(
      "도면 PDF를 이 해상도로 그려 찍습니다. 0.1~0.2 mm 선이 많은 1:20 실측도는 600을 고르세요. "
      "프린터 해상도보다 곱게 그리지는 않습니다."));
  form->addRow(QStringLiteral("해상도"), m_dpi);

  m_marks = new QCheckBox(QStringLiteral("자르는 점선·장 번호·5 cm 확인선 넣기"), this);
  m_marks->setObjectName(QStringLiteral("printMarks"));
  m_marks->setChecked(true);
  m_overview = new QCheckBox(QStringLiteral("맨 앞에 붙이는 순서 안내 한 장 넣기"), this);
  m_overview->setObjectName(QStringLiteral("printOverview"));
  m_overview->setChecked(true);
  m_checksRow = new QWidget(this);
  auto* checks = new QVBoxLayout(m_checksRow);
  checks->setContentsMargins(0, 0, 0, 0);
  checks->addWidget(m_marks);
  checks->addWidget(m_overview);
  form->addRow(QString(), m_checksRow);
  root->addLayout(form);

  m_summary = new QLabel(this);
  m_summary->setObjectName(QStringLiteral("printSummary"));
  m_summary->setWordWrap(true);
  QFont strong = m_summary->font();
  strong.setBold(true);
  m_summary->setFont(strong);
  root->addWidget(m_summary);
  m_warning = new QLabel(this);
  m_warning->setObjectName(QStringLiteral("printWarning"));
  m_warning->setWordWrap(true);
  m_warning->setStyleSheet(QStringLiteral("QLabel#printWarning { color: #92400e; background: #fef3c7; border: 1px solid #f59e0b; border-radius: 4px; padding: 6px 8px; font-weight: 700; }"));
  root->addWidget(m_warning);
  m_preview = new KaTilePreview(this);
  m_preview->onToggle = [this](int index) { toggleSheet(index); };
  root->addWidget(m_preview, 1);
  auto* pickRow = new QHBoxLayout;
  m_pickHint = new QLabel(QStringLiteral("미리보기에서 장을 누르면 이번 인쇄에서 빼거나 다시 넣습니다. "
                                         "잘못 나온 장만 다시 찍을 때 쓰세요."),
                          this);
  m_pickHint->setWordWrap(true);
  m_pickAll = new QPushButton(QStringLiteral("모두 다시 넣기"), this);
  m_pickAll->setObjectName(QStringLiteral("printPickAll"));
  pickRow->addWidget(m_pickHint, 1);
  pickRow->addWidget(m_pickAll);
  root->addLayout(pickRow);

  auto* buttons = new QHBoxLayout;
  m_savePdf = new QPushButton(QStringLiteral("나눈 장을 PDF로 저장…"), this);
  m_savePdf->setObjectName(QStringLiteral("printSaveTiles"));
  m_savePdf->setToolTip(QStringLiteral("프린터가 없는 PC나 출력소에서 찍을 수 있게 한 장이 한 쪽인 PDF로 저장합니다."));
  m_print = new QPushButton(this);
  m_print->setObjectName(QStringLiteral("printStart"));
  m_print->setDefault(true);
  auto* close = new QPushButton(QStringLiteral("닫기"), this);
  buttons->addWidget(m_savePdf);
  buttons->addStretch(1);
  buttons->addWidget(m_print);
  buttons->addWidget(close);
  root->addLayout(buttons);
  if (m_plate) arrangePlate(root, pickRow, paperRow);

  buildOutputs();
  loadSettings();
  // 용지 한 장에 실제 크기로 들어가면 한 장 인쇄, 넘치면 나눠 찍기로 연다.
  const TileFit probe = TilePrint::fit(m_drawingMm, sheetSize(), QMarginsF(kDefaultMarginMm, kDefaultMarginMm,
                                                                          kDefaultMarginMm, kDefaultMarginMm));
  (probe.actualSize ? m_modeFit : m_modeTiles)->setChecked(true);

  connect(m_modeTiles, &QRadioButton::toggled, this, [this]() { rebuild(); });
  connect(m_printer, &QComboBox::currentIndexChanged, this, [this]() { rebuild(); });
  connect(m_output, &QComboBox::currentIndexChanged, this, [this]() { rebuild(); });
  connect(m_sheet, &QComboBox::currentIndexChanged, this, [this]() { rebuild(); });
  connect(m_overlap, &QDoubleSpinBox::valueChanged, this, [this]() { rebuild(); });
  connect(m_marks, &QCheckBox::toggled, this, [this]() { rebuild(); });
  connect(m_overview, &QCheckBox::toggled, this, [this]() { rebuild(); });
  connect(m_pickAll, &QPushButton::clicked, this, [this]() {
    m_skipped.clear();
    rebuild();
  });
  connect(m_print, &QPushButton::clicked, this, &KaPrintDialog::printNow);
  connect(m_properties, &QPushButton::clicked, this, &KaPrintDialog::printNow);
  connect(m_savePdf, &QPushButton::clicked, this, &KaPrintDialog::saveTilesAs);
  connect(close, &QPushButton::clicked, this, &QDialog::reject);
  // 「조용한 도판」 large window, 860 × 640; stock keeps the size its contents ask for.
  if (const int large = KaWindowSize::width(KaWindowSize::Large, 0)) resize(large, 640);

  if (m_drawingMm.isEmpty()) {
    m_summary->setText(readError.isEmpty() ? QStringLiteral("도면을 읽지 못했습니다.") : readError);
    m_print->setText(QStringLiteral("인쇄"));
    m_print->setEnabled(false);
    m_properties->setEnabled(false);
    m_savePdf->setEnabled(false);
    refreshPlate();
    return;
  }
  rebuild();
}

void KaPrintDialog::buildOutputs() {
  m_output->clear();
  auto add = [this](const QString& label, const QString& kind, double factor, int paper, double denominator) {
    const QVariantMap item{{QStringLiteral("kind"), kind},
                           {QStringLiteral("factor"), factor},
                           {QStringLiteral("paper"), paper},
                           {QStringLiteral("denominator"), denominator}};
    m_output->addItem(label, item);
  };
  QString drawing = QStringLiteral("도면 크기 그대로");
  if (m_drawingScale > 0.0) drawing += QStringLiteral(" · ") + TilePrint::scaleLabel(m_drawingScale);
  drawing += QStringLiteral(" · ") + sizeText(m_drawingMm);
  add(drawing, QStringLiteral("drawing"), 1.0, OutputDrawing, m_drawingScale);
  // 발굴 도면은 딱 떨어지는 축척이 쓸모 있다. A0 보다 커지지 않는 범위에서만 보인다.
  const double a0 = TilePrint::fitScale(m_drawingMm, QPageSize(QPageSize::A0));
  for (double denominator : TilePrint::enlargedScales(m_drawingScale, a0)) {
    const double factor = m_drawingScale / denominator;
    add(QStringLiteral("%1으로 키우기 · %2배 · 약 %3")
            .arg(TilePrint::scaleLabel(denominator), decimal(factor), sizeText(m_drawingMm * factor)),
        QStringLiteral("scale"), factor, -1, denominator);
  }
  for (Output output : {OutputA0, OutputA1, OutputA2, OutputA3}) {
    const double factor = TilePrint::fitScale(m_drawingMm, outputPage(output));
    if (std::abs(factor - 1.0) < 0.01) continue;  // 도면과 같은 크기
    QString label = QStringLiteral("A%1에 맞춤 · ").arg(int(output) - 1);
    label += m_drawingScale > 0.0
        ? QStringLiteral("종이 위 약 %1").arg(TilePrint::scaleLabel(m_drawingScale / factor))
        : QStringLiteral("도면의 %1%").arg(percent(factor));
    add(label, QStringLiteral("paper"), factor, int(output), m_drawingScale > 0.0 ? m_drawingScale / factor : 0.0);
  }
}

void KaPrintDialog::setTiled(bool tiled) { (tiled ? m_modeTiles : m_modeFit)->setChecked(true); }

void KaPrintDialog::setOutput(Output output) {
  for (int i = 0; i < m_output->count(); ++i) {
    if (m_output->itemData(i).toMap().value(QStringLiteral("paper")).toInt() == int(output)) {
      m_output->setCurrentIndex(i);
      return;
    }
  }
  m_output->setCurrentIndex(0);  // 도면과 같은 크기라 따로 없다
}

bool KaPrintDialog::setOutputScale(double denominator) {
  for (int i = 0; i < m_output->count(); ++i) {
    const QVariantMap item = m_output->itemData(i).toMap();
    if (item.value(QStringLiteral("kind")).toString() == QLatin1String("scale") &&
        std::abs(item.value(QStringLiteral("denominator")).toDouble() - denominator) < 0.5) {
      m_output->setCurrentIndex(i);
      return true;
    }
  }
  return false;
}

void KaPrintDialog::setSheet(Sheet sheet) { m_sheet->setCurrentIndex(int(sheet)); }
void KaPrintDialog::setOverlapMm(double mm) { m_overlap->setValue(mm); }
void KaPrintDialog::setMarks(bool on) { m_marks->setChecked(on); }
void KaPrintDialog::setOverview(bool on) { m_overview->setChecked(on); }

void KaPrintDialog::setPrintDpi(double dpi) {
  const int index = m_dpi->findData(int(std::lround(dpi)));
  if (index >= 0) m_dpi->setCurrentIndex(index);
}

double KaPrintDialog::printDpi() const {
  const int dpi = m_dpi ? m_dpi->currentData().toInt() : 0;
  return dpi > 0 ? double(dpi) : TilePrint::kDefaultPrintDpi;
}
bool KaPrintDialog::isTiled() const { return m_modeTiles->isChecked(); }

void KaPrintDialog::toggleSheet(int index) {
  if (!isTiled() || !m_plan.ok || index < 0 || index >= m_plan.sheets.size()) return;
  if (!m_skipped.remove(index)) m_skipped.insert(index);
  // 하나도 안 남기면 찍을 것이 없다. 마지막 한 장은 빼지 않는다.
  if (m_skipped.size() >= m_plan.sheets.size()) m_skipped.remove(index);
  rebuild();
}

QList<int> KaPrintDialog::chosenSheets() const {
  QList<int> chosen;
  if (!m_plan.ok) return chosen;
  for (int i = 0; i < m_plan.sheets.size(); ++i)
    if (!m_skipped.contains(i)) chosen.append(i);
  return chosen;
}

int KaPrintDialog::pageCount() const {
  if (!isTiled()) return 1;
  const int sheets = int(chosenSheets().size());
  return sheets + (tileOptions().overview && sheets > 0 ? 1 : 0);
}

QString KaPrintDialog::printerName() const { return m_printer->currentData().toString(); }

QPageSize KaPrintDialog::sheetSize() const {
  return QPageSize(m_sheet->currentIndex() == SheetA3 ? QPageSize::A3 : QPageSize::A4);
}

bool KaPrintDialog::printerSupports(QPageSize::PageSizeId id) const {
  const QString name = printerName();
  if (name.isEmpty()) return true;
  const QList<QPageSize> sizes = QPrinterInfo::printerInfo(name).supportedPageSizes();
  return sizes.isEmpty() ||
         std::any_of(sizes.cbegin(), sizes.cend(), [id](const QPageSize& size) { return size.id() == id; });
}

QMarginsF KaPrintDialog::marginsFor(const QPageSize& sheet) const {
  QMarginsF margins(kDefaultMarginMm, kDefaultMarginMm, kDefaultMarginMm, kDefaultMarginMm);
  const QString name = printerName();
  if (name.isEmpty()) return margins;
  const QString key = name + QLatin1Char('|') + QString::number(int(sheet.id()));
  if (auto cached = m_marginCache.constFind(key); cached != m_marginCache.constEnd()) return *cached;
  // 프린터가 찍지 못하는 가장자리보다 좁게 잡으면 인쇄 때 줄어들어 축척이 틀어진다.
  QPrinter printer(QPrinterInfo::printerInfo(name), QPrinter::HighResolution);
  printer.setPageSize(sheet);
  QPageLayout layout = printer.pageLayout();
  layout.setUnits(QPageLayout::Millimeter);
  const QMarginsF minimum = layout.minimumMargins();
  margins = QMarginsF(std::max(margins.left(), minimum.left()), std::max(margins.top(), minimum.top()),
                      std::max(margins.right(), minimum.right()), std::max(margins.bottom(), minimum.bottom()));
  m_marginCache.insert(key, margins);
  return margins;
}

double KaPrintDialog::outputFactor() const {
  return m_output->currentData().toMap().value(QStringLiteral("factor"), 1.0).toDouble();
}

double KaPrintDialog::paperScale() const {
  if (m_drawingScale <= 0.0) return 0.0;
  if (isTiled()) return m_plan.ok ? m_drawingScale / m_plan.scale : 0.0;
  return m_drawingScale / m_fit.ratio;
}

QString KaPrintDialog::paperScaleText() const {
  const double denominator = paperScale();
  if (denominator <= 0.0) return {};
  const QString kind = m_output->currentData().toMap().value(QStringLiteral("kind")).toString();
  const bool exact = isTiled() ? kind != QLatin1String("paper") : m_fit.actualSize;
  return (exact ? QString() : QStringLiteral("약 ")) + TilePrint::scaleLabel(denominator);
}

TilePrint::Options KaPrintDialog::tileOptions() const {
  TilePrint::Options options;
  options.cutLines = m_marks->isChecked();
  // 빠진 장만 다시 찍을 때는 안내 장을 또 뽑지 않는다.
  options.overview = m_overview->isChecked() && m_skipped.isEmpty();
  options.title = m_title;
  options.scaleText = paperScaleText();
  if (!m_skipped.isEmpty()) options.sheets = chosenSheets();
  return options;
}

void KaPrintDialog::rebuild() {
  const bool tiled = isTiled();
  // 한 장 인쇄에서 쓸 수 없는 칸은 체크된 채 회색으로 남기지 않고 숨긴다.
  if (m_form) {
    m_form->setRowVisible(m_output, tiled);
    m_form->setRowVisible(m_checksRow, tiled);
  }
  m_overlapLabel->setVisible(tiled);
  m_overlap->setVisible(tiled);
  m_savePdf->setVisible(tiled);
  m_margins = marginsFor(sheetSize());
  m_fit = TilePrint::fit(m_drawingMm, sheetSize(), m_margins);
  m_plan = TilePlan();
  if (tiled) {
    TilePlanRequest request;
    request.drawingMm = m_drawingMm;
    request.sheet = sheetSize();
    request.printerMarginMm = m_margins;
    request.overlapMm = m_overlap->value();
    request.labelBandMm = m_marks->isChecked() ? kLabelBandMm : 0.0;
    request.scale = outputFactor();
    m_plan = TilePrint::bestPlan(request);
  }
  // 칸 모양이 바뀌면 뺀 장 번호가 다른 곳을 가리킨다. 처음부터 다시 고르게 한다.
  if (!m_plan.ok || m_plan.rows != m_gridRows || m_plan.cols != m_gridCols) {
    m_skipped.clear();
    m_gridRows = m_plan.rows;
    m_gridCols = m_plan.cols;
  }
  m_summary->setText(summary());
  const QString warn = warning();
  m_warning->setText(warn);
  m_warning->setVisible(!warn.isEmpty());
  m_preview->setContent(m_thumb, m_plan, m_skipped, tiled, m_drawingMm, sheetSize(), m_fit);
  const bool several = tiled && m_plan.ok && m_plan.sheets.size() > 1;
  m_pickHint->setVisible(several);
  m_pickAll->setVisible(several && !m_skipped.isEmpty());
  const bool canDraw = !m_drawingMm.isEmpty() && (!tiled || (m_plan.ok && pageCount() > 0));
  m_print->setText(printButtonText());
  const bool canPrint = canDraw && !printerName().isEmpty();
  m_print->setEnabled(canPrint);
  m_properties->setEnabled(canPrint);
  const QString printTip = printerName().isEmpty()
      ? QStringLiteral("설치된 프린터가 없습니다. 나눈 장을 PDF로 저장해 다른 PC나 출력소에서 찍으세요.")
      : QStringLiteral("프린터 속성이 열립니다. 양면으로 나오면 기본 설정에서 단면을 고르세요.");
  m_print->setToolTip(printTip);
  // Same flow as 「인쇄」: the properties window prints on 확인, 취소 prints nothing.
  m_properties->setToolTip(printerName().isEmpty()
      ? printTip
      : printTip + QStringLiteral(" 속성 창에서 「인쇄」를 누르면 바로 찍히고, 「취소」를 누르면 찍지 않습니다."));
  m_savePdf->setEnabled(tiled && canDraw);
  refreshPlate();
}

QString KaPrintDialog::summary() const {
  const QString sheetName = m_sheet->currentText();
  const QString scale = paperScaleText();
  if (!isTiled()) {
    QString text = m_fit.actualSize ? QStringLiteral("%1 한 장에 실제 크기(100%)로 찍습니다").arg(sheetName)
                                    : QStringLiteral("%1 한 장에 %2%로 줄여 찍습니다").arg(sheetName, percent(m_fit.ratio));
    if (!scale.isEmpty()) text += QStringLiteral(" · 종이 위 축척 ") + scale;
    return text;
  }
  if (!m_plan.ok) return m_plan.error;
  QString text = QStringLiteral("%1 용지 %2장 · %3로 놓고 가로 %4장 × 세로 %5장 · 붙이면 %6 × %7 cm")
                     .arg(sheetName)
                     .arg(m_plan.sheets.size())
                     .arg(m_plan.landscape ? QStringLiteral("가로") : QStringLiteral("세로"))
                     .arg(m_plan.cols)
                     .arg(m_plan.rows)
                     .arg(decimal(m_plan.outputMm.width() / 10.0), decimal(m_plan.outputMm.height() / 10.0));
  text += std::abs(m_plan.scale - 1.0) < 0.005 ? QStringLiteral(" · 도면 크기 그대로")
                                               : QStringLiteral(" · 도면의 %1%").arg(percent(m_plan.scale));
  if (!scale.isEmpty()) text += QStringLiteral("\n종이 위 축척 ") + scale;
  if (!m_skipped.isEmpty()) {
    text += QStringLiteral("\n이번에는 %1장만 찍습니다 (미리보기에서 뺀 장 %2장)")
                .arg(chosenSheets().size())
                .arg(m_skipped.size());
    if (m_overview->isChecked()) text += QStringLiteral(" · 안내 장은 다시 찍지 않습니다");
  }
  return text;
}

QString KaPrintDialog::warning() const {
  if (m_drawingMm.isEmpty()) return {};
  const QString drawnScale = m_drawingScale > 0.0 ? TilePrint::scaleLabel(m_drawingScale) : QString();
  if (!isTiled()) {
    if (m_fit.actualSize) {
      if (!m_fit.edgeMayClip) return {};
      const double edge = std::max({m_margins.left(), m_margins.top(), m_margins.right(), m_margins.bottom()});
      return QStringLiteral("종이 가장자리 %1 mm 안쪽은 프린터가 찍지 못해 비어 나올 수 있습니다. "
                            "도면 테두리가 그보다 안쪽이면 괜찮습니다.")
          .arg(decimal(edge));
    }
    const QString tail = QStringLiteral(" 실제 크기로 찍으려면 「여러 장으로 나눠 크게 인쇄」를 고르세요.");
    if (!drawnScale.isEmpty())
      return QStringLiteral("용지에 맞추느라 %1%로 줄입니다. 도면에 적힌 %2 대신 종이 위 축척은 %3입니다.")
                 .arg(percent(m_fit.ratio), drawnScale, paperScaleText()) + tail;
    return QStringLiteral("용지에 맞추느라 %1%로 줄입니다. 숫자로 적은 축척은 실제와 달라집니다.").arg(percent(m_fit.ratio)) +
           tail;
  }
  if (!m_plan.ok || std::abs(m_plan.scale - 1.0) < 0.005) return {};
  if (!drawnScale.isEmpty())
    return QStringLiteral("도면에 적힌 축척 글씨(%1)는 그대로 찍히지만 종이 위 실제 축척은 %2입니다. "
                          "각 장의 번호 띠에 실제 축척을 함께 찍습니다. 막대 축척은 그대로 맞습니다.")
        .arg(drawnScale, paperScaleText());
  return QStringLiteral("도면을 %1%로 %2 찍습니다. 숫자로 적은 축척(예: 1:5,000)은 실제와 달라지고, "
                        "막대 축척은 그대로 맞습니다.")
      .arg(percent(m_plan.scale), m_plan.scale > 1.0 ? QStringLiteral("키워") : QStringLiteral("줄여"));
}

QString KaPrintDialog::printButtonText() const {
  if (!isTiled()) return QStringLiteral("인쇄 · 1장");
  const int sheets = int(chosenSheets().size());
  return tileOptions().overview ? QStringLiteral("인쇄 · %1장 + 안내 1장").arg(sheets)
                                : QStringLiteral("인쇄 · %1장").arg(sheets);
}

QStringList KaPrintDialog::outputLabels() const {
  QStringList labels;
  for (int i = 0; i < m_output->count(); ++i) labels.append(m_output->itemText(i));
  return labels;
}

void KaPrintDialog::loadSettings() {
  QSettings settings;
  settings.beginGroup(kSettingsGroup);
  const QString printer = settings.value(QStringLiteral("printer")).toString();
  if (!printer.isEmpty()) {
    const int index = m_printer->findData(printer);
    if (index >= 0) m_printer->setCurrentIndex(index);
  }
  if (settings.contains(QStringLiteral("sheet"))) {
    const int sheet = settings.value(QStringLiteral("sheet")).toInt();
    if (sheet == SheetA4 || (sheet == SheetA3 && printerSupports(QPageSize::A3))) m_sheet->setCurrentIndex(sheet);
  }
  m_overlap->setValue(settings.value(QStringLiteral("overlap"), m_overlap->value()).toDouble());
  setPrintDpi(settings.value(QStringLiteral("dpi"), TilePrint::kDefaultPrintDpi).toDouble());
  m_marks->setChecked(settings.value(QStringLiteral("marks"), true).toBool());
  m_overview->setChecked(settings.value(QStringLiteral("overview"), true).toBool());
}

void KaPrintDialog::saveSettings() const {
  QSettings settings;
  settings.beginGroup(kSettingsGroup);
  if (!printerName().isEmpty()) settings.setValue(QStringLiteral("printer"), printerName());
  settings.setValue(QStringLiteral("sheet"), m_sheet->currentIndex());
  settings.setValue(QStringLiteral("overlap"), m_overlap->value());
  settings.setValue(QStringLiteral("dpi"), int(printDpi()));
  settings.setValue(QStringLiteral("marks"), m_marks->isChecked());
  settings.setValue(QStringLiteral("overview"), m_overview->isChecked());
}

bool KaPrintDialog::saveTilesPdf(const QString& path, QString* error, bool* cancelled) {
  if (!m_plan.ok) {
    if (error) *error = m_plan.error;
    return false;
  }
  QPdfWriter writer(path);
  writer.setResolution(int(printDpi()));
  writer.setTitle(m_title);
  writer.setCreator(QStringLiteral("ka-hgis"));
  return TilePrint::renderTiles(m_pdfPath, m_plan, &writer, printDpi(), tileOptions(), error, cancelled);
}

bool KaPrintDialog::renderWithProgress(QPagedPaintDevice* device, const QString& verb, QString* error,
                                       bool* cancelled) {
  QProgressDialog progress(verb + QStringLiteral("…"), QStringLiteral("멈추기"), 0, pageCount(), this);
  progress.setWindowTitle(windowTitle());
  progress.setWindowModality(Qt::WindowModal);
  progress.setMinimumDuration(0);
  progress.setValue(0);
  TilePrint::Options options = tileOptions();
  options.progress = [&progress, verb](int done, int total) {
    progress.setMaximum(total);
    progress.setLabelText(QStringLiteral("%1… %2 / %3쪽").arg(verb).arg(done).arg(total));
    progress.setValue(done);
    return !progress.wasCanceled();
  };
  return TilePrint::renderTiles(m_pdfPath, m_plan, device, printDpi(), options, error, cancelled);
}

void KaPrintDialog::saveTilesAs() {
  const QString base = m_title.trimmed().isEmpty() ? QStringLiteral("도면") : m_title.trimmed();
  const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("나눈 장 PDF 저장"),
                                                    base + QStringLiteral("-나눠찍기.pdf"),
                                                    QStringLiteral("PDF (*.pdf)"));
  if (path.isEmpty()) return;
  QString error;
  bool cancelled = false;
  bool ok = false;
  {
    QPdfWriter writer(path);
    writer.setResolution(int(printDpi()));
    writer.setTitle(m_title);
    writer.setCreator(QStringLiteral("ka-hgis"));
    ok = renderWithProgress(&writer, QStringLiteral("나눈 장을 PDF로 저장하는 중"), &error, &cancelled);
  }
  if (cancelled || !ok) {
    QFile::remove(path);  // 반쯤 쓴 파일은 남기지 않는다
    if (!cancelled) KaUserError::warn(this, {QStringLiteral("나눈 장 PDF"), QStringLiteral("나눈 장을 PDF로 저장하지 못했습니다"), error, {}});
    return;
  }
  saveSettings();
  QMessageBox::information(
      this, QStringLiteral("나눈 장 PDF"),
      QStringLiteral("%1쪽을 저장했습니다.\n%2\n\n다른 PC나 출력소에서 찍을 때는 꼭 「실제 크기(100%)」로 찍으세요. "
                     "「용지에 맞춤」으로 찍으면 축척이 틀어집니다. 각 장 아래 5 cm 확인선을 자로 재어 확인할 수 있습니다.")
          .arg(pageCount())
          .arg(path));
}

void KaPrintDialog::printNow() {
  const QString name = printerName();
  if (name.isEmpty()) return;
  if (!printerSupports(sheetSize().id())) {
    KaUserError::warn(this, {QStringLiteral("인쇄"), QStringLiteral("이 용지로는 인쇄할 수 없습니다"),
                             QStringLiteral("이 프린터는 %1 용지를 지원하지 않습니다.").arg(m_sheet->currentText()), QStringLiteral("다른 용지를 고르세요.")});
    return;
  }
  QPrinter printer(QPrinterInfo::printerInfo(name), QPrinter::HighResolution);
  printer.setDocName(m_title);
  printer.setFullPage(false);
  // 사무실 프린터 기본값이 양면인 경우가 많다. 속성 창은 단면으로 연다.
  printer.setDuplex(QPrinter::DuplexNone);
  // 용지와 방향을 속성 창 전에 넣는다. 그래야 속성 창도, 거기서 돌아온 설정도 같은 용지를 말한다.
  printer.setPageSize(sheetSize());
  const bool landscape = isTiled() ? m_plan.landscape : m_fit.landscape;
  printer.setPageOrientation(landscape ? QPageLayout::Landscape : QPageLayout::Portrait);
  QPrintDialog systemDialog(&printer, this);
  systemDialog.setWindowTitle(QStringLiteral("프린터 속성"));
  systemDialog.setOption(QPrintDialog::PrintToFile, false);
  systemDialog.setOption(QPrintDialog::PrintSelection, false);
  systemDialog.setOption(QPrintDialog::PrintPageRange, false);
  systemDialog.setOption(QPrintDialog::PrintCurrentPage, false);
  if (systemDialog.exec() != QDialog::Accepted) return;
  QString error;
  bool cancelled = false;
  bool ok = false;
  if (isTiled()) {
    ok = renderWithProgress(&printer, QStringLiteral("프린터로 보내는 중"), &error, &cancelled);
  } else {
    QApplication::setOverrideCursor(Qt::WaitCursor);
    ok = TilePrint::renderFit(m_pdfPath, sheetSize(), m_margins, &printer, printDpi(), &error);
    QApplication::restoreOverrideCursor();
  }
  if (cancelled) {
    QMessageBox::information(this, QStringLiteral("인쇄"),
                             QStringLiteral("인쇄를 멈췄습니다. 이미 넘어간 쪽은 프린터에 따라 나올 수 있습니다."));
    return;
  }
  if (!ok) {
    KaUserError::warn(this, {QStringLiteral("인쇄"), QStringLiteral("인쇄하지 못했습니다"), error, {}});
    return;
  }
  saveSettings();
  QString done = QStringLiteral("%1 %2 %3쪽을 프린터로 보냈습니다.")
                     .arg(m_sheet->currentText(), landscape ? QStringLiteral("가로") : QStringLiteral("세로"))
                     .arg(pageCount());
  if (isTiled() && m_plan.sheets.size() > 1)
    done += tileOptions().overview ? QStringLiteral("\n다 나오면 첫 장(붙이는 순서)을 보며 붙이세요.")
                                   : QStringLiteral("\n다 나오면 장 번호 순서대로 붙이세요.");
  QMessageBox::information(this, QStringLiteral("인쇄"), done);
  accept();
}
