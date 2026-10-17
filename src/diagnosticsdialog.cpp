#include "diagnosticsdialog.h"
#include "geoipservice.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QDesktopServices>
#include <QDir>
#include <QFormLayout>
#include <QGuiApplication>
#include <QJsonValue>
#include <QLabel>
#include <QLibraryInfo>
#include <QOperatingSystemVersion>
#include <QPushButton>
#include <QSettings>
#include <QSysInfo>
#include <QTextEdit>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <QFontDatabase>

namespace {
QString valueOrUnknown(const QJsonObject &o, const QString &key)
{
    const QJsonValue value = o.value(key);
    if (value.isString() && !value.toString().trimmed().isEmpty()) return value.toString();
    if (value.isDouble()) return QString::number(value.toInt());
    if (value.isBool()) return value.toBool() ? QStringLiteral("Yes") : QStringLiteral("No");
    return QStringLiteral("Unknown");
}

QString yesNo(bool value) { return value ? QStringLiteral("Yes") : QStringLiteral("No"); }
QString line(const QString &name, const QString &value)
{
    return name.leftJustified(28) + value + QLatin1Char('\n');
}
}

DiagnosticsDialog::DiagnosticsDialog(const QJsonObject &sessionSettings,
                                     const QString &serverName,
                                     const QString &rpcUrl,
                                     const QString &backendName,
                                     const QString &protocolDescription,
                                     GeoIpService *geoIpService,
                                     int refreshIntervalMs,
                                     QWidget *parent)
    : QDialog(parent), sessionSettings(sessionSettings), serverName(serverName), rpcUrl(rpcUrl),
      backendName(backendName), protocolDescription(protocolDescription),
      geoIpService(geoIpService), refreshIntervalMs(refreshIntervalMs)
{
    setWindowTitle(tr("Diagnostics"));
    resize(720, 620);

    auto *layout = new QVBoxLayout(this);
    auto *intro = new QLabel(tr("Diagnostic information collected from Planetary and the active torrent session. Passwords and authentication data are not included."), this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    reportEdit = new QTextEdit(this);
    reportEdit->setReadOnly(true);
    reportEdit->setLineWrapMode(QTextEdit::NoWrap);
    reportEdit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    reportEdit->setPlainText(buildReport());
    layout->addWidget(reportEdit, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    auto *copyButton = buttons->addButton(tr("Copy to Clipboard"), QDialogButtonBox::ActionRole);
    auto *supportButton = buttons->addButton(tr("Contact Support..."),
                                             QDialogButtonBox::ActionRole);
    connect(copyButton, &QPushButton::clicked, this, [this]() {
        QGuiApplication::clipboard()->setText(reportEdit->toPlainText());
    });
    connect(supportButton, &QPushButton::clicked, this, [this, intro]() {
        // QUrlQuery performs the escaping required to preserve the report's
        // line breaks and punctuation in a mailto query string.
        QUrl supportUrl(QStringLiteral("mailto:planetary@mvgrafx.net"));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("subject"),
                           tr("Planetary %1 support request")
                               .arg(QCoreApplication::applicationVersion()));
        query.addQueryItem(QStringLiteral("body"),
                           tr("Please describe the issue above the diagnostic report below.\n\n%1")
                               .arg(reportEdit->toPlainText()));
        supportUrl.setQuery(query);

        if (!QDesktopServices::openUrl(supportUrl))
            intro->setText(tr("Could not open the default email application. "
                              "Contact planetary@mvgrafx.net directly."));
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

QString DiagnosticsDialog::buildReport() const
{
    QString out;
    out += QStringLiteral("PLANETARY\n---------\n");
    out += line(QStringLiteral("Application version"), QCoreApplication::applicationVersion());
    out += line(QStringLiteral("Build date/time"), QStringLiteral(__DATE__ " " __TIME__));
    out += line(QStringLiteral("Qt compile version"), QStringLiteral(QT_VERSION_STR));
    out += line(QStringLiteral("Qt runtime version"), QString::fromLatin1(qVersion()));
    out += line(QStringLiteral("Platform"), QSysInfo::prettyProductName());
    out += line(QStringLiteral("Kernel"), QSysInfo::kernelType() + QLatin1Char(' ') + QSysInfo::kernelVersion());
    out += line(QStringLiteral("CPU architecture"), QSysInfo::currentCpuArchitecture());
    out += line(QStringLiteral("Application directory"), QCoreApplication::applicationDirPath());
    QSettings settings;
    out += line(QStringLiteral("Settings file"), settings.fileName());
    out += line(QStringLiteral("Refresh interval"), QString::number(refreshIntervalMs) + QStringLiteral(" ms"));

    out += QStringLiteral("\nACTIVE SERVER / RPC\n-------------------\n");
    out += line(QStringLiteral("Server name"), serverName.isEmpty() ? QStringLiteral("Unknown") : serverName);
    out += line(QStringLiteral("Backend"), backendName.isEmpty() ? QStringLiteral("Unknown") : backendName);
    out += line(QStringLiteral("Protocol"), protocolDescription.isEmpty() ? QStringLiteral("Unknown") : protocolDescription);
    out += line(QStringLiteral("RPC endpoint"), rpcUrl.isEmpty() ? QStringLiteral("Unknown") : rpcUrl);
    out += line(QStringLiteral("Session data available"), yesNo(!sessionSettings.isEmpty()));
    out += line(QStringLiteral("Transmission version"), valueOrUnknown(sessionSettings, QStringLiteral("version")));
    out += line(QStringLiteral("RPC version"), valueOrUnknown(sessionSettings, QStringLiteral("rpc-version")));
    out += line(QStringLiteral("Minimum RPC version"), valueOrUnknown(sessionSettings, QStringLiteral("rpc-version-minimum")));
    out += line(QStringLiteral("RPC semantic version"), valueOrUnknown(sessionSettings, QStringLiteral("rpc-version-semver")));
    out += line(QStringLiteral("Download directory"), valueOrUnknown(sessionSettings, QStringLiteral("download-dir")));
    out += line(QStringLiteral("Peer port"), valueOrUnknown(sessionSettings, QStringLiteral("peer-port")));
    out += line(QStringLiteral("Port forwarding enabled"), valueOrUnknown(sessionSettings, QStringLiteral("port-forwarding-enabled")));
    out += line(QStringLiteral("DHT enabled"), valueOrUnknown(sessionSettings, QStringLiteral("dht-enabled")));
    out += line(QStringLiteral("PEX enabled"), valueOrUnknown(sessionSettings, QStringLiteral("pex-enabled")));
    out += line(QStringLiteral("LPD enabled"), valueOrUnknown(sessionSettings, QStringLiteral("lpd-enabled")));
    out += line(QStringLiteral("Alternative speed enabled"), valueOrUnknown(sessionSettings, QStringLiteral("alt-speed-enabled")));

    out += QStringLiteral("\nGEOIP\n-----\n");
    if (!geoIpService) {
        out += line(QStringLiteral("Status"), QStringLiteral("Not initialized"));
    } else {
        const GeoIpDatabaseInfo info = geoIpService->databaseInfo();
        out += line(QStringLiteral("libmaxminddb support"), yesNo(info.maxMindDbSupport));
        out += line(QStringLiteral("Database loaded"), yesNo(info.loaded));
        out += line(QStringLiteral("Fallback lookup active"), yesNo(info.fallbackLookupActive));
        out += line(QStringLiteral("Database path"), info.path.isEmpty() ? QStringLiteral("Unknown") : info.path);
        out += line(QStringLiteral("Load message"), info.errorMessage.isEmpty() ? QStringLiteral("None") : info.errorMessage);
        out += line(QStringLiteral("Database type"), info.databaseType.isEmpty() ? QStringLiteral("Unknown") : info.databaseType);
        out += line(QStringLiteral("Description"), info.description.isEmpty() ? QStringLiteral("Unknown") : info.description);
        out += line(QStringLiteral("Database build date"), info.buildDateUtc.isEmpty() ? QStringLiteral("Unknown") : info.buildDateUtc);
        out += line(QStringLiteral("IP version"), info.ipVersion > 0 ? QString::number(info.ipVersion) : QStringLiteral("Unknown"));
        out += line(QStringLiteral("Node count"), info.nodeCount > 0 ? QString::number(info.nodeCount) : QStringLiteral("Unknown"));
        out += line(QStringLiteral("Record size"), info.recordSize > 0 ? QString::number(info.recordSize) : QStringLiteral("Unknown"));
        out += line(QStringLiteral("Binary format"), info.binaryFormatMajor > 0 ? QStringLiteral("%1.%2").arg(info.binaryFormatMajor).arg(info.binaryFormatMinor) : QStringLiteral("Unknown"));
        out += line(QStringLiteral("Lookup cache entries"), QString::number(info.cacheEntries));
    }
    return out;
}
