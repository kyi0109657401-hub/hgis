#include "KaTopographicBrowser.h"
#include "KaDownloadUi.h"
#include "KaStageRail.h"
#include "KaWindowGeometry.h"
#include <QCoreApplication>
#include <QDebug>
#include <QScreen>
#include <QScrollArea>
#include <QShowEvent>
#include <QCloseEvent>

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPointer>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QSet>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QTreeWidget>
#include <QUuid>
#include <QVBoxLayout>
#include <QWebEngineDownloadRequest>
#include <QWebEngineLoadingInfo>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QWebEngineView>
#include <functional>
#include <algorithm>
#include <cmath>

namespace {
const QUrl kHome(QStringLiteral("https://map.ngii.go.kr/ms/map/NlipMap.do?tabGb=total"));

class TopographicPage final : public QWebEnginePage {
public:
  TopographicPage(QWebEngineProfile* profile, QObject* parent,
      std::function<QWebEnginePage*(QWebEnginePage*)> popup, std::function<void(const QUrl&)> external,
      std::function<bool()> suppressDialog, QWebEnginePage* opener)
      : QWebEnginePage(profile, parent), m_popup(std::move(popup)), m_external(std::move(external)),
        m_suppressDialog(std::move(suppressDialog)), m_opener(opener) {}
  QWebEnginePage* openerPage() const { return m_opener; }
protected:
  // The official application page logs birth dates. Do not forward its console.
  void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel, const QString&, int, const QString&) override {}
  void javaScriptAlert(const QUrl& origin,const QString& message) override {
    if(!m_suppressDialog())QWebEnginePage::javaScriptAlert(origin,message);
  }
  bool javaScriptConfirm(const QUrl& origin,const QString& message) override {
    return !m_suppressDialog() && QWebEnginePage::javaScriptConfirm(origin,message);
  }
  bool javaScriptPrompt(const QUrl& origin,const QString& message,const QString& defaultValue,QString* result) override {
    return !m_suppressDialog() && QWebEnginePage::javaScriptPrompt(origin,message,defaultValue,result);
  }
  QWebEnginePage* createWindow(WebWindowType) override { return m_popup(this); }
  bool acceptNavigationRequest(const QUrl& url, NavigationType, bool) override {
    const auto scheme = url.scheme().toLower();
    if (scheme == QLatin1String("https") || scheme == QLatin1String("http") ||
        scheme == QLatin1String("about") || scheme == QLatin1String("blob") ||
        scheme == QLatin1String("data")) return true;
    if (scheme != QLatin1String("file") && scheme != QLatin1String("javascript") &&
        scheme != QLatin1String("qrc") && !scheme.isEmpty()) m_external(url);
    return false;
  }
private:
  std::function<QWebEnginePage*(QWebEnginePage*)> m_popup;
  std::function<void(const QUrl&)> m_external;
  std::function<bool()> m_suppressDialog;
  QPointer<QWebEnginePage> m_opener;
};

// Defer retirement when an account change arrives inside a download callback.
// Explicit teardown keeps all old pages before their shared profile.
class RetiredBrowserSession final : public QWidget {
public:
  RetiredBrowserSession(QWebEngineProfile* profile, QWidget* parent)
      : QWidget(parent), m_profile(profile) { hide(); profile->setParent(nullptr); }
  ~RetiredBrowserSession() override {
    const auto views=findChildren<QWebEngineView*>();
    for(auto* view:views)delete view;
    delete m_profile;
  }
private:
  QWebEngineProfile* m_profile;
};

QString safeFileName(QString name) {
  name.replace(QLatin1Char('\\'), QLatin1Char('/'));
  name = QFileInfo(name).fileName();
  name.replace(QRegularExpression(QStringLiteral("[<>:\"/\\\\|?*\\x00-\\x1f]")), QStringLiteral("_"));
  name = name.left(180).trimmed();
  while (name.endsWith(QLatin1Char('.')) || name.endsWith(QLatin1Char(' '))) name.chop(1);
  const QString stem = name.section(QLatin1Char('.'), 0, 0);
  static const QRegularExpression reserved(QStringLiteral("^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])$"),
                                           QRegularExpression::CaseInsensitiveOption);
  if (reserved.match(stem).hasMatch()) name.prepend(QLatin1Char('_'));
  return name.isEmpty() ? QStringLiteral("수치지형도.zip") : name;
}
}

