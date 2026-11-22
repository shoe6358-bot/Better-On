#include <QApplication>
#include <QCheckBox>
#include <QBuffer>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFrame>
#include <QHash>
#include <QHBoxLayout>
#include <QIcon>
#include <QImage>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProgressBar>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QStandardPaths>
#include <QStyle>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>
#include <QPixmap>

#include <algorithm>
#include <utility>

namespace {

struct CatalogApp {
    QString id;
    QString name;
    QString summary;
    QString developer;
    QString category;
    QString verification;
    QString iconUrl;
    QString detailsUrl;
    QString note;
};

bool validAppId(const QString &id)
{
    static const QRegularExpression pattern(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._-]*$"));
    return pattern.match(id).hasMatch() && id.contains(QLatin1Char('.'));
}

bool loadCatalog(QHash<QString, CatalogApp> *apps, QString *error)
{
    const QString path = QStandardPaths::locate(
        QStandardPaths::GenericDataLocation,
        QStringLiteral("better-on/hub/catalog.json"));
    if (path.isEmpty()) {
        *error = QStringLiteral("The Better On Hub catalog file is missing.");
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = QStringLiteral("The Better On Hub catalog could not be read.");
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        *error = QStringLiteral("The Better On Hub catalog is invalid: %1").arg(parseError.errorString());
        return false;
    }

    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("schemaVersion")).toInt() != 1 ||
        root.value(QStringLiteral("remote")).toObject().value(QStringLiteral("name")).toString() != QStringLiteral("flathub")) {
        *error = QStringLiteral("The catalog schema or application source is not supported.");
        return false;
    }

    const QJsonArray entries = root.value(QStringLiteral("applications")).toArray();
    if (entries.isEmpty()) {
        *error = QStringLiteral("The Better On Hub catalog contains no applications.");
        return false;
    }

    for (const QJsonValue &entry : entries) {
        if (!entry.isObject()) {
            *error = QStringLiteral("The catalog contains an invalid application entry.");
            return false;
        }
        const QJsonObject object = entry.toObject();
        CatalogApp app{
            object.value(QStringLiteral("id")).toString(),
            object.value(QStringLiteral("name")).toString(),
            object.value(QStringLiteral("summary")).toString(),
            object.value(QStringLiteral("developer")).toString(),
            object.value(QStringLiteral("category")).toString(),
            object.value(QStringLiteral("verification")).toString(),
            object.value(QStringLiteral("icon")).toString(),
            object.value(QStringLiteral("details")).toString(),
            object.value(QStringLiteral("note")).toString()
        };

        const QUrl iconUrl(app.iconUrl);
        const QUrl detailsUrl(app.detailsUrl);
        if (!validAppId(app.id) || app.name.isEmpty() || app.summary.isEmpty() ||
            app.category.isEmpty() || apps->contains(app.id) ||
            iconUrl.scheme() != QStringLiteral("https") ||
            iconUrl.host() != QStringLiteral("dl.flathub.org") ||
            iconUrl.port(-1) != -1 || !iconUrl.userInfo().isEmpty() ||
            !iconUrl.query().isEmpty() || !iconUrl.fragment().isEmpty() ||
            !iconUrl.path().endsWith(QStringLiteral(".png")) ||
            detailsUrl.scheme() != QStringLiteral("https") ||
            detailsUrl.host() != QStringLiteral("flathub.org") ||
            detailsUrl.port(-1) != -1 || !detailsUrl.userInfo().isEmpty() ||
            !detailsUrl.query().isEmpty() || !detailsUrl.fragment().isEmpty() ||
            detailsUrl.path() != QStringLiteral("/en/apps/") + app.id) {
            *error = QStringLiteral("The catalog contains invalid or duplicate application metadata.");
            return false;
        }
        apps->insert(app.id, app);
    }

    return true;
}

QString iconNameForCategory(const QString &category)
{
    if (category == QStringLiteral("Browsers")) return QStringLiteral("applications-internet");
    if (category == QStringLiteral("Gaming")) return QStringLiteral("applications-games");
    if (category == QStringLiteral("Communication")) return QStringLiteral("internet-mail");
    if (category == QStringLiteral("Media & Creativity")) return QStringLiteral("applications-multimedia");
    if (category == QStringLiteral("Productivity")) return QStringLiteral("applications-office");
    if (category == QStringLiteral("Utilities")) return QStringLiteral("applications-system");
    if (category == QStringLiteral("Development")) return QStringLiteral("applications-development");
    return QStringLiteral("application-x-executable");
}

