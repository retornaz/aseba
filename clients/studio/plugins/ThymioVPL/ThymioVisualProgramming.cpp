#include <QCloseEvent>
#include <QMessageBox>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QSizePolicy>
#include <QFileDialog>
#include <QFile>
#include <QDomElement>
#include <QDesktopWidget>
#include <QApplication>
#include <QScrollBar>
#include <QAction>
#include <QUrl>
#include <QDesktopServices>
#include <QSettings>
#include <QImageReader>
#include <QtDebug>

#include "ThymioVisualProgramming.h"
#include "Block.h"
#include "EventBlocks.h"
#include "ActionBlocks.h"
#include "Scene.h"
#include "Buttons.h"

#include "../../TargetModels.h"
#include "../../../../common/utils/utils.h"

using namespace std;

namespace Aseba { namespace ThymioVPL
{
	ResizingView::ResizingView(QGraphicsScene * scene, QWidget * parent):
		QGraphicsView(scene, parent),
		wasResized(false),
		computedScale(1)
	{
		assert(scene);
		connect(scene, SIGNAL(sceneSizeChanged()), SLOT(recomputeScale()));
	}
	
	void ResizingView::resizeEvent(QResizeEvent * event)
	{
		QGraphicsView::resizeEvent(event);
		if (!wasResized)
			recomputeScale();
	}
	
	void ResizingView::recomputeScale()
	{
		// set transform
		resetTransform();
		const qreal widthScale(0.95*qreal(viewport()->width())/qreal(scene()->width()));
		const qreal heightScale(qreal(viewport()->height()) / qreal(80+410*3));
		computedScale = qMin(widthScale, heightScale);
		scale(computedScale, computedScale);
		wasResized = true;
		QTimer::singleShot(0, this, SLOT(resetResizedFlag()));
	}
	
	void ResizingView::resetResizedFlag()
	{
		wasResized = false;
	}
	