KaTopographicBrowser::KaTopographicBrowser(QWidget* parent, const QString& downloadDirectory)
    : QDialog(parent), m_portalUrl(kHome) {
  setObjectName(QStringLiteral("topographicBrowser"));
  setWindowTitle(QStringLiteral("수치지형도 내려받기"));
  setModal(false);
  m_downloadDirectory = downloadDirectory.isEmpty()
      ? QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/topographic-downloads")
      : QDir(downloadDirectory).absolutePath();
  auto* layout = new QVBoxLayout(this);
  KaDownloadUi::configure(this, layout, QStringLiteral("수치지형도 다운로드"),
      QStringLiteral("국토정보플랫폼 · 조사 주변 도엽을 확인하고 지도 자료를 받습니다."));
  auto* statusCard = KaDownloadUi::statusCard(this);
  auto* statusLayout = qobject_cast<QVBoxLayout*>(statusCard->layout());
  layout->addWidget(statusCard);
  // Colours come from the application theme (object names below), not from
  // widget-local style sheets, so the official details follow light/dark themes.
  m_details=new QWidget(this);m_details->setObjectName(QStringLiteral("topographicOfficialDetails"));
  auto* detailsLayout=new QVBoxLayout(m_details);detailsLayout->setContentsMargins(0,0,0,0);
  m_detailsScroll=new QScrollArea(this);m_detailsScroll->setObjectName(QStringLiteral("topographicDetailsScroll"));
  m_detailsScroll->setWidgetResizable(true);m_detailsScroll->setFrameShape(QFrame::NoFrame);
  m_detailsScroll->setWidget(m_details);m_details->setMinimumSize(680,480);
  layout->addWidget(m_detailsScroll,1);
  auto* toolbar = new QToolBar(this);
  detailsLayout->addWidget(toolbar);
  auto current = [this] { return qobject_cast<QWebEngineView*>(m_tabs->currentWidget()); };
  connect(toolbar->addAction(QStringLiteral("뒤로")), &QAction::triggered, this, [current] { if (auto* v = current()) v->back(); });
  connect(toolbar->addAction(QStringLiteral("앞으로")), &QAction::triggered, this, [current] { if (auto* v = current()) v->forward(); });
  connect(toolbar->addAction(QStringLiteral("새로고침")), &QAction::triggered, this, [current] { if (auto* v = current()) v->reload(); });
  connect(toolbar->addAction(QStringLiteral("국토정보맵")), &QAction::triggered, this, [this] { navigate(kHome); });
  connect(toolbar->addAction(QStringLiteral("받은 폴더 열기")), &QAction::triggered, this, [this] {
    if (!QDir().mkpath(m_downloadDirectory) || !QDesktopServices::openUrl(QUrl::fromLocalFile(m_downloadDirectory)))
      m_status->setText(QStringLiteral("받은 폴더를 열지 못했습니다. 저장 공간과 폴더 권한을 확인하세요."));
  });
  connect(toolbar->addAction(QStringLiteral("받은 자료 불러오기…")), &QAction::triggered,
          this, &KaTopographicBrowser::importFolderRequested);
  auto* hint = new QLabel(QStringLiteral("현재 조사 주변의 1:25,000 DXF 도엽을 선택합니다. 로그인·인증이 필요하면 공식 화면의 안내를 따라 주세요."), this);
  hint->setWordWrap(true); detailsLayout->addWidget(hint);
  auto* login = new QHBoxLayout;
  m_loginId=new QLineEdit(this);m_loginId->setObjectName(QStringLiteral("topographicLoginId"));
  m_loginId->setPlaceholderText(QStringLiteral("국토지리정보원 아이디"));
  m_loginPassword=new QLineEdit(this);m_loginPassword->setObjectName(QStringLiteral("topographicLoginPassword"));
  m_loginPassword->setEchoMode(QLineEdit::Password);
  m_loginPassword->setPlaceholderText(QStringLiteral("비밀번호 — 이번 로그인에만 사용"));
  auto* retryLogin=new QPushButton(QStringLiteral("로그인 다시 시도"),this);
  connect(retryLogin,&QPushButton::clicked,this,[this]{++m_loginAttempt;m_lastAutomationMessage.clear();});
  login->addWidget(m_loginId);login->addWidget(m_loginPassword);login->addWidget(retryLogin);detailsLayout->addLayout(login);
  auto* form = new QHBoxLayout;
  m_birthDate = new QLineEdit(this); m_birthDate->setObjectName(QStringLiteral("topographicBirthDate"));
  m_birthDate->setPlaceholderText(QStringLiteral("생년월일 (공식 신청서 형식)")); m_birthDate->setMaximumWidth(220);
  m_purpose = new QLineEdit(this); m_purpose->setObjectName(QStringLiteral("topographicPurpose"));
  m_purpose->setPlaceholderText(QStringLiteral("사용목적 — 이번 실행 동안만 사용"));
  m_purpose->setText(QStringLiteral("고고학 발굴조사 현장 도면 작성 및 지형 확인"));
  auto* stop = new QPushButton(QStringLiteral("자동 진행 중지"), this);
  connect(stop, &QPushButton::clicked, this, &KaTopographicBrowser::cancelFromUser);
  form->addWidget(m_birthDate); form->addWidget(m_purpose,1); form->addWidget(stop); detailsLayout->addLayout(form);
  m_tabs = new QTabWidget(this); m_tabs->setObjectName(QStringLiteral("topographicTabs"));
  m_tabs->setTabsClosable(true); detailsLayout->addWidget(m_tabs, 1);
  connect(m_tabs, &QTabWidget::tabCloseRequested, this, &KaTopographicBrowser::closeTab);
  m_compactStatus=new QLabel(this);m_compactStatus->setObjectName(QStringLiteral("topographicCompactStatus"));
  m_stageTrail=new QLabel(QStringLiteral("도엽 확인 → 로그인 → 신청 → 파일 준비 → 다운로드 → 지도에 올리기"),this);
  m_stageTrail->setObjectName(QStringLiteral("topographicStageTrail"));
  m_stageTrail->setWordWrap(true);detailsLayout->insertWidget(0,m_stageTrail);
  m_stageRail=KaStageRail::replace(m_stageTrail);  // 새 모양: six steps on a line (2차 보드 판 5)
  m_currentStage=new QLabel(this);m_currentStage->setObjectName(QStringLiteral("topographicCurrentStage"));
  auto stageFont=m_currentStage->font();stageFont.setBold(true);m_currentStage->setFont(stageFont);
  m_currentStage->setWordWrap(true);m_currentStage->hide();statusLayout->addWidget(m_currentStage);
  m_compactStatus->setWordWrap(true);m_compactStatus->hide();statusLayout->addWidget(m_compactStatus);
  m_orderSheets=new QLabel(this);m_orderSheets->setObjectName(QStringLiteral("topographicOrderSheets"));
  m_orderSheets->setWordWrap(true);m_orderSheets->setTextFormat(Qt::PlainText);m_orderSheets->hide();
  statusLayout->addWidget(m_orderSheets);
  m_failureOutline=new QLabel(this);m_failureOutline->setObjectName(QStringLiteral("topographicFailureOutline"));
  m_failureOutline->setWordWrap(true);m_failureOutline->setTextFormat(Qt::PlainText);
  m_failureOutline->setTextInteractionFlags(Qt::TextSelectableByMouse);m_failureOutline->hide();
  statusLayout->addWidget(m_failureOutline);
  m_transferSummary=new QLabel(this);m_transferSummary->setObjectName(QStringLiteral("topographicTransferSummary"));
  m_transferSummary->setWordWrap(true);detailsLayout->insertWidget(1,m_transferSummary);
  m_progress = new QProgressBar(this); m_progress->setRange(0, 1);
  m_progress->setObjectName(QStringLiteral("topographicProgress"));m_progress->setTextVisible(true);
  KaDownloadUi::styleProgress(m_progress);
  statusLayout->addWidget(m_progress);
  m_downloads = new QTreeWidget(this); m_downloads->setObjectName(QStringLiteral("topographicDownloads"));
  m_downloads->setHeaderLabels({QStringLiteral("파일"), QStringLiteral("진행"), QStringLiteral("상태"), QStringLiteral("취소")});
  m_downloads->setRootIsDecorated(false); m_downloads->setMaximumHeight(150);
  m_downloads->setColumnWidth(0, 280); m_downloads->setColumnWidth(1, 200); detailsLayout->addWidget(m_downloads);
  m_status = new QLabel(QStringLiteral("수치지형도를 준비하고 있습니다."), this);
  m_status->setWordWrap(true); detailsLayout->insertWidget(2, m_status);
  m_compactActions=new QWidget(this);
  auto* compactActions=new QHBoxLayout(m_compactActions);compactActions->setContentsMargins(0,0,0,0);
  m_expandButton=new QPushButton(QStringLiteral("상세 보기"),m_compactActions);
  m_expandButton->setToolTip(QStringLiteral("도엽·파일별 진행 상황과 공식 로그인·인증 화면을 확인합니다."));
  m_expandButton->setObjectName(QStringLiteral("topographicExpandOfficial"));
  auto* compactCancel=new QPushButton(QStringLiteral("취소"),m_compactActions);
  m_compactCancel=compactCancel;
  compactCancel->setObjectName(QStringLiteral("topographicCompactCancel"));
  connect(m_expandButton,&QPushButton::clicked,this,[this]{setCompactMode(!m_compactMode);});
  connect(compactCancel,&QPushButton::clicked,this,&KaTopographicBrowser::reject);
  m_importFallback=new QPushButton(QStringLiteral("받은 자료 불러오기…"),m_compactActions);
  m_importFallback->setObjectName(QStringLiteral("topographicImportFallback"));
  m_importFallback->setToolTip(QStringLiteral("공식 화면에서 직접 받은 DXF·ZIP 폴더를 골라 지도에 올립니다. 자동 신청은 다시 하지 않습니다."));
  m_importFallback->hide();
  connect(m_importFallback,&QPushButton::clicked,this,&KaTopographicBrowser::importFolderRequested);
  compactActions->addWidget(m_expandButton);compactActions->addWidget(m_importFallback);
  compactActions->addStretch();compactActions->addWidget(compactCancel);
  layout->addWidget(m_compactActions);m_compactActions->hide();
  m_automationTimer = new QTimer(this); m_automationTimer->setInterval(400);
  connect(m_automationTimer,&QTimer::timeout,this,&KaTopographicBrowser::pollAutomation);
  m_phaseClock.start();
  auto* progressTimer=new QTimer(this);progressTimer->setInterval(1000);
  connect(progressTimer,&QTimer::timeout,this,[this]{
    if(m_automationTimer->isActive()) {
      const bool transfer=m_automationStage==QLatin1String("transfer");
      const qint64 limit=m_phase==QStringLiteral("로그인")?300000:90000;
      if((transfer && m_transferClock.isValid() && m_transferClock.elapsed()>=60000) ||
          (!transfer && m_phaseClock.elapsed()>=limit)) {
        failAutomation(transfer?QStringLiteral("60초 동안 새 파일 데이터가 수신되지 않았습니다. 연결 상태를 확인한 뒤 다시 시도하세요."):
          QStringLiteral("이 단계의 서버 응답 시간이 초과되었습니다. 계정 설정과 연결 상태를 확인하세요. 신청은 자동으로 반복하지 않습니다."));
        return;
      }
    }
    if(isVisible())updateProgress();
  });
  progressTimer->start();
  for(const QString& path : {QStringLiteral(":/providers/ngii-topographic.js"),
      QCoreApplication::applicationDirPath()+QStringLiteral("/data/providers/ngii-topographic.js"),
      QDir::currentPath()+QStringLiteral("/data/providers/ngii-topographic.js")}) {
    QFile script(path);
    if(script.open(QIODevice::ReadOnly)){m_automationScript=QString::fromUtf8(script.readAll());break;}
  }
  setCompactMode(true);
}

void KaTopographicBrowser::setCompactMode(bool compact) {
  m_compactMode=compact;
  if (!compact && m_tabs && m_tabs->count() == 0) addPage();
  m_details->setVisible(!compact);m_detailsScroll->setVisible(!compact);
  m_downloads->setVisible(!compact);
  // Keep the common status card visible while opening the official details.
  m_compactStatus->show();m_expandButton->show();m_currentStage->show();
  m_compactActions->show();
  m_expandButton->setText(compact?QStringLiteral("상세 보기"):QStringLiteral("상세 접기"));
  updateProgress();
  if(layout())layout()->activate();
  fitToAvailableScreen();
}

void KaTopographicBrowser::setCredentials(const QString& username,const QString& password) {
  if(m_loginId->text()==username && m_loginPassword->text()==password)return;
  stopAutomatic();
  replaceSession();
  m_loginId->setText(username);m_loginPassword->setText(password);
  m_birthDate->clear();
  m_suppliedCredentials=!password.isEmpty();
  ++m_loginAttempt;m_lastAutomationMessage.clear();
  navigate(m_portalUrl);
}

