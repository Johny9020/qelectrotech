/*
	Copyright 2006-2026 The QElectroTech Team
	This file is part of QElectroTech.

	QElectroTech is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	QElectroTech is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with QElectroTech.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "qetapptest.h"

#include "diagram.h"
#include "projectview.h"
#include "qetproject.h"
#include "titleblock/qettemplateeditor.h"
#include "titleblock/templatescollection.h"
#include "titleblockcell.h"
#include "titleblocktemplate.h"

#include <QBuffer>
#include <QDomDocument>
#include <QImage>
#include <QPainter>

/**
	Logos of a title block template: a width and a height of their own
	(either one keeping the proportions of the image, none filling the
	cell as logos always did), placed by the cell's alignment, kept in
	the file only when set, and drawn inside their cell.

	And a template changed in a collection reaching the open projects
	which keep their own copy of it: only a copy which really differs
	is offered, and replacing it redraws the folios and marks the
	project as changed.
*/
class tst_titleblocklogo : public QObject
{
	Q_OBJECT

	QTemporaryDir m_dir;

	static TitleBlockCell logoCell(int width, int height,
				       int alignment = Qt::AlignHCenter | Qt::AlignVCenter)
	{
		TitleBlockCell cell;
		cell.cell_type = TitleBlockCell::LogoCell;
		cell.logo_reference = "logo";
		cell.logo_width = width;
		cell.logo_height = height;
		cell.alignment = alignment;
		return cell;
	}

		///A template of one cell, 100 x 40, holding a red image 40 x 20
	static QString redTemplate(const QString &logo_attributes)
	{
		QImage red(40, 20, QImage::Format_ARGB32);
		red.fill(Qt::red);
		QByteArray png;
		QBuffer buffer(&png);
		buffer.open(QIODevice::WriteOnly);
		red.save(&buffer, "PNG");
		return QString("<titleblocktemplate name=\"t\">"
			       "<logos><logo name=\"red.png\" type=\"png\" storage=\"base64\">%1</logo></logos>"
			       "<grid rows=\"40\" cols=\"100\">"
			       "<logo name=\"\" row=\"0\" col=\"0\" resource=\"red.png\" %2/>"
			       "</grid></titleblocktemplate>")
				.arg(QString::fromLatin1(png.toBase64()), logo_attributes);
	}

		///Where the red of the logo lands when that template is drawn
	static QRect redArea(const QString &logo_attributes)
	{
		QDomDocument doc;
		doc.setContent(redTemplate(logo_attributes));
		TitleBlockTemplate tbt;
		if (!tbt.loadFromXmlElement(doc.documentElement())) return QRect();

		QImage image(100, 40, QImage::Format_ARGB32);
		image.fill(Qt::white);
		QPainter painter(&image);
		tbt.render(painter, DiagramContext(), 100);
		painter.end();

		int left = 1000, top = 1000, right = -1, bottom = -1;
		for (int y = 0 ; y < image.height() ; ++y)
			for (int x = 0 ; x < image.width() ; ++x) {
				const QColor c = image.pixelColor(x, y);
				if (c.red() > 200 && c.green() < 60 && c.blue() < 60) {
					left = qMin(left, x); right = qMax(right, x);
					top = qMin(top, y); bottom = qMax(bottom, y);
				}
			}
		if (right < 0) return QRect();
		return QRect(QPoint(left, top), QPoint(right, bottom));
	}

	private slots:
		void noSizeFillsTheCell()
		{
			const QRectF cell(10, 20, 100, 40);
			QCOMPARE(TitleBlockTemplate::logoRect(logoCell(0, 0), cell, QSizeF(40, 20)), cell);
		}

		void oneSideKeepsTheProportions()
		{
			const QRectF cell(0, 0, 100, 40);
			QCOMPARE(TitleBlockTemplate::logoRect(logoCell(20, 0), cell, QSizeF(40, 20)).size(),
				 QSizeF(20, 10));
			QCOMPARE(TitleBlockTemplate::logoRect(logoCell(0, 30), cell, QSizeF(40, 20)).size(),
				 QSizeF(60, 30));
			QCOMPARE(TitleBlockTemplate::logoRect(logoCell(25, 15), cell, QSizeF(40, 20)).size(),
				 QSizeF(25, 15));
		}

		void placedByTheAlignment_data()
		{
			QTest::addColumn<int>("alignment");
			QTest::addColumn<QPointF>("top_left");
			QTest::newRow("centered") << int(Qt::AlignHCenter | Qt::AlignVCenter) << QPointF(40, 15);
			QTest::newRow("left top") << int(Qt::AlignLeft | Qt::AlignTop) << QPointF(0, 0);
			QTest::newRow("right bottom") << int(Qt::AlignRight | Qt::AlignBottom) << QPointF(80, 30);
		}
		void placedByTheAlignment()
		{
			QFETCH(int, alignment);
			QFETCH(QPointF, top_left);
			const QRectF cell(0, 0, 100, 40);
			QCOMPARE(TitleBlockTemplate::logoRect(logoCell(20, 10, alignment), cell, QSizeF(40, 20)).topLeft(),
				 top_left);
		}

