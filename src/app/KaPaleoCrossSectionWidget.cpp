#include "KaPaleoStudio.h"
#include "KaIcons.h"
#include "KaPaleoMenu.h"

#include <QAction>
#include <QColor>
#include <QFont>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QToolButton>
#include <algorithm>
#include <cmath>

KaPaleoCrossSectionWidget::KaPaleoCrossSectionWidget(QWidget* parent) : QWidget(parent) {
  setObjectName(QStringLiteral("paleoCrossSection"));
  setMinimumHeight(185);
}

void KaPaleoCrossSectionWidget::setResult(const hgis::paleo::PaleoAnalysisResult& res) {
  m_result = res;
  m_hasResult = !res.center_cross_section.dist_m.empty();
  update();
}

void KaPaleoCrossSectionWidget::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, true);
  p.fillRect(rect(), QColor(250, 250, 249));

  const QRect plot = rect().adjusted(52, 26, -18, -28);
  p.setPen(QPen(QColor(214, 211, 209), 1));
  p.drawRect(plot);

  QFont titleFont(QStringLiteral("Malgun Gothic"), 9, QFont::Bold);
  p.setFont(titleFont);
  p.setPen(QColor(28, 25, 23));
  p.drawText(QRect(12, 4, width() - 24, 20), Qt::AlignLeft | Qt::AlignVCenter,
             QStringLiteral("2D 가상 지층 단면도 (현 지표면 · 매립 제거 원지형 · 충적 문화층 · 생토면 · 하천 수면)"));

  if (!m_hasResult || plot.width() < 40 || plot.height() < 30) {
    p.setFont(QFont(QStringLiteral("Malgun Gothic"), 9));
    p.setPen(QColor(120, 113, 108));
    p.drawText(plot, Qt::AlignCenter,
               QStringLiteral("「반경 10km 고지형 분석 실행」을 누르면 조사구역 지층 단면도가 표시됩니다."));
    return;
  }

  const auto& cs = m_result.center_cross_section;
  const size_t n = cs.dist_m.size();
  double zMin = cs.z_bedrock_base_m[0];
  double zMax = cs.z_surface_curr_m[0];
  for (size_t i = 0; i < n; ++i) {
    zMin = std::min({zMin, cs.z_bedrock_base_m[i], cs.z_river_level_m[i]});
    zMax = std::max({zMax, cs.z_surface_curr_m[i], cs.z_paleo_top_m[i]});
  }
  zMin -= 1.5;
  zMax = std::max(zMin + 4.0, zMax + 1.5);
  const double maxDist = std::max(1.0, cs.dist_m.back());

  auto toPt = [&](double d, double z) -> QPointF {
    const double x = plot.left() + (d / maxDist) * plot.width();
    const double y = plot.bottom() - ((z - zMin) / (zMax - zMin)) * plot.height();
    return QPointF(x, std::clamp<double>(y, plot.top(), plot.bottom()));
  };

  QPainterPath alluvPoly;
  alluvPoly.moveTo(toPt(cs.dist_m[0], cs.z_paleo_top_m[0]));
  for (size_t i = 1; i < n; ++i) alluvPoly.lineTo(toPt(cs.dist_m[i], cs.z_paleo_top_m[i]));
  for (size_t i = n; i-- > 0;) alluvPoly.lineTo(toPt(cs.dist_m[i], cs.z_bedrock_base_m[i]));
  alluvPoly.closeSubpath();
  p.fillPath(alluvPoly, QColor(251, 192, 45, 70));

  auto drawCurve = [&](const std::vector<double>& zv, const QColor& col, Qt::PenStyle style,
                       double w) {
    QPainterPath path;
    path.moveTo(toPt(cs.dist_m[0], zv[0]));
    for (size_t i = 1; i < n; ++i) path.lineTo(toPt(cs.dist_m[i], zv[i]));
    p.setPen(QPen(col, w, style));
    p.drawPath(path);
  };

  drawCurve(cs.z_river_level_m, QColor(2, 132, 199), Qt::DashLine, 1.4);
  drawCurve(cs.z_bedrock_base_m, QColor(120, 53, 15), Qt::SolidLine, 1.8);
  drawCurve(cs.z_paleo_top_m, QColor(217, 119, 6), Qt::SolidLine, 2.2);
  drawCurve(cs.z_surface_curr_m, QColor(87, 83, 78), Qt::DotLine, 1.5);

  p.setFont(QFont(QStringLiteral("Malgun Gothic"), 8));
  p.setPen(QColor(87, 83, 78));
  p.drawText(QRect(4, plot.top() - 4, 46, 16), Qt::AlignRight,
             QStringLiteral("%1m").arg(zMax, 0, 'f', 0));
  p.drawText(QRect(4, plot.bottom() - 12, 46, 16), Qt::AlignRight,
             QStringLiteral("%1m").arg(zMin, 0, 'f', 0));
  p.drawText(QRect(plot.left(), plot.bottom() + 4, 340, 18), Qt::AlignLeft,
             QStringLiteral("점선: 현 지표면 | 주황 실선: 복원 원지형(Z_paleo) | 갈색 실선: 생토면(풍화토) | 파란 파선: 하천 수면"));
}