void KaTopographicBrowser::replaceSession() {
  if (!m_profile) return;
  auto* retired=new RetiredBrowserSession(m_profile,this);
  disconnect(m_profile,nullptr,this,nullptr);
  for(auto* request:m_profile->findChildren<QWebEngineDownloadRequest*>()) {
    if(!request->isFinished())request->cancel();
    disconnect(request,nullptr,this,nullptr);
  }
  for(int i=0;i<m_downloads->topLevelItemCount();++i) {
    auto* row=m_downloads->topLevelItem(i);
    if(row->text(2)!=QStringLiteral("내려받는 중"))continue;
    row->setText(2,QStringLiteral("취소됨"));
    if(auto* cancel=m_downloads->itemWidget(row,3))cancel->setEnabled(false);
    if(auto* progress=qobject_cast<QProgressBar*>(m_downloads->itemWidget(row,1)))progress->setRange(0,100);
  }
  const auto views=m_tabs->findChildren<QWebEngineView*>();
  for(auto* view:views) {
    disconnect(view,nullptr,this,nullptr);disconnect(view->page(),nullptr,this,nullptr);
    m_tabs->removeTab(m_tabs->indexOf(view));view->setParent(retired);view->hide();
  }
  retired->deleteLater();
  m_profile=new QWebEngineProfile(this);
  m_profile->setPersistentCookiesPolicy(QWebEngineProfile::NoPersistentCookies);
  m_profile->setHttpCacheType(QWebEngineProfile::MemoryHttpCache);
  connect(m_profile,&QWebEngineProfile::downloadRequested,this,&KaTopographicBrowser::requestDownload);
  addPage();
}

void KaTopographicBrowser::fitToAvailableScreen() {
  if(!screen())return;
  const QRect available=screen()->availableGeometry().adjusted(8,8,-8,-8);
  const QSize decorations=frameGeometry().size()-size();
  const QSize clientLimit=(available.size()-decorations).expandedTo(QSize(1,1));
  setMaximumSize(clientLimit);
  if (m_compactMode) {
    KaWindowGeometry::placeDownload(this);
    return;
  }
  const QSize wanted = QSize(1100, 780);
  resize(wanted.boundedTo(clientLimit));
  const int x=std::clamp(pos().x(),available.left(),std::max(available.left(),available.right()-width()-decorations.width()));
  const int y=std::clamp(pos().y(),available.top(),std::max(available.top(),available.bottom()-height()-decorations.height()));
  move(x,y);
}

void KaTopographicBrowser::showEvent(QShowEvent* event) {
  QDialog::showEvent(event);
  if (m_compactMode) KaWindowGeometry::placeDownload(this);
  else fitToAvailableScreen();
}

void KaTopographicBrowser::cancelFromUser() {
  if(m_cancelNotified)return;
  m_cancelNotified=true;
  stopAutomatic();
  if (m_profile)
  for(auto* request:m_profile->findChildren<QWebEngineDownloadRequest*>())
    if(!request->isFinished())request->cancel();
  emit cancelRequested();
}

void KaTopographicBrowser::reject() {
  const QPointer<KaTopographicBrowser> alive(this);
  cancelFromUser();
  if(alive)QDialog::reject();
}

void KaTopographicBrowser::setActivityStatus(const QString& message) {
  m_status->setText(message);
}

void KaTopographicBrowser::setActivityAttention(const QString& message) {
  m_activityFailurePhase=message.isEmpty()?QString():
      (m_preparationPhase.isEmpty()?visiblePhase():m_preparationPhase);
  m_activityAttention=message;
  setActivityStatus(message);updateProgress();
}

void KaTopographicBrowser::setProcessing(bool processing) {
  m_processing=processing;
  if(processing)m_cancelNotified=false;
  updateProgress();
}

void KaTopographicBrowser::setPreparationProgress(const QString& phase,const QString& detail) {
  if(m_preparationPhase!=phase)m_preparationClock.restart();
  m_preparationPhase=phase;m_preparationDetail=detail;updateProgress();
}

void KaTopographicBrowser::setPhase(const QString& phase) {
  if(m_phase!=phase){m_phase=phase;m_phaseClock.restart();}
  updateProgress();
}

QString KaTopographicBrowser::visiblePhase() const {
  if(m_automationStage==QLatin1String("complete") && m_processing)
    return m_preparationPhase.isEmpty()?QStringLiteral("지도에 올리기"):m_preparationPhase;
  if(m_automationStage.isEmpty() && !m_preparationPhase.isEmpty())return m_preparationPhase;
  return m_phase;
}

void KaTopographicBrowser::failAutomation(const QString& message) {
  const QString phase=visiblePhase();
  const QString activity=m_activityAttention;
  const QString activityPhase=m_activityFailurePhase;
  const QPointer<KaTopographicBrowser> alive(this);
  // Capture where the official site stopped before the stop resets page state.
  const QString outline=pageOutline();
  const QString ordered=m_orderSheetsText;
  stopAutomatic();
  if(!alive)return;
  m_failurePhase=phase;m_activityAttention=activity;m_activityFailurePhase=activityPhase;
  m_failureOutlineText=outline;m_orderSheetsText=ordered;
  automationMessage(message);
}

QString KaTopographicBrowser::pageOutline() const {
  // Diagnostic outline for a changed site: page titles and paths only. Query
  // strings, fragments and form values may hold personal data and are omitted.
  QStringList pages;
  if(m_tabs) for(int i=0;i<m_tabs->count()&&pages.size()<6;++i) {
    const auto* view=qobject_cast<QWebEngineView*>(m_tabs->widget(i));
    if(!view)continue;
    const QUrl url=view->url();
    if(url.isEmpty())continue;
    const QString where=url.host()+url.path();
    const QString title=view->title().left(40).trimmed();
    pages.append(title.isEmpty()||title==where?where:QStringLiteral("%1 (%2)").arg(title,where));
  }
  return pages.isEmpty()?QString():QStringLiteral("멈춘 화면: ")+pages.join(QStringLiteral(" · "));
}

void KaTopographicBrowser::updateProgress() {
  const QString attention=!m_failurePhase.isEmpty()?m_automationAttention:
      (m_activityAttention.isEmpty()?m_automationAttention:m_activityAttention);
  const bool completed=m_automationStage==QLatin1String("complete") && !m_processing;
  const bool stopped=m_automationStage==QLatin1String("stopped") && !m_processing;
  const QString phase=visiblePhase();
  const QStringList phases={QStringLiteral("도엽 확인"),QStringLiteral("로그인"),QStringLiteral("신청"),
    QStringLiteral("파일 준비"),QStringLiteral("다운로드"),QStringLiteral("지도에 올리기")};
  const int phaseIndex=phases.indexOf(phase);
  m_currentStage->setText(QStringLiteral("[%1/6] %2").arg(phase==QStringLiteral("보관 자료 확인")?1:(phaseIndex<0?6:phaseIndex+1)).arg(phase));
  if(m_stageRail)m_stageRail->setCurrent(completed?6:(phase==QStringLiteral("보관 자료 확인")?0:(phaseIndex<0?5:phaseIndex)));
  QString summary=m_officialRecords.isEmpty()?QStringLiteral("받을 도엽 수 확인 중"):
      QStringLiteral("도엽 수신 완료 %1/%2장").arg(m_sheetIndex).arg(m_officialRecords.size());
  if(m_officialRecords.isEmpty() && m_automationStage==QLatin1String("complete"))
    summary=m_currentFileName.isEmpty()?QStringLiteral("보관 도엽 %1장 사용 · 새 다운로드 0장").arg(m_scope.value(QStringLiteral("numbers")).toArray().size()):
        QStringLiteral("파일 수신 완료 %1개").arg(m_receivedFileCount);
  QJsonObject displayedRecord=m_currentRecord;
  if(!m_transferManifest.isEmpty() && m_receivedFileCount<m_transferManifest.size()) {
    const auto current=m_transferManifest.at(m_receivedFileCount).toObject().value(QStringLiteral("number")).toString();
    for(const auto& item:m_officialRecords)if(item.toObject().value(QStringLiteral("num")).toString()==current){displayedRecord=item.toObject();break;}
  }
  const QString number=displayedRecord.value(QStringLiteral("num")).toString();
  if(!number.isEmpty() && m_sheetIndex<m_officialRecords.size())
    summary+=QStringLiteral(" · 현재 %1 %2").arg(number,displayedRecord.value(QStringLiteral("name")).toString());
  if(m_sheetFileCount>0)summary+=(m_transferManifest.isEmpty()?QStringLiteral("\n현재 도엽 파일 완료 %1/%2개"):QStringLiteral("\n전체 파일 완료 %1/%2개")).arg(m_receivedFileCount).arg(m_sheetFileCount);
  if(!m_currentFileName.isEmpty()) {
    const QLocale locale;
    summary+=QStringLiteral("\n%1\n%2 / %3").arg(m_currentFileName,locale.formattedDataSize(m_receivedBytes,2,QLocale::DataSizeTraditionalFormat),
        m_totalBytes>0?locale.formattedDataSize(m_totalBytes,2,QLocale::DataSizeTraditionalFormat):QStringLiteral("전체 크기 확인 중"));
  }
  m_transferSummary->setTextFormat(Qt::PlainText);m_transferSummary->setText(summary);
  m_progress->setTextVisible(true);m_progress->setRange(0,1);m_progress->setValue(0);
  m_progress->setFormat(QStringLiteral("진행률 확인 중"));
  m_compactCancel->setText(QStringLiteral("취소"));
  // The hidden scope panel orders sheets automatically; the list is shown here
  // before and during the official application (no extra request is made).
  const bool failedClosed=!m_failurePhase.isEmpty();
  m_orderSheets->setText(m_orderSheetsText);
  m_orderSheets->setVisible(!m_orderSheetsText.isEmpty() && !completed && (!stopped || failedClosed));
  m_failureOutline->setText(m_failureOutlineText);
  m_failureOutline->setVisible(failedClosed && !m_failureOutlineText.isEmpty());
  m_importFallback->setVisible(failedClosed);
  if(!attention.isEmpty()) {
    const QString failurePhase=!m_failurePhase.isEmpty()?m_failurePhase:
        (m_activityFailurePhase.isEmpty()?phase:m_activityFailurePhase);
    m_currentStage->setText((m_failurePhase.isEmpty()?QStringLiteral("확인 필요 · %1"):QStringLiteral("다운로드 실패 · %1")).arg(failurePhase));
    m_compactStatus->setTextFormat(Qt::PlainText);
    m_compactStatus->setText(failedClosed ? attention+QStringLiteral("\n자동 진행은 안전하게 멈췄습니다. 공식 화면에서 직접 받은 자료는 "
        "「받은 자료 불러오기」로 지도에 올릴 수 있습니다.") : attention);
    m_progress->setFormat(m_failurePhase.isEmpty()?QStringLiteral("확인 필요"):QStringLiteral("실패"));
    if(!m_failurePhase.isEmpty())m_compactCancel->setText(QStringLiteral("닫기"));
  } else if(completed) {
    m_currentStage->setText(QStringLiteral("완료"));
    m_compactStatus->setText(QStringLiteral("수치지형도를 준비했습니다."));
    m_progress->setRange(0,100);m_progress->setValue(100);m_progress->setFormat(QStringLiteral("완료"));
    m_compactCancel->setText(QStringLiteral("닫기"));
  } else if(stopped) {
    m_currentStage->setText(QStringLiteral("중지됨 · %1").arg(phase));
    m_compactStatus->setText(QStringLiteral("수치지형도 받기를 중지했습니다."));
    m_progress->setFormat(QStringLiteral("중지됨"));m_compactCancel->setText(QStringLiteral("닫기"));
  } else {
    const bool preparing=(m_automationStage==QLatin1String("complete") || m_automationStage.isEmpty()) &&
        !m_preparationPhase.isEmpty() && m_preparationClock.isValid();
    QString detail=QStringLiteral("현재 단계 %1초 경과 · 지도 작업을 계속할 수 있습니다.")
        .arg((preparing?m_preparationClock.elapsed():m_phaseClock.elapsed())/1000);
    if(!m_preparationPhase.isEmpty())detail+=QStringLiteral("\n%1 · %2").arg(m_preparationPhase,m_preparationDetail);
    m_compactStatus->setText(detail);
    if(m_automationStage==QLatin1String("transfer") && m_totalBytes>0) {
      m_progress->setRange(0,100);
      m_progress->setValue(std::clamp(int(100.0*double(m_receivedBytes)/double(m_totalBytes)),0,100));
      m_progress->setFormat(QStringLiteral("현재 파일 수신 %p%"));
    }
  }
}