	// Visual Programming
	ThymioVisualProgramming::ThymioVisualProgramming(DevelopmentEnvironmentInterface *_de, bool showCloseButton):
		de(_de),
		scene(new Scene(this)),
		loading(false),
		undoPos(-1),
		runAnimFrame(0),
		runAnimTimer(0)
	{
		runAnimFrames.resize(17);
		
		// Create the gui ...
		setWindowTitle(tr("Thymio Visual Programming Language"));
		setMinimumSize(QSize(400,400));
		
		mainLayout = new QVBoxLayout(this);
		
		//toolBar = new QToolBar();
		//mainLayout->addWidget(toolBar);
		toolLayout = new  QGridLayout();
		mainLayout->addLayout(toolLayout);

		newButton = new QPushButton();
		newButton->setIcon(QIcon(":/images/filenew.svgz"));
		newButton->setToolTip(tr("New"));
		newButton->setFlat(true);
		toolLayout->addWidget(newButton,0,0);
		
		openButton = new QPushButton();
		openButton->setIcon(QIcon(":/images/fileopen.svgz"));
		openButton->setToolTip(tr("Open"));
		openButton->setFlat(true);
		toolLayout->addWidget(openButton,0,1);
		
		saveButton = new QPushButton();
		saveButton->setIcon(QIcon(":/images/save.svgz"));
		saveButton->setToolTip(tr("Save"));
		saveButton->setFlat(true);
		toolLayout->addWidget(saveButton,1,0);
		
		saveAsButton = new QPushButton();
		saveAsButton->setIcon(QIcon(":/images/saveas.svgz"));
		saveAsButton->setToolTip(tr("Save as"));
		saveAsButton->setFlat(true);
		toolLayout->addWidget(saveAsButton,1,1);
		
		undoButton = new QPushButton();
		undoButton->setIcon(QIcon(":/images/edit-undo.svgz"));
		undoButton->setToolTip(tr("Undo"));
		undoButton->setFlat(true);
		undoButton->setEnabled(false);
		undoButton->setShortcut(QKeySequence::Undo);
		toolLayout->addWidget(undoButton,0,2);
		
		redoButton = new QPushButton();
		redoButton->setIcon(QIcon(":/images/edit-redo.svgz"));
		redoButton->setToolTip(tr("Redo"));
		redoButton->setFlat(true);
		redoButton->setEnabled(false);
		redoButton->setShortcut(QKeySequence::Redo);
		toolLayout->addWidget(redoButton,1,2);
		//toolLayout->addSeparator();

		//runButton = new QPushButton();
		runButton = new QPushButton();
		runButton->setIcon(QIcon(":/images/play.svgz"));
		runButton->setToolTip(tr("Load & Run"));
		runButton->setFlat(true);
		toolLayout->addWidget(runButton,0,3,2,2);

		stopButton = new QPushButton();
		stopButton->setIcon(QIcon(":/images/stop1.svgz"));
		stopButton->setToolTip(tr("Stop"));
		stopButton->setFlat(true);
		toolLayout->addWidget(stopButton,0,5,2,2);
		//toolLayout->addSeparator();
		
		advancedButton = new QPushButton();
		advancedButton->setIcon(QIcon(":/images/vpl_advanced_mode.svgz"));
		advancedButton->setToolTip(tr("Advanced mode"));
		advancedButton->setFlat(true);
		toolLayout->addWidget(advancedButton,0,7,2,2);
		//toolLayout->addSeparator();
	
		colorComboButton = new QComboBox();
		colorComboButton->setToolTip(tr("Color scheme"));
		setColors(colorComboButton);
		toolLayout->addWidget(colorComboButton,1,9,1,2);
		//toolLayout->addSeparator();

		helpButton = new QPushButton();
		helpButton->setIcon(QIcon(":/images/info.svgz"));
		helpButton->setToolTip(tr("Help"));
		helpButton->setFlat(true);
		toolLayout->addWidget(helpButton,0,9);

		if (showCloseButton)
		{
			//toolLayout->addSeparator();
			quitButton = new QPushButton();
			quitButton->setIcon(QIcon(":/images/exit.svgz"));
			quitButton->setToolTip(tr("Quit"));
			quitButton->setFlat(true);
			toolLayout->addWidget(quitButton,0,10);
			connect(quitButton, SIGNAL(clicked()), this, SLOT(close()));
			quitSpotSpacer = 0;
		}
		else
		{
			quitSpotSpacer = new QSpacerItem(1,1);
			toolLayout->addItem(quitSpotSpacer, 0, 10);
			quitButton = 0;
		}
		
		connect(newButton, SIGNAL(clicked()), this, SLOT(newFile()));
		connect(openButton, SIGNAL(clicked()), this, SLOT(openFile()));
		connect(saveButton, SIGNAL(clicked()), this, SLOT(save()));
		connect(saveAsButton, SIGNAL(clicked()), this, SLOT(saveAs()));
		connect(undoButton, SIGNAL(clicked()), this, SLOT(undo()));
		connect(redoButton, SIGNAL(clicked()), this, SLOT(redo()));
		
		connect(runButton, SIGNAL(clicked()), this, SLOT(run()));
		connect(stopButton, SIGNAL(clicked()), this, SLOT(stop()));
		connect(advancedButton, SIGNAL(clicked()), this, SLOT(toggleAdvancedMode()));
		connect(helpButton, SIGNAL(clicked()), this, SLOT(openHelp()));
		
		connect(colorComboButton, SIGNAL(currentIndexChanged(int)), this, SLOT(setColorScheme(int)));
		
		horizontalLayout = new QHBoxLayout();
		mainLayout->addLayout(horizontalLayout);
		
		// events
		eventsLayout = new QVBoxLayout();

		eventButtons.push_back(new BlockButton("button", this));
		eventButtons.push_back(new BlockButton("prox", this));
		eventButtons.push_back(new BlockButton("proxground", this));
		eventButtons.push_back(new BlockButton("acc", this));
		eventButtons.push_back(new BlockButton("clap", this));
		eventButtons.push_back(new BlockButton("timeout", this));
		
		eventsLabel = new QLabel(tr("<b>Events</b>"));
		eventsLabel ->setStyleSheet("QLabel { font-size: 10pt; }");
		eventsLayout->setAlignment(Qt::AlignTop);
		eventsLayout->setSpacing(0);
		BlockButton* button;
		eventsLayout->addWidget(eventsLabel);
		foreach (button, eventButtons)
		{
			eventsLayout->addItem(new QSpacerItem(0,10));
			eventsLayout->addWidget(button);
			connect(button, SIGNAL(clicked()), SLOT(addEvent()));
		}
		
		horizontalLayout->addLayout(eventsLayout);
		
		sceneLayout = new QVBoxLayout();

		// compilation
		compilationResultImage = new QLabel();
		compilationResult = new QLabel(tr("Compilation success."));
		compilationResult->setStyleSheet("QLabel { font-size: 10pt; }");
		
		compilationResultImage->setPixmap(QPixmap(QString(":/images/ok.png")));
		compilationResult->setWordWrap(true);
		compilationResult->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
		
		showCompilationError = new QPushButton(tr("show line"));
		showCompilationError->hide();
		connect(showCompilationError, SIGNAL(clicked()), SLOT(showErrorLine()));

		compilationResultLayout = new QHBoxLayout;
		compilationResultLayout->addWidget(compilationResultImage);  
		compilationResultLayout->addWidget(compilationResult,10000);
		compilationResultLayout->addWidget(showCompilationError);
		sceneLayout->addLayout(compilationResultLayout);

		// view
		view = new ResizingView(scene);
		view->setRenderHint(QPainter::Antialiasing);
		view->setAcceptDrops(true);
		view->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
		sceneLayout->addWidget(view);
		Q_ASSERT(scene->setsCount() > 0);
		view->centerOn(*scene->setsBegin());
		
		connect(scene, SIGNAL(contentRecompiled()), SLOT(processCompilationResult()));
		connect(scene, SIGNAL(undoCheckpoint()), SLOT(pushUndo()));
		connect(scene, SIGNAL(highlightChanged()), SLOT(processHighlightChange()));
		connect(scene, SIGNAL(modifiedStatusChanged(bool)), SIGNAL(modifiedStatusChanged(bool)));
		
		/*zoomSlider = new QSlider(Qt::Horizontal);
		zoomSlider->setRange(1,10);
		zoomSlider->setValue(1);
		zoomSlider->setInvertedAppearance(true);
		zoomSlider->setInvertedControls(true);
		sceneLayout->addWidget(zoomSlider);
		
		connect(zoomSlider, SIGNAL(valueChanged(int)), scene, SLOT(updateZoomLevel()));*/

		horizontalLayout->addLayout(sceneLayout);
     
		// actions
		actionsLayout = new QVBoxLayout();

		actionButtons.push_back(new BlockButton("move", this));
		actionButtons.push_back(new BlockButton("colortop", this));
		actionButtons.push_back(new BlockButton("colorbottom", this));
		actionButtons.push_back(new BlockButton("sound", this));
		actionButtons.push_back(new BlockButton("timer", this));
		actionButtons.push_back(new BlockButton("setstate", this));
		
		actionsLabel = new QLabel(tr("<b>Actions</b>"));
		actionsLabel ->setStyleSheet("QLabel { font-size: 10pt; }");
		actionsLayout->setAlignment(Qt::AlignTop);
		actionsLayout->setSpacing(0);
		actionsLayout->addWidget(actionsLabel);
		foreach (button, actionButtons)
		{
			actionsLayout->addItem(new QSpacerItem(0,10));
			actionsLayout->addWidget(button);
			connect(button, SIGNAL(clicked()), SLOT(addAction()));
		}
		
		actionButtons.last()->hide(); // memory

		horizontalLayout->addLayout(actionsLayout);
		
		// window properties
		setWindowModality(Qt::ApplicationModal);
		
		// save initial state
		pushUndo();
	}
	