namespace KaPaleoMenu {

void install(QToolButton* button, QWidget* host, const std::function<void()>& trigger) {
  if (!button || !host || button->menu()) return;
  auto bindMenu = [&](QToolButton* tb, QMenu* m) {
    tb->setPopupMode(QToolButton::MenuButtonPopup);
    tb->setContextMenuPolicy(Qt::CustomContextMenu);
    tb->setMenu(m);
    QObject::connect(tb, &QToolButton::customContextMenuRequested, host,
                     [tb, m](const QPoint& pos) { m->exec(tb->mapToGlobal(pos)); });
  };
  auto addAct = [&](QMenu* m, const QString& iconId, const QString& objName, const QString& text) {
    auto* act = m->addAction(KaIcons::icon(iconId), text);
    act->setObjectName(objName);
    QObject::connect(act, &QAction::triggered, host, [button, objName, trigger] {
      if (button) button->setProperty("paleoAction", objName);
      if (trigger) trigger();
    });
  };
  auto* menu = new QMenu(button);
  addAct(menu, QStringLiteral("paleo"), QStringLiteral("actionPaleoRun"),
         QStringLiteral("「고지형분석」 전용 탭 열기"));
  addAct(menu, QStringLiteral("paleo"), QStringLiteral("actionPaleoSourcesDialog"),
         QStringLiteral("수치지질도 · 시추주상도(지반) · KIGAM 별도 내려받기 창…"));
  menu->addSeparator();
  addAct(menu, QStringLiteral("geology"), QStringLiteral("actionGeology5k"),
         QStringLiteral("수치지질도(KIGAM 1:5만) 별도 받기 (반경 5km · 도면 범례)"));
  addAct(menu, QStringLiteral("paleo"), QStringLiteral("actionPaleoGeoInfo"),
         QStringLiteral("국토지반정보 시추주상도·퇴적층두께 별도 받기 (반경 5km · 도면 범례)"));
  addAct(menu, QStringLiteral("paleo"), QStringLiteral("actionPaleoKigam"),
         QStringLiteral("KIGAM 단층·지하수위·단구연대 별도 받기 (반경 10km)"));
  menu->addSeparator();
  addAct(menu, QStringLiteral("paleo"), QStringLiteral("actionPaleoPortalGeoInfo"),
         QStringLiteral("국토지반정보 포털 열기 (geoinfo.or.kr)…"));
  addAct(menu, QStringLiteral("paleo"), QStringLiteral("actionPaleoPortalKigam"),
         QStringLiteral("KIGAM 지오빅데이터 포털 열기 (data.kigam.re.kr)…"));
  bindMenu(button, menu);

  for (auto* tb : host->findChildren<QToolButton*>()) {
    if (!tb || tb->text() != QStringLiteral("지질") || tb->menu()) continue;
    auto* geoMenu = new QMenu(tb);
    addAct(geoMenu, QStringLiteral("geology"), QStringLiteral("actionGeology5k"),
           QStringLiteral("조사구역 반경 5km 수치지질도 받기 (1:5만 · 권장)"));
    addAct(geoMenu, QStringLiteral("geology"), QStringLiteral("actionGeology2k"),
           QStringLiteral("조사구역 반경 2km 수치지질도 받기"));
    addAct(geoMenu, QStringLiteral("geology"), QStringLiteral("actionGeology10k"),
           QStringLiteral("조사구역 반경 10km 수치지질도 받기"));
    geoMenu->addSeparator();
    addAct(geoMenu, QStringLiteral("paleo"), QStringLiteral("actionPaleoGeoInfo"),
           QStringLiteral("국토지반정보 시추주상도·퇴적층두께 별도 받기 (반경 5km)"));
    addAct(geoMenu, QStringLiteral("paleo"), QStringLiteral("actionPaleoKigam"),
           QStringLiteral("KIGAM 단층·선구조·수문 별도 받기 (반경 10km)"));
    addAct(geoMenu, QStringLiteral("paleo"), QStringLiteral("actionPaleoSourcesDialog"),
           QStringLiteral("수치지질도 · 시추주상도 별도 내려받기 창…"));
    bindMenu(tb, geoMenu);
    break;
  }
}

}  // namespace KaPaleoMenu