void KaTopographicBrowser::setDownloadRoot(const QString& directory) {
  if(directory.isEmpty() || !QDir::isAbsolutePath(directory)) {
    automationMessage(QStringLiteral("조사 폴더의 지형도 저장 위치를 확인하지 못했습니다."));return;
  }
  m_downloadDirectory=QDir(directory).absolutePath();
}

KaTopographicBrowser::~KaTopographicBrowser() {
  m_destroying=true; ++m_automationGeneration; m_automationTimer->stop();
  if (!m_profile) {
    delete m_tabs;
    m_tabs = nullptr;
    return;
  }
  // WebEngine requires every page to die before its shared profile.
  for (auto* request : m_profile->findChildren<QWebEngineDownloadRequest*>()) {
    disconnect(request, nullptr, this, nullptr);
    if (!request->isFinished()) request->cancel();
  }
  for (auto* view : m_tabs->findChildren<QWebEngineView*>()) {
    disconnect(view, nullptr, this, nullptr);
    disconnect(view->page(), nullptr, this, nullptr);
  }
  delete m_tabs;
  m_tabs = nullptr;
  delete m_profile;
  m_profile = nullptr;
}

void KaTopographicBrowser::ensureEngine() {
  if (m_profile) return;
  m_profile = new QWebEngineProfile(this);
  m_profile->setPersistentCookiesPolicy(QWebEngineProfile::NoPersistentCookies);
  m_profile->setHttpCacheType(QWebEngineProfile::MemoryHttpCache);
  connect(m_profile, &QWebEngineProfile::downloadRequested, this, &KaTopographicBrowser::requestDownload);
}

QWebEnginePage* KaTopographicBrowser::addPage(QWebEnginePage* opener) {
  ensureEngine();
  auto* view = new QWebEngineView(m_tabs);
  auto* page = new TopographicPage(m_profile, view, [this](QWebEnginePage* source) { return addPage(source); },
      [this](const QUrl& url) { offerExternalProgram(url); },[this]{
        if(!m_compactMode)return false;
        automationMessage(QStringLiteral("자동 받기에 추가 확인이 필요합니다. 로그인 설정과 연결 상태를 확인해 주세요."));
        return true;
      },opener);
  view->setPage(page);
  QWebEngineScript transferSetup;
  transferSetup.setName(QStringLiteral("ka-hgis-official-browser-transfer"));
  transferSetup.setWorldId(QWebEngineScript::MainWorld);
  transferSetup.setInjectionPoint(QWebEngineScript::DocumentReady);
  transferSetup.setRunsOnSubFrames(false);
  transferSetup.setSourceCode(m_automationScript+QStringLiteral("({command:'configureTransfer',testMode:")+
    (QStandardPaths::isTestModeEnabled()?QStringLiteral("true"):QStringLiteral("false"))+QStringLiteral("});"));
  page->scripts().insert(transferSetup);
  page->settings()->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, true);
  page->settings()->setUnknownUrlSchemePolicy(QWebEngineSettings::AllowUnknownUrlSchemesFromUserInteraction);
  const int index = m_tabs->addTab(view, QStringLiteral("국토정보맵")); m_tabs->setCurrentIndex(index);
  if(opener && opener==m_orderPage && (m_automationStage==QLatin1String("order") || m_automationStage==QLatin1String("files"))) {
    page->setProperty("kaTopographicOrderGeneration",m_automationGeneration);
    page->setProperty("kaTopographicOrderSheet",m_currentRecord.value(QStringLiteral("num")).toString());
  }
  connect(view,&QWebEngineView::loadStarted,this,[this,page]{
    // Named downForm windows can be reused. Only a fresh navigation from the
    // current submitting page may supply the current order's file list.
    if(page->openerPage() && page->openerPage()==m_orderPage &&
       (m_automationStage==QLatin1String("order") || m_automationStage==QLatin1String("files"))) {
      page->setProperty("kaTopographicOrderGeneration",m_automationGeneration);
      page->setProperty("kaTopographicOrderSheet",m_currentRecord.value(QStringLiteral("num")).toString());
    }
  });
  connect(view,&QWebEngineView::loadFinished,this,[this,page](bool ok){
    const auto number=m_currentRecord.value(QStringLiteral("num")).toString();
    if(!ok || page->openerPage()!=m_orderPage || !transferPage(page->url()) ||
        page->property("kaTopographicOrderGeneration").toULongLong()!=m_automationGeneration ||
        page->property("kaTopographicOrderSheet").toString()!=number)return;
    const QPointer<KaTopographicBrowser> alive(this);const QPointer<QWebEnginePage> candidate(page);
    const auto generation=m_automationGeneration;
    // fnFileDownload's verified official window name, rather than tab order.
    page->runJavaScript(QStringLiteral("window.name === 'downForm'"),[alive,candidate,generation,number](const QVariant& value){
      if(alive && candidate && value.toBool() && alive->m_automationGeneration==generation &&
          alive->m_currentRecord.value(QStringLiteral("num")).toString()==number &&
          (alive->m_automationStage==QLatin1String("order") || alive->m_automationStage==QLatin1String("files")))
        alive->m_orderDownloadPage=candidate;
    });
  });
  connect(view, &QWebEngineView::titleChanged, this, [this, view](const QString& title) {
    const int tab = m_tabs->indexOf(view); if (tab >= 0) m_tabs->setTabText(tab, title.left(30));
  });
  // Page loading is not download progress. File progress stays in the hidden
  // detail list; the compact indicator covers transfer and map preparation.
  connect(page, &QWebEnginePage::loadingChanged, this, [this, view](const QWebEngineLoadingInfo& info) {
    // loadFinished(false) also covers the official login popup closing during
    // its redirect. Qt emits that coarse signal before loadingChanged, so
    // classify the detailed state here instead of caching it for loadFinished.
    if(info.status()!=QWebEngineLoadingInfo::LoadSucceededStatus &&
        info.status()!=QWebEngineLoadingInfo::LoadFailedStatus)return;
    const bool ok=info.status()==QWebEngineLoadingInfo::LoadSucceededStatus;
    if (m_tabs->currentWidget() == view)
      m_status->setText(ok ? QStringLiteral("공식 사이트의 안내에 따라 로그인하고 자료를 선택하세요.")
                          : QStringLiteral("페이지를 불러오지 못했습니다. 인터넷 연결을 확인한 뒤 새로고침하세요."));
    if(!ok && m_automationTimer->isActive() && m_automationStage!=QLatin1String("transfer")) {
      failAutomation(QStringLiteral("공식 사이트 페이지를 불러오지 못했습니다. 인터넷 연결을 확인한 뒤 다시 시도하세요."));return;
    }
  });
  connect(view, &QWebEngineView::loadFinished, this, [this, view](bool ok) {
    if(ok && trustedPage(view->url()) && view->url().path()==QLatin1String("/ms/map/NlipMap.do") &&
       (m_automationStage==QLatin1String("openForm") || m_automationStage==QLatin1String("form") ||
        m_automationStage==QLatin1String("submit"))) {
      ++m_automationGeneration; m_scriptInFlight=false; m_automationStage=QStringLiteral("select");
    }
  });
  connect(page, &QWebEnginePage::windowCloseRequested, this, [this, view] { closeTab(m_tabs->indexOf(view)); });
  connect(view, &QWebEngineView::renderProcessTerminated, this,
      [this](QWebEnginePage::RenderProcessTerminationStatus, int) {
    failAutomation(QStringLiteral("수치지형도 연결이 중단됐습니다. 다시 받기를 눌러 주세요. 지도 작업은 계속할 수 있습니다."));
  });
  return page;
}