	ThymioVisualProgramming::~ThymioVisualProgramming()
	{
		saveGeometryIfVisible();
	}
	
	void ThymioVisualProgramming::openHelp() const
	{
		QDesktopServices::openUrl(QUrl(tr("http://aseba.wikidot.com/en:thymiovpl")));
	}
	
	// TODO: add state to color scheme, use static data for color arrays
	
	static QColor currentEventColor = QColor(0,191,255);
	static QColor currentStateColor = QColor(122, 204, 0);
	static QColor currentActionColor = QColor(218,112,214);
	
	void ThymioVisualProgramming::setColors(QComboBox *button)
	{
		eventColors.push_back(QColor(0,191,255)); actionColors.push_back(QColor(218,112,214));
		eventColors.push_back(QColor(155,48,255)); actionColors.push_back(QColor(159,182,205));
		eventColors.push_back(QColor(67,205,128)); actionColors.push_back(QColor(0,197,205)); 
		eventColors.push_back(QColor(255,215,0)); actionColors.push_back(QColor(255,99,71));
		eventColors.push_back(QColor(255,97,3)); actionColors.push_back(QColor(142,56,142));
		eventColors.push_back(QColor(125,158,192)); actionColors.push_back(QColor(56,142,142)); 

		if( button )
		{
			for(int i=0; i<eventColors.size(); ++i)
				button->addItem(drawColorScheme(eventColors.at(i), actionColors.at(i)), "");
		}
	}
	
