#include "TilePrint.h"
#include "StandardScales.h"

#include <QFont>
#include <QFontMetricsF>
#include <QImage>
#include <QLocale>
#include <QPageLayout>
#include <QPagedPaintDevice>
#include <QPainter>
#include <QPdfDocument>
#include <QPdfDocumentRenderOptions>
#include <QPen>
#include <QPrinter>

#include <algorithm>
#include <cmath>

namespace {
constexpr double kMmPerInch = 25.4;
constexpr double kPointsPerMm = 72.0 / kMmPerInch;
// pdfium 에 한 번에 맡기는 띠 높이(px). A3 한 장을 300 DPI 로 통째로 그리면 70 MB 라 나눠 그린다.
constexpr int kBandPx = 1024;
// 각 장 아래의 크기 확인선. 자로 재어 이 길이면 프린터가 줄이거나 키우지 않은 것이다.
constexpr double kCheckBarMm = 50.0;

bool fail(QString* error, const QString& message) {
  if (error) *error = message;
  return false;
}

QSizeF portraitMm(const QPageSize& size) {
  const QSizeF s = size.size(QPageSize::Millimeter);
  return s.width() <= s.height() ? s : s.transposed();
}

// 첫 장은 content 만큼, 둘째 장부터는 앞 장과 overlap 만큼 겹치므로 content - overlap 씩 나아간다.
int sheetsAlong(double lengthMm, double contentMm, double overlapMm) {
  if (lengthMm <= contentMm + 1e-6) return 1;
  return int(std::ceil((lengthMm - overlapMm) / (contentMm - overlapMm) - 1e-9));
}

bool loadPdf(QPdfDocument& doc, const QString& pdfPath, QString* error) {
  if (doc.load(pdfPath) != QPdfDocument::Error::None || doc.pageCount() < 1)
    return fail(error, QStringLiteral("도면 PDF를 열지 못했습니다."));
  return true;
}

QSizeF pageMm(QPdfDocument& doc) {
  const QSizeF points = doc.pagePointSize(0);
  return QSizeF(points.width() / kPointsPerMm, points.height() / kPointsPerMm);
}

// 인쇄 창과 같게 84.0 이 아니라 84 로 적는다.
QString cm(double mm) {
  QString text = QLocale(QLocale::Korean).toString(mm / 10.0, 'f', 1);
  if (text.endsWith(QLatin1String(".0"))) text.chop(2);
  return text;
}

QFont fontMm(const QFont& base, double mm, double pxPerMm, bool bold = false) {
  QFont font = base;
  font.setPixelSize(std::max(6, int(std::lround(mm * pxPerMm))));
  font.setBold(bold);
  return font;
}

// 쪽 전체를 fullPx 로 키웠을 때 clipPx 부분만 그려 painter 의 target 에 놓는다.
bool drawRegion(QPdfDocument& doc, QPainter& painter, const QSize& fullPx, const QRect& clipPx,
                const QRectF& target, QString* error) {
  const double ky = target.height() / clipPx.height();
  for (int y = 0; y < clipPx.height(); y += kBandPx) {
    const int h = std::min(kBandPx, clipPx.height() - y);
    QPdfDocumentRenderOptions options;
    options.setScaledSize(fullPx);
    options.setScaledClipRect(QRect(clipPx.x(), clipPx.y() + y, clipPx.width(), h));
    const QImage band = doc.render(0, QSize(clipPx.width(), h), options);
    if (band.isNull())
      return fail(error, QStringLiteral("도면 그림을 만들지 못했습니다. 메모리가 부족할 수 있습니다."));
    painter.drawImage(QRectF(target.x(), target.y() + y * ky, target.width(), h * ky), band);
  }
  return true;
}

// A3 처럼 표준 용지면 그 이름, 아니면 29.7×42 cm.
QString paperName(const QPageSize& size) {
  const QSizeF mm = size.size(QPageSize::Millimeter);
  const QString iso = TilePrint::isoName(mm);
  return iso.isEmpty() ? QStringLiteral("%1×%2 cm").arg(cm(mm.width()), cm(mm.height())) : iso;
}

QString orientationName(bool landscape) {
  return landscape ? QStringLiteral("가로") : QStringLiteral("세로");
}

// 용지·방향·여백을 맞추고, 장치가 정말 그 용지로 바뀌었는지와 도면이 찍힐 칸이 들어가는지 확인한다.
bool preparePage(QPagedPaintDevice* device, const QPageSize& sheet, bool landscape,
                 const QMarginsF& marginMm, const QSizeF& needMm, QString* error) {
  const QPageLayout::Orientation orientation = landscape ? QPageLayout::Landscape : QPageLayout::Portrait;
  // 한 번에 넣는 setPageLayout 은 여백이 프린터 최소보다 좁으면 통째로 무시된다. 그러면 따로따로 넣는다.
  if (!device->setPageLayout(QPageLayout(sheet, orientation, marginMm, QPageLayout::Millimeter))) {
    device->setPageSize(sheet);
    device->setPageOrientation(orientation);
    device->setPageMargins(marginMm, QPageLayout::Millimeter);
  }
  const QPageLayout layout = device->pageLayout();
  const QSizeF want = portraitMm(sheet);
  const QSizeF have = portraitMm(layout.pageSize());
  const bool haveLandscape = layout.orientation() == QPageLayout::Landscape;
  if (std::abs(want.width() - have.width()) > 1.0 || std::abs(want.height() - have.height()) > 1.0 ||
      haveLandscape != landscape) {
    // 어떤 프린터 드라이버는 용지를 바꿔 달라는 요청을 받지 않고 자기 용지로 둔다(2026-10-28, A3 9장이 A4로 나옴).
    // 그 종이에 잘라 찍으면 붙였을 때 축척이 틀어지니 찍지 않고 말한다.
    return fail(error, QStringLiteral("프린터가 %1 %2 용지로 바꾸지 않고 %3 %4로 두었습니다. "
                                      "「프린터 속성」의 기본 설정에서 용지를 %1로 고른 뒤 다시 인쇄하세요.")
                           .arg(paperName(sheet), orientationName(landscape), paperName(layout.pageSize()),
                                orientationName(haveLandscape)));
  }
  const QRectF paint = layout.paintRect(QPageLayout::Millimeter);
  if (paint.width() + 0.5 < needMm.width() || paint.height() + 0.5 < needMm.height()) {
    // 줄여 찍으면 붙였을 때 축척이 틀어진다. 여백을 다시 잡도록 멈춘다.
    return fail(error, QStringLiteral("프린터가 여백을 더 크게 잡습니다. 인쇄 창을 닫았다가 다시 열어 주세요."));
  }
  return true;
}

QString sheetName(const TilePlan& plan) {
  const QString iso = TilePrint::isoName(plan.sheet.size(QPageSize::Millimeter));
  return iso.isEmpty() ? QStringLiteral("용지") : iso;
}

// 자르는 점선(오른쪽·아래 이웃이 시작하는 곳)과 앞 장을 맞출 시작선.
void drawCutMarks(QPainter& painter, const TilePlan& plan, const TileSheet& tile, double d) {
  const double cw = plan.contentMm.width() * d;
  const double ch = plan.contentMm.height() * d;
  QPen cut(QColor(80, 80, 80));
  cut.setWidthF(0.25 * d);
  cut.setDashPattern({8.0, 6.0});
  painter.setPen(cut);
  if (tile.col + 1 < plan.cols) {
    const double x = (plan.contentMm.width() - plan.overlapMm) * d;
    painter.drawLine(QPointF(x, 0), QPointF(x, ch));
  }
  if (tile.row + 1 < plan.rows) {
    const double y = (plan.contentMm.height() - plan.overlapMm) * d;
    painter.drawLine(QPointF(0, y), QPointF(cw, y));
  }
  // 도면이 하얀 곳이어도 보이게 모서리에 짧게 찍는다.
  QPen tick(QColor(80, 80, 80));
  tick.setWidthF(0.3 * d);
  painter.setPen(tick);
  const double len = 5.0 * d;
  if (tile.col > 0) {
    painter.drawLine(QPointF(0, 0), QPointF(0, len));
    painter.drawLine(QPointF(0, ch - len), QPointF(0, ch));
  }
  if (tile.row > 0) {
    painter.drawLine(QPointF(0, 0), QPointF(len, 0));
    painter.drawLine(QPointF(cw - len, 0), QPointF(cw, 0));
  }
}

// 아래 띠: 전체에서 이 장의 자리(작은 칸 지도), 장 번호·종이 위 축척, 5 cm 확인선.
void drawBand(QPainter& painter, const TilePlan& plan, int index, const TilePrint::Options& options, double d) {
  const double cw = plan.contentMm.width() * d;
  const double top = (plan.contentMm.height() + 0.6) * d;
  const double h = (plan.labelBandMm - 0.6) * d;
  if (h <= 0.0) return;
  const TileSheet& tile = plan.sheets.at(index);
  const double cell = std::min({1.4 * d, (h - 0.4 * d) / plan.rows, 30.0 * d / plan.cols});
  const double mapTop = top + (h - cell * plan.rows) / 2.0;
  painter.setPen(QPen(QColor(90, 90, 90), std::max(1.0, 0.12 * d)));
  for (int r = 0; r < plan.rows; ++r) {
    for (int c = 0; c < plan.cols; ++c) {
      painter.setBrush(r == tile.row && c == tile.col ? QBrush(QColor(40, 40, 40)) : QBrush(Qt::NoBrush));
      painter.drawRect(QRectF(c * cell, mapTop + r * cell, cell, cell));
    }
  }
  painter.setBrush(Qt::NoBrush);
  const double mapW = plan.cols * cell;
  double textRight = cw;
  const double bar = kCheckBarMm * d;
  if (cw - mapW > bar + 70.0 * d) {
    const double x0 = cw - bar;
    const double y = top + h * 0.72;
    painter.setPen(QPen(Qt::black, 0.25 * d));
    painter.drawLine(QPointF(x0, y), QPointF(cw, y));
    painter.drawLine(QPointF(x0, y - 1.0 * d), QPointF(x0, y + 1.0 * d));
    painter.drawLine(QPointF(cw, y - 1.0 * d), QPointF(cw, y + 1.0 * d));
    painter.setFont(fontMm(painter.font(), 2.2, d));
    painter.drawText(QRectF(x0, top, bar, h * 0.55), Qt::AlignCenter, QStringLiteral("5 cm 확인선"));
    textRight = x0 - 3.0 * d;
  }
  const QFont font = fontMm(painter.font(), 2.6, d);
  painter.setFont(font);
  painter.setPen(QColor(50, 50, 50));
  QString text = QStringLiteral("%1번 장 (%2행 %3열) / 전체 %4장")
                     .arg(index + 1)
                     .arg(tile.row + 1)
                     .arg(tile.col + 1)
                     .arg(plan.sheets.size());
  if (!options.scaleText.isEmpty()) text += QStringLiteral(" · 종이 위 축척 ") + options.scaleText;
  if (!options.title.trimmed().isEmpty()) text += QStringLiteral(" · ") + options.title.trimmed();
  const QRectF textRect(mapW + 2.0 * d, top, std::max(0.0, textRight - mapW - 2.0 * d), h);
  painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                   QFontMetricsF(font).elidedText(text, Qt::ElideRight, textRect.width()));
}

