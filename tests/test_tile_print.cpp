#include <QtTest>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFileInfo>
#include <QPageSize>
#include <QPainter>
#include <QPdfDocument>
#include <QPdfWriter>
#include <QPushButton>
#include <QTemporaryDir>

#include "app/KaPrintDialog.h"
#include "core/StandardScales.h"
#include "core/TilePrint.h"

namespace {
constexpr double kPointsPerMm = 72.0 / 25.4;

// 네 모서리 칸이 다른 색인 도면 PDF(widthMm × heightMm).
QString makeQuadrantPdf(const QString& path, const QSizeF& mm) {
  QPdfWriter writer(path);
  writer.setResolution(72);
  // QPageSize keeps sizes portrait; a wide drawing is a portrait size turned landscape.
  const QSizeF portrait(qMin(mm.width(), mm.height()), qMax(mm.width(), mm.height()));
  writer.setPageLayout(QPageLayout(QPageSize(portrait, QPageSize::Millimeter),
                                   mm.width() > mm.height() ? QPageLayout::Landscape : QPageLayout::Portrait,
                                   QMarginsF(0, 0, 0, 0), QPageLayout::Millimeter));
  QPainter painter(&writer);
  const double w = writer.width();
  const double h = writer.height();
  painter.fillRect(QRectF(0, 0, w / 2, h / 2), QColor(220, 20, 20));        // 왼쪽 위 빨강
  painter.fillRect(QRectF(w / 2, 0, w / 2, h / 2), QColor(20, 170, 40));    // 오른쪽 위 초록
  painter.fillRect(QRectF(0, h / 2, w / 2, h / 2), QColor(20, 40, 220));    // 왼쪽 아래 파랑
  painter.fillRect(QRectF(w / 2, h / 2, w / 2, h / 2), QColor(240, 210, 20));  // 오른쪽 아래 노랑
  painter.end();
  return path;
}

// 출력 PDF 쪽을 그려 종이 위치(mm)의 색을 읽는다.
QColor colorAtMm(QPdfDocument& doc, int page, const QPointF& mm) {
  const QSizeF points = doc.pagePointSize(page);
  const double pxPerMm = 2.0;
  const QSize size(qRound(points.width() / kPointsPerMm * pxPerMm), qRound(points.height() / kPointsPerMm * pxPerMm));
  const QImage image = doc.render(page, size).convertToFormat(QImage::Format_ARGB32);
  return image.pixelColor(qRound(mm.x() * pxPerMm), qRound(mm.y() * pxPerMm));
}

bool near(const QColor& c, int r, int g, int b) {
  return c.alpha() > 200 && qAbs(c.red() - r) < 40 && qAbs(c.green() - g) < 40 && qAbs(c.blue() - b) < 40;
}

bool paper(const QColor& c) {
  return c.alpha() < 30 || (c.red() > 230 && c.green() > 230 && c.blue() > 230);
}

// 용지 요청을 무시하고 자기 용지(A4)를 고집하는 장치. 어떤 프린터 드라이버가 이렇게 군다.
class StubbornA4Writer : public QPdfWriter {
public:
  using QPdfWriter::QPdfWriter;
  bool setPageLayout(const QPageLayout&) override { return false; }
  bool setPageSize(const QPageSize&) override { return false; }
  bool setPageOrientation(QPageLayout::Orientation) override { return false; }
  bool setPageMargins(const QMarginsF&, QPageLayout::Unit) override { return false; }
};

TilePlanRequest request(const QSizeF& drawingMm, QPageSize::PageSizeId sheet, double labelBandMm) {
  TilePlanRequest r;
  r.drawingMm = drawingMm;
  r.sheet = QPageSize(sheet);
  r.printerMarginMm = QMarginsF(5, 5, 5, 5);
  r.overlapMm = 10.0;
  r.labelBandMm = labelBandMm;
  return r;
}
}  // namespace