class HubWindow final : public QMainWindow {
    enum class Operation { Install, Update, Uninstall };

public:
    HubWindow()
    {
        icons.setTransferTimeout(10000);
        setWindowTitle(QStringLiteral("Better On Hub"));
        setWindowIcon(QIcon::fromTheme(QStringLiteral("better-on")));
        setMinimumSize(780, 620);
        resize(940, 760);
        setStyleSheet(QStringLiteral(R"(
            QWidget { background: #171b21; color: #edf2f8; font-family: "Noto Sans", sans-serif; font-size: 14px; }
            QLabel#title { font-size: 30px; font-weight: 700; }
            QLabel#subtitle, QLabel#muted { color: #bac5d1; }
            QLineEdit, QComboBox { background: #202630; border: 1px solid #394655; border-radius: 8px; padding: 9px 11px; }
            QFrame#appCard { background: #202630; border: 1px solid #303b49; border-radius: 11px; }
            QLabel#appName { font-size: 17px; font-weight: 650; }
            QLabel#appMeta { color: #aebbc9; font-size: 12px; }
            QLabel#appNote { color: #f3c77b; font-size: 12px; }
            QPushButton { background: #263442; border: 1px solid #40556a; border-radius: 9px; padding: 10px 15px; font-weight: 600; }
            QPushButton:hover { background: #30475b; border-color: #78bce9; }
            QPushButton:disabled { color: #78838e; background: #202630; border-color: #303b49; }
            QPushButton#primaryButton { background: #a8d9fa; color: #14212b; border: 0; }
            QPushButton#primaryButton:hover { background: #c0e5ff; }
            QProgressBar { background: #14181d; border: 0; border-radius: 3px; height: 6px; color: transparent; }
            QProgressBar::chunk { background: #89c9f5; border-radius: 3px; }
            QScrollArea { border: 0; }
        )"));

        auto *central = new QWidget;
        auto *root = new QVBoxLayout(central);
        root->setContentsMargins(28, 24, 28, 22);
        root->setSpacing(14);

        auto *title = new QLabel(QStringLiteral("Better On Hub"));
        title->setObjectName(QStringLiteral("title"));
        auto *subtitle = new QLabel(QStringLiteral("Install new apps, update Flathub apps, or remove apps from your account."));
        subtitle->setObjectName(QStringLiteral("subtitle"));
        auto *sourceNote = new QLabel(QStringLiteral("Apps install from Flathub using Flatpak's signature checks. No administrator access is requested."));
        sourceNote->setObjectName(QStringLiteral("muted"));
        sourceNote->setWordWrap(true);
        root->addWidget(title);
        root->addWidget(subtitle);
        root->addWidget(sourceNote);

        auto *filters = new QHBoxLayout;
        searchField = new QLineEdit;
        searchField->setPlaceholderText(QStringLiteral("Search applications"));
        categoryFilter = new QComboBox;
        categoryFilter->addItem(QStringLiteral("All categories"));
        operationFilter = new QComboBox;
        operationFilter->addItems({QStringLiteral("Install"), QStringLiteral("Update"),
                                   QStringLiteral("Uninstall")});
        operationFilter->setToolTip(QStringLiteral("Manage applications installed for your user account."));
        filters->addWidget(searchField, 1);
        filters->addWidget(categoryFilter);
        filters->addWidget(operationFilter);
        root->addLayout(filters);

        auto *scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        auto *list = new QWidget;
        cardsLayout = new QVBoxLayout(list);
        cardsLayout->setContentsMargins(2, 2, 8, 2);
        cardsLayout->setSpacing(10);
        cardsLayout->addStretch(1);
        scroll->setWidget(list);
        root->addWidget(scroll, 1);

        overallProgress = new QProgressBar;
        overallProgress->setRange(0, 1);
        overallProgress->setValue(0);
        overallProgress->setTextVisible(false);
        overallProgress->hide();
        root->addWidget(overallProgress);

        footerStatus = new QLabel(QStringLiteral("Loading catalog..."));
        footerStatus->setObjectName(QStringLiteral("muted"));
        footerStatus->setWordWrap(true);
        root->addWidget(footerStatus);

        auto *footer = new QHBoxLayout;
        retryButton = new QPushButton(QStringLiteral("Retry failed"));
        cancelPendingButton = new QPushButton(QStringLiteral("Cancel pending"));
        installButton = new QPushButton(QStringLiteral("Install selected applications"));
        installButton->setObjectName(QStringLiteral("primaryButton"));
        retryButton->setEnabled(false);
        cancelPendingButton->setEnabled(false);
        installButton->setEnabled(false);
        footer->addWidget(retryButton);
        footer->addWidget(cancelPendingButton);
        footer->addStretch(1);
        footer->addWidget(installButton);
        root->addLayout(footer);
        setCentralWidget(central);

        const QString flatpakPath = QStandardPaths::findExecutable(QStringLiteral("flatpak"));
        if (flatpakPath.isEmpty()) {
            footerStatus->setText(QStringLiteral("Flatpak is not available in this system image."));
        } else {
            flatpakExecutable = flatpakPath;
        }

        QString catalogError;
        if (!loadCatalog(&apps, &catalogError)) {
            footerStatus->setText(catalogError);
        } else {
            populateCards();
            requestIcons();
            refreshInstalled();
        }

        connect(searchField, &QLineEdit::textChanged, this, [this] { applyFilters(); });
        connect(categoryFilter, &QComboBox::currentTextChanged, this, [this] { applyFilters(); });
        connect(operationFilter, &QComboBox::currentTextChanged, this, [this](const QString &) {
            operation = static_cast<Operation>(operationFilter->currentIndex());
            for (auto it = checkboxes.begin(); it != checkboxes.end(); ++it) it.value()->setChecked(false);
            updateSelectionAvailability();
            updateButtons();
        });
        connect(installButton, &QPushButton::clicked, this, [this] { confirmAndRun(selectedIds(), operation); });
        connect(retryButton, &QPushButton::clicked, this, [this] { confirmAndRun(failedIds, queueOperation); });
        connect(cancelPendingButton, &QPushButton::clicked, this, [this] { cancelPending(); });

        installProcess.setProcessChannelMode(QProcess::MergedChannels);
        connect(&installProcess, &QProcess::readyRead, this, [this] {
            const QString chunk = QString::fromLocal8Bit(installProcess.readAll());
            if (!chunk.trimmed().isEmpty()) {
                processOutput += chunk;
                if (processOutput.size() > 8192) processOutput = processOutput.right(8192);
            }
        });
        connect(&installProcess, &QProcess::started, this, [this] {
            if (apps.contains(currentId)) {
                footerStatus->setText(QStringLiteral("%1 %2...")
                                          .arg(operationVerb(queueOperation), apps.value(currentId).name));
            }
        });
        connect(&installProcess, &QProcess::finished, this,
                [this](int code, QProcess::ExitStatus status) {
            completeCurrentOperation(status == QProcess::NormalExit && code == 0,
                                   status == QProcess::NormalExit && code == 0
                                       ? QString()
                                       : QStringLiteral("Flatpak exited with code %1.").arg(code));
        });
        connect(&installProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart) {
                completeCurrentOperation(false, installProcess.errorString());
            }
        });

        connect(&remoteListProcess, &QProcess::finished, this,
                [this](int code, QProcess::ExitStatus status) {
            if (status != QProcess::NormalExit || code != 0) {
                abortQueue(QStringLiteral("Could not check the Flathub repository configuration."));
                return;
            }
            const QString output = QString::fromLocal8Bit(remoteListProcess.readAllStandardOutput());
            bool foundFlathub = false;
            for (const QString &line : output.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
                const QStringList columns = line.trimmed().split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
                if (columns.size() < 2 || columns.first() != QStringLiteral("flathub")) continue;
                const QUrl remoteUrl(columns.at(1));
                const QString options = columns.mid(2).join(QLatin1Char(' '));
                const bool officialUrl = remoteUrl.scheme() == QStringLiteral("https") &&
                    remoteUrl.port(-1) == -1 && remoteUrl.userInfo().isEmpty() &&
                    remoteUrl.query().isEmpty() && remoteUrl.fragment().isEmpty() &&
                    (remoteUrl.host() == QStringLiteral("dl.flathub.org") ||
                     remoteUrl.host() == QStringLiteral("flathub.org")) &&
                    (remoteUrl.path() == QStringLiteral("/repo") ||
                     remoteUrl.path() == QStringLiteral("/repo/"));
                if (!officialUrl || options.contains(QStringLiteral("no-gpg-verify"))) {
                    abortQueue(QStringLiteral("A configured Flathub remote is not the official HTTPS source with signature checks enabled. Review it in Flatpak settings before installing."));
                    return;
                }
                foundFlathub = true;
            }

            if (foundFlathub) {
                startQueuedOperations();
                return;
            }

            footerStatus->setText(QStringLiteral("Adding Flathub for this user..."));
            remoteAddProcess.start(flatpakExecutable,
                {QStringLiteral("remote-add"), QStringLiteral("--user"), QStringLiteral("--if-not-exists"),
                 QStringLiteral("flathub"), QStringLiteral("https://dl.flathub.org/repo/flathub.flatpakrepo")});
        });
        connect(&remoteListProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart) abortQueue(remoteListProcess.errorString());
        });
        connect(&remoteAddProcess, &QProcess::finished, this,
                [this](int code, QProcess::ExitStatus status) {
            if (status != QProcess::NormalExit || code != 0) {
                abortQueue(QStringLiteral("Could not add the official Flathub remote for this user. Check your network connection and try again."));
                return;
            }
            startQueuedOperations();
        });
        connect(&remoteAddProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart) abortQueue(remoteAddProcess.errorString());
        });
        connect(&installedProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart) {
                installedStateKnown = true;
                footerStatus->setText(QStringLiteral("Could not check installed applications. Installation attempts are still available."));
                updateButtons();
            }
        });
    }

private:
    QHash<QString, CatalogApp> apps;
    QHash<QString, QWidget *> cardWidgets;
    QHash<QString, QCheckBox *> checkboxes;
    QHash<QString, QLabel *> statusLabels;
    QHash<QString, QProgressBar *> appProgress;
    QHash<QString, bool> installed;
    QHash<QString, QString> appOrigins;
    QStringList pendingIds;
    QStringList failedIds;
    QString currentId;
    QString processOutput;
    QString flatpakExecutable;
    Operation operation = Operation::Install;
    Operation queueOperation = Operation::Install;
    QLineEdit *searchField = nullptr;
    QComboBox *categoryFilter = nullptr;
    QComboBox *operationFilter = nullptr;
    QVBoxLayout *cardsLayout = nullptr;
    QLabel *footerStatus = nullptr;
    QProgressBar *overallProgress = nullptr;
    QPushButton *installButton = nullptr;
    QPushButton *retryButton = nullptr;
    QPushButton *cancelPendingButton = nullptr;
    QProcess installProcess;
    QProcess installedProcess;
    QProcess remoteListProcess;
    QProcess remoteAddProcess;
    QNetworkAccessManager icons;
    QHash<QString, QString> failureReasons;
    int queueTotal = 0;
    int queueCompleted = 0;
    bool processFinalized = false;
    bool installedStateKnown = false;
    bool sourceCheckInProgress = false;
    bool pendingWasCanceled = false;

