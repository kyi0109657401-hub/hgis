#pragma once

#include <QList>
#include <QMarginsF>
#include <QPageSize>
#include <QRectF>
#include <QSizeF>
#include <QString>

#include <functional>

class QImage;
class QPagedPaintDevice;

// 큰 도면을 작은 용지 여러 장에 나눠 찍는다(예: A0 도면을 A3 9장으로).
// 종이에는 「PDF 내보내기」와 같은 도면 PDF를 잘라 그린다. 그래서 인쇄물과 PDF가 같다.
// 길이 단위는 모두 mm.
struct TilePlanRequest {
  QSizeF drawingMm;           // 도면 PDF 한 쪽 크기
  double scale = 1.0;         // 붙였을 때 크기 = 도면 × scale. 1이면 도면 축척 그대로
  QPageSize sheet;            // 나눌 용지. 방향은 plan 이 정한다
  QMarginsF printerMarginMm;  // 프린터가 찍지 못하는 가장자리
  double overlapMm = 10.0;    // 이웃 장과 같은 그림이 겹치는 폭
  double labelBandMm = 0.0;   // 아래쪽에 장 번호·안내를 찍는 띠. 도면은 그 위까지만 찍는다
};

struct TileSheet {
  int row = 0;
  int col = 0;
  QRectF areaMm;  // 붙였을 때의 도면 좌표에서 이 장이 맡는 칸. 끝 장은 도면 밖으로 나갈 수 있다
};

struct TilePlan {
  bool ok = false;
  QString error;
  QPageSize sheet;
  bool landscape = false;
  QMarginsF printerMarginMm;
  double scale = 1.0;
  double overlapMm = 0.0;
  double labelBandMm = 0.0;
  QSizeF outputMm;   // 붙였을 때 크기
  QSizeF contentMm;  // 한 장에서 도면이 찍히는 칸(프린터 여백·번호 띠 제외)
  int rows = 0;
  int cols = 0;
  QList<TileSheet> sheets;  // 위에서 아래로, 왼쪽에서 오른쪽으로. 번호는 1번부터 이 순서
};

// 한 장에 찍을 때. 도면이 용지에 들어가면 실제 크기(100%)로 찍어 축척을 지키고,
// 넘칠 때만 인쇄 가능 영역에 맞춰 줄인다. 키우지는 않는다.
struct TileFit {
  double ratio = 1.0;
  bool actualSize = true;
  bool landscape = false;    // 용지 방향. 도면 방향에 맞춘다
  bool edgeMayClip = false;  // 실제 크기인데 도면이 프린터가 못 찍는 가장자리에 걸친다
};

class TilePrint {
public:
  // 도면을 target 용지 안에 비율대로 넣는 배율. 용지 방향은 도면 방향에 맞춘다.
  static double fitScale(const QSizeF& drawingMm, const QPageSize& target);
  static TilePlan plan(const TilePlanRequest& request, bool landscapeSheet);
  // 세로·가로 중 장수가 적은 쪽. 같으면 세로.
  static TilePlan bestPlan(const TilePlanRequest& request);
  static TileFit fit(const QSizeF& drawingMm, const QPageSize& sheet, const QMarginsF& printerMarginMm);
  // thumbnail 을 주면 첫 쪽을 긴 변 thumbnailLongPx 로 함께 그린다(문서를 두 번 열지 않으려고).
  static QSizeF pdfPageSizeMm(const QString& pdfPath, QString* error = nullptr, QImage* thumbnail = nullptr,
                              int thumbnailLongPx = 0);
  // 도면 축척(분모)보다 크게 뽑을 때 고를 딱 떨어지는 축척. 배율이 작은 것부터, maxFactor 이하만.
  static QList<double> enlargedScales(double drawingDenominator, double maxFactor, int limit = 3);
  // 1:2,500 처럼 세 자리마다 쉼표를 찍는다.
  static QString scaleLabel(double denominator);
  // A0~A4 중 크기가 (방향과 관계없이) 3% 안으로 맞는 이름. 없으면 빈 문자열.
  static QString isoName(const QSizeF& mm);

  struct Options {
    bool cutLines = true;     // 자르는 점선과 시작선
    bool overview = false;    // 맨 앞에 붙이는 순서 안내 한 장
    QString title;            // 번호 띠·안내 장에 찍는 이름
    QString scaleText;        // 종이 위 축척(예: 1:2,500). 비면 찍지 않는다
    QList<int> sheets;        // 찍을 장(0부터). 비면 모두
    // 한 쪽을 끝낼 때마다 (끝낸 쪽수, 모두)로 부른다. false 를 돌려주면 멈춘다.
    std::function<bool(int, int)> progress;
  };
  // 인쇄 그림 해상도 상한 선택지. 300 은 도면 PDF 래스터와 같고, 600 은 0.1~0.2 mm 선이
  // 많은 1:20 실측도용이다. 저장 PDF 와 인쇄는 같은 도면 PDF 를 그리므로 모양은 같다.
  static QList<int> printDpiChoices();
  static constexpr double kDefaultPrintDpi = 300.0;
  // pdfPath 첫 쪽을 plan 대로 잘라 device 의 각 쪽에 그린다. 용지·방향·여백은 여기서 맞춘다.
  // dpi 는 그림 해상도의 상한이다. 프린터가 더 높아도 이보다 곱게 그리지 않는다.
  static bool renderTiles(const QString& pdfPath, const TilePlan& plan, QPagedPaintDevice* device,
                          double dpi, const Options& options, QString* error, bool* cancelled = nullptr);
  // 한 장에 찍는다. 배율과 방향은 fit() 과 같다.
  static bool renderFit(const QString& pdfPath, const QPageSize& sheet,
                        const QMarginsF& printerMarginMm, QPagedPaintDevice* device, double dpi,
                        QString* error);
};
