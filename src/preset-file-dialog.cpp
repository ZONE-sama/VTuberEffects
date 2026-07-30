#include "preset-file-dialog.h"

#include <QApplication>
#include <QByteArray>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLayout>
#include <QSizePolicy>
#include <QTimer>
#include <QString>
#include <QUrl>
#include <QWidget>

#include <cstdio>

static bool copy_path(const QString &path, char *buffer, size_t buffer_size)
{
	if (path.isEmpty() || !buffer || buffer_size == 0)
		return false;

	const QByteArray utf8 = path.toUtf8();
	const int result =
		std::snprintf(buffer, buffer_size, "%s", utf8.constData());
	return result >= 0 && static_cast<size_t>(result) < buffer_size;
}

extern "C" bool vtuber_effects_choose_preset_save(char *buffer,
						    size_t buffer_size)
{
	QWidget *parent = QApplication::activeWindow();
	QString path = QFileDialog::getSaveFileName(
		parent, QStringLiteral("Export VTuber Effects Settings"),
		QDir::homePath() + QStringLiteral("/VTuberEffectsPreset.json"),
		QStringLiteral("VTuber Effects preset (*.json);;JSON files (*.json)"));
	if (path.isEmpty())
		return false;

	if (QFileInfo(path).suffix().isEmpty())
		path += QStringLiteral(".json");

	return copy_path(path, buffer, buffer_size);
}

extern "C" bool vtuber_effects_choose_preset_open(char *buffer,
						    size_t buffer_size)
{
	QWidget *parent = QApplication::activeWindow();
	const QString path = QFileDialog::getOpenFileName(
		parent, QStringLiteral("Import VTuber Effects Settings"),
		QDir::homePath(),
		QStringLiteral("VTuber Effects preset (*.json);;JSON files (*.json)"));
	return copy_path(path, buffer, buffer_size);
}

extern "C" bool vtuber_effects_make_header_html(const char *image_path,
						 char *buffer,
						 size_t buffer_size)
{
	if (!image_path)
		return false;

	const QString url = QUrl::fromLocalFile(QString::fromUtf8(image_path))
				    .toString(QUrl::FullyEncoded);
	const QString html =
		QStringLiteral("<p align=\"left\" style=\"margin:0\"><img "
			       "src=\"%1\" width=\"120\" height=\"40\">"
			       "<span style=\"font-size:1px;color:transparent\">"
			       "VTFX_HEADER_MARKER</span></p>")
			.arg(url);
	return copy_path(html, buffer, buffer_size);
}

static void span_property_row(QLabel *label)
{
	if (!label)
		return;

	QWidget *row_widget = label;
	QWidget *parent = label->parentWidget();
	while (parent) {
		QFormLayout *form =
			qobject_cast<QFormLayout *>(parent->layout());
		if (form) {
			int row = -1;
			QFormLayout::ItemRole role =
				QFormLayout::FieldRole;
			form->getWidgetPosition(row_widget, &row, &role);
			if (row >= 0) {
				form->removeWidget(row_widget);
				form->setWidget(row,
						QFormLayout::SpanningRole,
						row_widget);
				row_widget->setSizePolicy(
					QSizePolicy::Expanding,
					QSizePolicy::Preferred);
				return;
			}
		}

		QGridLayout *grid = qobject_cast<QGridLayout *>(parent->layout());
		if (grid) {
			const int index = grid->indexOf(row_widget);
			if (index >= 0) {
				int row = 0;
				int column = 0;
				int row_span = 1;
				int column_span = 1;
				grid->getItemPosition(index, &row, &column,
						      &row_span,
						      &column_span);
				grid->removeWidget(row_widget);
				grid->addWidget(row_widget, row, 0, row_span,
						qMax(2, grid->columnCount()),
						Qt::AlignLeft);
				row_widget->setSizePolicy(
					QSizePolicy::Expanding,
					QSizePolicy::Preferred);
				return;
			}
		}

		row_widget = parent;
		parent = parent->parentWidget();
	}
}

static void align_header_rows_in_window(QWidget *window,
					const QString &version_text)
{
	if (!window)
		return;

	const QList<QLabel *> labels = window->findChildren<QLabel *>();
	for (QLabel *label : labels) {
		const QString text = label->text();
		if (text.contains(
			    QStringLiteral("VTFX_HEADER_MARKER")) ||
		    text == version_text)
			span_property_row(label);
	}
}

static void align_header_rows(const QString &version_text)
{
	const QWidgetList windows = QApplication::topLevelWidgets();
	for (QWidget *window : windows)
		align_header_rows_in_window(window, version_text);
}

extern "C" void
vtuber_effects_align_header_rows(const char *version_text)
{
	const QString version =
		QString::fromUtf8(version_text ? version_text : "");
	for (const int delay :
	     {0, 50, 150, 300, 600, 1000, 1600, 2400}) {
		QTimer::singleShot(delay, [version]() {
			align_header_rows(version);
		});
	}
}