// 맨 앞 안내 장: 도면 전체에 나눈 칸과 번호를 그리고 붙이는 순서를 적는다.
bool drawOverview(QPdfDocument& doc, QPainter& painter, const TilePlan& plan, const TilePrint::Options& options,
                  const QSizeF& paintMm, double d, QString* error) {
  const double w = paintMm.width() * d;
  const double pageH = paintMm.height() * d;
  const QString name = options.title.trimmed().isEmpty() ? QStringLiteral("도면") : options.title.trimmed();
  painter.setPen(Qt::black);
  painter.setFont(fontMm(painter.font(), 6.0, d, true));
  painter.drawText(QRectF(0, 0, w, 9 * d), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                   QStringLiteral("%1 — 붙이는 순서").arg(name));
  const QFont body = fontMm(painter.font(), 3.4, d);
  painter.setFont(body);
  QString line = QStringLiteral("%1 용지 %2장 (%3로 놓고 가로 %4장 × 세로 %5장) · 붙이면 %6 × %7 cm")
                     .arg(sheetName(plan))
                     .arg(plan.sheets.size())
                     .arg(plan.landscape ? QStringLiteral("가로") : QStringLiteral("세로"))
                     .arg(plan.cols)
                     .arg(plan.rows)
                     .arg(cm(plan.outputMm.width()), cm(plan.outputMm.height()));
  if (!options.scaleText.isEmpty()) line += QStringLiteral(" · 종이 위 축척 ") + options.scaleText;
  const QRectF lineRect(0, 10 * d, w, 6 * d);
  painter.drawText(lineRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                   QFontMetricsF(body).elidedText(line, Qt::ElideRight, lineRect.width()));

  const QStringList steps = {
      QStringLiteral("1. 번호 순서대로 늘어놓습니다. 1번이 왼쪽 위입니다."),
      QStringLiteral("2. 각 장의 오른쪽·아래 점선을 따라 자릅니다. 맨 오른쪽 줄과 맨 아래 줄은 그쪽을 자르지 않습니다."),
      QStringLiteral("3. 자른 가장자리를 옆 장 그림의 시작선(모서리의 짧은 선)에 맞춰 겹쳐 붙입니다."),
      QStringLiteral("4. 각 장 아래의 확인선을 자로 재어 5 cm면 크기가 맞게 찍힌 것입니다.")};
  const double lineH = 5.4 * d;
  const double stepsTop = pageH - steps.size() * lineH;
  const QRectF box(0, 18 * d, w, stepsTop - 22 * d);
  if (box.height() > 10 * d) {
    QRectF extent(QPointF(0, 0), plan.outputMm);
    for (const TileSheet& tile : plan.sheets) extent = extent.united(tile.areaMm);
    const double k = std::min(box.width() / extent.width(), box.height() / extent.height());
    const QPointF origin(box.center().x() - extent.width() * k / 2.0, box.top());
    auto map = [&](const QRectF& mm) {
      return QRectF(origin.x() + mm.x() * k, origin.y() + mm.y() * k, mm.width() * k, mm.height() * k);
    };
    const QRectF drawing = map(QRectF(QPointF(0, 0), plan.outputMm));
    // 안내 그림은 한 번만 그린다. 긴 변 2400 px 이면 종이에서도 충분히 또렷하다.
    const double cap = 2400.0 / std::max(drawing.width(), drawing.height());
    const QSize imageSize = (drawing.size() * std::min(1.0, cap)).toSize();
    const QImage image = doc.render(0, imageSize.expandedTo(QSize(1, 1)));
    if (image.isNull()) return fail(error, QStringLiteral("안내 장의 도면 그림을 만들지 못했습니다."));
    painter.drawImage(drawing, image);
    for (int i = 0; i < plan.sheets.size(); ++i) {
      const QRectF r = map(plan.sheets.at(i).areaMm);
      painter.setPen(QPen(QColor(37, 99, 235), 0.4 * d));
      painter.setBrush(Qt::NoBrush);
      painter.drawRect(r);
      const double numberMm = std::clamp(std::min(r.width(), r.height()) / d * 0.3, 4.0, 18.0);
      const QFont numberFont = fontMm(painter.font(), numberMm, d, true);
      const QString number = QString::number(i + 1);
      const QSizeF size = QFontMetricsF(numberFont).size(Qt::TextSingleLine, number) + QSizeF(2.4 * d, 0.8 * d);
      const QRectF chip(r.center().x() - size.width() / 2.0, r.center().y() - size.height() / 2.0,
                        size.width(), size.height());
      painter.setPen(Qt::NoPen);
      painter.setBrush(QColor(255, 255, 255, 220));
      painter.drawRoundedRect(chip, 1.2 * d, 1.2 * d);
      painter.setFont(numberFont);
      painter.setPen(QColor(30, 64, 175));
      painter.drawText(chip, Qt::AlignCenter, number);
    }
    painter.setBrush(Qt::NoBrush);
  }
  painter.setPen(Qt::black);
  painter.setFont(body);
  for (int i = 0; i < steps.size(); ++i) {
    const QRectF stepRect(0, stepsTop + i * lineH, w, lineH);
    painter.drawText(stepRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                     QFontMetricsF(body).elidedText(steps.at(i), Qt::ElideRight, stepRect.width()));
  }
  return true;
}
}  // namespace