	QColor ThymioVisualProgramming::getBlockColor(const QString& type)
	{
		if (type == "event")
			return currentEventColor;
		else if (type == "state")
			return currentStateColor;
		else
			return currentActionColor;
	}
	
	QPixmap ThymioVisualProgramming::drawColorScheme(QColor color1, QColor color2)
	{
		QPixmap pixmap(128,58);
		pixmap.fill(Qt::transparent);
		QPainter painter(&pixmap);
		
		painter.setBrush(color1);
		painter.drawRoundedRect(0,0,54,54,4,4);
		
		painter.setBrush(color2);
		painter.drawRoundedRect(66,0,54,54,4,4);
		
		return pixmap;
	}
	
	QWidget* ThymioVisualProgramming::createMenuEntry()
	{
		QPushButton *vplButton = new QPushButton(QIcon(":/images/thymiovpl.png"), tr("Launch VPL"));
		vplButton->setIconSize(QSize(86,32));
		connect(vplButton, SIGNAL(clicked()), SLOT(showVPLModal()));
		return vplButton;
	}
	
	void ThymioVisualProgramming::closeAsSoonAsPossible()
	{
		close();
	}
	
	//! prevent recursion of changes triggered by VPL itself
	void ThymioVisualProgramming::clearSceneWithoutRecompilation()
	{
		loading = true;
		scene->clear(false);
		loading = false;
	}
	
	void ThymioVisualProgramming::showAtSavedPosition()
	{
		QSettings settings;
		restoreGeometry(settings.value("ThymioVisualProgramming/geometry").toByteArray());
		QWidget::show();
	}

	void ThymioVisualProgramming::showVPLModal()
	{
		if (de->newFile())
		{
			scene->reset();
			clearUndo();
			toggleAdvancedMode(false);
			showAtSavedPosition();
			processCompilationResult();
		}
	}
	
	void ThymioVisualProgramming::newFile()
	{
		if (de->newFile())
		{
			scene->reset();
			clearUndo();
			toggleAdvancedMode(false);
			processCompilationResult();
		}
	}

	void ThymioVisualProgramming::openFile()
	{
		de->openFile();
	}
	
	bool ThymioVisualProgramming::save()
	{
		return de->saveFile(false);
	}
	
	bool ThymioVisualProgramming::saveAs()
	{
		return de->saveFile(true);
	}

	bool ThymioVisualProgramming::closeFile()
	{
		if (!isVisible())
			return true;
		
		if (scene->isEmpty() || preDiscardWarningDialog(true)) 
		{
			de->clearOpenedFileName(scene->isModified());
			clearHighlighting(true);
			clearSceneWithoutRecompilation();
			return true;
		}
		else
		{
			de->clearOpenedFileName(scene->isModified());
			return false;
		}
	}
	
	void ThymioVisualProgramming::showErrorLine()
	{
		if (!scene->compilationResult().isSuccessful())
			view->ensureVisible(scene->getSetRow(scene->compilationResult().errorLine));
	}
	
	void ThymioVisualProgramming::setColorScheme(int index)
	{
		currentEventColor = eventColors.at(index);
		currentActionColor = actionColors.at(index);
		
		scene->update();
		updateBlockButtonImages();
	}
	
