#include <QApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMessageBox>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStringList>
#include <QVBoxLayout>
#include <QWidget>

namespace {

bool wasCompleted()
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("BetterOn"), QStringLiteral("Welcome"));
    return settings.value(QStringLiteral("setup/completed"), false).toBool();
}

bool markCompleted()
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("BetterOn"), QStringLiteral("Welcome"));
    settings.setValue(QStringLiteral("setup/completed"), true);
    settings.sync();
    return settings.status() == QSettings::NoError;
}

void finishSetup(QWidget *window)
{
    if (!markCompleted()) {
        QMessageBox::warning(
            window, QStringLiteral("Could not save setup choice"),
            QStringLiteral("Better On could not save this choice. Check that your home folder is writable, then try again."));
        return;
    }
    window->close();
}

void launchApplication(QWidget *parent, const QString &program,
                        const QString &displayName, const QStringList &arguments)
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
            QStringLiteral("Better On could not start %1. You can open it from the application menu.")
                .arg(displayName));
    }
}

QFrame *makeCard(const QString &title, const QString &description)
{
    auto *card = new QFrame;
    card->setObjectName(QStringLiteral("welcomeCard"));

    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(6);

    auto *heading = new QLabel(title);
    heading->setObjectName(QStringLiteral("cardTitle"));
    auto *body = new QLabel(description);
    body->setObjectName(QStringLiteral("cardDescription"));
    body->setWordWrap(true);

    layout->addWidget(heading);
    layout->addWidget(body);
    return card;
}