class TestTilePrint : public QObject {
  Q_OBJECT
private slots:
  // 2026-10-28 현장: A0를 A3 9장으로 나눠 찍는데 A4가 나왔다. 장치가 요청한 용지를 받지 않으면
  // 엉뚱한 종이에 찍지 말고, 어느 용지를 달라 했고 장치가 어느 용지로 두었는지 말하고 멈춘다.
  void tilesStopWhenTheDeviceKeepsAnotherPaper() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString source = makeQuadrantPdf(dir.filePath(QStringLiteral("a0.pdf")), QSizeF(841, 1189));
    const TilePlan plan = TilePrint::bestPlan(request(QSizeF(841, 1189), QPageSize::A3, 6.0));
    QVERIFY(plan.ok);
    StubbornA4Writer writer(dir.filePath(QStringLiteral("stubborn.pdf")));
    writer.setResolution(100);
    QString error;
    QVERIFY(!TilePrint::renderTiles(source, plan, &writer, 100.0, TilePrint::Options(), &error));
    QVERIFY2(error.contains(QStringLiteral("A3")) && error.contains(QStringLiteral("A4")), qPrintable(error));
    // 한 장 인쇄도 같다.
    error.clear();
    StubbornA4Writer one(dir.filePath(QStringLiteral("stubborn-one.pdf")));
    QVERIFY(!TilePrint::renderFit(source, QPageSize(QPageSize::A3), QMarginsF(5, 5, 5, 5), &one, 100.0, &error));
    QVERIFY2(error.contains(QStringLiteral("A3")) && error.contains(QStringLiteral("A4")), qPrintable(error));
  }

  void a0DrawingNeedsNineA3Sheets() {
    // A3 한 장: 가장자리 5 mm, 번호 띠 6 mm → 도면 칸 287×404 mm, 겹침 10 mm.
    const TilePlan plan = TilePrint::bestPlan(request(QSizeF(841, 1189), QPageSize::A3, 6.0));
    QVERIFY2(plan.ok, qPrintable(plan.error));
    QVERIFY(!plan.landscape);
    QCOMPARE(plan.cols, 3);
    QCOMPARE(plan.rows, 3);
    QCOMPARE(plan.sheets.size(), 9);
    QCOMPARE(plan.contentMm, QSizeF(287, 404));
    QCOMPARE(plan.sheets.first().areaMm, QRectF(0, 0, 287, 404));
    // 이웃 장은 겹침만큼 앞 장 끝을 다시 찍는다.
    QCOMPARE(plan.sheets.at(1).areaMm.left(), 277.0);
    QCOMPARE(plan.sheets.at(3).areaMm.top(), 394.0);
    const QRectF last = plan.sheets.last().areaMm;
    QVERIFY2(last.right() >= 841.0 && last.bottom() >= 1189.0, "끝 장이 도면 끝까지 닿지 않습니다.");
  }

  void a3DrawingEnlargedToA0KeepsAspect() {
    const QSizeF a3(297, 420);
    const double scale = TilePrint::fitScale(a3, QPageSize(QPageSize::A0));
    QVERIFY(qAbs(scale - 1189.0 / 420.0) < 0.001);
    QVERIFY(qAbs(TilePrint::fitScale(a3.transposed(), QPageSize(QPageSize::A0)) - scale) < 1e-9);
    TilePlanRequest r = request(a3, QPageSize::A3, 6.0);
    r.scale = scale;
    const TilePlan plan = TilePrint::bestPlan(r);
    QVERIFY2(plan.ok, qPrintable(plan.error));
    QCOMPARE(plan.sheets.size(), 9);
    QVERIFY(plan.outputMm.width() <= 841.0 + 0.01 && plan.outputMm.height() <= 1189.0 + 0.01);
  }

  void smallDrawingStaysOnOneSheet() {
    const TilePlan plan = TilePrint::bestPlan(request(QSizeF(210, 297), QPageSize::A3, 6.0));
    QVERIFY2(plan.ok, qPrintable(plan.error));
    QCOMPARE(plan.sheets.size(), 1);
  }

  void overlapWiderThanSheetIsRefused() {
    TilePlanRequest r = request(QSizeF(841, 1189), QPageSize::A4, 0.0);
    r.overlapMm = 250.0;
    const TilePlan plan = TilePrint::bestPlan(r);
    QVERIFY(!plan.ok);
    QVERIFY(!plan.error.isEmpty());
  }

  void tilesPutEachPartOfTheDrawingOnItsOwnPage() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString source = makeQuadrantPdf(dir.filePath(QStringLiteral("drawing.pdf")), QSizeF(400, 300));
    QCOMPARE(TilePrint::pdfPageSizeMm(source).toSize(), QSize(400, 300));
    const TilePlan plan = TilePrint::bestPlan(request(QSizeF(400, 300), QPageSize::A4, 0.0));
    QVERIFY2(plan.ok, qPrintable(plan.error));
    // A4 가로 칸 287×200 mm, 겹침 10 mm → 가로 2장 × 세로 2장.
    QVERIFY(plan.landscape);
    QCOMPARE(plan.cols, 2);
    QCOMPARE(plan.rows, 2);
    const QString tiles = dir.filePath(QStringLiteral("tiles.pdf"));
    {
      QPdfWriter writer(tiles);
      writer.setResolution(100);
      QString error;
      TilePrint::Options options;
      options.cutLines = false;
      QVERIFY2(TilePrint::renderTiles(source, plan, &writer, 100.0, options, &error), qPrintable(error));
    }
    QPdfDocument out;
    QCOMPARE(out.load(tiles), QPdfDocument::Error::None);
    QCOMPARE(out.pageCount(), 4);
    const QSizeF page = out.pagePointSize(0) / kPointsPerMm;
    QVERIFY2(qAbs(page.width() - 297.0) < 1.0 && qAbs(page.height() - 210.0) < 1.0, "A4 가로 쪽이 아닙니다.");
    // 종이 위치 = 가장자리 5 mm + 이 장의 칸 안 위치.
    QVERIFY2(near(colorAtMm(out, 0, QPointF(5 + 20, 5 + 20)), 220, 20, 20), "첫 장 왼쪽 위가 도면 왼쪽 위(빨강)가 아닙니다.");
    QVERIFY2(near(colorAtMm(out, 0, QPointF(5 + 250, 5 + 20)), 20, 170, 40), "첫 장 오른쪽이 도면 오른쪽 위(초록)가 아닙니다.");
    // 넷째 장(2행 2열) 칸은 도면 (277, 190)부터다. 도면 (390, 290)은 노랑.
    QVERIFY2(near(colorAtMm(out, 3, QPointF(5 + 113, 5 + 100)), 240, 210, 20), "마지막 장이 도면 오른쪽 아래(노랑)가 아닙니다.");
    // 도면 밖(가로 400 mm 너머)은 비어 있어야 한다.
    QVERIFY2(paper(colorAtMm(out, 3, QPointF(5 + 200, 5 + 50))), "도면 밖에 그림이 찍혔습니다.");
  }

  void fitPrintsTheWholeDrawingCentred() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString source = makeQuadrantPdf(dir.filePath(QStringLiteral("drawing.pdf")), QSizeF(400, 300));
    const QString out = dir.filePath(QStringLiteral("fit.pdf"));
    {
      QPdfWriter writer(out);
      writer.setResolution(100);
      QString error;
      QVERIFY2(TilePrint::renderFit(source, QPageSize(QPageSize::A4), QMarginsF(5, 5, 5, 5), &writer, 100.0, &error),
               qPrintable(error));
    }
    QPdfDocument doc;
    QCOMPARE(doc.load(out), QPdfDocument::Error::None);
    QCOMPARE(doc.pageCount(), 1);
    // 가로 도면은 A4 가로에 2/3 배율로 들어가고 좌우 10.2 mm씩 비운다.
    const double k = 200.0 / 300.0;
    const double left = 5 + (287 - 400 * k) / 2;
    QVERIFY2(near(colorAtMm(doc, 0, QPointF(left + 100 * k, 5 + 75 * k)), 220, 20, 20), "왼쪽 위 칸(빨강)이 아닙니다.");
    QVERIFY2(near(colorAtMm(doc, 0, QPointF(left + 300 * k, 5 + 225 * k)), 240, 210, 20), "오른쪽 아래 칸(노랑)이 아닙니다.");
    QVERIFY2(paper(colorAtMm(doc, 0, QPointF(left - 5, 5 + 100))), "도면 왼쪽 여백에 그림이 찍혔습니다.");
  }

  void printDialogCountsSheetsAndWarnsAboutEnlarging() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString source = makeQuadrantPdf(dir.filePath(QStringLiteral("drawing.pdf")), QSizeF(297, 420));
    KaPrintDialog dialog(source, QStringLiteral("시험 도면"));
    auto* properties = dialog.findChild<QPushButton*>(QStringLiteral("printProperties"));
    QVERIFY2(properties, "프린터 속성 단추가 없습니다.");
    QCOMPARE(properties->text(), QStringLiteral("프린터 속성…"));
    dialog.setTiled(true);
    dialog.setSheet(KaPrintDialog::SheetA4);
    dialog.setOutput(KaPrintDialog::OutputA0);
    QVERIFY2(dialog.plan().ok, qPrintable(dialog.plan().error));
    const int count = int(dialog.plan().sheets.size());
    QVERIFY(count > 1);
    QVERIFY2(dialog.summary().contains(QStringLiteral("용지 %1장").arg(count)), qPrintable(dialog.summary()));
    QVERIFY2(dialog.warning().contains(QStringLiteral("283%")), qPrintable(dialog.warning()));
    // 안내 장 한 장이 맨 앞에 붙는다.
    dialog.setOverview(true);
    QCOMPARE(dialog.pageCount(), count + 1);
    QCOMPARE(dialog.printButtonText(), QStringLiteral("인쇄 · %1장 + 안내 1장").arg(count));
    const QString tiles = dir.filePath(QStringLiteral("dialog-tiles.pdf"));
    QString error;
    QVERIFY2(dialog.saveTilesPdf(tiles, &error), qPrintable(error));
    QPdfDocument doc;
    QCOMPARE(doc.load(tiles), QPdfDocument::Error::None);
    QCOMPARE(doc.pageCount(), count + 1);

    dialog.setOutput(KaPrintDialog::OutputDrawing);
    QVERIFY2(dialog.warning().isEmpty(), "도면 크기 그대로인데 축척 경고가 나옵니다.");
    QVERIFY(dialog.summary().contains(QStringLiteral("도면 크기 그대로")));
    dialog.setTiled(false);
    QVERIFY(dialog.summary().contains(QStringLiteral("한 장에")));
  }

  // 창 크기: in 새 모양 「도면 인쇄」 opens as the large window of the design (860 × 640).
  void paperPrintWindowIsTheLargeSize() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString source = makeQuadrantPdf(dir.filePath(QStringLiteral("drawing.pdf")), QSizeF(297, 420));
    qApp->setProperty("kaPaperLook", true);
    KaPrintDialog dialog(source, QStringLiteral("시험 도면"));
    qApp->setProperty("kaPaperLook", QVariant());
    QCOMPARE(dialog.size(), QSize(860, 640));
  }

  void oneSheetKeepsActualSizeWhenTheDrawingFits() {
    const QMarginsF edge(5, 5, 5, 5);
    // A4 도면을 A4 에: 실제 크기. 가장자리 5 mm 는 프린터가 못 찍을 수 있다고 알린다.
    const TileFit same = TilePrint::fit(QSizeF(210, 297), QPageSize(QPageSize::A4), edge);
    QVERIFY(same.actualSize);
    QCOMPARE(same.ratio, 1.0);
    QVERIFY(same.edgeMayClip);
    // A4 도면을 A3 에: 키우지 않고 실제 크기로 가운데.
    const TileFit smaller = TilePrint::fit(QSizeF(210, 297), QPageSize(QPageSize::A3), edge);
    QVERIFY(smaller.actualSize);
    QVERIFY(!smaller.edgeMayClip);
    // A3 도면을 A4 에: 넘치므로 인쇄 가능 영역에 맞춰 줄인다.
    const TileFit bigger = TilePrint::fit(QSizeF(297, 420), QPageSize(QPageSize::A4), edge);
    QVERIFY(!bigger.actualSize);
    QVERIFY(qAbs(bigger.ratio - 200.0 / 297.0) < 1e-9);
    // 가로 도면은 가로 용지.
    QVERIFY(TilePrint::fit(QSizeF(297, 210), QPageSize(QPageSize::A4), edge).landscape);
  }

  void oneSheetActualSizeIsCentredOnThePaper() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString source = makeQuadrantPdf(dir.filePath(QStringLiteral("a4.pdf")), QSizeF(210, 297));
    const QString out = dir.filePath(QStringLiteral("on-a3.pdf"));
    {
      QPdfWriter writer(out);
      writer.setResolution(100);
      QString error;
      QVERIFY2(TilePrint::renderFit(source, QPageSize(QPageSize::A3), QMarginsF(5, 5, 5, 5), &writer, 100.0, &error),
               qPrintable(error));
    }
    QPdfDocument doc;
    QCOMPARE(doc.load(out), QPdfDocument::Error::None);
    // A3(297×420) 한가운데에 A4(210×297)를 그대로: 왼쪽 43.5 mm, 위 61.5 mm 부터.
    const QPointF origin(43.5, 61.5);
    QVERIFY2(near(colorAtMm(doc, 0, origin + QPointF(50, 50)), 220, 20, 20), "실제 크기 도면의 왼쪽 위(빨강)가 제자리가 아닙니다.");
    QVERIFY2(near(colorAtMm(doc, 0, origin + QPointF(160, 250)), 240, 210, 20), "실제 크기 도면의 오른쪽 아래(노랑)가 제자리가 아닙니다.");
    QVERIFY2(paper(colorAtMm(doc, 0, origin + QPointF(-10, 50))), "도면 왼쪽 바깥에 그림이 찍혔습니다.");
    QVERIFY2(paper(colorAtMm(doc, 0, origin + QPointF(215, 50))), "도면 오른쪽 바깥에 그림이 찍혔습니다(키워 찍었습니다).");
  }

  void enlargedScalesAreRoundAndStayWithinA0() {
    const double a0 = TilePrint::fitScale(QSizeF(297, 420), QPageSize(QPageSize::A0));
    QCOMPARE(TilePrint::enlargedScales(5000, a0), (QList<double>{2500, 2000}));
    QCOMPARE(TilePrint::enlargedScales(1000, 2.0), (QList<double>{600, 500}));
    QVERIFY(TilePrint::enlargedScales(0, a0).isEmpty());
    QCOMPARE(TilePrint::scaleLabel(2500), QStringLiteral("1:2,500"));
    QCOMPARE(TilePrint::scaleLabel(1766.2), QStringLiteral("1:1,766"));
    QCOMPARE(TilePrint::isoName(QSizeF(420, 297)), QStringLiteral("A3"));
    QCOMPARE(TilePrint::isoName(QSizeF(594, 841)), QStringLiteral("A1"));
    QVERIFY(TilePrint::isoName(QSizeF(400, 300)).isEmpty());
  }

  void overviewAndChosenSheetsDecideThePages() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString source = makeQuadrantPdf(dir.filePath(QStringLiteral("drawing.pdf")), QSizeF(400, 300));
    const TilePlan plan = TilePrint::bestPlan(request(QSizeF(400, 300), QPageSize::A4, 6.0));
    QVERIFY2(plan.ok && plan.sheets.size() == 4, qPrintable(plan.error));
    const QString out = dir.filePath(QStringLiteral("chosen.pdf"));
    int lastDone = 0;
    int lastTotal = 0;
    {
      QPdfWriter writer(out);
      writer.setResolution(100);
      TilePrint::Options options;
      options.overview = true;
      options.title = QStringLiteral("시험 도면");
      options.scaleText = QStringLiteral("1:2,500");
      options.sheets = {3, 0, 3};  // 순서가 섞이고 겹쳐도 번호 순서로 한 번씩
      options.progress = [&](int done, int total) {
        lastDone = done;
        lastTotal = total;
        return true;
      };
      QString error;
      QVERIFY2(TilePrint::renderTiles(source, plan, &writer, 100.0, options, &error), qPrintable(error));
    }
    QCOMPARE(lastTotal, 3);
    QCOMPARE(lastDone, 3);
    QPdfDocument doc;
    QCOMPARE(doc.load(out), QPdfDocument::Error::None);
    QCOMPARE(doc.pageCount(), 3);  // 안내 1 + 1번 장 + 4번 장
    QVERIFY2(near(colorAtMm(doc, 1, QPointF(5 + 20, 5 + 20)), 220, 20, 20), "둘째 쪽이 1번 장(빨강)이 아닙니다.");
    QVERIFY2(near(colorAtMm(doc, 2, QPointF(5 + 113, 5 + 100)), 240, 210, 20), "셋째 쪽이 4번 장(노랑)이 아닙니다.");
  }

  void stoppingMidwayEndsTheRun() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString source = makeQuadrantPdf(dir.filePath(QStringLiteral("drawing.pdf")), QSizeF(400, 300));
    const TilePlan plan = TilePrint::bestPlan(request(QSizeF(400, 300), QPageSize::A4, 0.0));
    QVERIFY(plan.ok);
    TilePrint::Options options;
    options.progress = [](int done, int) { return done < 1; };
    QPdfWriter writer(dir.filePath(QStringLiteral("stopped.pdf")));
    QString error;
    bool cancelled = false;
    QVERIFY(!TilePrint::renderTiles(source, plan, &writer, 100.0, options, &error, &cancelled));
    QVERIFY(cancelled);
    QVERIFY(!error.isEmpty());
  }

  void dialogShowsPaperScaleAndReprintsChosenSheets() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString source = makeQuadrantPdf(dir.filePath(QStringLiteral("drawing.pdf")), QSizeF(297, 420));
    KaPrintDialog dialog(source, QStringLiteral("시험 도면"), 5000.0);
    QVERIFY2(dialog.outputLabels().first().contains(QStringLiteral("1:5,000")), qPrintable(dialog.outputLabels().join('\n')));
    QVERIFY(dialog.outputLabels().contains(QStringLiteral("1:2,500으로 키우기 · 2배 · 약 A1")));
    dialog.setTiled(true);
    dialog.setSheet(KaPrintDialog::SheetA4);
    dialog.setOverview(true);
    QVERIFY(dialog.setOutputScale(2500));
    QVERIFY2(dialog.plan().ok, qPrintable(dialog.plan().error));
    QCOMPARE(dialog.paperScale(), 2500.0);
    QVERIFY2(dialog.summary().contains(QStringLiteral("종이 위 축척 1:2,500")), qPrintable(dialog.summary()));
    QVERIFY2(dialog.warning().contains(QStringLiteral("1:5,000")) && dialog.warning().contains(QStringLiteral("1:2,500")),
             qPrintable(dialog.warning()));
    const int count = int(dialog.plan().sheets.size());
    QVERIFY(count > 2);
    // 눈으로 볼 견본: KA_PRINT_SAMPLE_DIR 를 주면 인쇄 창과 안내 장·첫 장 그림을 남긴다.
    const QString sampleDir = qEnvironmentVariable("KA_PRINT_SAMPLE_DIR");
    if (!sampleDir.isEmpty()) {
      dialog.resize(760, 820);
      QVERIFY(dialog.grab().save(QDir(sampleDir).filePath(QStringLiteral("dialog-tiles.png"))));
      const QString sample = QDir(sampleDir).filePath(QStringLiteral("sample-tiles.pdf"));
      QString sampleError;
      QVERIFY2(dialog.saveTilesPdf(sample, &sampleError), qPrintable(sampleError));
      QPdfDocument pages;
      QCOMPARE(pages.load(sample), QPdfDocument::Error::None);
      for (int page = 0; page < 2; ++page) {
        const QSizeF points = pages.pagePointSize(page);
        const QImage image = pages.render(page, (points * 1.6).toSize());
        QVERIFY(image.save(QDir(sampleDir).filePath(QStringLiteral("sample-page%1.png").arg(page + 1))));
      }
    }
    // 잘못 나온 장만 다시: 미리보기에서 두 장을 빼면 그 두 장과 안내 장이 빠진다.
    dialog.toggleSheet(0);
    dialog.toggleSheet(1);
    QCOMPARE(dialog.chosenSheets().size(), count - 2);
    QCOMPARE(dialog.pageCount(), count - 2);
    QCOMPARE(dialog.printButtonText(), QStringLiteral("인쇄 · %1장").arg(count - 2));
    QVERIFY(dialog.summary().contains(QStringLiteral("이번에는 %1장만").arg(count - 2)));
    QVERIFY2(dialog.summary().contains(QStringLiteral("안내 장은 다시 찍지 않습니다")), qPrintable(dialog.summary()));
    const QString tiles = dir.filePath(QStringLiteral("again.pdf"));
    QString error;
    QVERIFY2(dialog.saveTilesPdf(tiles, &error), qPrintable(error));
    QPdfDocument doc;
    QCOMPARE(doc.load(tiles), QPdfDocument::Error::None);
    QCOMPARE(doc.pageCount(), count - 2);
    if (!sampleDir.isEmpty())
      QVERIFY(dialog.grab().save(QDir(sampleDir).filePath(QStringLiteral("dialog-skipped.png"))));
    dialog.toggleSheet(1);
    QCOMPARE(dialog.chosenSheets().size(), count - 1);

    // 한 장: 넘치면 줄이고 종이 위 축척을 알려 준다. 들어가면 실제 크기.
    dialog.setTiled(false);
    QVERIFY2(dialog.warning().contains(QStringLiteral("종이 위 축척은 약 1:")), qPrintable(dialog.warning()));
    if (!sampleDir.isEmpty())
      QVERIFY(dialog.grab().save(QDir(sampleDir).filePath(QStringLiteral("dialog-fit-shrink.png"))));
    dialog.setSheet(KaPrintDialog::SheetA3);
    QVERIFY2(dialog.summary().contains(QStringLiteral("실제 크기(100%)")) &&
                 dialog.summary().contains(QStringLiteral("1:5,000")),
             qPrintable(dialog.summary()));
  }

  void enlargedScalesComeFromTheStandardTable() {
    for (double d : TilePrint::enlargedScales(50000, 1000.0, 50))
      QVERIFY2(StandardScales::isStandard(d, StandardScales::PrintEnlarge), qPrintable(QString::number(d)));
    QCOMPARE(TilePrint::scaleLabel(1200), QStringLiteral("1:1,200"));
  }

  void printResolutionCanBeRaisedForFineLines() {
    // Printing still draws the same drawing PDF; only the picture resolution cap changes.
    QCOMPARE(TilePrint::printDpiChoices(), (QList<int>{300, 600}));
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString source = makeQuadrantPdf(dir.filePath(QStringLiteral("drawing.pdf")), QSizeF(297, 420));
    KaPrintDialog dialog(source, QStringLiteral("시험 도면"));
    QVERIFY(dialog.findChild<QComboBox*>(QStringLiteral("printDpi")));
    dialog.setPrintDpi(300);
    QCOMPARE(dialog.printDpi(), 300.0);
    dialog.setTiled(true);
    dialog.setSheet(KaPrintDialog::SheetA4);
    dialog.setOutput(KaPrintDialog::OutputDrawing);
    dialog.setOverview(false);
    QVERIFY2(dialog.plan().ok, qPrintable(dialog.plan().error));
    QString error;
    const QString normal = dir.filePath(QStringLiteral("300.pdf"));
    QVERIFY2(dialog.saveTilesPdf(normal, &error), qPrintable(error));
    dialog.setPrintDpi(600);
    QCOMPARE(dialog.printDpi(), 600.0);
    const QString fine = dir.filePath(QStringLiteral("600.pdf"));
    QVERIFY2(dialog.saveTilesPdf(fine, &error), qPrintable(error));
    QPdfDocument a;
    QPdfDocument b;
    QCOMPARE(a.load(normal), QPdfDocument::Error::None);
    QCOMPARE(b.load(fine), QPdfDocument::Error::None);
    QCOMPARE(a.pageCount(), b.pageCount());
    QCOMPARE(a.pagePointSize(0), b.pagePointSize(0));
    QVERIFY2(QFileInfo(fine).size() > QFileInfo(normal).size(),
             "600 DPI tiles must carry a finer picture than 300 DPI");
    dialog.setPrintDpi(1234);  // not offered: keep the current choice
    QCOMPARE(dialog.printDpi(), 600.0);
  }
};

QTEST_MAIN(TestTilePrint)
#include "test_tile_print.moc"
