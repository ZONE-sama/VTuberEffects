#include "update-checker.h"

#include <obs-module.h>
#include <plugin-support.h>

#include <QApplication>
#include <QCheckBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QString>
#include <QTimer>
#include <QUrl>

namespace {

constexpr qint64 CHECK_INTERVAL_SECONDS = 24 * 60 * 60;
constexpr int STARTUP_DELAY_MILLISECONDS = 8000;
constexpr int REQUEST_TIMEOUT_MILLISECONDS = 15000;

struct SemanticVersion {
	int major = 0;
	int minor = 0;
	int patch = 0;
	int stage = 0;
	int stage_number = 0;
	bool valid = false;
};

struct ReleaseInformation {
	SemanticVersion version;
	QString tag;
	QString name;
	QString notes;
	QUrl page_url;
};

SemanticVersion parse_version(const QString &text)
{
	static const QRegularExpression expression(
		QStringLiteral("^v?(\\d+)\\.(\\d+)\\.(\\d+)"
			       "(?:-(beta|rc)(\\d+))?$"),
		QRegularExpression::CaseInsensitiveOption);
	const QRegularExpressionMatch match = expression.match(text.trimmed());
	SemanticVersion version;
	if (!match.hasMatch())
		return version;

	version.major = match.captured(1).toInt();
	version.minor = match.captured(2).toInt();
	version.patch = match.captured(3).toInt();
	const QString stage = match.captured(4).toLower();
	version.stage = stage == QStringLiteral("beta")
				? 0
				: stage == QStringLiteral("rc") ? 1 : 2;
	version.stage_number = match.captured(5).toInt();
	version.valid = true;
	return version;
}

int compare_versions(const SemanticVersion &left,
		     const SemanticVersion &right)
{
	const int left_values[] = {left.major, left.minor, left.patch,
				   left.stage, left.stage_number};
	const int right_values[] = {right.major, right.minor, right.patch,
				    right.stage, right.stage_number};
	for (size_t index = 0;
	     index < sizeof(left_values) / sizeof(left_values[0]); index++) {
		if (left_values[index] < right_values[index])
			return -1;
		if (left_values[index] > right_values[index])
			return 1;
	}
	return 0;
}

QString settings_path()
{
	char *raw_path = obs_module_config_path("update-check.ini");
	if (!raw_path)
		return QString();
	const QString path = QString::fromUtf8(raw_path);
	bfree(raw_path);
	QDir().mkpath(QFileInfo(path).absolutePath());
	return path;
}

QSettings make_settings()
{
	return QSettings(settings_path(), QSettings::IniFormat);
}

QWidget *dialog_parent()
{
	return QApplication::activeWindow();
}

class UpdateChecker : public QObject {
public:
	explicit UpdateChecker(const QString &version) : current_version(version) {}

	void set_current_version(const QString &version)
	{
		if (!version.isEmpty())
			current_version = version;
	}

	void schedule_automatic_check()
	{
		QTimer::singleShot(STARTUP_DELAY_MILLISECONDS, this, [this]() {
			QSettings settings = make_settings();
			if (!settings.value(QStringLiteral("automaticChecks"), true)
				     .toBool())
				return;

			const qint64 last_check = settings
						  .value(QStringLiteral("lastCheckUtc"), 0)
						  .toLongLong();
			const qint64 now = QDateTime::currentSecsSinceEpoch();
			if (last_check > 0 &&
			    now - last_check < CHECK_INTERVAL_SECONDS)
				return;
			request(false);
		});
	}