double TilePrint::fitScale(const QSizeF& drawingMm, const QPageSize& target) {
  if (drawingMm.isEmpty() || !target.isValid()) return 1.0;
  QSizeF t = portraitMm(target);
  if (drawingMm.width() > drawingMm.height()) t.transpose();
  return std::min(t.width() / drawingMm.width(), t.height() / drawingMm.height());
}

TilePlan TilePrint::plan(const TilePlanRequest& request, bool landscapeSheet) {
  TilePlan p;
  p.sheet = request.sheet;
  p.landscape = landscapeSheet;
  p.printerMarginMm = request.printerMarginMm;
  p.scale = request.scale;
  p.overlapMm = std::max(0.0, request.overlapMm);
  p.labelBandMm = std::max(0.0, request.labelBandMm);
  if (request.drawingMm.isEmpty() || request.scale <= 0.0 || !request.sheet.isValid()) {
    p.error = QStringLiteral("도면이나 용지 크기를 알 수 없습니다.");
    return p;
  }
  p.outputMm = request.drawingMm * request.scale;
  QSizeF sheet = portraitMm(request.sheet);
  if (landscapeSheet) sheet.transpose();
  const QMarginsF& m = request.printerMarginMm;
  p.contentMm = QSizeF(sheet.width() - m.left() - m.right(),
                       sheet.height() - m.top() - m.bottom() - p.labelBandMm);
  if (p.contentMm.width() <= p.overlapMm + 1.0 || p.contentMm.height() <= p.overlapMm + 1.0) {
    p.error = QStringLiteral("겹침과 여백이 용지보다 큽니다. 겹침을 줄여 주세요.");
    return p;
  }
  p.cols = sheetsAlong(p.outputMm.width(), p.contentMm.width(), p.overlapMm);
  p.rows = sheetsAlong(p.outputMm.height(), p.contentMm.height(), p.overlapMm);
  const double stepW = p.contentMm.width() - p.overlapMm;
  const double stepH = p.contentMm.height() - p.overlapMm;
  for (int row = 0; row < p.rows; ++row) {
    for (int col = 0; col < p.cols; ++col) {
      TileSheet tile;
      tile.row = row;
      tile.col = col;
      tile.areaMm = QRectF(col * stepW, row * stepH, p.contentMm.width(), p.contentMm.height());
      p.sheets.append(tile);
    }
  }
  p.ok = true;
  return p;
}