	void ThymioVisualProgramming::updateBlockButtonImages()
	{
		for(QList<BlockButton*>::iterator itr(eventButtons.begin());
			itr != eventButtons.end(); ++itr)
			(*itr)->updateBlockImage(scene->getAdvanced());

		for(QList<BlockButton*>::iterator itr(actionButtons.begin());
			itr != actionButtons.end(); ++itr)
			(*itr)->updateBlockImage(scene->getAdvanced());
	}
	
	void ThymioVisualProgramming::run()
	{
		if(runButton->isEnabled())
		{
			de->loadAndRun();
			if (runAnimTimer)
			{
				killTimer(runAnimTimer);
				runAnimTimer = 0;
				runButton->setIcon(runAnimFrames[0]);
			}
		}
	}

	void ThymioVisualProgramming::stop()
	{
		de->stop();
		const unsigned leftSpeedVarPos = de->getVariablesModel()->getVariablePos("motor.left.target");
		de->setVariableValues(leftSpeedVarPos, VariablesDataVector(1, 0));
		const unsigned rightSpeedVarPos = de->getVariablesModel()->getVariablePos("motor.right.target");
		de->setVariableValues(rightSpeedVarPos, VariablesDataVector(1, 0));
	}
	
	void ThymioVisualProgramming::toggleAdvancedMode()
	{
		toggleAdvancedMode(!scene->getAdvanced());
	}
	
	void ThymioVisualProgramming::toggleAdvancedMode(bool advanced, bool force)
	{
		if (actionButtons.last()->isVisible() == advanced)
			return;
		qDebug() << "toggling advanced mode to" << advanced;
		if (advanced)
		{
			advancedButton->setIcon(QIcon(":/images/vpl_simple_mode.svgz"));
			actionButtons.last()->show(); // state button
			scene->setAdvanced(true);
		}
		else
		{
			bool doClear(true);
			if (!force && scene->isAnyAdvancedFeature())
			{
				if (QMessageBox::warning(this, tr("Returning to simple mode"),
					tr("You are currently using states. Returning to simple mode will discard any state filter or state setting card.<p>Are you sure you want to continue?"),
				QMessageBox::Yes|QMessageBox::No, QMessageBox::No) == QMessageBox::No)
					doClear = false;
			}
			
			if (doClear)
			{
				advancedButton->setIcon(QIcon(":/images/vpl_advanced_mode.svgz"));
				actionButtons.last()->hide(); // state button
				scene->setAdvanced(false);
			}
		}
		updateBlockButtonImages();
	}
	
	void ThymioVisualProgramming::closeEvent ( QCloseEvent * event )
	{
		if (!closeFile())
			event->ignore();
		else
			saveGeometryIfVisible();
	}
	
	void ThymioVisualProgramming::saveGeometryIfVisible()
	{
		if (isVisible())
		{
			QSettings settings;
			settings.setValue("ThymioVisualProgramming/geometry", saveGeometry());
		}
	}
	
	bool ThymioVisualProgramming::preDiscardWarningDialog(bool keepCode)
	{
		if (!scene->isModified())
			return true;
		
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("Warning"));
		msgBox.setText(tr("The VPL document has been modified.<p>Do you want to save the changes?"));
		msgBox.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
		msgBox.setDefaultButton(QMessageBox::Save);
		
		int ret = msgBox.exec();
		switch (ret)
		{
			case QMessageBox::Save:
			{
				//clearHighlighting(keepCode);
				if (save())
					return true;
				else
					return false;
			}
			case QMessageBox::Discard:
			{
				//clearHighlighting(keepCode);
				return true;
			}
			case QMessageBox::Cancel:
			default:
				return false;
		}

		return false;
	}
	
	void ThymioVisualProgramming::clearHighlighting(bool keepCode)
	{
		if (keepCode && scene->compilationResult().isSuccessful())
			de->displayCode(scene->getCode(), -1);
		else
			de->displayCode(QList<QString>(), -1);
	}

	QDomDocument ThymioVisualProgramming::saveToDom() const 
	{
		// if VPL is in its virign state, do not return a valid document
		// to prevent erasing of text-base code
		if (scene->isEmpty())
			return QDomDocument();
		
		QDomDocument document("tool-plugin-data");
		
		QDomElement vplroot = document.createElement("vplroot");
		vplroot.setAttribute("xml-format-version", 1);
		document.appendChild(vplroot);

		QDomElement settings = document.createElement("settings");
		settings.setAttribute("color-scheme", QString::number(colorComboButton->currentIndex()));
		vplroot.appendChild(settings);
		
		vplroot.appendChild(scene->serialize(document));

		scene->setModified(false);

		return document;
	}
	
