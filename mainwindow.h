#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QNetworkAccessManager>
#include <QLabel>
#include <QProcess>
#include <QFuture>
#include <QFutureWatcher>
#include <QTime>
#include <QFile>
#include <QSystemTrayIcon>
#include <QAtomicInt>
#include <QToolButton>

class Settings;
class LoginServers;
class GameProcess;
class SelfUpdater;
class GameMods;
class FileScanner;

namespace Ui {
  class MainWindow;
}

class MainWindow : public QMainWindow
{
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = 0);
  ~MainWindow();

  static QString patchUrl;
  static QString newsUrl;
  static QString gameExecutable;
  static const QString version;
  static QString selfUpdateUrl;  

public slots:
  void showSettings();
  void statusXmlIsReady(QNetworkReply*);
  void startSWG();
  void loadFinished();
  //void fullScanFinished();

  void startFullScan(bool forceConfigRestore = false);
  static QVector<QPair<QString, qint64> > getRequiredFiles();
  void downloadFinished();
  void downloadFileFinished(QNetworkReply *reply);
  void readBasiliskServerStatus();
  void readNovaServerStatus();
  void readBimaServerStatus();
  void webPageLoadFinished(bool ok);
  void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
  void closeEvent(QCloseEvent *event);

  void triggerNews();
  void updateBasicLoadProgress(QString successFile);
  void updateFullScanProgress(QString successFile, bool success);
  void triggerMultipleInstances(bool newValue);
  void startFileDownload();
  void updateServerStatus();
  void updateLoginServerList();
  void gameProcessFinished(GameProcess* process, int);
  void closeTab(int index);
  void checkForUpdates();
  void requiredFileDownloadFileFinished(QNetworkReply* reply);
  void patchesDownloadFileFinished(QNetworkReply* reply);
  void runUpdateCheckTimer();
  void startSWGSetup();
  void showAboutDialog();
  QFile* getRequiredFilesFile();
  void systemTrayActivated(QSystemTrayIcon::ActivationReason reason);
  void deleteProfiles();
  void startLoadBasicCheck();
  void showGameModsOptions();
  void showMacroEditor();

  void updateDonationMeter();

  //void fullScanFile(const QString& file, const QString& name, qint64, const QString &md5);
  void addFileToDownloadSlot(QString file); 
  //int fullScanSingleThreaded(bool);
  //void fullScanMultiThreaded(bool);

  void toolBarOrientationChanged(Qt::Orientation);
  void startKodanCalculator();
  void installSWGEmu();

  void enableStart();

  bool decrementFullScanWorkingThreads() {
      return fullScanWorkingThreads.deref();
  }

  volatile bool doCancelWorkingThreads() {
      return cancelWorkingThreads;
  }

  void appendToFilesToDownloadStringList(const QString& file) {
      filesToDownload.append(file);
  }

signals:
  void startDownload();
  //void requiredFileExists(QString);
  //void fullScannedFile(QString, bool);
  void fileDownloaded(QString);
  //void addFileToDownload(QString);

private:
  Ui::MainWindow *ui;
  LoginServers* loginServers;
  Settings* settings;
  QNetworkAccessManager networkAccessManager;
  QNetworkAccessManager clientFilesNetworkAccessManager;
  QNetworkAccessManager novaNetworkAccessManager;
  QNetworkAccessManager requiredFilesNetworkManager;
  QNetworkAccessManager patchesNetworkManager;
  QFutureWatcher<int> loadWatcher;
  QFutureWatcher<int> fullScanWatcher;
  QFutureWatcher<int> downloadWatcher;
  volatile bool cancelWorkingThreads;
  QStringList filesToDownload;
  int requiredFilesCount;
  int currentReadFiles;
  int nextFileToDownload;
  QTime lastReceivedBytesTime;
  quint64 lastReceivedBytes;
  QVector<GameProcess*> gameProcesses;
  SelfUpdater* silentSelfUpdater;

  QSystemTrayIcon* systemTrayIcon;
  QMenu* systemTrayMenu;
  QAction* closeAction;
  volatile bool runningFullScan;
  int gameProcessesCount;

  int updateTimeCounter;

  QAtomicInt fullScanWorkingThreads;

  QVector<QToolButton*> toolButtons;
  FileScanner* fileScanner;

  //GameMods* gameMods;

};

#endif // MAINWINDOW_H