TilePlan TilePrint::bestPlan(const TilePlanRequest& request) {
  const TilePlan portrait = plan(request, false);
  const TilePlan landscape = plan(request, true);
  if (!landscape.ok) return portrait;
  if (!portrait.ok) return landscape;
  return landscape.sheets.size() < portrait.sheets.size() ? landscape : portrait;
}

TileFit TilePrint::fit(const QSizeF& drawingMm, const QPageSize& sheet, const QMarginsF& m) {
  TileFit f;
  if (drawingMm.isEmpty() || !sheet.isValid()) return f;
  f.landscape = drawingMm.width() > drawingMm.height();
  QSizeF paper = portraitMm(sheet);
  if (f.landscape) paper.transpose();
  if (drawingMm.width() <= paper.width() + 0.5 && drawingMm.height() <= paper.height() + 0.5) {
    // 용지 한가운데에 실제 크기로 놓는다. 프린터가 못 찍는 가장자리에 걸치는지만 알린다.
    const double left = (paper.width() - drawingMm.width()) / 2.0;
    const double top = (paper.height() - drawingMm.height()) / 2.0;
    f.edgeMayClip = left + 0.01 < std::max(m.left(), m.right()) || top + 0.01 < std::max(m.top(), m.bottom());
    return f;
  }
  f.actualSize = false;
  f.ratio = std::min((paper.width() - m.left() - m.right()) / drawingMm.width(),
                     (paper.height() - m.top() - m.bottom()) / drawingMm.height());
  return f;
}