	void ThymioVisualProgramming::aboutToLoad()
	{
		clearHighlighting(false);
		saveGeometryIfVisible();
		hide();
	}

	void ThymioVisualProgramming::loadFromDom(const QDomDocument& document, bool fromFile) 
	{
		loading = true;
		
		// first load settings
		const QDomElement settingsElement(document.documentElement().firstChildElement("settings"));
		if (!settingsElement.isNull())
		{
			colorComboButton->setCurrentIndex(settingsElement.attribute("color-scheme").toInt());
		}
		
		// then load program
		const QDomElement programElement(document.documentElement().firstChildElement("program"));
		if (!programElement.isNull())
		{
			scene->deserialize(programElement);
			toggleAdvancedMode(scene->getAdvanced(), true);
		}
		
		scene->setModified(!fromFile);
		scene->recompileWithoutSetModified();
		
		if (!scene->isEmpty())
			showAtSavedPosition();
		
		loading = false;
		clearUndo();
	}
	
	void ThymioVisualProgramming::codeChangedInEditor()
	{
		if (!isVisible() && !loading)
		{
			clearSceneWithoutRecompilation();
		}
	}
	
	bool ThymioVisualProgramming::isModified() const
	{
		return scene->isModified();
	}
	
	void ThymioVisualProgramming::pushUndo()
	{
		undoStack.push(scene->toString());
		undoPos = undoStack.size()-2;
		undoButton->setEnabled(undoPos >= 0);
		
		if (undoStack.size() > 50)
			undoStack.pop_front();
		
		redoButton->setEnabled(false);
	}
	
	void ThymioVisualProgramming::clearUndo()
	{
		undoButton->setEnabled(false);
		redoButton->setEnabled(false);
		undoStack.clear();
		undoPos = -1;
		pushUndo();
	}
	
	void ThymioVisualProgramming::undo()
	{
		if (undoPos < 0)
			return;
		
		scene->fromString(undoStack[undoPos]);
		toggleAdvancedMode(scene->getAdvanced(), true);
		scene->setModified(true);
		scene->recompileWithoutSetModified();

		--undoPos;
		
		undoButton->setEnabled(undoPos >= 0);
		redoButton->setEnabled(true);
	}
	
	void ThymioVisualProgramming::redo()
	{
		if (undoPos >= undoStack.size()-2)
			return;
		
		++undoPos;
		
		scene->fromString(undoStack[undoPos+1]);
		toggleAdvancedMode(scene->getAdvanced(), true);
		scene->setModified(true);
		scene->recompileWithoutSetModified();
		
		undoButton->setEnabled(true);
		redoButton->setEnabled(undoPos < undoStack.size()-2);
	}

	void ThymioVisualProgramming::processCompilationResult()
	{
		compilationResult->setText(scene->compilationResult().getMessage(scene->getAdvanced()));
		if (scene->compilationResult().isSuccessful())
		{
			compilationResultImage->setPixmap(QPixmap(QString(":/images/ok.png")));
			de->displayCode(scene->getCode(), scene->getSelectedSetCodeId());
			runButton->setEnabled(true);
			showCompilationError->hide();
			emit compilationOutcome(true);
		}
		else
		{
			compilationResultImage->setPixmap(QPixmap(QString(":/images/warning.png")));
			runButton->setEnabled(false);
			showCompilationError->show();
			emit compilationOutcome(false);
		}
		// content changed but not uploaded
		if (!runAnimTimer)
			runAnimTimer = startTimer(50);
	}
	
	void ThymioVisualProgramming::processHighlightChange()
	{
		if (scene->compilationResult().isSuccessful())
			de->displayCode(scene->getCode(), scene->getSelectedSetCodeId());
	}
	
	void ThymioVisualProgramming::addEvent()
	{
		BlockButton* button(polymorphic_downcast<BlockButton*>(sender()));
		view->ensureVisible(scene->addEvent(button->getName()));
	}
	