    QWidget *makeCard(const CatalogApp &app)
    {
        auto *card = new QFrame;
        card->setObjectName(QStringLiteral("appCard"));
        auto *row = new QHBoxLayout(card);
        row->setContentsMargins(16, 14, 16, 14);
        row->setSpacing(14);

        auto *icon = new QLabel;
        icon->setFixedSize(52, 52);
        icon->setAlignment(Qt::AlignCenter);
        icon->setObjectName(QStringLiteral("appIcon_%1").arg(app.id));
        icon->setPixmap(QIcon::fromTheme(iconNameForCategory(app.category),
                                         style()->standardIcon(QStyle::SP_FileIcon))
                            .pixmap(48, 48));
        row->addWidget(icon);

        auto *details = new QVBoxLayout;
        details->setSpacing(4);
        auto *name = new QLabel(app.name);
        name->setObjectName(QStringLiteral("appName"));
        auto *summary = new QLabel(app.summary);
        summary->setObjectName(QStringLiteral("muted"));
        summary->setWordWrap(true);
        auto *meta = new QLabel(QStringLiteral("%1  ·  %2  ·  %3")
                                    .arg(app.category, app.developer, app.verification));
        meta->setObjectName(QStringLiteral("appMeta"));
        details->addWidget(name);
        details->addWidget(summary);
        details->addWidget(meta);
        auto *listing = new QLabel(QStringLiteral("<a href=\"%1\">View Flathub listing</a>")
                                       .arg(app.detailsUrl.toHtmlEscaped()));
        listing->setTextFormat(Qt::RichText);
        listing->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
        listing->setOpenExternalLinks(true);
        details->addWidget(listing);
        if (!app.note.isEmpty()) {
            auto *note = new QLabel(app.note);
            note->setObjectName(QStringLiteral("appNote"));
            note->setWordWrap(true);
            details->addWidget(note);
        }
        auto *status = new QLabel(QStringLiteral("Available"));
        status->setObjectName(QStringLiteral("appMeta"));
        statusLabels.insert(app.id, status);
        auto *progress = new QProgressBar;
        progress->setRange(0, 100);
        progress->setValue(0);
        progress->setFixedWidth(120);
        progress->hide();
        appProgress.insert(app.id, progress);
        details->addWidget(status);
        details->addWidget(progress);
        row->addLayout(details, 1);

        auto *select = new QCheckBox(QStringLiteral("Select"));
        select->setAccessibleName(QStringLiteral("Select %1 for installation").arg(app.name));
        checkboxes.insert(app.id, select);
        connect(select, &QCheckBox::toggled, this, [this] { updateButtons(); });
        row->addWidget(select, 0, Qt::AlignVCenter);

        cardWidgets.insert(app.id, card);
        return card;
    }