QSizeF TilePrint::pdfPageSizeMm(const QString& pdfPath, QString* error, QImage* thumbnail, int thumbnailLongPx) {
  QPdfDocument doc;
  if (!loadPdf(doc, pdfPath, error)) return {};
  const QSizeF mm = pageMm(doc);
  if (thumbnail && thumbnailLongPx > 0 && !mm.isEmpty()) {
    const double k = thumbnailLongPx / std::max(mm.width(), mm.height());
    *thumbnail = doc.render(0, QSize(int(mm.width() * k), int(mm.height() * k)));
  }
  return mm;
}

QList<double> TilePrint::enlargedScales(double drawingDenominator, double maxFactor, int limit) {
  // 발굴·지적 도면에서 자주 쓰는 축척(StandardScales 한 표). 큰 분모부터 두어 배율이 작은 것이 먼저 나온다.
  const QList<int> ascending = StandardScales::denominators(StandardScales::PrintEnlarge);
  QList<double> out;
  if (drawingDenominator <= 0.0 || maxFactor <= 1.0) return out;
  for (auto it = ascending.crbegin(); it != ascending.crend(); ++it) {
    const double d = double(*it);
    if (d >= drawingDenominator * (1.0 - 1e-6)) continue;
    if (drawingDenominator / d > maxFactor * (1.0 + 1e-9)) break;
    out.append(d);
    if (out.size() >= limit) break;
  }
  return out;
}

