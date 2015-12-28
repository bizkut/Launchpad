#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "settings.h"
#include <QNetworkReply>
#include <QtXml>
#include "statusxmlcontenthandler.h"
#include <QProcess>
#include <QMessageBox>
#include <iostream>
#include <QDesktopWidget>
#include "loginservers.h"
#include "configparser.h"
#include "gameprocess.h"
#ifdef Q_OS_WIN32
#include "windebugmonitor.h"
#endif
#include "selfupdater.h"
#include <QCloseEvent>
#include "installfromswg.h"
#include "utils.h"
#include "filescanner.h"
#include "macroeditor.h"
#include <QWebElement>
#include <QWebFrame>

#if QT_VERSION >= 0x050000
#include <QtConcurrent/QtConcurrentRun>
#endif

#include "gamemods.h"

#define ENABLE_MACRO_EDITOR

//QString MainWindow::patchUrl = "http://www.launchpad2.net/SWGEmu/"; // Insert download URL here
QString MainWindow::patchUrl = "http://cdnwarz.kongsi.asia/swg/patches/"; // Insert download URL here
//QString MainWindow::newsUrl = "http://www.swgemu.com/forums/index.php#bd";
QString MainWindow::newsUrl = "http://swg.openkod.com";
QString MainWindow::gameExecutable = "SWGEmu.exe";
#ifdef Q_OS_WIN32
QString MainWindow::selfUpdateUrl = "http://cdnwarz.kongsi.asia/swg/setup.cfg"; // Insert update URL here
#else
QString MainWindow::selfUpdateUrl = "http://cdnwarz.kongsi.asia/swg/setuplinux86_64.cfg"; // Insert linux update URL here
#endif
const QString MainWindow::version = "0.23";

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow), networkAccessManager(this), clientFilesNetworkAccessManager(this),
    novaNetworkAccessManager(this), requiredFilesNetworkManager(this), patchesNetworkManager(this),
    fullScanWorkingThreads(0) {
    ui->setupUi(this);

    QCoreApplication::setOrganizationName("SWGEmu");
    QCoreApplication::setOrganizationDomain("swg.openkod.com");
    QCoreApplication::setApplicationName("Launchpad");

    requiredFilesCount = 0;
    nextFileToDownload = 0;

    updateTimeCounter = 5;

    gameProcessesCount = 0;
    runningFullScan = false;

    fileScanner = new FileScanner(this);

    settings = new Settings(this);
    loginServers = new LoginServers(this);
    systemTrayIcon = new QSystemTrayIcon(this);
    systemTrayIcon->setIcon(QIcon(":/img/swgemu.svg"));
    systemTrayMenu = new QMenu();
    closeAction = new QAction("Close", NULL);
    systemTrayMenu->addAction(closeAction);
    systemTrayIcon->setContextMenu(systemTrayMenu);

    QToolButton* newsButton = new QToolButton(ui->mainToolBar);
    newsButton->setIcon(QIcon(":/img/globe.svg"));
    newsButton->setText("News");
    newsButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    newsButton->setCheckable(true);
    ui->mainToolBar->addWidget(newsButton);
    connect(newsButton, SIGNAL(clicked()), this, SLOT(triggerNews()));
    toolButtons.append(newsButton);

    QToolButton* updateStatusButton = new QToolButton(ui->mainToolBar);
    updateStatusButton->setIcon(QIcon(":/img/update_status.svg"));
    updateStatusButton->setText("Update status");
    updateStatusButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    ui->mainToolBar->addWidget(updateStatusButton);
    connect(updateStatusButton, SIGNAL(clicked()), this, SLOT(updateServerStatus()));
    toolButtons.append(updateStatusButton);

    QToolButton* gameSettingsButton = new QToolButton(ui->mainToolBar);
    gameSettingsButton->setIcon(QIcon(":/img/game_settings.svg"));
    gameSettingsButton->setText("Game settings");
    gameSettingsButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    ui->mainToolBar->addWidget(gameSettingsButton);
    connect(gameSettingsButton, SIGNAL(clicked()), this, SLOT(startSWGSetup()));
    toolButtons.append(gameSettingsButton);

    QToolButton* gameModsButton = new QToolButton(ui->mainToolBar);
    gameModsButton->setIcon(QIcon(":/img/magic.svg"));
    gameModsButton->setText("Game mods");
    gameModsButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    ui->mainToolBar->addWidget(gameModsButton);
    connect(gameModsButton, SIGNAL(clicked()), this, SLOT(showGameModsOptions()));
    toolButtons.append(gameModsButton);

#ifdef ENABLE_MACRO_EDITOR
    QToolButton* macroEditorButton = new QToolButton(ui->mainToolBar);
    macroEditorButton->setIcon(QIcon(":/img/book.svg"));
    macroEditorButton->setText("Macro Editor");
    macroEditorButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    ui->mainToolBar->addWidget(macroEditorButton);
    connect(macroEditorButton, SIGNAL(clicked()), this, SLOT(showMacroEditor()));
    toolButtons.append(macroEditorButton);
#endif

    QToolButton* profCalculatorButton = new QToolButton(ui->mainToolBar);
    profCalculatorButton->setIcon(QIcon(":/img/design.svg"));
    profCalculatorButton->setText("Profession Calculator");
    profCalculatorButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    ui->mainToolBar->addWidget(profCalculatorButton);
    connect(profCalculatorButton, SIGNAL(clicked()), this, SLOT(startKodanCalculator()));
    toolButtons.append(profCalculatorButton);

    QToolButton* deleteProfilesButton = new QToolButton(ui->mainToolBar);
    deleteProfilesButton->setIcon(QIcon(":/img/bin.svg"));
    deleteProfilesButton->setText("Delete game profiles");
    deleteProfilesButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    ui->mainToolBar->addWidget(deleteProfilesButton);
    connect(deleteProfilesButton, SIGNAL(clicked()), this, SLOT(deleteProfiles()));
    toolButtons.append(deleteProfilesButton);

    QToolButton* updateButton = new QToolButton(ui->mainToolBar);
    updateButton->setIcon(QIcon(":/img/cloud_down.svg"));
    updateButton->setText("Check for updates");
    updateButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    ui->mainToolBar->addWidget(updateButton);
    connect(updateButton, SIGNAL(clicked()), this, SLOT(checkForUpdates()));
    toolButtons.append(updateButton);

    cancelWorkingThreads = false;

    connect(ui->mainToolBar, SIGNAL(orientationChanged(Qt::Orientation)), this, SLOT(toolBarOrientationChanged(Qt::Orientation)));
    connect(systemTrayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(systemTrayActivated(QSystemTrayIcon::ActivationReason)));
    connect(closeAction, SIGNAL(triggered()), qApp, SLOT(quit()));
    connect(ui->actionFolders, SIGNAL(triggered()), this, SLOT(showSettings()));
    connect(&networkAccessManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(statusXmlIsReady(QNetworkReply*)) );
    connect(&novaNetworkAccessManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(statusXmlIsReady(QNetworkReply*)) );
    connect(&clientFilesNetworkAccessManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(downloadFileFinished(QNetworkReply*)));
    connect(&requiredFilesNetworkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(requiredFileDownloadFileFinished(QNetworkReply*)));
    connect(&patchesNetworkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(patchesDownloadFileFinished(QNetworkReply*)));
    connect(ui->webView, SIGNAL(loadFinished(bool)), this, SLOT(webPageLoadFinished(bool)));
    connect(ui->pushButton_Start, SIGNAL(clicked()), this, SLOT(startSWG()));
    connect(fileScanner, SIGNAL(requiredFileExists(QString)), this, SLOT(updateBasicLoadProgress(QString)));
    connect(fileScanner, SIGNAL(fullScannedFile(QString, bool)), this, SLOT(updateFullScanProgress(QString, bool)));
    connect(this, SIGNAL(startDownload()), this, SLOT(startFileDownload()));
    connect(ui->actionLogin_Servers, SIGNAL(triggered()), loginServers, SLOT(show()));
    connect(ui->actionShow_news, SIGNAL(triggered()), this, SLOT(triggerNews()));
    connect(ui->checkBox_instances, SIGNAL(toggled(bool)), this, SLOT(triggerMultipleInstances(bool)));
    connect(ui->actionUpdate_Status, SIGNAL(triggered()), this, SLOT(updateServerStatus()));
    connect(ui->tabWidget, SIGNAL(tabCloseRequested(int)), this, SLOT(closeTab(int)));
    connect(ui->actionCheck_for_updates, SIGNAL(triggered()), this, SLOT(checkForUpdates()));
    connect(ui->actionGame_Settings, SIGNAL(triggered()), this, SLOT(startSWGSetup()));
    connect(ui->actionAbout, SIGNAL(triggered()), this, SLOT(showAboutDialog()));
    connect(ui->actionDelete_Profiles, SIGNAL(triggered()), this, SLOT(deleteProfiles()));
    connect(fileScanner, SIGNAL(addFileToDownload(QString)), this, SLOT(addFileToDownloadSlot(QString)));
    connect(ui->actionInstall_from_SWG, SIGNAL(triggered()), this, SLOT(installSWGEmu()));

    ui->groupBox_browser->hide();

    QTabBar* tabBar = ui->tabWidget->tabBar();
    tabBar->setTabButton(0, QTabBar::RightSide, 0);
    tabBar->setTabButton(0, QTabBar::LeftSide, 0);

    QSettings settingsOptions;

    QString swgFolder = settingsOptions.value("swg_folder").toString();
    bool multipleInstances = settingsOptions.value("multiple_instances").toBool();

    ui->checkBox_instances->setChecked(multipleInstances);
    ui->textBrowser->viewport()->setAutoFillBackground(false);
    ui->textBrowser->setAutoFillBackground(false);

    updateServerStatus();

    connect(&loadWatcher, SIGNAL(finished()), this, SLOT(loadFinished()));
    //connect(&fullScanWatcher, SIGNAL(finished()), this, SLOT(fullScanFinished()));
    connect(ui->pushButton_FullScan, SIGNAL(clicked()), this, SLOT(startFullScan()));

    loginServers->reloadServers();
    updateLoginServerList();

    silentSelfUpdater = new SelfUpdater(true, this);

    if (!swgFolder.isEmpty())
        startLoadBasicCheck();
    else {
#ifdef Q_OS_WIN32
        QDir dir("C:/SWGEmu");

        if (dir.exists() && FileScanner::checkSwgFolder("C:/SWGEmu")) {
            settingsOptions.setValue("swg_folder", "C:/SWGEmu");
            startLoadBasicCheck();
        } else
#endif
        QMessageBox::warning(this, "Error", "Please set the swgemu folder in Settings->Options or install using Settings->Select install folder option");
    }

    restoreGeometry(settingsOptions.value("mainWindowGeometry").toByteArray());
    restoreState(settingsOptions.value("mainWindowState").toByteArray());

    QString savedLogin = settingsOptions.value("selected_login_server", "").toString();

    if (!savedLogin.isEmpty()) {
        int idx = ui->comboBox_login->findText(savedLogin);

        if (idx >= 0) {
            ui->comboBox_login->setCurrentIndex(idx);
        }
    }

    requiredFilesNetworkManager.get(QNetworkRequest(QUrl(patchUrl + "required2.txt")));
    //patchesNetworkManager.get(QNetworkRequest(QUrl(patchUrl + "patches.txt")));
    silentSelfUpdater->silentCheck();

    //ui->webView->setUrl(newsUrl);

    //gameMods = new GameMods(this);
}