void KaTopographicBrowser::closeTab(int index) {
  if (index < 0 || index >= m_tabs->count()) return;
  if (m_tabs->count() == 1) return;
  QWidget* view = m_tabs->widget(index); m_tabs->removeTab(index); view->deleteLater();
}

void KaTopographicBrowser::navigate(const QUrl& url) {
  m_explicitNavigation = true;
  if (!m_tabs || m_tabs->count() == 0) addPage();
  if (auto* view = qobject_cast<QWebEngineView*>(m_tabs->currentWidget())) view->load(url);
}

void KaTopographicBrowser::offerExternalProgram(const QUrl& url) {
  if(m_compactMode) {
    failAutomation(QStringLiteral("자동 전송을 계속할 수 없습니다. 전송 프로그램 연결이 필요합니다. 지도 작업은 계속할 수 있습니다."));
    return;
  }
  auto* box = new QMessageBox(QMessageBox::Information, QStringLiteral("외부 전송 프로그램"),
      QStringLiteral("공식 사이트가 별도 전송 프로그램 연결을 요청했습니다 (%1). 설치된 프로그램으로 계속하려면 아래 버튼을 누르세요. 취소하면 HGIS에서 계속 작업할 수 있습니다.").arg(url.scheme()),
      QMessageBox::Cancel, this);
  box->setAttribute(Qt::WA_DeleteOnClose);
  auto* open = box->addButton(QStringLiteral("외부 전송 프로그램으로 연결"), QMessageBox::AcceptRole);
  connect(box, &QMessageBox::buttonClicked, this, [this, box, open, url](QAbstractButton* button) {
    if (button == open && !QDesktopServices::openUrl(url))
      m_status->setText(QStringLiteral("전송 프로그램을 열지 못했습니다. 공식 사이트의 설치 안내를 확인하세요."));
  });
  box->open();
}

bool KaTopographicBrowser::trustedPage(const QUrl& url) const {
  return url.scheme()==m_portalUrl.scheme() && url.host()==m_portalUrl.host() &&
         url.port()==m_portalUrl.port();
}

bool KaTopographicBrowser::transferPage(const QUrl& url) const {
  const bool localTest=QStandardPaths::isTestModeEnabled() && trustedPage(url) &&
    (url.host()==QLatin1String("127.0.0.1") || url.host()==QLatin1String("localhost"));
  if(localTest && url.path()==QLatin1String("/pd/purchs/download.do"))return true;
  return url.path()==QLatin1String("/nlippd/pd/innorix/innorixdownLoad.do") &&
    (localTest || (url.scheme()==QLatin1String("https") && url.host()==QLatin1String("nlippd.ngii.go.kr") && url.port(443)==443));
}

void KaTopographicBrowser::setPortalTestUrl(const QUrl& url) {
  if(QStandardPaths::isTestModeEnabled() && url.scheme()==QLatin1String("http") &&
      (url.host()==QLatin1String("127.0.0.1") || url.host()==QLatin1String("localhost")) &&
      url.path()==QLatin1String("/ms/map/NlipMap.do")) m_portalUrl=url;
}

void KaTopographicBrowser::automationMessage(const QString& message) {
  m_status->setText(message);
  m_automationAttention=message;updateProgress();
  if(message!=m_lastAutomationMessage){m_lastAutomationMessage=message;emit automationNeedsInput(message);}
}

void KaTopographicBrowser::stopAutomatic() {
  ++m_automationGeneration;m_automationTimer->stop();m_scriptInFlight=false;
  if(m_transferPage)m_transferPage->setProperty("kaTopographicStoppedTransfer",true);
  m_pendingScope={};m_automationStage=QStringLiteral("stopped");m_requestedNumbers.clear();m_transferPage.clear();
  m_orderPage.clear();m_orderDownloadPage.clear();
  for(int i=0;i<m_tabs->count();++i)if(auto* view=qobject_cast<QWebEngineView*>(m_tabs->widget(i)))
    if(trustedPage(view->url()))view->page()->runJavaScript(QStringLiteral(
      "window.__kaTopographicSelection=null;window.__kaTopographicOrder=null;window.__kaTopographicSubmission=null;"));
  if (m_profile)
  for(auto* request:m_profile->findChildren<QWebEngineDownloadRequest*>())
    if(request->property("kaTopographicAutomatic").toBool() && !request->isFinished())request->cancel();
  m_status->setText(QStringLiteral("자동 진행을 중지했습니다. 이미 신청한 자료는 공식 신청 내역에서 확인할 수 있습니다."));
  m_orderSheetsText.clear();m_failureOutlineText.clear();
  m_automationAttention.clear();m_activityAttention.clear();updateProgress();
  m_failurePhase.clear();m_activityFailurePhase.clear();m_transferClock.invalidate();updateProgress();
}

void KaTopographicBrowser::prepareSheets(double x, double y, double radius, const QStringList& numbers,
                                        const QStringList& cachedNumbers) {
  QStringList unique=numbers; unique.removeDuplicates(); unique.sort();
  QStringList cached=cachedNumbers;cached.removeDuplicates();cached.sort();
  const QRegularExpression sheet(property("kaDemMode").toBool()?QStringLiteral("^[0-9]{5,6}$"):QStringLiteral("^[0-9]{6}$"));
  if(!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(radius) || radius<=0 ||
     unique.size()>100 || std::any_of(unique.cbegin(),unique.cend(),[&](const QString& n){return !sheet.match(n).hasMatch();}) ||
     std::any_of(cached.cbegin(),cached.cend(),[&](const QString& n){return !sheet.match(n).hasMatch();})) {
    automationMessage(QStringLiteral("자동 선택할 1:25,000 도엽 목록과 반경을 확인하지 못했습니다.")); return;
  }
  const QJsonObject scope{{QStringLiteral("x"),x},{QStringLiteral("y"),y},{QStringLiteral("radius"),radius},
                         {QStringLiteral("numbers"),QJsonArray::fromStringList(unique)},
                         {QStringLiteral("cachedNumbers"),QJsonArray::fromStringList(cached)}};
  const QString key=QString::fromUtf8(QJsonDocument(scope).toJson(QJsonDocument::Compact));
  if(m_scopeKey==key && m_automationStage!=QLatin1String("stopped")) return;
  // The portal query uses a 5 km circle. A few metres of canvas wobble must
  // not start the same 도엽 order again.
  if(m_automationStage!=QLatin1String("stopped") && !m_requestedNumbers.isEmpty() &&
      m_requestedNumbers==unique) return;
  if(m_automationStage==QLatin1String("order") || m_automationStage==QLatin1String("files") || m_automationStage==QLatin1String("download") ||
      m_automationStage==QLatin1String("transfer")) {
    m_pendingScope=scope;
    m_status->setText(QStringLiteral("현재 신청한 도엽을 받은 뒤 새 조사 범위의 자료를 확인합니다.")); return;
  }
  startAutomation(scope);
}