QList<int> TilePrint::printDpiChoices() {
  return {300, 600};
}

QString TilePrint::scaleLabel(double denominator) {
  return StandardScales::label(denominator);
}

QString TilePrint::isoName(const QSizeF& mm) {
  if (mm.isEmpty()) return {};
  const double shortSide = std::min(mm.width(), mm.height());
  const double longSide = std::max(mm.width(), mm.height());
  const QPageSize::PageSizeId ids[] = {QPageSize::A0, QPageSize::A1, QPageSize::A2, QPageSize::A3, QPageSize::A4};
  for (int i = 0; i < 5; ++i) {
    const QSizeF iso = portraitMm(QPageSize(ids[i]));
    if (std::abs(shortSide - iso.width()) <= iso.width() * 0.03 &&
        std::abs(longSide - iso.height()) <= iso.height() * 0.03)
      return QStringLiteral("A%1").arg(i);
  }
  return {};
}

bool TilePrint::renderTiles(const QString& pdfPath, const TilePlan& plan, QPagedPaintDevice* device,
                            double dpi, const Options& options, QString* error, bool* cancelled) {
  if (cancelled) *cancelled = false;
  if (!device) return fail(error, QStringLiteral("인쇄할 곳이 없습니다."));
  if (!plan.ok || plan.sheets.isEmpty())
    return fail(error, plan.error.isEmpty() ? QStringLiteral("나눠 찍을 계획이 없습니다.") : plan.error);
  QList<int> order;
  const int count = int(plan.sheets.size());
  if (options.sheets.isEmpty()) {
    for (int i = 0; i < count; ++i) order.append(i);
  } else {
    for (int i : options.sheets)
      if (i >= 0 && i < count && !order.contains(i)) order.append(i);
    std::sort(order.begin(), order.end());
  }
  if (order.isEmpty() && !options.overview) return fail(error, QStringLiteral("찍을 장을 하나 이상 고르세요."));
  QPdfDocument doc;
  if (!loadPdf(doc, pdfPath, error)) return false;
  const QSizeF need(plan.contentMm.width(), plan.contentMm.height() + plan.labelBandMm);
  if (!preparePage(device, plan.sheet, plan.landscape, plan.printerMarginMm, need, error)) return false;
  const QSizeF paintMm = device->pageLayout().paintRect(QPageLayout::Millimeter).size();

  QPainter painter;
  if (!painter.begin(device)) return fail(error, QStringLiteral("프린터를 시작하지 못했습니다."));
  painter.setRenderHint(QPainter::SmoothPixmapTransform);
  painter.setRenderHint(QPainter::Antialiasing);
  const double d = device->logicalDpiX() / kMmPerInch;                        // 종이 px/mm
  const double r = std::min(dpi, double(device->logicalDpiX())) / kMmPerInch;  // 그림 px/mm
  const QSize fullPx(qRound(plan.outputMm.width() * r), qRound(plan.outputMm.height() * r));
  const QRect fullRect(QPoint(0, 0), fullPx);
  const int total = int(order.size()) + (options.overview ? 1 : 0);
  int done = 0;
  bool first = true;
  auto finish = [&](const QString& message) {
    if (painter.isActive()) painter.end();
    return fail(error, message);
  };
  auto nextPage = [&]() {
    if (first) {
      first = false;
      return true;
    }
    return device->newPage();
  };
  // 한 쪽을 끝낼 때마다 묻는다. 멈추면 프린터 작업도 거둔다.
  auto stopped = [&]() {
    ++done;
    if (!options.progress || options.progress(done, total)) return false;
    if (auto* printer = dynamic_cast<QPrinter*>(device)) printer->abort();
    if (cancelled) *cancelled = true;
    return true;
  };

  if (options.overview) {
    if (!nextPage()) return finish(QStringLiteral("다음 장으로 넘기지 못했습니다."));
    if (!drawOverview(doc, painter, plan, options, paintMm, d, error)) {
      const QString message = error ? *error : QString();
      return finish(message);
    }
    if (stopped()) return finish(QStringLiteral("인쇄를 멈췄습니다."));
  }
  for (int index : order) {
    if (!nextPage()) return finish(QStringLiteral("다음 장으로 넘기지 못했습니다."));
    const TileSheet& tile = plan.sheets.at(index);
    const QRectF area = tile.areaMm;
    // 이웃 장과 경계가 1 px 도 어긋나지 않게 그림 px 칸을 먼저 정하고 그 칸을 종이에 옮긴다.
    const QRect clip = QRect(QPoint(int(std::floor(area.left() * r)), int(std::floor(area.top() * r))),
                             QPoint(int(std::ceil(area.right() * r)) - 1,
                                    int(std::ceil(area.bottom() * r)) - 1)) & fullRect;
    if (!clip.isEmpty()) {
      const QRectF target((clip.x() / r - area.x()) * d, (clip.y() / r - area.y()) * d,
                          clip.width() / r * d, clip.height() / r * d);
      if (!drawRegion(doc, painter, fullPx, clip, target, error)) {
        const QString message = error ? *error : QString();
        return finish(message);
      }
    }
    if (options.cutLines) drawCutMarks(painter, plan, tile, d);
    if (plan.labelBandMm > 0.0) drawBand(painter, plan, index, options, d);
    if (stopped()) return finish(QStringLiteral("인쇄를 멈췄습니다."));
  }
  painter.end();
  return true;
}

