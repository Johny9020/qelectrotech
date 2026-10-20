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

#include "conductorsegment.h"
#include "projectview.h"
#include "qetgraphicsitem/conductor.h"
#include "qetgraphicsitem/terminal.h"
#include "qetproject.h"

#include <QDomDocument>

/**
	"Add a bend point" on a conductor (Conductor::addBendAt): a detour of
	three new segments where it was clicked, ends still on the terminals,
	every segment horizontal or vertical, one undo step, saved with the
	project, and nothing when the click is not on the conductor.
*/
class tst_conductorbend : public QObject
{
	Q_OBJECT

	QTemporaryDir m_dir;
	Conductor *m_conductor = nullptr;
	QPointF m_click; ///< on a segment long enough for a bend

	QUndoStack &stack() { return m_conductor->diagram()->undoStack(); }

	static QList<QPointF> scenePoints(const Conductor *c)
	{
		QList<QPointF> points;
		const QList<ConductorSegment *> segs = c->segmentsList();
		if (segs.isEmpty()) return points;
		points << c->mapToScene(segs.first()->firstPoint());
		for (ConductorSegment *s : segs) points << c->mapToScene(s->secondPoint());
		return points;
	}

	static bool isOrthogonal(const QList<QPointF> &points)
	{
		for (int i = 0 ; i + 1 < points.size() ; ++i) {
			const QPointF a = points.at(i), b = points.at(i + 1);
			if (qAbs(a.x() - b.x()) > 0.01 && qAbs(a.y() - b.y()) > 0.01)
				return false;
		}
		return true;
	}

	private slots:
		void initTestCase()
		{
			QETDiagramEditor *editor = QetAppTest::editor();
			QVERIFY(editor);
				// The first example conductor with a middle segment long
				// enough for a bend
			const QStringList files = QDir(QET_EXAMPLES_DIR).entryList({"*.qet"}, QDir::Files, QDir::Name);
			for (const QString &file : files)
			{
				const QString copy = m_dir.filePath(file);
				QVERIFY(QFile::copy(QDir(QET_EXAMPLES_DIR).filePath(file), copy));
				QVERIFY(editor->openAndAddProject(copy, false));
				QETProject *project = editor->currentProjectView()->project();
				for (Diagram *d : project->diagrams())
				{
					for (QGraphicsItem *qgi : d->items())
					{
						if (qgi->type() != Conductor::Type) continue;
						Conductor *c = static_cast<Conductor *>(qgi);
						const QList<ConductorSegment *> segs = c->segmentsList();
						for (int i = 1 ; i + 1 < segs.size() ; ++i)
						{
							const QPointF middle = c->mapToScene(segs.at(i)->middle());
							if (!c->pathPointsWithBendAt(middle).isEmpty()) {
								m_conductor = c;
								m_click = middle;
								break;
							}
						}
						if (m_conductor) break;
					}
					if (m_conductor) break;
				}
				if (m_conductor) break;
			}
			QVERIFY2(m_conductor, "no example conductor with room for a bend");
		}

		void init()
		{
			stack().setClean();
		}

		void cleanup()
		{
			while (stack().index() > stack().cleanIndex())
				stack().undo();
		}

		void addsADetourKeepingTheEnds()
		{
			const QList<QPointF> before = scenePoints(m_conductor);
			QVERIFY(m_conductor->addBendAt(m_click));
			const QList<QPointF> after = scenePoints(m_conductor);

			QCOMPARE(after.size(), before.size() + 4);
			QCOMPARE(after.first(), m_conductor->terminal1->dockConductor());
			QCOMPARE(after.last(), m_conductor->terminal2->dockConductor());
			QVERIFY(isOrthogonal(after));
				// No zero-length segment: they would all count as horizontal
			for (int i = 0 ; i + 1 < after.size() ; ++i)
				QVERIFY(after.at(i) != after.at(i + 1));
		}

		void isOneUndoStep()
		{
			const QList<QPointF> before = scenePoints(m_conductor);
			const int index = stack().index();
			QVERIFY(m_conductor->addBendAt(m_click));
			QCOMPARE(stack().index(), index + 1);
			stack().undo();
			QCOMPARE(scenePoints(m_conductor), before);
		}

		void bendsTwiceAtTheSamePlace()
		{
			const int count = scenePoints(m_conductor).size();
			QVERIFY(m_conductor->addBendAt(m_click));
				// The click is now inside the detour, still on the conductor
			const QList<QPointF> again = m_conductor->pathPointsWithBendAt(m_click);
			if (!again.isEmpty()) {
				QVERIFY(m_conductor->addBendAt(m_click));
				QCOMPARE(scenePoints(m_conductor).size(), count + 8);
				QVERIFY(isOrthogonal(scenePoints(m_conductor)));
			}
		}

		void savedWithTheProject()
		{
			QVERIFY(m_conductor->addBendAt(m_click));
			const int segments = m_conductor->segmentsList().size();
			QDomDocument doc;
			QHash<Terminal *, int> table;
			const QDomElement e = m_conductor->toXml(doc, table);
			QCOMPARE(e.elementsByTagName("segment").size(), segments);
		}

		void handlesFollowTheNewSegments()
		{
			m_conductor->setSelected(true);
			QVERIFY(m_conductor->addBendAt(m_click));
				// One handle per middle segment
			QCOMPARE(m_conductor->handlerPoints().size(),
				 m_conductor->segmentsList().size() - 2);
			m_conductor->setSelected(false);
		}

		void newSegmentsCanBeDragged()
		{
			QVERIFY(m_conductor->addBendAt(m_click));
			const int count = m_conductor->segmentsList().size();
				// Every middle segment, as its handle would move it
			for (int i = 1 ; i + 1 < count ; ++i)
			{
				const int index = stack().index();
				QVERIFY(m_conductor->moveSegment(i, 20, 20));
				const QList<QPointF> points = scenePoints(m_conductor);
				QCOMPARE(points.first(), m_conductor->terminal1->dockConductor());
				QCOMPARE(points.last(), m_conductor->terminal2->dockConductor());
				QVERIFY2(isOrthogonal(points), qPrintable(QString("segment %1").arg(i)));
				while (stack().index() > index) stack().undo();
			}
		}

		void nothingAwayFromTheConductor()
		{
			const QPointF far = m_click + QPointF(5000, 5000);
			QVERIFY(m_conductor->pathPointsWithBendAt(far).isEmpty());
			const int index = stack().index();
			QVERIFY(!m_conductor->addBendAt(far));
			QCOMPARE(stack().index(), index);
		}
};

QET_APP_TEST_MAIN(tst_conductorbend)

#include "tst_conductorbend.moc"