MainWindow::~MainWindow() {
    delete ui;
    ui = NULL;

    //delete fileScanner;
    fileScanner = NULL;

    //delete settings;
    settings = NULL;

    //delete loginServers;
    loginServers = NULL;

    while (gameProcesses.size() > 0) {
        GameProcess* process = gameProcesses[0];
        gameProcesses.remove(0);

        delete process;
    }

    #ifdef Q_OS_WIN32
    if (GameProcess::debugMonitor) {
        delete GameProcess::debugMonitor;
        GameProcess::debugMonitor = NULL;
    }
    #endif

    //delete silentSelfUpdater;
    silentSelfUpdater = NULL;

    //gameMods = NULL;
}

void MainWindow::toolBarOrientationChanged(Qt::Orientation ) {
    /*
    Qt::ToolButtonStyle style = Qt::ToolButtonTextBesideIcon;

    switch (orientation) {
    case Qt::Horizontal:
        style = Qt::ToolButtonTextBesideIcon;
        break;
    case Qt::Vertical:
        style = Qt::ToolButtonTextUnderIcon;
        break;
    }

    for (int i = 0; i < toolButtons.size(); ++i) {
        QToolButton* button = toolButtons.at(i);
        button->setToolButtonStyle(style);
        button->set
//        button->set
    }
    */
}

