#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QHash>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLabel>
#include <QMainWindow>
#include <QMessageBox>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QScreen>
#include <QScrollArea>
#include <QStorageInfo>
#include <QStandardPaths>
#include <QStringList>
#include <QSysInfo>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <optional>

namespace {

QString readTextFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(file.readAll()).trimmed();
}

QString cpuModel()
{
    const QStringList lines = readTextFile(QStringLiteral("/proc/cpuinfo")).split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const qsizetype colon = line.indexOf(QLatin1Char(':'));
        if (colon < 0) {
            continue;
        }
        const QString key = line.left(colon).trimmed();
        if (key == QStringLiteral("model name") || key == QStringLiteral("Hardware") ||
            key == QStringLiteral("Processor")) {
            const QString value = line.mid(colon + 1).trimmed();
            if (!value.isEmpty()) {
                return value;
            }
        }
    }
    return QStringLiteral("CPU details are unavailable");
}

QString vendorName(const QString &pciId)
{
    if (pciId.compare(QStringLiteral("0x1002"), Qt::CaseInsensitive) == 0) return QStringLiteral("AMD");
    if (pciId.compare(QStringLiteral("0x10de"), Qt::CaseInsensitive) == 0) return QStringLiteral("NVIDIA");
    if (pciId.compare(QStringLiteral("0x8086"), Qt::CaseInsensitive) == 0) return QStringLiteral("Intel");
    if (pciId.isEmpty()) return QStringLiteral("Unknown vendor");
    return QStringLiteral("PCI vendor %1").arg(pciId);
}

QString graphicsDevices()
{
    QDir drmDirectory(QStringLiteral("/sys/class/drm"));
    const QStringList cards = drmDirectory.entryList({QStringLiteral("card[0-9]*")}, QDir::Dirs | QDir::NoDotAndDotDot);
    QStringList devices;
    for (const QString &card : cards) {
        if (card.contains(QLatin1Char('-'))) {
            continue;
        }
        const QString devicePath = drmDirectory.filePath(card + QStringLiteral("/device"));
        const QString vendor = readTextFile(devicePath + QStringLiteral("/vendor"));
        const QString device = readTextFile(devicePath + QStringLiteral("/device"));
        if (vendor.isEmpty() && device.isEmpty()) {
            continue;
        }

        QString detail = QStringLiteral("%1 graphics adapter").arg(vendorName(vendor));
        if (!device.isEmpty()) {
            detail += QStringLiteral(" (PCI device %1)").arg(device);
        }

        const QString driverPath = QFileInfo(devicePath + QStringLiteral("/driver")).symLinkTarget();
        if (!driverPath.isEmpty()) {
            detail += QStringLiteral(" - driver %1").arg(QFileInfo(driverPath).fileName());
        }
        if (!devices.contains(detail)) {
            devices.append(detail);
        }
    }
    return devices.isEmpty() ? QStringLiteral("No display adapter detected through DRM sysfs")
                             : devices.join(QLatin1Char('\n'));
}