    void populateCards()
    {
        QStringList ids = apps.keys();
        QStringList categories;
        for (auto it = apps.cbegin(); it != apps.cend(); ++it) {
            if (!categories.contains(it.value().category)) categories.append(it.value().category);
        }
        std::sort(categories.begin(), categories.end(), [](const QString &left, const QString &right) {
            return left.localeAwareCompare(right) < 0;
        });
        categoryFilter->addItems(categories);
        std::sort(ids.begin(), ids.end(), [this](const QString &left, const QString &right) {
            return apps.value(left).name.localeAwareCompare(apps.value(right).name) < 0;
        });
        for (const QString &id : ids) {
            QWidget *card = makeCard(apps.value(id));
            cardsLayout->insertWidget(cardsLayout->count() - 1, card);
        }
        footerStatus->setText(QStringLiteral("%1 applications available from Flathub.").arg(apps.size()));
        updateButtons();
    }

    void applyFilters()
    {
        const QString query = searchField->text().trimmed();
        const QString category = categoryFilter->currentText();
        for (auto it = apps.cbegin(); it != apps.cend(); ++it) {
            const CatalogApp &app = it.value();
            const bool matchesQuery = query.isEmpty() || app.name.contains(query, Qt::CaseInsensitive) ||
                                      app.summary.contains(query, Qt::CaseInsensitive) ||
                                      app.developer.contains(query, Qt::CaseInsensitive);
            const bool matchesCategory = category == QStringLiteral("All categories") || app.category == category;
            cardWidgets.value(app.id)->setVisible(matchesQuery && matchesCategory);
        }
    }