void MainWindow::systemTrayActivated(QSystemTrayIcon::ActivationReason reason) {
    if (reason == QSystemTrayIcon::Trigger) {
        this->showNormal();
        this->raise();
    }
}

void MainWindow::deleteProfiles() {
    if (QMessageBox::question(this, "Warning", "Are you sure you want to delete the game profiles folder?") != QMessageBox::Yes )
        return;

    QSettings settings;
    QString folder = settings.value("swg_folder").toString();

#ifdef Q_OS_WIN32
    QDir dir(folder + "\\profiles");
#else
    QDir dir(folder + "/profiles");
#endif

    if (!dir.exists()) {
        QMessageBox::warning(this, "Warning", "No profiles folder found");

        return;
    }

    if (dir.removeRecursively()) {
        QMessageBox::information(this, "Success", "Profiles directory deleted!");
    } else {
        QMessageBox::warning(this, "Warning", "There was an error deleting the profiles directory!");
    }
}

void MainWindow::readBasiliskServerStatus() {
    networkAccessManager.get(QNetworkRequest(QUrl("http://www.swgemu.com/status/basilisk.xml")));
}

void MainWindow::readNovaServerStatus() {
    novaNetworkAccessManager.get(QNetworkRequest(QUrl("http://www.swgemu.com/status/nova.xml")));
}

void MainWindow::readBimaServerStatus() {
    novaNetworkAccessManager.get(QNetworkRequest(QUrl("http://status.swg.openkod.com/bima.xml")));
}

void MainWindow::checkForUpdates() {
    SelfUpdater updater(false, this);
    updater.execUpdate();
}

void MainWindow::updateLoginServerList() {
    ui->comboBox_login->clear();

    QSettings settings;
    settings.beginWriteArray("login_servers");

    for (int i = 0; i < loginServers->count(); ++i) {
        LoginServer* server = loginServers->getServer(i);
        settings.setArrayIndex(i);

        ui->comboBox_login->addItem(server->text());

        settings.setValue("name", server->text());
        settings.setValue("host", server->getHost());
        settings.setValue("port", server->getPort());
    }

    settings.endArray();
}

void MainWindow::updateServerStatus() {
    //ui->statusBar->showMessage("Updating server status..");
    ui->textBrowser->clear();

    // readBasiliskServerStatus();
    // readNovaServerStatus();
    readBimaServerStatus();
}

void MainWindow::triggerMultipleInstances(bool newValue) {
    QSettings settings;
    settings.setValue("multiple_instances", newValue);
}

void MainWindow::triggerNews() {
    if (ui->groupBox_browser->isHidden()) {
        ui->statusBar->showMessage("Loading page...");
        ui->webView->setUrl(newsUrl);

        if (!isMaximized()) {
            QDesktopWidget* mydesk = QApplication::desktop();
            int screen = mydesk->screenNumber(this);
            QRect screenSize = mydesk->screenGeometry(screen);
            QRect windowGeometry = this->geometry();

            int difference = 811 - 356;

            if (windowGeometry.height() + difference + pos().y() > screenSize.bottom()) {
                this->move(pos().x(), pos().y() - (windowGeometry.height() + difference + pos().y() - screenSize.bottom()));
            }

            this->resize(907, 811);
        }

        ui->actionShow_news->setText("Hide news");
        //ui->actionShow_news->setChecked(true);
        ui->groupBox_browser->show();
    } else {
        ui->groupBox_browser->hide();

        if (!isMaximized())
            this->resize(907, 256);

        //ui->actionShow_news->setChecked(false);
        ui->actionShow_news->setText("Show news");
    }
}



QFile* MainWindow::getRequiredFilesFile() {
    QSettings settings;
    //QString folder = settings.value("swg_folder").toString();

    QFile* file = NULL;

    //if (QDir(folder).exists()) {
        file = new QFile("required2.txt");

        if (file->exists()) {
            if (file->open(QIODevice::ReadOnly | QIODevice::Text)) {
                return file;
            } else
                delete file;
        } else {
            delete file;
        }
    //}

    file = new QFile(":/files/required2.txt");
    file->open(QIODevice::ReadOnly | QIODevice::Text);

    return file;
}

void MainWindow::addFileToDownloadSlot(QString file) {
    filesToDownload.append(file);
}

void MainWindow::showAboutDialog() {
    QMessageBox::about(this, "SWGemu", "SWGEmu Launchpad version " + version + "\n\nThis program is distributed in the hope that it will be useful,"
                       " but WITHOUT ANY WARRANTY; without even the implied warranty of"
                       " MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the"
                       " GNU General Public License for more details.");
}

void MainWindow::requiredFileDownloadFileFinished(QNetworkReply* reply) {
    if (reply && reply->error() != QNetworkReply::NoError) {
       // QMessageBox::warning(this, "ERROR getting new patch information", reply->errorString());

        return;
    }

    qDebug() << "got required2.txt";

    QString data = reply->readAll();

  //  qDebug() << data;

    QSettings settings;
   // QString folder = settings.value("swg_folder").toString();

   // if (QDir(folder).exists()) {
        QFile file("required2.txt");

        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream stream(&file);

            stream << data;

            file.close();

            return;
        }
    //}

    startLoadBasicCheck();

    /*
        ui->pushButton_FullScan->setEnabled(false);
        ui->pushButton_Start->setEnabled(false);

        ui->label_current_work->setText("Checking for new client updates...");

        QStringList currentFiles = getRequiredFiles();
        */
}