void KaTopographicBrowser::startAutomation(const QJsonObject& scope) {
  m_cancelNotified=false;m_automationAttention.clear();m_failurePhase.clear();
  m_orderSheetsText.clear();m_failureOutlineText.clear();
  m_currentFileName.clear();m_receivedBytes=0;m_totalBytes=-1;m_sheetFileCount=0;m_receivedFileCount=0;
  m_phase=QStringLiteral("도엽 확인");m_phaseClock.restart();m_transferClock.invalidate();
  ++m_automationGeneration; m_scriptInFlight=false; m_waitTicks=0;
  m_scope=scope; m_pendingScope={}; m_officialRecords={}; m_currentRecord={}; m_sheetIndex=0;
  m_transferManifest={};m_sheetDownloadDirectories.clear();
  m_scopeKey=QString::fromUtf8(QJsonDocument(scope).toJson(QJsonDocument::Compact));
  QStringList requested;
  for (const auto& item : scope.value(QStringLiteral("numbers")).toArray())
    requested.append(item.toString());
  requested.removeDuplicates(); requested.sort();
  m_requestedNumbers=requested;
  m_transferPage.clear(); m_automationStage=QStringLiteral("select"); m_lastAutomationMessage.clear();
  m_orderPage.clear();m_orderDownloadPage.clear();
  for(int i=0;i<m_tabs->count();++i)if(auto* view=qobject_cast<QWebEngineView*>(m_tabs->widget(i)))
    if(trustedPage(view->url()))view->page()->runJavaScript(QStringLiteral(
      "window.__kaTopographicSelection=null;window.__kaTopographicOrder=null;window.__kaTopographicSubmission=null;"));
  if(m_automationScript.isEmpty()) {
    failAutomation(QStringLiteral("자동 선택 구성 파일을 찾지 못했습니다. 앱 설치 파일을 확인하세요."));return;
  }
  bool portalFound=false;
  for(int i=0;i<m_tabs->count();++i)if(auto* view=qobject_cast<QWebEngineView*>(m_tabs->widget(i))) {
    if(trustedPage(view->url()) && view->url().path()==QLatin1String("/ms/map/NlipMap.do")){portalFound=true;break;}
  }
  if(!portalFound)navigate(m_portalUrl);
  m_automationTimer->start();
  updateProgress();
}

void KaTopographicBrowser::pollAutomation() {
  if(m_destroying || m_scriptInFlight || m_automationStage.isEmpty() ||
      m_automationStage==QLatin1String("stopped") || m_automationStage==QLatin1String("complete"))return;
  if(m_automationStage==QLatin1String("transfer")) {
    updateProgress();
    return;
  }
  QWebEnginePage* page=nullptr;
  const bool download=m_automationStage==QLatin1String("files") || m_automationStage==QLatin1String("download");
  const QString path=download?QStringLiteral("/pd/purchs/download.do"):QStringLiteral("/ms/map/NlipMap.do");
  for(int i=0;i<m_tabs->count();++i)if(auto* view=qobject_cast<QWebEngineView*>(m_tabs->widget(i))) {
    if(download ? (transferPage(view->url()) && view->page()==m_orderDownloadPage) :
        (trustedPage(view->url()) && view->url().path()==path))page=view->page();
  }
  bool loginPage=false;
  if(!download) {
    for(const QString& loginPath:{QStringLiteral("/mn/loginPopup.do"),
        QStringLiteral("/anyid/common/login.do"),
        QStringLiteral("/anyid/common/anyidLogin.do"),QStringLiteral("/member/login.do")}) {
      for(int i=0;i<m_tabs->count();++i)if(auto* view=qobject_cast<QWebEngineView*>(m_tabs->widget(i))) {
        const QUrl url=view->url();
        const bool member=url.path()==QLatin1String("/member/login.do") &&
            url.scheme()==QLatin1String("https") && url.port(-1)==-1 &&
            (url.host()==QLatin1String("www.ngii.go.kr") || url.host()==QLatin1String("ngii.go.kr"));
        if(url.path()==loginPath && (trustedPage(url) || member)){page=view->page();loginPage=true;}
      }
    }
  }
  if(!page) {
    if(++m_waitTicks==75)failAutomation(download?QStringLiteral("파일 전송 화면이 열리지 않았습니다. 신청 내역과 연결 상태를 확인하세요."):
      QStringLiteral("공식 사이트의 응답을 확인하지 못했습니다. 계정 설정과 연결 상태를 확인하세요."));
    return;
  }
  QJsonObject input=m_scope;
  const QString command=loginPage?QStringLiteral("login"):m_automationStage;
  if(command==QLatin1String("select"))setPhase(QStringLiteral("도엽 확인"));
  else if(command==QLatin1String("login") || (command==QLatin1String("openForm") && m_phase!=QStringLiteral("신청")))setPhase(QStringLiteral("로그인"));
  else if(command==QLatin1String("form") || command==QLatin1String("submit"))setPhase(QStringLiteral("신청"));
  else if(command==QLatin1String("order") || command==QLatin1String("files"))setPhase(QStringLiteral("파일 준비"));
  else if(command==QLatin1String("download"))setPhase(QStringLiteral("다운로드"));
  input.insert(QStringLiteral("command"),command);if(property("kaDemMode").toBool())input.insert(QStringLiteral("product"),QStringLiteral("dem"));
  input.insert(QStringLiteral("diagnostics"),qEnvironmentVariableIntValue("KA_HGIS_NGII_DIAGNOSTICS")==1);
  input.insert(QStringLiteral("generation"),QString::number(m_automationGeneration));
  input.insert(QStringLiteral("number"),m_currentRecord.value(QStringLiteral("num")));
  input.insert(QStringLiteral("record"),m_currentRecord);
  input.insert(QStringLiteral("records"),m_officialRecords);
  input.insert(QStringLiteral("fileIndex"),m_receivedFileCount);
  if(command==QLatin1String("form") || command==QLatin1String("submit")) {
    input.insert(QStringLiteral("birthDate"),m_birthDate->text().trimmed());
    input.insert(QStringLiteral("purpose"),m_purpose->text().trimmed());
  }
  if(loginPage) {
    input.insert(QStringLiteral("loginId"),m_loginId->text());
    input.insert(QStringLiteral("loginPassword"),m_loginPassword->text());
    input.insert(QStringLiteral("loginAttempt"),QString::number(m_loginAttempt));
  }
  // Mark irreversible stages first; a lost callback must not retry an order.
  if(command==QLatin1String("submit")){
    m_automationStage=QStringLiteral("order");m_orderPage=page;m_orderDownloadPage.clear();
  }
  if(command==QLatin1String("download")){
    m_automationStage=QStringLiteral("transfer");m_transferPage=page;m_waitTicks=0;
    m_transferClock.restart();
    page->setProperty("kaTopographicStoppedTransfer",false);
  }
  const auto generation=m_automationGeneration;
  const QPointer<KaTopographicBrowser> guard(this);
  const QPointer<QWebEnginePage> pageGuard(page);
  m_scriptInFlight=true;
  page->runJavaScript(m_automationScript+QLatin1Char('(')+QString::fromUtf8(QJsonDocument(input).toJson(QJsonDocument::Compact))+QStringLiteral(");"),
      [guard,pageGuard,generation,command](const QVariant& value) {
    if(!guard || guard->m_destroying || generation!=guard->m_automationGeneration)return;
    auto* self=guard.data(); self->m_scriptInFlight=false;
    if(!pageGuard)return;
    const QJsonObject result=QJsonObject::fromVariantMap(value.toMap());
    const auto diagnostic=result.value(QStringLiteral("diagnostics")).toObject();
    if(!diagnostic.isEmpty() && qEnvironmentVariableIntValue("KA_HGIS_NGII_DIAGNOSTICS")==1) {
      const auto bytes=QJsonDocument(diagnostic).toJson(QJsonDocument::Compact);
      if(bytes!=self->m_lastDiagnostic){self->m_lastDiagnostic=bytes;qInfo().noquote()<<"NGII transfer diagnostics:"<<bytes;}
    }
    const QString state=result.value(QStringLiteral("state")).toString();
    if(state.isEmpty()) {
      if(++self->m_waitTicks==75)self->failAutomation(QStringLiteral("포털 자동 진행 응답을 확인하지 못했습니다. 연결 상태를 확인한 뒤 다시 시도하세요."));
      return;
    }
    if(state==QLatin1String("blocked")) {
      self->failAutomation(result.value(QStringLiteral("message")).toString());return;
    }
    if(state==QLatin1String("input") || state==QLatin1String("login")) {
      self->automationMessage(result.value(QStringLiteral("message")).toString());
      if(command==QLatin1String("submit"))self->m_automationStage=QStringLiteral("form");
      if(command==QLatin1String("download"))self->m_automationStage=QStringLiteral("files");
      return;
    }
    if(command==QLatin1String("openForm") && (state==QLatin1String("waitingForm") || state==QLatin1String("openingForm")))
      self->setPhase(QStringLiteral("신청"));
    if(state==QLatin1String("selected") || state==QLatin1String("cached") ||
        state==QLatin1String("form") || state==QLatin1String("ready") || state==QLatin1String("orderReady")) {
      self->m_automationAttention.clear();self->updateProgress();
    }
    if(command==QLatin1String("submit") && state==QLatin1String("waitingForm")) {
      self->m_automationStage=QStringLiteral("form");return;
    }
    if(command==QLatin1String("order") && state==QLatin1String("orderReady")) {
      self->m_automationStage=QStringLiteral("files");self->m_waitTicks=0;self->setPhase(QStringLiteral("파일 준비"));return;
    }
    if(command==QLatin1String("order") && state==QLatin1String("delayed")) {
      self->automationMessage(result.value(QStringLiteral("message")).toString());return;
    }
    if(command==QLatin1String("select") && state==QLatin1String("unavailable")) {
      self->m_automationStage=QStringLiteral("stopped");self->m_automationTimer->stop();
      emit self->selectionUpdated(QJsonObject{{QStringLiteral("items"),QJsonArray()},
        {QStringLiteral("unavailable"),result.value(QStringLiteral("unavailable"))},
        {QStringLiteral("mapCrs"),QStringLiteral("EPSG:5179")}});
      self->automationMessage(QStringLiteral("이 범위에서 제공 중인 1:25,000 DXF 도엽이 없습니다."));
    } else if(command==QLatin1String("select") && (state==QLatin1String("selected") || state==QLatin1String("cached"))) {
      const auto all=result.value(QStringLiteral("items")).toArray();
      QJsonArray numbers;for(const auto& record:all)numbers.append(record.toObject().value(QStringLiteral("num")));
      self->m_scope.insert(QStringLiteral("numbers"),numbers);
      self->m_officialRecords=result.value(QStringLiteral("downloadItems")).toArray();
      if(self->m_officialRecords.isEmpty()) {
        self->m_automationStage=QStringLiteral("complete");self->m_automationTimer->stop();
        self->m_status->setText(QStringLiteral("필요한 지형도를 모두 보관하고 있습니다. 다시 신청하지 않습니다."));
        self->updateProgress();
        emit self->selectionUpdated(QJsonObject{{QStringLiteral("items"),all},
          {QStringLiteral("cached"),result.value(QStringLiteral("cached"))},
          {QStringLiteral("unavailable"),result.value(QStringLiteral("unavailable"))},
          {QStringLiteral("mapCrs"),QStringLiteral("EPSG:5179")}});
        return;
      }
      if(self->m_sheetIndex>=self->m_officialRecords.size())return;
      {
        // Show exactly which sheets the automatic order is about to apply for.
        QStringList order;
        for(const auto& item:std::as_const(self->m_officialRecords)) {
          const auto record=item.toObject();
          order.append(QStringLiteral("%1 %2").arg(record.value(QStringLiteral("num")).toString(),
              record.value(QStringLiteral("name")).toString()).trimmed());
        }
        const int total=order.size();
        if(total>8){order=order.mid(0,8);order.append(QStringLiteral("외 %1장").arg(total-8));}
        self->m_orderSheetsText=QStringLiteral("공식 신청할 도엽 %1장 · %2 — 중지하려면 「취소」를 누르세요.")
            .arg(total).arg(order.join(QStringLiteral(", ")));
      }
      self->m_currentRecord=self->m_officialRecords[self->m_sheetIndex].toObject();
      self->m_automationStage=QStringLiteral("openForm"); self->m_waitTicks=0;
      emit self->selectionUpdated(QJsonObject{{QStringLiteral("items"),all},
        {QStringLiteral("cached"),result.value(QStringLiteral("cached"))},
        {QStringLiteral("unavailable"),result.value(QStringLiteral("unavailable"))},
        {QStringLiteral("mapCrs"),QStringLiteral("EPSG:5179")}});
    } else if(command==QLatin1String("openForm") && state==QLatin1String("form")) {
      if(!self->m_suppliedCredentials)self->m_loginPassword->clear();
      self->m_automationStage=QStringLiteral("form");self->m_waitTicks=0;
    } else if(command==QLatin1String("form") && state==QLatin1String("ready")) {
      self->m_automationStage=QStringLiteral("submit");
    } else if(command==QLatin1String("files") && state==QLatin1String("ready")) {
      self->m_expectedSheetFiles=result.value(QStringLiteral("files")).toInt();
      self->m_transferManifest=result.value(QStringLiteral("manifest")).toArray();
      self->m_sheetDownloadDirectories.clear();
      if(!self->m_transferManifest.isEmpty()) {
        QSet<QString> names,ids,numbers;
        bool valid=self->m_transferManifest.size()==self->m_expectedSheetFiles;
        for(const auto& item:self->m_transferManifest) {
          const auto file=item.toObject();
          const QString name=file.value(QStringLiteral("name")).toString(),id=file.value(QStringLiteral("id")).toString();
          const QString number=file.value(QStringLiteral("number")).toString();
          const auto found=std::count_if(self->m_officialRecords.cbegin(),self->m_officialRecords.cend(),[&](const QJsonValue& record){
            return record.toObject().value(QStringLiteral("num")).toString()==number;
          });
          valid=valid && !name.isEmpty() && safeFileName(name)==name && !id.isEmpty() && !names.contains(name) &&
              !ids.contains(id) && found==1 && file.value(QStringLiteral("bytes")).toDouble()>0;
          names.insert(name);ids.insert(id);numbers.insert(number);
        }
        if(!valid || numbers.size()!=self->m_officialRecords.size()) {
          self->failAutomation(QStringLiteral("신청한 도엽과 파일 연결을 확인하지 못해 전송을 중단했습니다."));return;
        }
      }
      self->m_sheetFileCount=self->m_expectedSheetFiles;self->m_receivedFileCount=0;
      self->m_currentFileName.clear();self->m_receivedBytes=0;self->m_totalBytes=-1;
      self->m_sheetDownloadDirectory.clear();self->m_completedSheetPaths.clear();
      self->m_transferPage=pageGuard;self->m_automationStage=QStringLiteral("download");
    }
    if(guard)guard->updateProgress();
  });
}