		void savedOnlyWhenSet()
		{
			QDomDocument doc;
			TitleBlockCell plain = logoCell(0, 0);
			QDomElement e = doc.createElement("cell");
			plain.saveContentToXml(e);
			QVERIFY(!e.hasAttribute("width"));
			QVERIFY(!e.hasAttribute("height"));
			QVERIFY(!e.hasAttribute("align"));

			TitleBlockCell sized = logoCell(150, 0, Qt::AlignLeft | Qt::AlignTop);
			QDomElement f = doc.createElement("cell");
			sized.saveContentToXml(f);
			TitleBlockCell read;
			read.loadContentFromXml(f);
			QCOMPARE(read.type(), TitleBlockCell::LogoCell);
			QCOMPARE(read.logo_width, 150);
			QCOMPARE(read.logo_height, 0);
			QCOMPARE(read.horizontalAlign(), int(Qt::AlignLeft));
			QCOMPARE(read.verticalAlign(), int(Qt::AlignTop));

				// A logo of an older file stands centered
			QDomElement old = doc.createElement("logo");
			old.setAttribute("resource", "logo");
			TitleBlockCell centered;
			centered.alignment = Qt::AlignLeft;
			centered.loadContentFromXml(old);
			QCOMPARE(centered.horizontalAlign(), int(Qt::AlignHCenter));
			QCOMPARE(centered.logo_width, 0);
		}

		void attributesGoThroughTheEditor()
		{
				// The editor changes a cell through these, with undo
			TitleBlockCell cell = logoCell(0, 0);
			cell.setAttribute("logo_width", 80);
			cell.setAttribute("logo_height", 30);
			QCOMPARE(cell.attribute("logo_width").toInt(), 80);
			QCOMPARE(cell.attribute("logo_height").toInt(), 30);
			cell.setAttribute("logo_width", -5);
			QCOMPARE(cell.logo_width, 0);
			TitleBlockCell copy;
			copy.loadContentFromCell(cell);
			QCOMPARE(copy.logo_height, 30);
		}

		void drawnAtItsSize()
		{
				// Nothing set: stretched over the whole cell, as before
			const QRect filled = redArea("");
			QVERIFY(filled.width() >= 96 && filled.height() >= 36);
				// 20 px wide: 20 x 10, in the middle of the cell
			const QRect sized = redArea("width=\"20\"");
			QVERIFY2(qAbs(sized.width() - 20) <= 1 && qAbs(sized.height() - 10) <= 1,
				 qPrintable(QString("%1x%2").arg(sized.width()).arg(sized.height())));
			QVERIFY(qAbs(sized.center().x() - 50) <= 1);
			QVERIFY(qAbs(sized.center().y() - 20) <= 1);
				// Larger than the cell: cut off at its edges
			const QRect large = redArea("height=\"200\"");
			QVERIFY(QRect(0, 0, 100, 40).contains(large));
		}

		void projectCopiesFollowTheTemplate()
		{
			QETDiagramEditor *editor = QetAppTest::editor();
			QVERIFY(editor);
			const QString copy = m_dir.filePath("iso_sfc_example.qet");
			QVERIFY(QFile::copy(QString(QET_EXAMPLES_DIR) + "/iso_sfc_example.qet", copy));
			QVERIFY(editor->openAndAddProject(copy, false));
			QETProject *project = editor->currentProjectView()->project();
			const QString name = "ISO7200_A4_V1";

			TitleBlockTemplatesProjectCollection *collection =
					project->embeddedTitleBlockTemplatesCollection();
			QVERIFY(collection->templates().contains(name));
			Diagram *diagram = nullptr;
			for (Diagram *d : project->diagrams())
				if (d->border_and_titleblock.titleBlockTemplateName() == name) diagram = d;
			QVERIFY(diagram);
			const qreal height = diagram->border_and_titleblock.titleBlockRect().height();

			QDomDocument doc;
			QDomElement same = doc.importNode(collection->getTemplateXmlDescription(name), true).toElement();
				// The very copy the project holds is nothing to offer
			QVERIFY(!QETTitleBlockTemplateEditor::projectsWithOtherCopy(name, same).contains(project));

				// Rows twice as tall: that project is offered, and updating
				// it makes its folio's title block twice as tall
			QDomElement taller = same.cloneNode(true).toElement();
			QDomElement grid = taller.firstChildElement("grid");
			QStringList rows = grid.attribute("rows").split(';', Qt::SkipEmptyParts);
			for (QString &row : rows) row = QString::number(row.toInt() * 2);
			grid.setAttribute("rows", rows.join(';'));
			const QList<QETProject *> offered = QETTitleBlockTemplateEditor::projectsWithOtherCopy(name, taller);
			QVERIFY(offered.contains(project));

			project->setModified(false);
			QETTitleBlockTemplateEditor::updateProjectCopies({project}, name, taller);
			QCOMPARE(diagram->border_and_titleblock.titleBlockRect().height(), height * 2);
			QVERIFY(project->projectWasModified());
			QVERIFY(!QETTitleBlockTemplateEditor::projectsWithOtherCopy(name, taller).contains(project));
		}
};

QET_APP_TEST_MAIN(tst_titleblocklogo)

#include "tst_titleblocklogo.moc"