void MainWindow::patchesDownloadFileFinished(QNetworkReply* reply) {
    if (reply && reply->error() != QNetworkReply::NoError) {
        // QMessageBox::warning(this, "ERROR getting new patch information", reply->errorString());
        updateTimeCounter = -1;
        enableStart();

        return;
    }

    try {
    QString data = reply->readAll();

    qDebug() << "got patches.txt" << data;

    QTextStream file(&data);

    QSettings settings;
    QString swgFolder = settings.value("swg_folder").toString();

    while (!file.atEnd()) {
        QString fileString = file.readLine();

        QByteArray line(fileString.toStdString().c_str());
        //process_line(line);

        //QRegExp rx("(\\ |\\,|\\.|\\;|\\t)"); //RegEx for ' ' or ',' or '.' or ':' or '\t'
        QList<QByteArray> query = line.split(';');

        //QListIterator<QByteArray>

        QString name = query.at(0);
        QString size = query.at(1);
        QString md5 = query.at(2).trimmed();
        QString action = query.at(3);

        QString file = swgFolder + "/" + name;

        QFile fileObject(file);

        if ((action == "A") && !fileObject.exists()) {
            qDebug() << file << "doesnt exist";
            //return -1;

            //filesToDownload.append(MainWindow::patchUrl + name);
            appendToFilesToDownloadStringList(MainWindow::patchUrl + name);

            continue;
        } else if ((action == "D") && fileObject.exists()) {
            fileObject.remove();
        }
    }
    }
    catch (...) {
        enableStart();
    }

    if (filesToDownload.size() > 0) {
        startFileDownload();

        updateTimeCounter = -2;
    } else {
        updateTimeCounter = -1;

        enableStart();
    }

}

void MainWindow::updateBasicLoadProgress(QString successFile) {
    //ui->progressBar_loading->set

    ui->progressBar_loading->setValue(++currentReadFiles);
}

void MainWindow::updateFullScanProgress(QString successFile, bool success) {
    /*if (success)
    ui->label_current_work->setText(successFile + " valid.");
  else
    ui->label_current_work->setText(successFile + " invalid!");*/

    if (!success) {
        //ui->label_current_work->setPalette(QPalette::);
        ui->label_current_work->setStyleSheet("color:red");
        ui->label_current_work->setText(successFile + " invalid!");
    }

    ui->progressBar_FullScan->setValue(++currentReadFiles);

    if (currentReadFiles == requiredFilesCount) {
        qDebug() << "full scan finished";

        runningFullScan = false;

        if (filesToDownload.size() == 0)
            ui->label_current_work->setText("Full scan successfull");

        if (!cancelWorkingThreads)
            emit startDownload();
    }
}

void MainWindow::startFullScan(bool forceConfigRestore) {
    if (!forceConfigRestore) {
        if (QMessageBox::question(this, "Warning", "This will restore your files to their original state removing any mods you might have. Do you want to continue?") != QMessageBox::Yes)
            return;
    }

    requiredFilesCount = getRequiredFiles().size();
    currentReadFiles = 0;

    QSettings settings;
    QString folder = settings.value("swg_folder").toString();
    QDir checkDir(folder);

    /* if (!checkDir.exists() || folder.isEmpty() || !FileScanner::checkSwgFolder(folder)) {
        QMessageBox::warning(this, "ERROR", "Invalid game folder!");

        return;
    } */

    bool restoreConfigFiles = forceConfigRestore ? true : QMessageBox::question(this, "Config files", "Do you want to restore the config files too?") == QMessageBox::Yes;

    if (requiredFilesCount == 0 && !restoreConfigFiles)
        return;

    ui->pushButton_Start->setEnabled(false);
    ui->pushButton_FullScan->setEnabled(false);

    ui->progressBar_FullScan->setMaximum(requiredFilesCount);
    ui->progressBar_FullScan->setValue(0);

    ui->label_current_work->setStyleSheet("color:black");
    ui->label_current_work->setText("Doing full scan..");
    ui->actionFolders->setEnabled(false);

    filesToDownload.clear();

    if (restoreConfigFiles || !QFile(folder + "/swgemu.cfg").exists()) {
        filesToDownload.append(patchUrl + "swgemu.cfg");
        filesToDownload.append(patchUrl + "swgemu_live.cfg");
        filesToDownload.append(patchUrl + "swgemu_login.cfg");
        filesToDownload.append(patchUrl + "swgemu_preload.cfg");
        //filesToDownload.append(patchUrl + "Emu_opt.cfg");
        filesToDownload.append(patchUrl + "swgemu_machineoptions.iff");
        //filesToDownload.append(patchUrl + "options.cfg");
        filesToDownload.append(patchUrl + "user.cfg");
    }

    bool multiThreaded = settings.value("multi_threaded_full_scan", false).toBool();

    if (multiThreaded) {
        fullScanWorkingThreads = getRequiredFiles().size();

        runningFullScan = true;
        QtConcurrent::run(fileScanner, &FileScanner::fullScanMultiThreaded, restoreConfigFiles);
    } else {
        runningFullScan = true;

        QFuture<int> future = QtConcurrent::run(fileScanner, &FileScanner::fullScanSingleThreaded, restoreConfigFiles);
        fullScanWatcher.setFuture(future);
    }
}

void MainWindow::startLoadBasicCheck() {
    if (runningFullScan)
        return;

    ui->pushButton_Start->setEnabled(false);
//    ui->pushButton_Start->setText("Start (" + QString::number(updateTimeCounter) + ")");
    ui->label_current_work->setStyleSheet("color:black");
    ui->label_current_work->setText("Checking files and updates..");

    requiredFilesCount = getRequiredFiles().size();
    currentReadFiles = 0;

    ui->progressBar_loading->setMaximum(requiredFilesCount);
    ui->progressBar_loading->setValue(0);

    QSettings settingsOptions;
    QString swgFolder = settingsOptions.value("swg_folder").toString();

    QFuture<int> future = QtConcurrent::run(fileScanner, &FileScanner::loadAndBasicCheckFiles, swgFolder);
    loadWatcher.setFuture(future);

    //QTimer::singleShot(1000, this, SLOT(runUpdateCheckTimer()));
}