	void request(bool manual)
	{
		if (request_in_progress) {
			if (manual)
				QMessageBox::information(
					dialog_parent(),
					QStringLiteral("VTuber Effects Update"),
					QStringLiteral("An update check is already in progress."));
			return;
		}

		request_in_progress = true;
		QNetworkRequest request(QUrl(QStringLiteral(
			"https://api.github.com/repos/ZONE-sama/VTuberEffects/releases?per_page=30")));
		request.setRawHeader("Accept", "application/vnd.github+json");
		request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
		request.setRawHeader("User-Agent", "VTuber-Effects-OBS-Plugin");
		request.setAttribute(
			QNetworkRequest::RedirectPolicyAttribute,
			QNetworkRequest::NoLessSafeRedirectPolicy);

		QNetworkReply *reply = network.get(request);
		QTimer *timeout = new QTimer(reply);
		timeout->setSingleShot(true);
		connect(timeout, &QTimer::timeout, reply, &QNetworkReply::abort);
		timeout->start(REQUEST_TIMEOUT_MILLISECONDS);
		connect(reply, &QNetworkReply::finished, this,
			[this, reply, manual]() { finish(reply, manual); });
	}

private:
	void finish(QNetworkReply *reply, bool manual)
	{
		request_in_progress = false;
		const QNetworkReply::NetworkError error = reply->error();
		const QByteArray response = reply->readAll();
		const QString error_text = reply->errorString();
		reply->deleteLater();

		if (error != QNetworkReply::NoError) {
			obs_log(LOG_WARNING, "update check failed: %s",
				error_text.toUtf8().constData());
			if (manual)
				QMessageBox::warning(
					dialog_parent(),
					QStringLiteral("VTuber Effects Update"),
					QStringLiteral("The update check could not be completed.\n\n%1")
						.arg(error_text));
			return;
		}

		QJsonParseError parse_error;
		const QJsonDocument document =
			QJsonDocument::fromJson(response, &parse_error);
		if (parse_error.error != QJsonParseError::NoError ||
		    !document.isArray()) {
			obs_log(LOG_WARNING, "update response was not valid JSON");
			if (manual)
				QMessageBox::warning(
					dialog_parent(),
					QStringLiteral("VTuber Effects Update"),
					QStringLiteral("GitHub returned an invalid update response."));
			return;
		}

		QSettings settings = make_settings();
		settings.setValue(QStringLiteral("lastCheckUtc"),
				  QDateTime::currentSecsSinceEpoch());

		const SemanticVersion installed = parse_version(current_version);
		if (!installed.valid) {
			obs_log(LOG_WARNING, "cannot compare plugin version %s",
				current_version.toUtf8().constData());
			return;
		}

		ReleaseInformation newest;
		const QJsonArray releases = document.array();
		for (const QJsonValue &value : releases) {
			const QJsonObject object = value.toObject();
			if (object.value(QStringLiteral("draft")).toBool())
				continue;
			if (installed.stage == 2 &&
			    object.value(QStringLiteral("prerelease")).toBool())
				continue;

			ReleaseInformation candidate;
			candidate.tag =
				object.value(QStringLiteral("tag_name")).toString();
			candidate.version = parse_version(candidate.tag);
			if (!candidate.version.valid ||
			    compare_versions(candidate.version, installed) <= 0)
				continue;
			if (newest.version.valid &&
			    compare_versions(candidate.version, newest.version) <= 0)
				continue;

			candidate.name =
				object.value(QStringLiteral("name")).toString();
			candidate.notes =
				object.value(QStringLiteral("body")).toString();
			candidate.page_url = QUrl(
				object.value(QStringLiteral("html_url")).toString());
			newest = candidate;
		}

		if (!newest.version.valid) {
			if (manual)
				QMessageBox::information(
					dialog_parent(),
					QStringLiteral("VTuber Effects Update"),
					QStringLiteral("VTuber Effects %1 is up to date.")
						.arg(current_version));
			return;
		}

		if (!manual &&
		    settings.value(QStringLiteral("skippedVersion")).toString() ==
			    newest.tag)
			return;

		show_update(newest, settings);
	}

	void show_update(const ReleaseInformation &release, QSettings &settings)
	{
		QMessageBox box(dialog_parent());
		box.setIcon(QMessageBox::Information);
		box.setWindowTitle(QStringLiteral("VTuber Effects Update"));
		box.setText(QStringLiteral("A new version of VTuber Effects is available."));
		QString information =
			QStringLiteral("Installed: %1\nAvailable: %2")
				.arg(current_version, release.tag);
		if (!release.name.isEmpty() && release.name != release.tag)
			information += QStringLiteral("\n\n%1").arg(release.name);
		box.setInformativeText(information);

		QString notes = release.notes.trimmed();
		if (notes.length() > 2000)
			notes = notes.left(2000) + QStringLiteral("\n\n…");
		if (!notes.isEmpty())
			box.setDetailedText(notes);

		QPushButton *download = box.addButton(
			QStringLiteral("Download Update"), QMessageBox::AcceptRole);
		QPushButton *skip = box.addButton(
			QStringLiteral("Skip This Version"),
			QMessageBox::DestructiveRole);
		box.addButton(QStringLiteral("Remind Me Later"),
			      QMessageBox::RejectRole);
		QCheckBox *disable = new QCheckBox(
			QStringLiteral("Do not check for updates automatically"));
		disable->setChecked(
			!settings.value(QStringLiteral("automaticChecks"), true)
				 .toBool());
		box.setCheckBox(disable);
		box.exec();

		settings.setValue(QStringLiteral("automaticChecks"),
				  !disable->isChecked());
		if (box.clickedButton() == download)
			QDesktopServices::openUrl(release.page_url);
		else if (box.clickedButton() == skip)
			settings.setValue(QStringLiteral("skippedVersion"),
					  release.tag);
	}

	QString current_version;
	QNetworkAccessManager network;
	bool request_in_progress = false;
};

QPointer<UpdateChecker> checker;

UpdateChecker *ensure_checker(const char *current_version)
{
	const QString version =
		QString::fromUtf8(current_version ? current_version : "");
	if (!checker)
		checker = new UpdateChecker(version);
	else
		checker->set_current_version(version);
	return checker;
}

} // namespace

extern "C" void
vtuber_effects_start_update_checker(const char *current_version)
{
	UpdateChecker *instance = ensure_checker(current_version);
	instance->schedule_automatic_check();
}

extern "C" void vtuber_effects_stop_update_checker(void)
{
	if (checker) {
		delete checker;
		checker = nullptr;
	}
}

extern "C" void
vtuber_effects_check_for_updates(const char *current_version)
{
	ensure_checker(current_version)->request(true);
}