	void ThymioVisualProgramming::addAction()
	{
		BlockButton* button(polymorphic_downcast<BlockButton*>(sender()));
		view->ensureVisible(scene->addAction(button->getName()));
	}
	
	static QImage blend(const QImage& firstImage, const QImage& secondImage, qreal ratio)
	{
		// only attempt to blend images of similar sizes and formats
		if (firstImage.size() != secondImage.size())
			return QImage();
		if (firstImage.format() != secondImage.format())
			return QImage();
		if ((firstImage.format() != QImage::Format_ARGB32) && (firstImage.format() != QImage::Format_ARGB32_Premultiplied))
			return QImage();
		
		// handcoded blend, because Qt painter-based function have problems on many platforms
		QImage destImage(firstImage.size(), firstImage.format());
		const quint32 a(ratio*255.);
		const quint32 oma(255-a);
		for (int y=0; y<firstImage.height(); ++y)
		{
			const quint32 *pF(reinterpret_cast<const quint32*>(firstImage.scanLine(y)));
			const quint32 *pS(reinterpret_cast<const quint32*>(secondImage.scanLine(y)));
			quint32 *pD(reinterpret_cast<quint32*>(destImage.scanLine(y)));
			
			for (int x=0; x<firstImage.width(); ++x)
			{
				const quint32 vF(*pF++);
				const quint32 vS(*pS++);
				
				*pD++ = 
					(((((vF >> 24) & 0xff) * a + ((vS >> 24) & 0xff) * oma) & 0xff00) << 16) |
					(((((vF >> 16) & 0xff) * a + ((vS >> 16) & 0xff) * oma) & 0xff00) <<  8) |
					(((((vF >>  8) & 0xff) * a + ((vS >>  8) & 0xff) * oma) & 0xff00)      ) |
					(((((vF >>  0) & 0xff) * a + ((vS >>  0) & 0xff) * oma) & 0xff00) >>  8);
			}
		}
		
		return destImage;
	}
	
	void ThymioVisualProgramming::regenerateRunButtonAnimation(const QSize& iconSize)
	{
		// load the play animation
		// first image
		QImageReader playReader(":/images/play.svgz");
		playReader.setScaledSize(iconSize);
		const QImage playImage(playReader.read());
		// last image
		QImageReader playRedReader(":/images/play-red.svgz");
		playRedReader.setScaledSize(iconSize);
		const QImage playRedImage(playRedReader.read());
		
		const int count(runAnimFrames.size());
		for (int i = 0; i<count; ++i)
		{
			qreal alpha((1.+cos(qreal(i)*M_PI/qreal(count)))*.5);
			runAnimFrames[i] = QPixmap::fromImage(blend(playImage, playRedImage, alpha));
		}
	}
	
	float ThymioVisualProgramming::computeScale(QResizeEvent *event, int desiredToolbarIconSize)
	{
		// FIXME: scale computation should be updated 
		// desired sizes for height
		const int idealContentHeight(6*256);
		const int uncompressibleHeight(
			max(actionsLabel->height(), eventsLabel->height()) +
			desiredToolbarIconSize * 2 + 
			4 * style()->pixelMetric(QStyle::PM_ButtonMargin) + 
			4 * style()->pixelMetric(QStyle::PM_DefaultFrameWidth) +
			style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing) +
			6 * 10 +
			2 * style()->pixelMetric(QStyle::PM_LayoutTopMargin) + 
			2 * style()->pixelMetric(QStyle::PM_LayoutBottomMargin)
		);
		const int availableHeight(event->size().height() - uncompressibleHeight);
		const qreal scaleHeight(qreal(availableHeight)/qreal(idealContentHeight));
		
		// desired sizes for width
		const int idealContentWidth(1088+256*2);
		const int uncompressibleWidth(
			2 * style()->pixelMetric(QStyle::PM_LayoutHorizontalSpacing) +
			style()->pixelMetric(QStyle::PM_LayoutLeftMargin) + 
			style()->pixelMetric(QStyle::PM_LayoutRightMargin) +
			#ifdef ANDROID
			40 + 
			#else // ANDROID
			style()->pixelMetric(QStyle::PM_ScrollBarSliderMin) +
			#endif // ANDROID
			2 * 20
		);
		const int availableWidth(event->size().width() - uncompressibleWidth);
		const qreal scaleWidth(qreal(availableWidth)/qreal(idealContentWidth));
		