void MainWindow::runUpdateCheckTimer() {
    if (--updateTimeCounter >= 0) {
        QTimer::singleShot(1000, this, SLOT(runUpdateCheckTimer()));

        ui->pushButton_Start->setText("Start (" + QString::number(updateTimeCounter) + ")");
    } else {
        ui->pushButton_Start->setText("Start");

        //ui->pushButton_Start->setEnabled(true);

    //    qDebug() << "hui is: " << updateTimeCounter;

        if (updateTimeCounter == -1) {
            enableStart();
        }
    }
}

void MainWindow::startFileDownload() {
    requiredFilesCount = filesToDownload.count();
    currentReadFiles = 0;

    if (filesToDownload.length() == 0) {
        ui->actionFolders->setEnabled(true);
        ui->pushButton_Start->setEnabled(true);
        ui->pushButton_FullScan->setEnabled(true);

        return;
    }

    ui->progressBar_loading->setMaximum(requiredFilesCount);
    ui->progressBar_loading->setValue(0);

    //filesToDownload = files;
    nextFileToDownload = 0;

    QString downloadingFile = filesToDownload.at(nextFileToDownload);
    downloadingFile = downloadingFile.mid(downloadingFile.lastIndexOf("/"));

    ui->label_current_work->setStyleSheet("color:black");
    ui->label_current_work->setText("Downloading: " + downloadingFile);
    qDebug() << "downloading " << filesToDownload.at(nextFileToDownload);
    QNetworkReply* reply = clientFilesNetworkAccessManager.get(QNetworkRequest(filesToDownload.at(nextFileToDownload)));
    connect(reply, SIGNAL(downloadProgress(qint64,qint64)), this, SLOT(downloadProgress(qint64,qint64)));

    lastReceivedBytesTime.restart();
    lastReceivedBytes = 0;
}

void MainWindow::downloadFileFinished(QNetworkReply* reply) {
    if (reply->error() != QNetworkReply::NoError) {
        QMessageBox::critical(this, "ERROR", reply->errorString());

        filesToDownload.clear();
        nextFileToDownload = 0;

        return;
    }

    int bufferSize = 8192 * 2;
    char* buffer = (char*) malloc(bufferSize);

    QSettings settings;
    QString swgFolder = settings.value("swg_folder").toString();

    QString downloadedFile = filesToDownload.at(nextFileToDownload);

    downloadedFile = downloadedFile.remove(0, patchUrl.length());

    QString dir;

    if (downloadedFile.contains("/")) {
        dir = downloadedFile.mid(0, downloadedFile.lastIndexOf("/"));
    }

    downloadedFile = downloadedFile.mid(downloadedFile.lastIndexOf("/") + 1);

    QString fullPath;

    if (dir.isEmpty())
        fullPath = swgFolder + "/" + downloadedFile;
    else
        fullPath = swgFolder + "/" + dir + "/" + downloadedFile;

    if (!QDir(swgFolder + "/" + dir).exists())
        QDir(swgFolder + "/" + dir).mkpath(".");

    QFile fileObject(fullPath);
    //fileObject.set

    if (!fileObject.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, "ERROR", "Could not open to write downloaded file to disk! " + swgFolder + "/" + downloadedFile);

        filesToDownload.clear();
        nextFileToDownload = 0;

        free(buffer);

        return;
    }

    int read = 0;
    while ((read = reply->read(buffer, bufferSize)) > 0) {
        if (fileObject.write(buffer, read) == -1) {
            QMessageBox::critical(this, "ERROR", "Could not write downloaded file to disk!");

            filesToDownload.clear();
            nextFileToDownload = 0;

            free(buffer);

            return;
        }
    }

    fileObject.close();

    free(buffer);

    qDebug() << "downloading file:" << downloadedFile << " finished!";

    if (++nextFileToDownload < filesToDownload.size()) {
        QString downloadingFile = filesToDownload.at(nextFileToDownload);
        downloadingFile = downloadingFile.mid(downloadingFile.lastIndexOf("/") + 1);

        ui->label_current_work->setStyleSheet("color:black");
        ui->label_current_work->setText("Downloading: " + downloadingFile);

        QNetworkReply* reply = clientFilesNetworkAccessManager.get(QNetworkRequest(filesToDownload.at(nextFileToDownload)));
        connect(reply, SIGNAL(downloadProgress(qint64,qint64)), this, SLOT(downloadProgress(qint64,qint64)));
        lastReceivedBytesTime.restart();
        lastReceivedBytes = 0;

        ui->progressBar_loading->setValue(nextFileToDownload);
    } else {
        ui->progressBar_loading->setValue(nextFileToDownload);

        downloadFinished();
    }
}

void MainWindow::downloadProgress(qint64 bytesReceived, qint64 bytesTotal) {
    if (lastReceivedBytes == 0) {
        ui->progressBar_FullScan->setMaximum(bytesTotal);
    }

    ui->progressBar_FullScan->setValue(bytesReceived);

    QTime nowTime;
    nowTime.restart();

    int delta = lastReceivedBytesTime.msecsTo(nowTime);

    if (delta < 500 && bytesReceived < bytesTotal)
        return;

    QString downloadingFile = filesToDownload.at(nextFileToDownload);
    downloadingFile = downloadingFile.mid(downloadingFile.lastIndexOf("/") + 1);

    int speed = 0;

    if (delta != 0)
        speed = ((bytesReceived - lastReceivedBytes) / delta);

    QString label = "Downloading: " + downloadingFile + " " + QString::number(bytesReceived / 1000) + " kB / " + QString::number(bytesTotal / 1000) + " kB " + QString::number(speed) + " kB/s";
    ui->label_current_work->setStyleSheet("color:black");
    ui->label_current_work->setText(label);

    lastReceivedBytesTime.restart();
    lastReceivedBytes = bytesReceived;
}