QString vdfValue(const QString &contents, const QString &key)
{
    const QRegularExpression expression(
        QStringLiteral("\\\"%1\\\"\\s+\\\"((?:\\\\.|[^\\\"])*)\\\"")
            .arg(QRegularExpression::escape(key)),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = expression.match(contents);
    if (!match.hasMatch()) return {};
    QString value = match.captured(1);
    value.replace(QStringLiteral("\\\\"), QStringLiteral("\\"));
    return value;
}

QString installedSteamGames()
{
    const QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    if (home.isEmpty()) return QStringLiteral("Home directory is unavailable");

    QStringList steamAppsPaths{
        home + QStringLiteral("/.local/share/Steam/steamapps"),
        home + QStringLiteral("/.steam/steam/steamapps"),
        home + QStringLiteral("/.var/app/com.valvesoftware.Steam/.local/share/Steam/steamapps"),
    };
    for (qsizetype index = 0; index < steamAppsPaths.size(); ++index) {
        const QString libraryFile = QDir(steamAppsPaths.at(index)).filePath(QStringLiteral("libraryfolders.vdf"));
        const QString contents = readTextFile(libraryFile);
        if (contents.isEmpty()) continue;

        const QRegularExpression pathExpression(
            QStringLiteral("\\\"path\\\"\\s+\\\"((?:\\\\.|[^\\\"])*)\\\""),
            QRegularExpression::CaseInsensitiveOption);
        auto matchIterator = pathExpression.globalMatch(contents);
        while (matchIterator.hasNext()) {
            QString libraryPath = matchIterator.next().captured(1);
            libraryPath.replace(QStringLiteral("\\\\"), QStringLiteral("\\"));
            const QString path = QDir::cleanPath(libraryPath + QStringLiteral("/steamapps"));
            if (!steamAppsPaths.contains(path)) steamAppsPaths.append(path);
        }
    }

    QStringList games;
    for (const QString &steamAppsPath : steamAppsPaths) {
        QDir steamApps(steamAppsPath);
        const QString commonDirectory = QFileInfo(steamApps.filePath(QStringLiteral("common"))).canonicalFilePath();
        if (commonDirectory.isEmpty()) continue;
        const QStringList manifests = steamApps.entryList({QStringLiteral("appmanifest_*.acf")}, QDir::Files);
        for (const QString &manifest : manifests) {
            const QString contents = readTextFile(steamApps.filePath(manifest));
            const QString name = vdfValue(contents, QStringLiteral("name"));
            const QString installDirectory = vdfValue(contents, QStringLiteral("installdir"));
            if (name.isEmpty() || installDirectory.isEmpty()) continue;
            const QString installedPath = QFileInfo(
                steamApps.filePath(QStringLiteral("common/") + installDirectory)).canonicalFilePath();
            if (installedPath.startsWith(commonDirectory + QDir::separator())) games.append(name);
        }
    }

    games.removeDuplicates();
    games.sort(Qt::CaseInsensitive);
    if (!games.isEmpty()) return games.join(QLatin1Char('\n'));

    bool hasSteamLibrary = false;
    for (const QString &steamAppsPath : steamAppsPaths) {
        if (QFileInfo::exists(steamAppsPath)) {
            hasSteamLibrary = true;
            break;
        }
    }
    return hasSteamLibrary ? QStringLiteral("No installed Steam games detected")
                           : QStringLiteral("No Steam library detected for this user");
}

QHash<QString, QString> gamingCatalog(QString *error)
{
    const QString catalogPath = QStandardPaths::locate(
        QStandardPaths::GenericDataLocation,
        QStringLiteral("better-on/hub/catalog.json"));
    if (catalogPath.isEmpty()) {
        *error = QStringLiteral("The Better On Hub catalog is unavailable.");
        return {};
    }

    QFile file(catalogPath);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = QStringLiteral("The Better On Hub catalog could not be read.");
        return {};
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        *error = QStringLiteral("The Better On Hub catalog is invalid.");
        return {};
    }

    QHash<QString, QString> entries;
    const QJsonArray applications = document.object().value(QStringLiteral("applications")).toArray();
    for (const QJsonValue &value : applications) {
        if (!value.isObject()) continue;
        const QJsonObject entry = value.toObject();
        if (entry.value(QStringLiteral("category")).toString() != QStringLiteral("Gaming")) continue;
        const QString id = entry.value(QStringLiteral("id")).toString();
        const QString name = entry.value(QStringLiteral("name")).toString();
        if (!id.isEmpty() && !name.isEmpty()) {
            entries.insert(id, name);
        }
    }
    if (entries.isEmpty()) {
        *error = QStringLiteral("The catalog does not contain any gaming applications.");
    }
    return entries;
}

struct MemoryStats {
    quint64 totalKiB = 0;
    quint64 availableKiB = 0;
};

MemoryStats memoryStats()
{
    MemoryStats stats;
    const QStringList lines = readTextFile(QStringLiteral("/proc/meminfo")).split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const QStringList fields = line.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        if (fields.size() < 2) {
            continue;
        }
        bool valid = false;
        const quint64 value = fields.at(1).toULongLong(&valid);
        if (!valid) {
            continue;
        }
        if (fields.at(0) == QStringLiteral("MemTotal:")) stats.totalKiB = value;
        if (fields.at(0) == QStringLiteral("MemAvailable:")) stats.availableKiB = value;
    }
    return stats;
}

QString formatBytes(quint64 bytes)
{
    static const QStringList units{QStringLiteral("B"), QStringLiteral("KB"),
                                   QStringLiteral("MB"), QStringLiteral("GB"),
                                   QStringLiteral("TB")};
    double value = static_cast<double>(bytes);
    qsizetype unit = 0;
    while (value >= 1024.0 && unit + 1 < units.size()) {
        value /= 1024.0;
        ++unit;
    }
    return QStringLiteral("%1 %2").arg(value, 0, 'f', unit == 0 ? 0 : 1).arg(units.at(unit));
}

