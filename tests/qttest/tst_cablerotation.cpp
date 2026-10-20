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

#include "cable/cable.h"
#include "cable/cablepart.h"

#include <QDomDocument>
#include <QImage>
#include <QPainter>
#include <QStyleOptionGraphicsItem>

/**
	Turning the text of a cable and the colours of its cores: kept in the
	file and read back, absent from the file while not turned, ignored
	when not finite, and drawn turned -- always inside the rectangle the
	item says it covers, so nothing is cut off or left behind on screen.

	And the cross-section written under every colour: "2,5" gets its
	unit, it is kept in the file, the tick hides it, and the two lines
	are drawn inside what the item covers, turned or not.
*/
class tst_cablerotation : public QObject
{
	Q_OBJECT

	Cable m_cable;
	CablePart *m_part = nullptr;

		///Where the drawing of the part puts ink, in scene coordinates
	QRectF inkRect()
	{
		const QRectF area(-400, -400, 800, 800);
		QImage image(area.size().toSize(), QImage::Format_ARGB32);
		image.fill(Qt::white);
		QPainter painter(&image);
		painter.translate(-area.topLeft());
		QStyleOptionGraphicsItem option;
		m_part->paint(&painter, &option, nullptr);
		painter.end();

		int left = image.width(), top = image.height(), right = -1, bottom = -1;
		for (int y = 0 ; y < image.height() ; ++y)
			for (int x = 0 ; x < image.width() ; ++x)
				if (qGray(image.pixel(x, y)) < 200) {
					left = qMin(left, x); right = qMax(right, x);
					top = qMin(top, y); bottom = qMax(bottom, y);
				}
		if (right < 0) return QRectF();
		return QRectF(QPointF(left, top), QPointF(right + 1, bottom + 1))
				.translated(area.topLeft());
	}

	void turn(qreal text, qreal cores)
	{
		CableProperties p = m_cable.properties();
		p.text_rotation = text;
		p.core_rotation = cores;
		m_cable.setProperties(p);
	}

	private slots:
		void initTestCase()
		{
			m_cable.setDesignation("W1");
			m_cable.setType("CYKY 3x6");
			m_cable.setLength("12 m");
			m_cable.setCoreCount(3);
			m_cable.setCoreColors({"BK", "GNYE", "BU"});

			CablePartData data;
			data.uuid = QUuid::createUuid();
			data.p1 = QPointF(0, 100);
			data.p2 = QPointF(200, 100);
			m_cable.addPart(data);
			for (int i = 0 ; i < 3 ; ++i) {
				CableCore core;
				core.core = i;
				core.part = data.uuid;
				core.position = QPointF(50 + 50 * i, 100);
				m_cable.setCore(core);
			}
			m_part = new CablePart(data);
			m_part->setCable(&m_cable);
		}

		void cleanupTestCase()
		{
			delete m_part;
		}

		void savedAndReadBack()
		{
			turn(45, -30);
			QDomDocument doc;
			const QDomElement e = m_cable.toXml(doc);
			Cable read;
			read.fromXml(e);
			QCOMPARE(read.properties().text_rotation, 45.0);
			QCOMPARE(read.properties().core_rotation, -30.0);
			turn(0, 0);
		}

		void notWrittenWhileNotTurned()
		{
			turn(0, 0);
			QDomDocument doc;
			const QDomElement e = m_cable.toXml(doc);
			const QDomElement texts = e.firstChildElement("texts");
			QVERIFY(!texts.hasAttribute("text_rotation"));
			QVERIFY(!texts.hasAttribute("core_rotation"));
		}

		void notFiniteIsNoTurn()
		{
			turn(45, 45);
			QDomDocument doc;
			QDomElement e = m_cable.toXml(doc);
			QDomElement texts = e.firstChildElement("texts");
			texts.setAttribute("text_rotation", "nan");
			texts.setAttribute("core_rotation", "inf");
			Cable read;
			read.fromXml(e);
			QCOMPARE(read.properties().text_rotation, 0.0);
			QCOMPARE(read.properties().core_rotation, 0.0);
			turn(0, 0);
		}

		void undoComparesTheTurn()
		{
			CableProperties a = m_cable.properties();
			CableProperties b = a;
			b.core_rotation = 90;
			QVERIFY(a != b);
			b.core_rotation = a.core_rotation;
			b.text_rotation = 15;
			QVERIFY(a != b);
		}

		void turningChangesTheDrawing_data()
		{
			QTest::addColumn<qreal>("text");
			QTest::addColumn<qreal>("cores");
			QTest::newRow("text 90") << 90.0 << 0.0;
			QTest::newRow("text -45") << -45.0 << 0.0;
			QTest::newRow("cores 90") << 0.0 << 90.0;
			QTest::newRow("cores 45") << 0.0 << 45.0;
			QTest::newRow("both 180") << 180.0 << 180.0;
		}
		void turningChangesTheDrawing()
		{
			QFETCH(qreal, text);
			QFETCH(qreal, cores);
			turn(0, 0);
			const QRectF plain = inkRect();
			turn(text, cores);
			const QRectF turned = inkRect();
			QVERIFY2(turned != plain, "nothing moved");
				// Everything drawn stays inside what the item covers
			const QRectF covered = m_part->boundingRect().adjusted(-1, -1, 1, 1);
			QVERIFY2(covered.contains(turned),
					 qPrintable(QString("ink %1,%2 %3x%4 outside %5,%6 %7x%8")
								.arg(turned.x()).arg(turned.y()).arg(turned.width()).arg(turned.height())
								.arg(covered.x()).arg(covered.y()).arg(covered.width()).arg(covered.height())));
			turn(0, 0);
		}