bool TilePrint::renderFit(const QString& pdfPath, const QPageSize& sheet, const QMarginsF& printerMarginMm,
                          QPagedPaintDevice* device, double dpi, QString* error) {
  if (!device) return fail(error, QStringLiteral("인쇄할 곳이 없습니다."));
  QPdfDocument doc;
  if (!loadPdf(doc, pdfPath, error)) return false;
  const QSizeF drawing = pageMm(doc);
  if (drawing.isEmpty()) return fail(error, QStringLiteral("도면 크기를 알 수 없습니다."));
  const TileFit f = fit(drawing, sheet, printerMarginMm);
  if (!preparePage(device, sheet, f.landscape, printerMarginMm, QSizeF(1, 1), error)) return false;
  const QPageLayout layout = device->pageLayout();
  const QRectF paint = layout.paintRect(QPageLayout::Millimeter);
  const QRectF page = layout.fullRect(QPageLayout::Millimeter);
  // 실제 크기면 종이 한가운데, 줄이면 인쇄 가능 영역 한가운데. painter 원점은 인쇄 가능 영역 왼쪽 위다.
  const double ratio = f.actualSize ? 1.0 : std::min(paint.width() / drawing.width(), paint.height() / drawing.height());
  const QSizeF outMm = drawing * ratio;
  const QPointF originMm = f.actualSize
      ? QPointF((page.width() - outMm.width()) / 2.0 - paint.left(), (page.height() - outMm.height()) / 2.0 - paint.top())
      : QPointF((paint.width() - outMm.width()) / 2.0, (paint.height() - outMm.height()) / 2.0);
  QPainter painter;
  if (!painter.begin(device)) return fail(error, QStringLiteral("프린터를 시작하지 못했습니다."));
  painter.setRenderHint(QPainter::SmoothPixmapTransform);
  const double d = device->logicalDpiX() / kMmPerInch;
  const double r = std::min(dpi, double(device->logicalDpiX())) / kMmPerInch;
  const QSize fullPx(qRound(outMm.width() * r), qRound(outMm.height() * r));
  const QRectF target(originMm.x() * d, originMm.y() * d, outMm.width() * d, outMm.height() * d);
  const bool ok = drawRegion(doc, painter, fullPx, QRect(QPoint(0, 0), fullPx), target, error);
  painter.end();
  return ok;
}