std::optional<double> cpuUsagePercent()
{
    static quint64 previousTotal = 0;
    static quint64 previousIdle = 0;
    static bool hasPreviousSample = false;
    const QString firstLine = readTextFile(QStringLiteral("/proc/stat")).section(QLatin1Char('\n'), 0, 0);
    const QStringList fields = firstLine.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    if (fields.size() < 5 || fields.first() != QStringLiteral("cpu")) {
        return std::nullopt;
    }

    quint64 total = 0;
    for (qsizetype index = 1; index < fields.size() && index <= 8; ++index) {
        bool valid = false;
        const quint64 value = fields.at(index).toULongLong(&valid);
        if (!valid) return std::nullopt;
        total += value;
    }
    bool idleValid = false;
    quint64 idle = fields.at(4).toULongLong(&idleValid);
    if (fields.size() > 5) {
        bool ioWaitValid = false;
        const quint64 ioWait = fields.at(5).toULongLong(&ioWaitValid);
        if (!ioWaitValid) return std::nullopt;
        idle += ioWait;
    }
    if (!idleValid || !hasPreviousSample || total <= previousTotal || idle < previousIdle) {
        previousTotal = total;
        previousIdle = idle;
        hasPreviousSample = idleValid;
        return std::nullopt;
    }

    const quint64 deltaTotal = total - previousTotal;
    const quint64 deltaIdle = idle - previousIdle;
    previousTotal = total;
    previousIdle = idle;
    if (deltaTotal == 0) return std::nullopt;
    return 100.0 * static_cast<double>(deltaTotal - std::min(deltaIdle, deltaTotal)) /
           static_cast<double>(deltaTotal);
}

QLabel *makeValueLabel(const QString &text)
{
    auto *label = new QLabel(text);
    label->setObjectName(QStringLiteral("value"));
    label->setWordWrap(true);
    label->setTextFormat(Qt::PlainText);
    return label;
}

QWidget *makeCard(const QString &title, QLabel **value)
{
    auto *card = new QWidget;
    card->setObjectName(QStringLiteral("card"));
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(8);
    auto *heading = new QLabel(title);
    heading->setObjectName(QStringLiteral("heading"));
    *value = makeValueLabel(QStringLiteral("Reading system information…"));
    layout->addWidget(heading);
    layout->addWidget(*value);
    return card;
}

void launchApplication(QWidget *parent, const QString &program,
                       const QStringList &arguments, const QString &displayName)
{
    const QString executable = QStandardPaths::findExecutable(program);
    if (executable.isEmpty()) {
        QMessageBox::information(
            parent, QStringLiteral("%1 is unavailable").arg(displayName),
            QStringLiteral("%1 is not installed in this image yet.").arg(displayName));
        return;
    }

    if (!QProcess::startDetached(executable, arguments)) {
        QMessageBox::warning(
            parent, QStringLiteral("Could not open %1").arg(displayName),
            QStringLiteral("Better On could not start %1. Try opening it from the application menu.")
                .arg(displayName));
    }
}