void MainWindow::startKodanCalculator() {
    #ifdef Q_OS_WIN32
    if (!QProcess::startDetached("KSWGProfCalcEditor.exe", QStringList(), QDir::currentPath())) {
        QMessageBox::warning(this, "ERROR", "Could not launch profession calculator!");
    }
    #else
    QSettings settings;
    QString wineBinary = settings.value("wine_binary").toString();

    if (wineBinary.isEmpty())
        wineBinary = "wine";

    QString args = settings.value("wine_args").toString();

    QStringList argsList;
    if (!args.isEmpty())
        argsList = Utils::getArgumentList(args);

    argsList.append("KSWGProfCalcEditor.exe");

    qDebug() << argsList;

    if (!QProcess::startDetached(wineBinary, argsList, QDir::currentPath())) {
        QMessageBox::warning(this, "ERROR", "Could not launch game settings!");
    }
    #endif
}

void MainWindow::startSWGSetup() {
    //QMessageBox::
    //QProcess* process = new QProcess();
    QSettings settings;
    QString folder = settings.value("swg_folder").toString();

#ifdef Q_OS_WIN32

    //qDebug() << folder;
    /*process->setWorkingDirectory(folder);
    process->start(folder + "\\" + "SWGEmu_Setup.exe");*/

    if (!QProcess::startDetached(folder + "\\" + "SWGEmu_Setup.exe", QStringList(), folder)) {
        QMessageBox::warning(this, "ERROR", "Could not launch game settings!");
    }
#else
    QString wineBinary = settings.value("wine_binary").toString();

    if (wineBinary.isEmpty())
        wineBinary = "wine";

    QString args = settings.value("wine_args").toString();

    QStringList argsList;
    if (!args.isEmpty())
        argsList = Utils::getArgumentList(args);

    argsList.append(folder + "/" + "SWGEmu_Setup.exe");

    qDebug() << argsList;

    if (!QProcess::startDetached(wineBinary, argsList, folder)) {
        QMessageBox::warning(this, "ERROR", "Could not launch game settings!");
    }
#endif
}

void MainWindow::downloadFinished() {
    nextFileToDownload = 0;

    if (filesToDownload.contains(patchUrl + "swgemu.cfg")) {
        QMessageBox::information(this, "Game Settings", "SWGEmu game settings application will now launch, please set your resolution in the Graphics tab!");

        startSWGSetup();
    }

    filesToDownload.clear();

    ui->label_current_work->setStyleSheet("color:green");
    ui->label_current_work->setText("Download finished");
    ui->pushButton_Start->setEnabled(true);
    ui->actionFolders->setEnabled(true);
    ui->pushButton_FullScan->setEnabled(true);

    qDebug() << "download finished";

    //startLoadBasicCheck();
}

void MainWindow::showSettings() {
    settings->exec();
}
/*
void MainWindow::fullScanFinished() {
    if (cancelWorkingThreads)
        return;

    int res = fullScanWatcher.result();

    qDebug() << "full scan finished with result " << res;

    if (res == 0)
        ui->label_current_work->setText("Full scan successfull");
}
*/

void MainWindow::enableStart() {
    ui->pushButton_Start->setEnabled(true);
    ui->pushButton_Start->setText("Start");
    ui->actionFolders->setEnabled(true);

    ui->label_current_work->setStyleSheet("color:green");
    ui->label_current_work->setText("Basic checks passed. Ready to play.");
}

void MainWindow::loadFinished() {
    if (cancelWorkingThreads)
        return;

    int res = loadWatcher.result();

    if (res == 0) {
        if (updateTimeCounter < 0) {
            enableStart();
        } else {
            updateTimeCounter = 5;

            ui->pushButton_Start->setText("Start (" + QString::number(updateTimeCounter) + ")");

            QTimer::singleShot(1000, this, SLOT(runUpdateCheckTimer()));

            patchesNetworkManager.get(QNetworkRequest(QUrl(patchUrl + "patches.txt")));
        }
    } else {
        ui->pushButton_Start->setEnabled(false);
        ui->actionFolders->setEnabled(true);

        ui->label_current_work->setStyleSheet("color:red");
        ui->label_current_work->setText("Basic checks failed. Please run full scan to try and fix the issues.");
        ui->progressBar_loading->setValue(ui->progressBar_loading->maximum());
    }
}

QVector<QPair<QString, qint64> > MainWindow::getRequiredFiles() {
    QSettings settings;
    //QString folder = settings.value("swg_folder").toString();

    QVector<QPair<QString, qint64> > data;

    //QStringList files;

    //if (QDir(folder).exists()) {
     {
        QFile file("required2.txt");

        if (file.exists()) {
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                while (!file.atEnd()) {
                    QByteArray line = file.readLine();

                    QList<QByteArray> query = line.split(';');

                    //QListIterator<QByteArray>

                    QString name = query.at(0);
                    QString size = query.at(1);
                    QString md5 = query.at(2);

                    //files.append(name);

                    data.append(QPair<QString, qint64>(name, size.toLongLong()));
                }

                return data;
            }
        }
    }


    QFile file(":/files/required2.txt");

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return data;

    while (!file.atEnd()) {
        QByteArray line = file.readLine();
        //process_line(line);

        //QRegExp rx("(\\ |\\,|\\.|\\;|\\t)"); //RegEx for ' ' or ',' or '.' or ':' or '\t'
        QList<QByteArray> query = line.split(';');

        //QListIterator<QByteArray>

        QString name = query.at(0);
        QString size = query.at(1);
        QString md5 = query.at(2);

        //files.append(name);
        //line.
        data.append(QPair<QString, qint64>(name, size.toLongLong()));
    }

    return data;
}