		// compute and set scale
		const qreal scale(qMin(scaleHeight, scaleWidth));
		return scale;
	}
	
	void ThymioVisualProgramming::resizeEvent( QResizeEvent *event)
	{
		// compute size of elements for toolbar
		const int toolbarWidgetCount(11);
		//const int toolbarSepCount(quitButton ? 5 : 4);
		// get width of combox box element (not content)
		QStyleOptionComboBox opt;
		QSize tmp(0, 0);
		tmp = style()->sizeFromContents(QStyle::CT_ComboBox, &opt, tmp);
		int desiredIconSize((
			event->size().width() -
			(
				(toolbarWidgetCount-1) * style()->pixelMetric(QStyle::PM_LayoutHorizontalSpacing) +
				(toolbarWidgetCount-1) * 2 * style()->pixelMetric(QStyle::PM_DefaultFrameWidth) + 
				2 * style()->pixelMetric(QStyle::PM_ComboBoxFrameWidth) +
				toolbarWidgetCount * 2 * style()->pixelMetric(QStyle::PM_ButtonMargin) + 
				style()->pixelMetric(QStyle::PM_LayoutLeftMargin) +
				style()->pixelMetric(QStyle::PM_LayoutRightMargin) +
				tmp.width()
				/*#ifdef Q_WS_MAC
				55 // safety factor, as it seems that metrics do miss some space
				#else // Q_WS_MAC
				20
				#endif // Q_WS_MAC
				//20 // safety factor, as it seems that metrics do miss some space*/
			)
		) / (toolbarWidgetCount));
		
		// two pass of layout computation, should be a good-enough approximation
		qreal testScale(computeScale(event, desiredIconSize));
		desiredIconSize = qMin(desiredIconSize, int(256.*testScale));
		desiredIconSize = qMin(desiredIconSize, event->size().height()/14);
		const qreal scale(computeScale(event, desiredIconSize));
		
		// set toolbar
		const QSize tbIconSize(QSize(desiredIconSize, desiredIconSize));
		const QSize importantIconSize(tbIconSize * 2);
		regenerateRunButtonAnimation(importantIconSize);
		newButton->setIconSize(tbIconSize);
		openButton->setIconSize(tbIconSize);
		saveButton->setIconSize(tbIconSize);
		saveAsButton->setIconSize(tbIconSize);
		undoButton->setIconSize(tbIconSize);
		redoButton->setIconSize(tbIconSize);
		runButton->setIconSize(importantIconSize);
		stopButton->setIconSize(importantIconSize);
		advancedButton->setIconSize(importantIconSize);
		colorComboButton->setIconSize(QSize((desiredIconSize*3)/2,desiredIconSize));
		helpButton->setIconSize(tbIconSize);
		if (quitButton)
			quitButton->setIconSize(tbIconSize);
		if (quitSpotSpacer)
		{
			quitSpotSpacer->changeSize(desiredIconSize, desiredIconSize);
			quitSpotSpacer->invalidate();
		}
		//toolBar->setIconSize(tbIconSize);
		
		// set view and cards on sides
		const QSize iconSize(256*scale, 256*scale);
		for(QList<BlockButton*>::iterator itr = eventButtons.begin();
			itr != eventButtons.end(); ++itr)
			(*itr)->setIconSize(iconSize);
		for(QList<BlockButton*>::iterator itr = actionButtons.begin();
			itr != actionButtons.end(); ++itr)
			(*itr)->setIconSize(iconSize);
	}
	
	void ThymioVisualProgramming::timerEvent ( QTimerEvent * event )
	{
		if (runAnimFrame >= 0)
			runButton->setIcon(runAnimFrames[runAnimFrame]);
		else
			runButton->setIcon(runAnimFrames[-runAnimFrame]);
		runAnimFrame++;
		if (runAnimFrame == runAnimFrames.size())
			runAnimFrame = -runAnimFrames.size()+1;
	}
} } // namespace ThymioVPL / namespace Aseba
// 