QPushButton *makeActionButton(const QString &text)
{
    auto *button = new QPushButton(text);
    button->setMinimumHeight(44);
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("Better On Welcome"));
    application.setOrganizationName(QStringLiteral("BetterOn"));

    const bool firstLoginLaunch = application.arguments().contains(QStringLiteral("--first-login"));
    if (firstLoginLaunch && wasCompleted()) {
        return 0;
    }

    QWidget window;
    window.setWindowTitle(QStringLiteral("Better On Welcome"));
    window.setWindowIcon(QIcon::fromTheme(QStringLiteral("better-on")));
    window.setMinimumSize(680, 560);
    window.resize(780, 640);
    window.setStyleSheet(QStringLiteral(R"(
        QWidget {
            background: #171b21;
            color: #edf2f8;
            font-family: "Noto Sans", sans-serif;
            font-size: 14px;
        }
        QLabel#eyebrow { color: #89c9f5; font-size: 12px; font-weight: 700; letter-spacing: 1px; }
        QLabel#title { font-size: 32px; font-weight: 700; }
        QLabel#subtitle { color: #bac5d1; font-size: 15px; }
        QFrame#welcomeCard { background: #202630; border: 1px solid #303b49; border-radius: 12px; }
        QLabel#cardTitle { font-size: 16px; font-weight: 650; }
        QLabel#cardDescription { color: #bac5d1; }
        QPushButton {
            background: #263442; border: 1px solid #40556a; border-radius: 9px;
            padding: 10px 16px; text-align: left; font-weight: 600;
        }
        QPushButton:hover { background: #30475b; border-color: #78bce9; }
        QPushButton:focus { border: 2px solid #89c9f5; }
        QPushButton#primaryButton { background: #a8d9fa; color: #14212b; border: 0; text-align: center; }
        QPushButton#primaryButton:hover { background: #c0e5ff; }
        QPushButton#quietButton { background: transparent; border: 0; color: #bac5d1; text-align: center; }
        QPushButton#quietButton:hover { color: #ffffff; text-decoration: underline; }
    )"));

    auto *root = new QVBoxLayout(&window);
    root->setContentsMargins(36, 30, 36, 26);
    root->setSpacing(16);

    auto *eyebrow = new QLabel(QStringLiteral("BETTER ON  |  GAMING, WITHOUT THE FUSS"));
    eyebrow->setObjectName(QStringLiteral("eyebrow"));
    auto *stepLabel = new QLabel;
    stepLabel->setObjectName(QStringLiteral("subtitle"));
    auto *progress = new QProgressBar;
    progress->setRange(0, 5);
    progress->setTextVisible(false);
    progress->setAccessibleName(QStringLiteral("Welcome setup progress"));
    auto *pages = new QStackedWidget;

    const auto addPage = [pages](const QString &title, const QString &description,
                                 const QString &cardTitle, const QString &cardDescription,
                                 QPushButton **actionButton,
                                 const QString &actionText) {
        auto *page = new QWidget;
        auto *layout = new QVBoxLayout(page);
        layout->setContentsMargins(0, 4, 0, 4);
        layout->setSpacing(14);
        auto *heading = new QLabel(title);
        heading->setObjectName(QStringLiteral("title"));
        auto *body = new QLabel(description);
        body->setObjectName(QStringLiteral("subtitle"));
        body->setWordWrap(true);
        layout->addWidget(heading);
        layout->addWidget(body);
        layout->addWidget(makeCard(cardTitle, cardDescription));
        if (actionButton) {
            *actionButton = makeActionButton(actionText);
            layout->addWidget(*actionButton);
        }
        layout->addStretch(1);
        pages->addWidget(page);
    };

    QPushButton *settingsButton = nullptr;
    QPushButton *gamingButton = nullptr;
    QPushButton *hubButton = nullptr;
    QPushButton *discoverButton = nullptr;
    addPage(QStringLiteral("Welcome to Better On"),
            QStringLiteral("Your desktop is ready. This short guide introduces the essentials; you can skip setup or any optional step."),
            QStringLiteral("Gaming, without the fuss."),
            QStringLiteral("Better On combines KDE Plasma with Bazzite's gaming foundation, familiar desktop tools, and a curated app hub."),
            nullptr, QString());
    addPage(QStringLiteral("Get oriented"),
            QStringLiteral("Use the panel to open applications, switch windows, and reach quick settings. Your files and familiar KDE tools are ready from the application launcher."),
            QStringLiteral("Make the desktop yours"),
            QStringLiteral("Choose a look and adjust display, audio, network, Bluetooth, input devices, and power in KDE System Settings."),
            &settingsButton, QStringLiteral("Open System Settings"));
    addPage(QStringLiteral("Set up for gaming"),
            QStringLiteral("Steam, Proton, Vulkan, controllers, and graphics support come from the Bazzite foundation. Game compatibility varies by title."),
            QStringLiteral("Check your hardware"),
            QStringLiteral("Better On Gaming shows detected hardware, resource use, installed Steam games, and the approximate primary display refresh rate."),
            &gamingButton, QStringLiteral("Open Better On Gaming"));
    addPage(QStringLiteral("Choose your applications"),
            QStringLiteral("These optional tools can be opened at any time. You can install several catalog applications together in Better On Hub."),
            QStringLiteral("Browse or install apps"),
            QStringLiteral("Better On Hub installs selected applications from Flathub. Discover provides another way to browse software."),
            &hubButton, QStringLiteral("Open Better On Hub"));
    addPage(QStringLiteral("You're all set"),
            QStringLiteral("Your desktop is ready to use. You can reopen Better On Hub, Gaming, System Settings, and Discover from the application launcher whenever you need them."),
            QStringLiteral("Setup complete"),
            QStringLiteral("Finishing this guide only saves your Welcome choice. It does not change system settings or install applications."),
            nullptr, QString());

    auto *discoverRow = new QHBoxLayout;
    discoverButton = makeActionButton(QStringLiteral("Browse in Discover"));
    discoverRow->addWidget(discoverButton);
    discoverRow->addStretch(1);
    static_cast<QVBoxLayout *>(pages->widget(3)->layout())->insertLayout(4, discoverRow);

    root->addWidget(eyebrow);
    root->addWidget(stepLabel);
    root->addWidget(progress);
    root->addWidget(pages, 1);

    auto *footer = new QHBoxLayout;
    auto *skipSetupButton = makeActionButton(QStringLiteral("Skip setup"));
    skipSetupButton->setObjectName(QStringLiteral("quietButton"));
    auto *skipStepButton = makeActionButton(QStringLiteral("Skip this step"));
    skipStepButton->setObjectName(QStringLiteral("quietButton"));
    auto *backButton = makeActionButton(QStringLiteral("Back"));
    auto *nextButton = makeActionButton(QStringLiteral("Continue"));
    nextButton->setObjectName(QStringLiteral("primaryButton"));
    footer->addWidget(skipSetupButton);
    footer->addWidget(skipStepButton);
    footer->addStretch(1);
    footer->addWidget(backButton);
    footer->addWidget(nextButton);
    root->addLayout(footer);

    const auto updatePage = [pages, stepLabel, progress, backButton, skipStepButton, nextButton] {
        const int index = pages->currentIndex();
        stepLabel->setText(QStringLiteral("Step %1 of %2").arg(index + 1).arg(pages->count()));
        progress->setValue(index + 1);
        backButton->setEnabled(index > 0);
        skipStepButton->setVisible(index > 0 && index < pages->count() - 1);
        nextButton->setText(index == pages->count() - 1
                                ? QStringLiteral("Finish setup")
                                : QStringLiteral("Continue"));
    };
    QObject::connect(settingsButton, &QPushButton::clicked, &window, [&window] {
        launchApplication(&window, QStringLiteral("systemsettings"), QStringLiteral("System Settings"),
                          QStringList{QStringLiteral("kcm_lookandfeel")});
    });
    QObject::connect(gamingButton, &QPushButton::clicked, &window, [&window] {
        launchApplication(&window, QStringLiteral("better-on-gaming"), QStringLiteral("Better On Gaming"), {});
    });
    QObject::connect(hubButton, &QPushButton::clicked, &window, [&window] {
        launchApplication(&window, QStringLiteral("better-on-hub"), QStringLiteral("Better On Hub"), {});
    });
    QObject::connect(discoverButton, &QPushButton::clicked, &window, [&window] {
        launchApplication(&window, QStringLiteral("plasma-discover"), QStringLiteral("Discover"), {});
    });
    QObject::connect(backButton, &QPushButton::clicked, &window, [pages, updatePage] {
        if (pages->currentIndex() > 0) pages->setCurrentIndex(pages->currentIndex() - 1);
        updatePage();
    });
    QObject::connect(nextButton, &QPushButton::clicked, &window, [pages, updatePage, &window] {
        if (pages->currentIndex() == pages->count() - 1) {
            finishSetup(&window);
            return;
        }
        pages->setCurrentIndex(pages->currentIndex() + 1);
        updatePage();
    });
    QObject::connect(skipStepButton, &QPushButton::clicked, &window, [pages, updatePage] {
        if (pages->currentIndex() < pages->count() - 1) pages->setCurrentIndex(pages->currentIndex() + 1);
        updatePage();
    });
    QObject::connect(skipSetupButton, &QPushButton::clicked, &window, [&window] {
        finishSetup(&window);
    });
    updatePage();

    window.show();
    return application.exec();
}