void MainWindow::startSWG() {
    QStringList arguments;

    QSettings settings;
    QString folder = settings.value("swg_folder").toString();
    bool multiple = settings.value("multiple_instances").toBool();
    ConfigParser* parser = new ConfigParser();
    GameProcess* process = new GameProcess(parser, NULL);

    parser->connect(parser, SIGNAL(errorSignal(QString)), process, SLOT(outputDebugString(QString)));

    try {
        if (parser->loadFile(folder, "swgemu.cfg") != 0) {
            QMessageBox::warning(this, "Warning", "There was an issue parsing the swg config files! To restore them run full scan!");
        }
    } catch (...) {
        QMessageBox::warning(this, "Warning", "There was an issue parsing the swg config files! To restore them run full scan!");
    }

    QVector<ConfigValue> loginServerAddresses = parser->getConfigValues("ClientGame", "loginServerAddress0");
    QVector<ConfigValue> loginServerPorts = parser->getConfigValues("ClientGame", "loginServerPort0");

    if (loginServerAddresses.size() > 1) {
        QString warningString;
        QTextStream stream(&warningString);

        stream << "You have several login server addresses defined in the following swg config files: ";

        for (int i = 0; i < loginServerAddresses.size(); ++i) {
            const ConfigValue val = loginServerAddresses.at(i);

            stream << val.fileName << " ";
        }

        stream << " client will use the value: " << parser->getStringConfigValue("ClientGame", "loginServerAddress0");

        QMessageBox::warning(this, "Warning", warningString);
    }

    if (loginServerPorts.size() > 1) {
        QString warningString;
        QTextStream stream(&warningString);

        stream << "You have several login server ports defined in the following swg config files: ";

        for (int i = 0; i < loginServerPorts.size(); ++i) {
            const ConfigValue val = loginServerPorts.at(i);

            stream << val.fileName << " ";
        }

        stream << " client will use the value: " << parser->getStringConfigValue("ClientGame", "loginServerPort0");

        QMessageBox::warning(this, "Warning", warningString);
    }

    LoginServer* server = loginServers->getServer(ui->comboBox_login->currentText());

    QString loginAddress = parser->getStringConfigValue("ClientGame", "loginServerAddress0", "loginServerAddress0");
    QString port = parser->getStringConfigValue("ClientGame", "loginServerAddress0", "loginServerPort0");

    if (loginAddress != server->getHost() || port != QString::number(server->getPort())) {
#ifdef Q_OS_WIN32
        QFile loginFile(folder + "\\swgemu_login.cfg");
#else
        QFile loginFile(folder + "/swgemu_login.cfg");
#endif

        if (loginFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream stream(&loginFile);
            stream << "[ClientGame]" << endl;
            stream << "loginServerAddress0=" << server->getHost() << endl;
            stream << "loginServerPort0=" << server->getPort();

            loginFile.close();
        } else {
            QMessageBox::warning(this, "Error", "Could not write swgemu_login.cfg!");
        }
    }
/*
    arguments.append("--");
    arguments.append("-s");
    arguments.append("ClientGame");
    arguments.append("cameraFarPlane=4096");
*/
/*
    arguments.append("--");
    arguments.append("-s");
    arguments.append("SharedFoundation");
    arguments.append("minFrameRate=60");
*/
  //  QFile executable()
/*
    arguments.append("--");
    arguments.append("-s");
    arguments.append("ClientTerrain");
    arguments.append("dynamicNearFloraDistance=128");

    arguments.append("--");
    arguments.append("-s");
    arguments.append("ClientTerrain");
    arguments.append("staticNonCollidableFloraDistance=2048");

    arguments.append("--");
    arguments.append("-s");
    arguments.append("ClientProceduralTerrainAppearance_LevelOfDetail");
    arguments.append("forceHighThreshold=40");

    arguments.append("--");
    arguments.append("-s");
    arguments.append("ClientProceduralTerrainAppearance_LevelOfDetail");
    arguments.append("threshold=40");

    arguments.append("--");
    arguments.append("-s");
    arguments.append("ClientProceduralTerrainAppearance_LevelOfDetail");
    arguments.append("heightBiasMax=4096");
*/
    //ClientProceduralTerrainAppearance_LevelOfDetail.heightBiasMax



    //ClientGame.cameraFarPlane

    /*ClientProceduralTerrainAppearance_LevelOfDetail.threshold*/


    /*ClientProceduralTerrainAppearance_LevelOfDetail.forceHighThreshold*/


    arguments.append("--");
    arguments.append("-s");
    arguments.append("ClientGame");

    if (server != NULL)
        arguments.append("loginServerAddress0=" + server->getHost());
    else
        arguments.append("loginServerAddress0=" + LoginServers::defaultLoginAddress);

    if (server == NULL)
        arguments.append("loginServerPort0=" + QString::number(LoginServers::defaultLoginPort));
    else
        arguments.append("loginServerPort0=" + QString::number(server->getPort()));

    arguments.append("-s");
    arguments.append("Station");
    arguments.append("gameFeatures=34929");

    if (multiple) {
        if (parser->hasConfigValue("SwgClient", "allowMultipleInstances")) {
            bool val = parser->getBooleanConfigValue("SwgClient", "allowMultipleInstances");

            if (!val) {
                QMessageBox::warning(this, "Warning", "You selected the multiple instances option but you have set it to false in the swg config files!");
            }
        }

        arguments.append("-s");
        arguments.append("SwgClient");
        arguments.append("allowMultipleInstances=true");
    }

    qDebug() << "start swg with arguments " << arguments;

    connect(process, SIGNAL(processFinished(GameProcess*,int)), this, SLOT(gameProcessFinished(GameProcess*,int)));

    ui->tabWidget->addTab(process, "Process " + QString::number(++gameProcessesCount));
    gameProcesses.append(process);
    QTabBar* bar = ui->tabWidget->tabBar();
    int tabIndex = ui->tabWidget->indexOf(process);
    bar->setTabTextColor(tabIndex, Qt::green);
    bar->setTabIcon(tabIndex, QIcon(":/img/tab.svg"));

    //process->show();
    bool startResult = process->start(folder, gameExecutable, arguments);

    if (startResult) {
        //showMinimized();
        if (settings.value("minimize_after_start", false).toBool()) {
            systemTrayIcon->show();

            QTimer::singleShot(0, this, SLOT(hide()));
        }
    }
}