void KaTopographicBrowser::requestDownload(QWebEngineDownloadRequest* request) {
  if (!request || request->state() != QWebEngineDownloadRequest::DownloadRequested) return;
  if(request->page() && request->page()->property("kaTopographicStoppedTransfer").toBool()) {
    request->cancel();
    automationMessage(QStringLiteral("중지한 자동 신청에서 늦게 시작된 다운로드를 취소했습니다. 다시 받으려면 공식 신청 내역을 새로 열어 주세요."));
    return;
  }
  const bool associated = request->page() && request->page()==m_transferPage &&
      m_automationStage==QLatin1String("transfer") && !m_currentRecord.isEmpty();
  const bool ownsProgress=associated || !m_automationTimer->isActive();
  QJsonObject officialRecord=associated?m_currentRecord:QJsonObject();
  const QString name = safeFileName(request->suggestedFileName());
  if(associated) {
    request->setProperty("kaTopographicAutomatic",true);
    if(!m_transferManifest.isEmpty()) {
      if(m_receivedFileCount>=m_transferManifest.size() ||
          m_transferManifest.at(m_receivedFileCount).toObject().value(QStringLiteral("name")).toString()!=name) {
        request->cancel();failAutomation(QStringLiteral("요청한 파일과 도착한 파일이 달라 전송을 중단했습니다."));return;
      }
      const auto number=m_transferManifest.at(m_receivedFileCount).toObject().value(QStringLiteral("number")).toString();
      officialRecord={};
      for(const auto& item:m_officialRecords)if(item.toObject().value(QStringLiteral("num")).toString()==number){officialRecord=item.toObject();break;}
      if(officialRecord.isEmpty()){request->cancel();failAutomation(QStringLiteral("받은 파일의 도엽 정보를 확인하지 못했습니다."));return;}
    }
    officialRecord.insert(QStringLiteral("source"),property("kaDemMode").toBool()?QStringLiteral("NGII demQuery.do"):QStringLiteral("NGII suchiQuery.do"));
    officialRecord.insert(QStringLiteral("requestScope"),m_scope);
    if(request->totalBytes()>1000000000) {
      request->cancel();failAutomation(QStringLiteral("파일 크기가 1GB를 넘어서 자동 내려받기를 중단했습니다. 공식 목록에서 나누어 내려받아 주세요."));return;
    }
  }
  const auto generation=m_automationGeneration;
  if (!QDir().mkpath(m_downloadDirectory)) {
    request->cancel(); failAutomation(QStringLiteral("다운로드 폴더를 만들지 못했습니다. 저장 공간과 쓰기 권한을 확인하세요.")); return;
  }
  const QString root = QFileInfo(m_downloadDirectory).canonicalFilePath();
  const QString number=officialRecord.value(QStringLiteral("num")).toString();
  const QString existing=associated?(!m_transferManifest.isEmpty()?m_sheetDownloadDirectories.value(number):m_sheetDownloadDirectory):QString();
  const QString directory = existing.isEmpty()?QDir(root).filePath(QUuid::createUuid().toString(QUuid::WithoutBraces)):existing;
  if (root.isEmpty() || !QDir().mkpath(directory)) {
    request->cancel(); failAutomation(QStringLiteral("파일을 저장할 폴더를 만들지 못했습니다. 폴더 권한을 확인하세요.")); return;
  }
  if(associated)m_sheetDownloadDirectory=directory;
  if(associated && !m_transferManifest.isEmpty())m_sheetDownloadDirectories.insert(number,directory);
  if(!associated && !m_automationTimer->isActive()) {
    m_automationStage=QStringLiteral("transfer");m_sheetFileCount=1;m_receivedFileCount=0;
    m_failurePhase.clear();m_automationAttention.clear();setPhase(QStringLiteral("다운로드"));
  }
  if(ownsProgress) {
    m_currentFileName=name;m_receivedBytes=0;m_totalBytes=request->totalBytes();
    m_transferClock.restart();updateProgress();
  }
  request->setDownloadDirectory(directory); request->setDownloadFileName(name);
  auto* row = new QTreeWidgetItem(m_downloads, {name, QString(), QStringLiteral("내려받는 중"), QString()});
  auto* progress = new QProgressBar(m_downloads); progress->setRange(0, 0);
  m_downloads->setItemWidget(row, 1, progress);
  auto* cancel = new QPushButton(QStringLiteral("취소"), m_downloads);
  m_downloads->setItemWidget(row, 3, cancel);
  const QPointer<QWebEngineDownloadRequest> guarded(request);
  connect(cancel, &QPushButton::clicked, this, [guarded] { if (guarded) guarded->cancel(); });
  auto update = [this,guarded,progress,generation,name,associated,ownsProgress] {
    if (!guarded) return;
    if(guarded->state()!=QWebEngineDownloadRequest::DownloadInProgress &&
        guarded->state()!=QWebEngineDownloadRequest::DownloadRequested)return;
    const qint64 total = guarded->totalBytes();
    if(guarded->receivedBytes()>guarded->property("kaTopographicLastReceived").toLongLong()) {
      guarded->setProperty("kaTopographicLastReceived",guarded->receivedBytes());
      guarded->setProperty("kaTopographicLastTotal",total);
    }
    if (total > 0) { progress->setRange(0, 100); progress->setValue(int(100. * double(guarded->receivedBytes()) / double(total))); }
    if(!ownsProgress || generation!=m_automationGeneration || !m_failurePhase.isEmpty())return;
    if(associated && m_automationStage!=QLatin1String("transfer"))return;
    if(m_currentFileName!=name || m_receivedBytes!=guarded->receivedBytes())m_transferClock.restart();
    m_currentFileName=name;m_receivedBytes=guarded->receivedBytes();m_totalBytes=total;
    updateProgress();
  };
  connect(request, &QWebEngineDownloadRequest::receivedBytesChanged, this, [guarded,update,associated] {
    if(associated && guarded && guarded->receivedBytes()>1000000000){guarded->cancel();return;}update();
  });
  connect(request, &QWebEngineDownloadRequest::totalBytesChanged, this, [guarded,update,associated] {
    if(associated && guarded && guarded->totalBytes()>1000000000){guarded->cancel();return;} update();
  });
  connect(request, &QWebEngineDownloadRequest::stateChanged, this,
      [this, guarded, row, cancel, progress, directory, name, officialRecord, generation,ownsProgress](QWebEngineDownloadRequest::DownloadState state) {
    if (state == QWebEngineDownloadRequest::DownloadInProgress || state == QWebEngineDownloadRequest::DownloadRequested) return;
    cancel->setEnabled(false); progress->setRange(0, 100);
    if(generation!=m_automationGeneration) {
      // This row belongs to the old request. Show its terminal state, while
      // keeping canceled callbacks away from current progress and map imports.
      row->setText(2,state==QWebEngineDownloadRequest::DownloadCancelled?QStringLiteral("취소됨"):
        (state==QWebEngineDownloadRequest::DownloadCompleted?QStringLiteral("수신 완료 · 지도 적재 취소"):QStringLiteral("중단됨")));
      return;
    }
    // Chromium may reset receivedBytes to zero on cancel/interruption. Retain
    // the last observed amount so a failed transfer never erases its evidence.
    if(ownsProgress && guarded) {
      m_currentFileName=name;
      m_receivedBytes=guarded->property("kaTopographicLastReceived").toLongLong();
      if(guarded->property("kaTopographicLastTotal").isValid())
        m_totalBytes=guarded->property("kaTopographicLastTotal").toLongLong();
    }
    if (state == QWebEngineDownloadRequest::DownloadCompleted) {
      const QString path = QDir(directory).filePath(name);
      if (!QFileInfo(path).isFile()) {
        row->setText(2,QStringLiteral("파일 확인 실패"));
        if(ownsProgress)failAutomation(QStringLiteral("받은 파일을 확인하지 못했습니다. 저장 공간과 폴더 권한을 확인한 뒤 다시 시도하세요."));
        return;
      }
      progress->setValue(100); row->setText(2, QStringLiteral("완료"));
      if(ownsProgress)++m_receivedFileCount;
      m_status->setText(QStringLiteral("내려받기가 완료되었습니다. 받은 자료를 확인해 지도에 불러오세요."));
      if(officialRecord.isEmpty()) {
        if(ownsProgress){m_automationStage=QStringLiteral("complete");updateProgress();}
        emit fileDownloaded(path);
      }
      else {
        if(generation!=m_automationGeneration)return;
        if(m_automationStage!=QLatin1String("transfer"))return;
        m_completedSheetPaths.append(path);
        if(!m_transferManifest.isEmpty()) {
          --m_expectedSheetFiles;
          const auto number=officialRecord.value(QStringLiteral("num")).toString();
          bool sheetComplete=true;
          for(int i=m_receivedFileCount;i<m_transferManifest.size();++i)
            if(m_transferManifest.at(i).toObject().value(QStringLiteral("number")).toString()==number){sheetComplete=false;break;}
          const QPointer<KaTopographicBrowser> alive(this);
          if(sheetComplete) {
            ++m_sheetIndex;
            const auto completed=m_completedSheetPaths;
            for(const auto& completedPath:completed) {
              if(QFileInfo(completedPath).absolutePath()!=directory)continue;
              const auto suffix=QFileInfo(completedPath).suffix().toLower();
              if(suffix==QLatin1String("xml")||(!property("kaDemMode").toBool()&&suffix!=QLatin1String("dxf")&&suffix!=QLatin1String("zip")))continue;
              emit sheetDownloaded(completedPath,officialRecord);
              if(!alive || generation!=m_automationGeneration || m_automationStage!=QLatin1String("transfer"))return;
            }
          }
          if(m_expectedSheetFiles>0) {
            // The SDK reuses one _self anchor. Request the next ID only after
            // Qt has finished saving this file, including slow response headers.
            m_automationStage=QStringLiteral("download");m_phaseClock.restart();m_transferClock.restart();
          } else {
            m_transferPage.clear();
            if(!m_pendingScope.isEmpty()){const auto pending=m_pendingScope;startAutomation(pending);}
            else {m_automationStage=QStringLiteral("complete");m_automationTimer->stop();
              m_status->setText(QStringLiteral("선택한 지형도 파일을 모두 받았습니다. 지도 적재 결과를 확인합니다."));}
          }
          updateProgress();return;
        }
        if(--m_expectedSheetFiles<=0) {
          // Keep metadata sidecars beside their data and wait for all companions.
          // XML contributes to completion, but is never imported as a map layer.
          const QStringList completed=m_completedSheetPaths;
          const QPointer<KaTopographicBrowser> alive(this);
          for(const auto& completedPath:completed) {
            const QString suffix=QFileInfo(completedPath).suffix().toLower();
            if(suffix==QLatin1String("xml")||(!property("kaDemMode").toBool()&&suffix!=QLatin1String("dxf")&&suffix!=QLatin1String("zip")))continue;
            emit sheetDownloaded(completedPath,officialRecord);
            if(!alive || generation!=m_automationGeneration || m_automationStage!=QLatin1String("transfer"))return;
          }
          m_transferPage.clear();
          if(!m_pendingScope.isEmpty()) {
            const auto scope=m_pendingScope;
            startAutomation(scope);
          } else if(++m_sheetIndex<m_officialRecords.size()) {
            m_currentRecord=m_officialRecords[m_sheetIndex].toObject();
            m_automationStage=QStringLiteral("openForm");m_waitTicks=0;
            m_currentFileName.clear();m_sheetFileCount=0;m_receivedFileCount=0;m_totalBytes=-1;m_receivedBytes=0;
            setPhase(QStringLiteral("로그인"));
          } else {
            m_automationStage=QStringLiteral("complete");m_automationTimer->stop();
            m_status->setText(QStringLiteral("선택한 지형도 파일을 모두 받았습니다. 좌표 정보와 자료를 확인합니다."));
            updateProgress();
          }
        }
        updateProgress();
      }
    } else if (state == QWebEngineDownloadRequest::DownloadCancelled) {
      row->setText(2, QStringLiteral("취소됨"));
      if(!officialRecord.isEmpty() && generation==m_automationGeneration) {
        failAutomation(QStringLiteral("현재 파일 수신이 취소되어 다음 도엽 신청을 중지했습니다. 기존 지도와 완료한 파일은 유지됩니다."));
      } else if(ownsProgress) {
        m_automationStage=QStringLiteral("stopped");updateProgress();
      }
    } else {
      row->setText(2, QStringLiteral("중단됨 · 다시 내려받으세요"));
      if (guarded) row->setToolTip(2, guarded->interruptReasonString());
      if(ownsProgress)failAutomation(QStringLiteral("내려받기가 중단됐습니다. %1\n인터넷 연결과 저장 공간을 확인한 뒤 다시 시도하세요.")
        .arg(guarded?guarded->interruptReasonString():QStringLiteral("전송 연결 끊김")));
    }
  });
  request->accept();
}