    void requestIcons()
    {
        const QString cachePath = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) +
                                  QStringLiteral("/icons");
        QDir().mkpath(cachePath);
        for (auto it = apps.cbegin(); it != apps.cend(); ++it) {
            const QString id = it.key();
            const QString imagePath = cachePath + QLatin1Char('/') + id + QStringLiteral(".png");
            QImageReader cachedReader(imagePath);
            const QSize cachedSize = cachedReader.size();
            QImage cached;
            if (!cachedSize.isEmpty() && cachedSize.width() <= 512 && cachedSize.height() <= 512 &&
                cachedReader.read(&cached)) {
                setAppIcon(id, cached);
                continue;
            }

            auto *reply = icons.get(QNetworkRequest(QUrl(it.value().iconUrl)));
            connect(reply, &QNetworkReply::downloadProgress, reply,
                    [reply](qint64 received, qint64) {
                if (received > 2 * 1024 * 1024) reply->abort();
            });
            connect(reply, &QNetworkReply::finished, this, [this, reply, imagePath, id] {
                if (reply->error() != QNetworkReply::NoError || reply->bytesAvailable() > 2 * 1024 * 1024) {
                    reply->deleteLater();
                    return;
                }
                const QByteArray data = reply->readAll();
                QBuffer buffer;
                buffer.setData(data);
                buffer.open(QIODevice::ReadOnly);
                QImageReader reader(&buffer);
                const QSize size = reader.size();
                QImage image;
                const bool downloaded = !size.isEmpty() && size.width() <= 512 && size.height() <= 512 &&
                                        reader.read(&image);
                reply->deleteLater();
                if (!downloaded) return;
                image.save(imagePath, "PNG");
                setAppIcon(id, image);
            });
        }
    }

    void setAppIcon(const QString &id, const QImage &image)
    {
        auto *label = cardWidgets.value(id)->findChild<QLabel *>(QStringLiteral("appIcon_%1").arg(id));
        if (label) {
            label->setPixmap(QPixmap::fromImage(image).scaled(48, 48, Qt::KeepAspectRatio,
                                                              Qt::SmoothTransformation));
        }
    }

    void refreshInstalled()
    {
        if (flatpakExecutable.isEmpty()) {
            return;
        }
        connect(&installedProcess, &QProcess::finished, this,
                [this](int code, QProcess::ExitStatus status) {
            installedStateKnown = true;
            if (status != QProcess::NormalExit || code != 0) {
                footerStatus->setText(QStringLiteral("Could not read user-installed Flatpak applications. Check the Flatpak setup and try again."));
                updateButtons();
                return;
            }
            const QString output = QString::fromLocal8Bit(installedProcess.readAllStandardOutput());
            for (const QString &line : output.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
                const QStringList columns = line.trimmed().split(
                    QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
                if (columns.size() < 2) continue;
                const QString id = columns.at(0).trimmed();
                const QString origin = columns.at(1).trimmed();
                if (apps.contains(id)) setInstalled(id, origin);
            }
            updateSelectionAvailability();
            footerStatus->setText(QStringLiteral("%1 applications available. Installed status covers this user account.").arg(apps.size()));
            updateButtons();
        });
        installedProcess.start(flatpakExecutable,
                               {QStringLiteral("list"), QStringLiteral("--user"), QStringLiteral("--app"),
                                QStringLiteral("--columns=application,origin")});
    }

    QStringList selectedIds() const
    {
        QStringList selected;
        for (auto it = checkboxes.cbegin(); it != checkboxes.cend(); ++it) {
            const bool isInstalled = installed.value(it.key(), false);
            const QString origin = appOrigins.value(it.key());
            const bool eligible = operation == Operation::Install ? !isInstalled
                : operation == Operation::Update ? isInstalled && origin == QStringLiteral("flathub")
                : isInstalled;
            if (it.value()->isChecked() && eligible) selected.append(it.key());
        }
        return selected;
    }

    QStringList appNames(const QStringList &ids) const
    {
        QStringList names;
        for (const QString &id : ids) {
            if (apps.contains(id)) names.append(QStringLiteral("%1 (%2)").arg(apps.value(id).name, id));
        }
        return names;
    }

    QString operationVerb(Operation requested) const
    {
        switch (requested) {
        case Operation::Install: return QStringLiteral("Installing");
        case Operation::Update: return QStringLiteral("Updating");
        case Operation::Uninstall: return QStringLiteral("Removing");
        }
        return QStringLiteral("Working on");
    }

    QString operationButtonText(Operation requested) const
    {
        switch (requested) {
        case Operation::Install: return QStringLiteral("Install selected applications");
        case Operation::Update: return QStringLiteral("Update selected applications");
        case Operation::Uninstall: return QStringLiteral("Uninstall selected applications");
        }
        return QStringLiteral("Manage selected applications");
    }

    void confirmAndRun(const QStringList &ids, Operation requested)
    {
        if (ids.isEmpty() || flatpakExecutable.isEmpty() || !pendingIds.isEmpty() || !currentId.isEmpty()) return;
        QString prompt;
        if (requested == Operation::Install) {
            prompt = QStringLiteral("Install these applications for your user from Flathub?\n\n%1\n\nFlatpak will verify repository signatures. No administrator access is requested.");
        } else if (requested == Operation::Update) {
            prompt = QStringLiteral("Update these applications installed for your user?\n\n%1\n\nFlatpak will verify repository signatures. No administrator access is requested.");
        } else {
            prompt = QStringLiteral("Uninstall these applications from your user account?\n\n%1\n\nApplication data and settings will be kept. No administrator access is requested.");
        }
        const QMessageBox::StandardButton answer = QMessageBox::question(
            this, QStringLiteral("Confirm application action"),
            prompt.arg(appNames(ids).join(QLatin1Char('\n'))),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
        if (answer != QMessageBox::Yes) return;

        pendingIds = ids;
        queueOperation = requested;
        pendingWasCanceled = false;
        sourceCheckInProgress = requested != Operation::Uninstall;
        failedIds.clear();
        queueTotal = pendingIds.size();
        queueCompleted = 0;
        for (const QString &id : std::as_const(pendingIds)) {
            setStatus(id, sourceCheckInProgress ? QStringLiteral("Checking source") : QStringLiteral("Queued"));
        }
        overallProgress->setRange(0, queueTotal);
        overallProgress->setValue(0);
        overallProgress->show();
        updateButtons();
        if (!sourceCheckInProgress) {
            footerStatus->setText(QStringLiteral("Preparing user-scoped uninstall..."));
            startQueuedOperations();
            return;
        }
        footerStatus->setText(QStringLiteral("Checking Flathub source and signature settings..."));
        remoteListProcess.start(flatpakExecutable,
                                {QStringLiteral("remotes"),
                                 QStringLiteral("--columns=name,url,options")});
    }

    void startQueuedOperations()
    {
        sourceCheckInProgress = false;
        for (const QString &id : std::as_const(pendingIds)) setStatus(id, QStringLiteral("Queued"));
        updateButtons();
        startNextOperation();
    }

    void abortQueue(const QString &reason)
    {
        sourceCheckInProgress = false;
        for (const QString &id : std::as_const(pendingIds)) setStatus(id, QStringLiteral("Not started"));
        pendingIds.clear();
        currentId.clear();
        overallProgress->hide();
        footerStatus->setText(reason);
        updateButtons();
    }

    void startNextOperation()
    {
        if (pendingIds.isEmpty()) {
            currentId.clear();
            overallProgress->hide();
            cancelPendingButton->setEnabled(false);
            retryButton->setEnabled(!failedIds.isEmpty() && !flatpakExecutable.isEmpty());
            updateButtons();
            if (failedIds.isEmpty()) {
                const QString completion = queueOperation == Operation::Install
                    ? QStringLiteral("Installation finished")
                    : queueOperation == Operation::Update
                        ? QStringLiteral("Application updates finished")
                        : QStringLiteral("Application removal finished");
                footerStatus->setText(pendingWasCanceled
                                          ? completion + QStringLiteral(". Pending applications were canceled.")
                                          : completion + QLatin1Char('.'));
            } else {
                QStringList failures;
                for (const QString &id : failedIds) {
                    failures.append(QStringLiteral("%1: %2").arg(apps.value(id).name, failureReasons.value(id)));
                }
                footerStatus->setText(QStringLiteral("Some application actions failed. Retry the failed items or open their details on Flathub."));
                QMessageBox::warning(this, QStringLiteral("Some application actions failed"), failures.join(QLatin1Char('\n')));
            }
            return;
        }

        currentId = pendingIds.takeFirst();
        processFinalized = false;
        processOutput.clear();
        setStatus(currentId, operationVerb(queueOperation), true);
        overallProgress->setRange(0, queueTotal);
        overallProgress->setValue(queueCompleted);
        cancelPendingButton->setEnabled(!pendingIds.isEmpty());
        updateButtons();
        QStringList arguments{QStringLiteral("--user"), QStringLiteral("--noninteractive"),
                              QStringLiteral("--assumeyes")};
        if (queueOperation == Operation::Install) {
            arguments.prepend(QStringLiteral("install"));
            arguments.append(QStringLiteral("flathub"));
            arguments.append(currentId);
        } else if (queueOperation == Operation::Update) {
            arguments.prepend(QStringLiteral("update"));
            arguments.append(QStringLiteral("--app"));
            arguments.append(currentId);
        } else {
            arguments.prepend(QStringLiteral("uninstall"));
            arguments.append(QStringLiteral("--app"));
            arguments.append(currentId);
        }
        installProcess.start(flatpakExecutable, arguments);
    }

    void completeCurrentOperation(bool success, const QString &reason)
    {
        if (currentId.isEmpty() || processFinalized) return;
        processFinalized = true;
        const QString completedId = currentId;
        if (success) {
            if (queueOperation == Operation::Uninstall) {
                setUninstalled(completedId);
            } else {
                setInstalled(completedId, queueOperation == Operation::Install
                                              ? QStringLiteral("flathub")
                                              : appOrigins.value(completedId));
                setStatus(completedId, queueOperation == Operation::Update
                                           ? QStringLiteral("Updated")
                                           : QStringLiteral("Installed"));
                appProgress.value(completedId)->setRange(0, 100);
                appProgress.value(completedId)->setValue(100);
            }
            failureReasons.remove(completedId);
            failedIds.removeAll(completedId);
        } else {
            setStatus(completedId, QStringLiteral("Failed - retry available"));
            appProgress.value(completedId)->setRange(0, 100);
            appProgress.value(completedId)->setValue(0);
            failedIds.append(completedId);
            const QString output = processOutput.trimmed().split(QLatin1Char('\n'), Qt::SkipEmptyParts).value(0);
            failureReasons.insert(completedId, output.isEmpty() ? reason : output.left(320));
            statusLabels.value(completedId)->setToolTip(processOutput.isEmpty() ? reason : processOutput.trimmed());
        }
        ++queueCompleted;
        overallProgress->setValue(queueCompleted);
        QTimer::singleShot(100, this, [this] { startNextOperation(); });
    }

    void cancelPending()
    {
        const QStringList canceled = pendingIds;
        pendingIds.clear();
        pendingWasCanceled = true;
        for (const QString &id : canceled) setStatus(id, QStringLiteral("Canceled"));
        cancelPendingButton->setEnabled(false);
        footerStatus->setText(currentId.isEmpty()
                                  ? QStringLiteral("Pending applications canceled.")
                                  : QStringLiteral("Pending applications canceled. The current action will finish."));
        updateButtons();
    }

    void setInstalled(const QString &id, const QString &origin)
    {
        installed.insert(id, true);
        appOrigins.insert(id, origin);
        if (checkboxes.contains(id)) {
            checkboxes.value(id)->setChecked(false);
            const bool canManage = operation == Operation::Uninstall ||
                (operation == Operation::Update && origin == QStringLiteral("flathub"));
            checkboxes.value(id)->setEnabled(canManage && installedStateKnown);
        }
        if (statusLabels.contains(id)) {
            const QString sourceStatus = origin.isEmpty()
                ? QStringLiteral("Installed (source unknown)")
                : QStringLiteral("Installed from %1").arg(origin);
            setStatus(id, origin == QStringLiteral("flathub") ? QStringLiteral("Installed") : sourceStatus);
        }
        if (appProgress.contains(id)) {
            appProgress.value(id)->setRange(0, 100);
            appProgress.value(id)->setValue(100);
        }
    }

    void setUninstalled(const QString &id)
    {
        installed.insert(id, false);
        appOrigins.remove(id);
        if (checkboxes.contains(id)) {
            checkboxes.value(id)->setChecked(false);
            checkboxes.value(id)->setEnabled(operation == Operation::Install && installedStateKnown);
        }
        if (statusLabels.contains(id)) setStatus(id, QStringLiteral("Not installed"));
        if (appProgress.contains(id)) appProgress.value(id)->hide();
    }

    void updateSelectionAvailability()
    {
        for (auto it = checkboxes.begin(); it != checkboxes.end(); ++it) {
            const bool isInstalled = installed.value(it.key(), false);
            const QString origin = appOrigins.value(it.key());
            const bool eligible = operation == Operation::Install ? !isInstalled
                : operation == Operation::Update ? isInstalled && origin == QStringLiteral("flathub")
                : isInstalled;
            it.value()->setEnabled(installedStateKnown && eligible);
        }
        installButton->setText(operationButtonText(operation));
    }

    void setStatus(const QString &id, const QString &text, bool active = false)
    {
        if (statusLabels.contains(id)) statusLabels.value(id)->setText(text);
        if (!appProgress.contains(id)) return;
        QProgressBar *progress = appProgress.value(id);
        const bool complete = text == QStringLiteral("Installed") || text == QStringLiteral("Updated");
        progress->setVisible(active || complete);
        if (active) {
            progress->setRange(0, 0);
        } else if (complete) {
            progress->setRange(0, 100);
            progress->setValue(100);
        } else {
            progress->setRange(0, 100);
            progress->setValue(0);
        }
    }

    void updateButtons()
    {
        const bool busy = !currentId.isEmpty() || !pendingIds.isEmpty();
        operationFilter->setEnabled(!busy);
        for (auto it = checkboxes.begin(); it != checkboxes.end(); ++it) {
            const bool isInstalled = installed.value(it.key(), false);
            const QString origin = appOrigins.value(it.key());
            const bool eligible = operation == Operation::Install ? !isInstalled
                : operation == Operation::Update ? isInstalled && origin == QStringLiteral("flathub")
                : isInstalled;
            it.value()->setEnabled(!busy && installedStateKnown && eligible);
        }
        installButton->setEnabled(!flatpakExecutable.isEmpty() && installedStateKnown &&
                                  !busy && !selectedIds().isEmpty());
        retryButton->setEnabled(!busy && !failedIds.isEmpty() && !flatpakExecutable.isEmpty());
        installButton->setText(operationButtonText(operation));
        if (busy) cancelPendingButton->setEnabled(!sourceCheckInProgress && !pendingIds.isEmpty());
        else cancelPendingButton->setEnabled(false);
    }

};

} // namespace

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("Better On Hub"));
    application.setOrganizationName(QStringLiteral("BetterOn"));
    HubWindow window;
    window.show();
    return application.exec();
}