void MainWindow::gameProcessFinished(GameProcess* process, int ) {
    //ui->tabWidget->fi
    int index = ui->tabWidget->indexOf(process);
    //ui->tabWidget->setTabText(index, "");

    if (index < 1)
        return;

    QTabBar* bar = ui->tabWidget->tabBar();
    bar->setTabTextColor(index, Qt::red);
}

void MainWindow::statusXmlIsReady(QNetworkReply* reply) {
    qDebug() << "updating server status";

    if (reply->error() != QNetworkReply::NoError) {
        QString errorStr = reply->errorString();

        if (!errorStr.isEmpty()) {
            ui->textBrowser->setText("Error while fetching server status:" + errorStr);
            return;
        }
    }

    QXmlSimpleReader xmlReader;
    QXmlInputSource inputSource;
    inputSource.setData(reply->readAll());

    //StatusXmlContentHandler* handler = new StatusXmlContentHandler(this);
    StatusXmlContentHandler handler(this);
    xmlReader.setContentHandler(&handler);
    xmlReader.parse(&inputSource);

    QMap<QString, QString>* values = handler.getValues();

    QString labelText;
    QTextStream stream(&labelText);

    bool up = values->value("status") == "up";

    stream << "<div align=\"center\"><b>" << values->value("name") << "</b></div>";

    if (up) {
        stream << "<div align=\"center\" style=\"color:green\">Status: " << values->value("status") << "</div>";

        stream << "<div align=\"center\">Current online connections: " << values->value("connected") << "</div>";
        stream << "<div align=\"center\">Max allowed connections: " << values->value("cap") << "</div>";
        /*QDateTime upTime;
        upTime.addSecs(values->value("uptime").toULong());*/
        //upTime.
        //stream << "<div align=\"center\">Uptime: " << values->value("uptime") << " seconds</div>";
        QString uptimeString;
        QTextStream uptimeStream(&uptimeString);
        qint64 uptimeSeconds = values->value("uptime").toULongLong();
        //if (uptimeSeconds % )
        qint64 minutes = uptimeSeconds / 60 % 60;
        qint64 hours = uptimeSeconds / 3600 % 24;
        qint64 days = uptimeSeconds / 86400;

        if (days != 0) {
            uptimeStream << days << (days == 1 ? " day " : " days ") << hours << (hours == 1 ? " hour " : " hours ") << minutes << (minutes == 1 ? " minute " : " minutes" );
        } else if (hours != 0) {
            uptimeStream << hours << (hours == 1 ? " hour " : " hours ") << minutes << (minutes == 1 ? " minute " : " minutes ");
        } else {
            uptimeStream << minutes << (minutes == 1 ? " minute " : " minutes ") << uptimeSeconds % 60 << " seconds";
        }

        stream << "<div align=\"center\">Uptime: " << uptimeString << " </div>";
    } else
        stream << "<div align=\"center\" style=\"color:red\">Status: " << values->value("status") << "</div>";

    QDateTime timestamp;
    timestamp.setTime_t(values->value("timestamp").toULong());
    stream << "<div align=\"center\">Last updated: " << timestamp.toString(Qt::SystemLocaleShortDate) << "</div><br>";
    stream << "<div align=\"center\"><i>Notice:</i> " << values->value("notice") << "</div><br>";
    stream << "<div align=\"center\">" << values->value("banner") << "</div><br><br>";

    ui->textBrowser->insertHtml(labelText);
}

void MainWindow::webPageLoadFinished(bool ok) {
    if (!ok) {
        ui->statusBar->showMessage("Error loading swg.openkod.com");
        return;
    }

    ui->statusBar->showMessage("swg.openkod.com loaded.");

    //updateDonationMeter();
    //e.
}

void MainWindow::updateDonationMeter() {
    QWebElement e = ui->webView->page()->mainFrame()->findFirstElement("span#ds_bar_67_percentText");

    QString value = e.toPlainText();
    QString num = value.mid(0, value.indexOf("%"));

    ui->donationBar->setValue(num.toInt());

    ui->label_current_work->setText("Donation meter loaded.");
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (QMessageBox::question(this, "Close", "Are you sure you want to exit the launchpad?") != QMessageBox::Yes) {
        event->ignore();
        return;
    }

    cancelWorkingThreads = true;

    ui->statusBar->showMessage("Shutting down working threads..");

    if (loadWatcher.isRunning()) {
        loadWatcher.cancel();
        loadWatcher.waitForFinished();
    }

    if (fullScanWatcher.isRunning()) {
        fullScanWatcher.cancel();
        fullScanWatcher.waitForFinished();
    }

    while (!fullScanWorkingThreads.testAndSetAcquire(0, 0)) {
        QThread::yieldCurrentThread();
        QThread::msleep(500);
    }

    ui->statusBar->showMessage("Threads canceled.");

    QSettings settings;
    settings.setValue("mainWindowGeometry", saveGeometry());
    settings.setValue("mainWindowState", saveState());

    QString currentLogin = ui->comboBox_login->currentText();
    settings.setValue("selected_login_server", currentLogin);

    QMainWindow::closeEvent(event);
}

void MainWindow::closeTab(int index) {
    if (index == 0)
        return;

    GameProcess* widget = dynamic_cast<GameProcess*>(ui->tabWidget->widget(index));

    ui->tabWidget->removeTab(index);

    if (widget != NULL) {
        int idx = gameProcesses.indexOf(widget);

        if (idx >= 0) {
            qDebug() << "removing game process from vector";
            gameProcesses.remove(idx);
        }

        widget->disconnect();
        widget->clearOutputLogScreen();
        widget->deleteLater();
        //delete widget;
    }
}

void MainWindow::installSWGEmu() {
    InstallFromSWG installation(this);
    int result = installation.installFiles();

    if (result == 0) {
        QSettings settingsVals;
        settingsVals.setValue("swg_folder", installation.getEmuFolder());

        settings->restoreFolder();

        startFullScan(true);
    }
}

void MainWindow::showGameModsOptions() {
    GameMods dialog(this);
    dialog.exec();
}

void MainWindow::showMacroEditor() {
    MacroEditor dialog(this);
    dialog.exec();
}