		void sectionText_data()
		{
			QTest::addColumn<QString>("typed");
			QTest::addColumn<QString>("drawn");
			QTest::newRow("empty") << "" << "";
			QTest::newRow("number") << "2,5" << QString::fromUtf8("2,5 mm²");
			QTest::newRow("dot") << " 1.5 " << QString::fromUtf8("1.5 mm²");
			QTest::newRow("integer") << "4" << QString::fromUtf8("4 mm²");
			QTest::newRow("mm2") << "2,5mm2" << QString::fromUtf8("2,5mm²");
			QTest::newRow("as typed") << QString::fromUtf8("2,5 mm²") << QString::fromUtf8("2,5 mm²");
			QTest::newRow("free text") << "AWG 14" << "AWG 14";
		}
		void sectionText()
		{
			QFETCH(QString, typed);
			QFETCH(QString, drawn);
			Cable cable;
			cable.setCoreSection(typed);
			QCOMPARE(cable.coreSectionText(), drawn);
		}

		void sectionUnderTheColor()
		{
			m_cable.setCoreSection("2,5");
			QCOMPARE(m_cable.coreMarkText(0), QString::fromUtf8("BK\n2,5 mm²"));
			CableProperties p = m_cable.properties();
			p.show_core_section = false;
			m_cable.setProperties(p);
			QCOMPARE(m_cable.coreMarkText(0), QString("BK"));
			p.show_core_section = true;
			m_cable.setProperties(p);
				// No colour, no mark: the section alone is never written
			QCOMPARE(m_cable.coreMarkText(7), QString());
			m_cable.setCoreSection(QString());
			QCOMPARE(m_cable.coreMarkText(0), QString("BK"));
		}

		void sectionSavedAndReadBack()
		{
			CableProperties p = m_cable.properties();
			p.core_section = "2,5";
			p.show_core_section = false;
			m_cable.setProperties(p);
			QDomDocument doc;
			Cable read;
			read.fromXml(m_cable.toXml(doc));
			QCOMPARE(read.properties().core_section, QString("2,5"));
			QCOMPARE(read.properties().show_core_section, false);

			p.core_section.clear();
			p.show_core_section = true;
			m_cable.setProperties(p);
			const QDomElement e = m_cable.toXml(doc);
			QVERIFY(!e.hasAttribute("core_section"));
			QVERIFY(!e.hasAttribute("show_core_section"));
		}

		void sectionDrawnInside_data()
		{
			QTest::addColumn<qreal>("cores");
			QTest::newRow("along") << 0.0;
			QTest::newRow("level") << 90.0;
			QTest::newRow("45") << 45.0;
		}
		void sectionDrawnInside()
		{
			QFETCH(qreal, cores);
			turn(0, cores);
			const QRectF plain = inkRect();
			m_cable.setCoreSection("2,5");
			const QRectF two_lines = inkRect();
			QVERIFY2(two_lines != plain, "the section is not drawn");
			const QRectF covered = m_part->boundingRect().adjusted(-1, -1, 1, 1);
			QVERIFY2(covered.contains(two_lines),
					 qPrintable(QString("ink %1,%2 %3x%4 outside %5,%6 %7x%8")
								.arg(two_lines.x()).arg(two_lines.y()).arg(two_lines.width()).arg(two_lines.height())
								.arg(covered.x()).arg(covered.y()).arg(covered.width()).arg(covered.height())));
			m_cable.setCoreSection(QString());
			turn(0, 0);
		}

		void coreLevelAt90()
		{
				// One core alone: its colour, written level, is wider than tall
			Cable cable;
			cable.setCoreCount(1);
			cable.setCoreColors({"GNYE"});
			CablePartData data;
			data.uuid = QUuid::createUuid();
			data.p1 = QPointF(0, 0);
			data.p2 = QPointF(0, 0.001);	// a dot: the trunk adds no ink
			cable.addPart(data);
			CableCore core;
			core.core = 0;
			core.part = data.uuid;
			core.position = QPointF(0, 0);
			cable.setCore(core);
			CablePart part(data);
			part.setCable(&cable);

			CablePart *saved = m_part;
			m_part = &part;
			const QRectF along = inkRect();
			CableProperties p = cable.properties();
			p.core_rotation = 90;
			cable.setProperties(p);
			const QRectF level = inkRect();
			m_part = saved;

			QVERIFY2(along.height() > along.width(), "default colour runs along the line");
			QVERIFY2(level.width() > level.height(), "turned by 90 it is written level");
		}
};

QET_APP_TEST_MAIN(tst_cablerotation)

#include "tst_cablerotation.moc"