class GamingWindow final : public QMainWindow {
public:
    GamingWindow()
    {
        setWindowTitle(QStringLiteral("Better On Gaming"));
        setWindowIcon(QIcon::fromTheme(QStringLiteral("applications-games")));
        setMinimumSize(600, 480);
        resize(900, 780);
        setStyleSheet(QStringLiteral(R"(
            QWidget { background: #171b21; color: #edf2f8; font-family: "Noto Sans", sans-serif; font-size: 14px; }
            QLabel#eyebrow { color: #89c9f5; font-size: 12px; font-weight: 700; letter-spacing: 1px; }
            QLabel#title { font-size: 30px; font-weight: 700; }
            QLabel#subtitle, QLabel#note { color: #bac5d1; }
            QWidget#card { background: #202630; border: 1px solid #303b49; border-radius: 12px; }
            QLabel#heading { color: #a8d9fa; font-weight: 700; }
            QLabel#value { font-size: 16px; }
            QProgressBar { background: #12161b; border: 0; border-radius: 4px; height: 8px; color: transparent; }
            QProgressBar::chunk { background: #89c9f5; border-radius: 4px; }
            QPushButton { background: #263442; border: 1px solid #40556a; border-radius: 9px; padding: 10px 15px; font-weight: 600; }
            QPushButton:hover { background: #30475b; border-color: #78bce9; }
            QPushButton:focus { border: 2px solid #89c9f5; }
        )"));

        auto *central = new QWidget;
        central->setObjectName(QStringLiteral("dashboardContent"));
        auto *root = new QVBoxLayout(central);
        root->setContentsMargins(28, 24, 28, 24);
        root->setSpacing(14);

        auto *eyebrow = new QLabel(QStringLiteral("BETTER ON  /  GAMING"));
        eyebrow->setObjectName(QStringLiteral("eyebrow"));
        auto *title = new QLabel(QStringLiteral("Your gaming system"));
        title->setObjectName(QStringLiteral("title"));
        auto *subtitle = new QLabel(QStringLiteral("A live view of hardware and resources reported by this system."));
        subtitle->setObjectName(QStringLiteral("subtitle"));
        root->addWidget(eyebrow);
        root->addWidget(title);
        root->addWidget(subtitle);

        auto *cpuCard = makeCard(QStringLiteral("PROCESSOR"), &cpuLabel);
        auto *gpuCard = makeCard(QStringLiteral("GRAPHICS"), &gpuLabel);
        auto *systemCard = makeCard(QStringLiteral("SYSTEM"), &systemLabel);
        auto *gamingAppsCard = makeCard(QStringLiteral("INSTALLED FLATPAK GAMING LAUNCHERS"), &gamingAppsLabel);
        auto *installedGamesCard = makeCard(QStringLiteral("INSTALLED STEAM GAMES"), &installedGamesLabel);
        auto *grid = new QGridLayout;
        grid->setHorizontalSpacing(12);
        grid->setVerticalSpacing(12);
        grid->addWidget(cpuCard, 0, 0);
        grid->addWidget(gpuCard, 0, 1);
        grid->addWidget(makeCard(QStringLiteral("MEMORY"), &memoryLabel), 1, 0);
        grid->addWidget(makeCard(QStringLiteral("STORAGE"), &storageLabel), 1, 1);
        grid->addWidget(systemCard, 2, 0, 1, 2);
        grid->addWidget(gamingAppsCard, 3, 0);
        grid->addWidget(installedGamesCard, 3, 1);
        root->addLayout(grid);

        cpuProgress = new QProgressBar;
        cpuProgress->setRange(0, 100);
        cpuProgress->setTextVisible(false);
        cpuProgress->setAccessibleName(QStringLiteral("CPU utilization"));
        root->addWidget(cpuProgress);
        memoryProgress = new QProgressBar;
        memoryProgress->setRange(0, 100);
        memoryProgress->setTextVisible(false);
        memoryProgress->setAccessibleName(QStringLiteral("Memory use"));
        root->addWidget(memoryProgress);

        auto *note = new QLabel(QStringLiteral("These values come from Linux system interfaces. The primary display refresh rate is approximate. GPU temperatures, FPS, and game compatibility are not estimated here."));
        note->setObjectName(QStringLiteral("note"));
        note->setWordWrap(true);
        root->addWidget(note);

        auto *actions = new QHBoxLayout;
        auto *displayButton = new QPushButton(QStringLiteral("Display settings"));
        auto *refreshAppsButton = new QPushButton(QStringLiteral("Refresh installed apps"));
        auto *appsButton = new QPushButton(QStringLiteral("Gaming apps in Hub"));
        actions->addWidget(displayButton);
        actions->addWidget(refreshAppsButton);
        actions->addWidget(appsButton);
        actions->addStretch(1);
        root->addLayout(actions);
        auto *scrollArea = new QScrollArea;
        scrollArea->setWidgetResizable(true);
        scrollArea->setFrameShape(QFrame::NoFrame);
        scrollArea->setWidget(central);
        setCentralWidget(scrollArea);

        connect(displayButton, &QPushButton::clicked, this, [this] {
            launchApplication(this, QStringLiteral("systemsettings"),
                              QStringList{QStringLiteral("kcm_kscreen")},
                              QStringLiteral("Display settings"));
        });
        connect(appsButton, &QPushButton::clicked, this, [this] {
            launchApplication(this, QStringLiteral("better-on-hub"), {},
                              QStringLiteral("Better On Hub"));
        });
        connect(refreshAppsButton, &QPushButton::clicked, this, [this] {
            refreshInstalledGamingApps();
        });

        const QString pciUtility = QStandardPaths::findExecutable(QStringLiteral("lspci"));
        if (!pciUtility.isEmpty()) {
            auto *pciProcess = new QProcess(this);
            auto *pciTimeout = new QTimer(pciProcess);
            pciTimeout->setSingleShot(true);
            pciTimeout->setInterval(3000);
            connect(pciTimeout, &QTimer::timeout, pciProcess, [pciProcess] {
                if (pciProcess->state() == QProcess::Running) pciProcess->kill();
            });
            pciProcess->setProgram(pciUtility);
            pciProcess->setArguments({QStringLiteral("-nn")});
            connect(pciProcess, &QProcess::finished, this,
                    [this, pciProcess, pciTimeout](int exitCode, QProcess::ExitStatus exitStatus) {
                pciTimeout->stop();
                if (exitStatus != QProcess::NormalExit || exitCode != 0) return;
                const QString output = QString::fromUtf8(pciProcess->readAllStandardOutput());
                const QRegularExpression graphicsController(
                    QStringLiteral("(?:VGA compatible controller|3D controller|Display controller)(?:\\s+\\[[^]]+\\])?:\\s*(.+)$"),
                    QRegularExpression::CaseInsensitiveOption);
                QStringList models;
                for (const QString &line : output.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
                    const QRegularExpressionMatch match = graphicsController.match(line);
                    if (match.hasMatch()) models.append(match.captured(1).trimmed());
                }
                models.removeDuplicates();
                if (!models.isEmpty()) {
                    QStringList drivers;
                    const QStringList adapters = graphicsDevices().split(QLatin1Char('\n'), Qt::SkipEmptyParts);
                    for (const QString &adapter : adapters) {
                        const qsizetype driverMarker = adapter.indexOf(QStringLiteral(" - driver "));
                        if (driverMarker >= 0) {
                            const QString driver = adapter.mid(driverMarker + 10).trimmed();
                            if (!driver.isEmpty()) drivers.append(driver);
                        }
                    }
                    drivers.removeDuplicates();
                    if (!drivers.isEmpty()) {
                        models.append(QStringLiteral("Kernel driver: %1").arg(drivers.join(QStringLiteral(", "))));
                    }
                    gpuLabel->setText(models.join(QLatin1Char('\n')));
                }
            });
            pciProcess->start();
            pciTimeout->start();
        }

        flatpakProcess = new QProcess(this);
        flatpakTimeout = new QTimer(this);
        flatpakTimeout->setSingleShot(true);
        flatpakTimeout->setInterval(10000);
        connect(flatpakTimeout, &QTimer::timeout, this, [this] {
            if (flatpakProcess->state() == QProcess::Running) {
                flatpakTimedOut = true;
                gamingAppsLabel->setText(QStringLiteral("Checking installed apps timed out. Select refresh to try again."));
                flatpakProcess->kill();
            }
        });
        connect(flatpakProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart) {
                flatpakTimeout->stop();
                gamingAppsLabel->setText(QStringLiteral("Flatpak is unavailable. Open Better On Hub to browse gaming apps."));
            }
        });
        connect(flatpakProcess, &QProcess::finished, this,
                [this](int exitCode, QProcess::ExitStatus exitStatus) {
            flatpakTimeout->stop();
            if (flatpakTimedOut) {
                flatpakTimedOut = false;
                return;
            }
            if (exitStatus != QProcess::NormalExit || exitCode != 0) {
                gamingAppsLabel->setText(QStringLiteral("Installed gaming apps could not be read. Select refresh to try again."));
                return;
            }

            const QStringList installedIds = QString::fromUtf8(flatpakProcess->readAllStandardOutput())
                                                 .split(QLatin1Char('\n'), Qt::SkipEmptyParts);
            QStringList installedNames;
            for (const QString &id : installedIds) {
                if (gamingCatalogEntries.contains(id)) {
                    installedNames.append(gamingCatalogEntries.value(id));
                }
            }
            installedNames.removeDuplicates();
            installedNames.sort(Qt::CaseInsensitive);
            gamingAppsLabel->setText(installedNames.isEmpty()
                ? QStringLiteral("No gaming launchers from the catalog are installed through Flatpak.")
                : installedNames.join(QStringLiteral("\n")));
        });

        refreshSystemInfo();
        installedGamesLabel->setText(installedSteamGames());
        refreshInstalledGamingApps();
        auto *timer = new QTimer(this);
        timer->setInterval(2000);
        connect(timer, &QTimer::timeout, this, [this] { refreshSystemInfo(); });
        timer->start();
    }

private:
    void refreshSystemInfo()
    {
        QString cpuStatus = cpuModel() + QStringLiteral("\n") + QSysInfo::currentCpuArchitecture();
        gpuLabel->setText(graphicsDevices());
        const QScreen *primaryScreen = QGuiApplication::primaryScreen();
        const QString refreshRate = primaryScreen && primaryScreen->refreshRate() > 0.0
            ? QStringLiteral("Primary display refresh (approx.): %1 Hz")
                  .arg(primaryScreen->refreshRate(), 0, 'f', 1)
            : QStringLiteral("Primary display refresh: unavailable");
        systemLabel->setText(QStringLiteral("Kernel %1\n%2\n%3")
                                 .arg(QSysInfo::kernelVersion(), QSysInfo::prettyProductName(), refreshRate));

        const MemoryStats memory = memoryStats();
        if (memory.totalKiB > 0 && memory.availableKiB <= memory.totalKiB) {
            const quint64 usedKiB = memory.totalKiB - memory.availableKiB;
            memoryLabel->setText(QStringLiteral("%1 in use of %2")
                                     .arg(formatBytes(usedKiB * 1024), formatBytes(memory.totalKiB * 1024)));
            memoryProgress->setValue(static_cast<int>((100 * usedKiB) / memory.totalKiB));
        } else {
            memoryLabel->setText(QStringLiteral("Memory details are unavailable"));
            memoryProgress->setValue(0);
        }

        const QStorageInfo storage = QStorageInfo::root();
        if (storage.isValid() && storage.isReady() && storage.bytesTotal() > 0 &&
            storage.bytesAvailable() >= 0) {
            storageLabel->setText(QStringLiteral("%1 available of %2\nMounted at %3")
                                      .arg(formatBytes(static_cast<quint64>(storage.bytesAvailable())),
                                           formatBytes(static_cast<quint64>(storage.bytesTotal())),
                                           storage.rootPath()));
        } else {
            storageLabel->setText(QStringLiteral("Root storage details are unavailable"));
        }

        const std::optional<double> cpuUsage = cpuUsagePercent();
        if (cpuUsage.has_value()) {
            cpuProgress->setValue(static_cast<int>(*cpuUsage));
            cpuStatus += QStringLiteral("\nCurrent use: %1%").arg(*cpuUsage, 0, 'f', 0);
            cpuProgress->setToolTip(QStringLiteral("CPU utilization: %1% sampled over the last 2 seconds")
                                        .arg(*cpuUsage, 0, 'f', 0));
        } else {
            cpuProgress->setValue(0);
            cpuStatus += QStringLiteral("\nWaiting for a CPU sample");
            cpuProgress->setToolTip(QStringLiteral("Waiting for a valid CPU utilization sample"));
        }
        cpuLabel->setText(cpuStatus);
    }

    void refreshInstalledGamingApps()
    {
        if (flatpakProcess && flatpakProcess->state() != QProcess::NotRunning) {
            return;
        }

        QString catalogError;
        gamingCatalogEntries = gamingCatalog(&catalogError);
        if (gamingCatalogEntries.isEmpty()) {
            gamingAppsLabel->setText(catalogError);
            return;
        }

        const QString flatpak = QStandardPaths::findExecutable(QStringLiteral("flatpak"));
        if (flatpak.isEmpty()) {
            gamingAppsLabel->setText(QStringLiteral("Flatpak is unavailable. Open Better On Hub to browse gaming apps."));
            return;
        }

        gamingAppsLabel->setText(QStringLiteral("Checking installed apps…"));
        flatpakProcess->setProgram(flatpak);
        flatpakProcess->setArguments({QStringLiteral("list"), QStringLiteral("--app"),
                                      QStringLiteral("--columns=application")});
        flatpakProcess->start();
        flatpakTimeout->start();
    }

    QLabel *cpuLabel = nullptr;
    QLabel *gpuLabel = nullptr;
    QLabel *memoryLabel = nullptr;
    QLabel *storageLabel = nullptr;
    QLabel *systemLabel = nullptr;
    QLabel *gamingAppsLabel = nullptr;
    QLabel *installedGamesLabel = nullptr;
    QProgressBar *cpuProgress = nullptr;
    QProgressBar *memoryProgress = nullptr;
    QProcess *flatpakProcess = nullptr;
    QTimer *flatpakTimeout = nullptr;
    QHash<QString, QString> gamingCatalogEntries;
    bool flatpakTimedOut = false;
};

} // namespace

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("Better On Gaming"));
    application.setOrganizationName(QStringLiteral("BetterOn"));

    GamingWindow window;
    window.show();
    return application.exec();
}


