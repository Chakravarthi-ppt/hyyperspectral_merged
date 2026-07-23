//#include "mainwindow.h"
//#include "ui_mainwindow.h"
//#include "ui_IHSFusion.h"

//#include "StatusBar/statusbarwidget.h"
//#include "Layer/layerproperties.h"
//#include "UI/ImportDialog/importdialog.h"
//#include "../UI/co_bandstack/co_bandstack.h"

//#include <QFileDialog>
//#include <QFileDialog>
//#include <QStatusBar>
//#include <QMessageBox>
//#include <QDebug>
//#include <QToolBar>
//#include <QKeyEvent>
//#include <QToolBar>
//#include <QLabel>
//#include <QToolButton>
//#include <QSettings>
//#include <proj.h>

//#include "database/rastermanager.h"
//#include "Hyperspectral/ui/HyperspectralPanel.h"

//MainWindow::MainWindow(QWidget *parent)
//    : QMainWindow(parent),
//      ui(new Ui::MainWindow),
//      mStatusBarWidget(nullptr)
//{
//    ui->setupUi(this);

//    qDebug() << ui->layerDockWidget;
//    qDebug() << ui->layerDockWidget->metaObject()->className();

//    initializeUI();
//    initializeStatusBar();

//    mES04Processor = new es04processor(this);

//    qDebug() << "MainWindow Processor =" << mES04Processor;

//    mLayerManager = new LayerManager(this);

//    mLayerManager->initialize(
//                ui->graphicsView,
//                ui->treeWidgetLayers);

//    initializeLayerDock();

//    connectSignals();

//    QString filePath =
//            "/home/pteck/Downloads/WESEE-Poc2-with-Hyperspectral (1)/WESEE-Poc2-staging/World.tif";

//    qDebug() << "Loading :" << filePath;

//    mCurrentFile = filePath;

//    if(ui->graphicsView->loadBaseMap(filePath))
//    {
//        mLayerManager->addLayer(filePath);

//        qDebug() << "Tree Count ="
//                 << ui->treeWidgetLayers->topLevelItemCount();
//    }
//    qDebug() << "Current Path =" << QDir::currentPath();

//    QSettings settings;

//    QString folder =
//            settings.value("PreProcessedFolder").toString();

//    if(!folder.isEmpty() &&
//            QDir(folder).exists())
//    {
//        qDebug() << "Loading Previous Folder :" << folder;

//        mLayerManager->loadFolder(folder);

//        mLayerManager->watchProject(folder);
//    }

//    connect(mES04Processor,
//            &es04processor::processingFinished,
//            this,
//            &MainWindow::onProcessingFinished);
//}

//MainWindow::~MainWindow()
//{
//    delete ui;
//}

//void MainWindow::initializeUI()
//{
//    showMaximized();

//    //-----------------------------------------
//    // Navigation Toolbar
//    //-----------------------------------------
//    QToolBar *toolBar = addToolBar("Navigation");
//    toolBar->setMovable(false);
//    toolBar->setIconSize(QSize(32,32));
//    toolBar->setStyleSheet("background:#0B4F6C");

//    //-----------------------------------------
//    // Logo
//    //-----------------------------------------
//    QLabel *logoLabel = new QLabel(this);

//    QPixmap logo(":/file/logo_dark-bg_vector.svg");   // Hardcoded path

//    logoLabel->setPixmap(
//                logo.scaled(250,200,
//                            Qt::KeepAspectRatio,
//                            Qt::SmoothTransformation));

//    toolBar->addWidget(logoLabel);


//    QWidget *logo_spacer = new QWidget(this);
//    logo_spacer->setFixedWidth(60);
//    logo_spacer->setStyleSheet("background: transparent;");
//    toolBar->addWidget(logo_spacer);

//    //-----------------------------------------
//    // SAR Optical Fusion Menu
//    //-----------------------------------------

//    QToolButton *fusionMenu = new QToolButton(this);

//    fusionMenu->setText("Maritime Geospatial Analytics");
//    fusionMenu->setPopupMode(QToolButton::InstantPopup);

//    fusionMenu->setStyleSheet(
//                "QToolButton {"
//                "background:#0B4F6C;"
//                "color:white;"
//                "font-weight:bold;"
//                "border-radius:3px;"
//                "padding:6px 10px;"
//                "}"
//                "QPushButton:hover{"
//                "background:#1565C0;"
//                "}"
//                "QToolButton:hover {"
//                "   background:#1565C0;"
//                "}"
//                );

//    // Create Menu
//    QMenu *menu = new QMenu(this);

//    // Create Open Menu
//    QAction *openAction = menu->addAction("Open");
//    connect(openAction,
//            &QAction::triggered,
//            this,
//            &MainWindow::on_btnOpenImport_clicked);

//    // Create CoRegistration Menu
//    QAction *coregAction = menu->addAction("Co-Registration");
//    connect(coregAction,
//            &QAction::triggered,
//            this,
//            &MainWindow::onOpenCoregistration);

//    // Create Fusion Menu
//    QAction *fusionAction = menu->addAction("Fusion");
//    connect(fusionAction,
//            &QAction::triggered,
//            this,
//            &MainWindow::on_btnIHSFusion_clicked);

//    fusionMenu->setMenu(menu);

//    toolBar->addWidget(fusionMenu);

//    QWidget *fusionHsiSpacer = new QWidget(this);
//    fusionHsiSpacer->setFixedWidth(12);
//    fusionHsiSpacer->setStyleSheet("background: transparent;");
//    toolBar->addWidget(fusionHsiSpacer);

//    //-----------------------------------------
//    // Hyperspectral Module Button
//    //-----------------------------------------

//    QToolButton *hyperspectralBtn = new QToolButton(this);

//    hyperspectralBtn->setText("Hyperspectral");
//    hyperspectralBtn->setToolTip("Open Hyperspectral Analytics Module");

//    hyperspectralBtn->setStyleSheet(
//                "QToolButton {"
//                "background:#0B4F6C;"
//                "color:white;"
//                "font-weight:bold;"
//                "border-radius:3px;"
//                "padding:6px 10px;"
//                "}"
//                "QToolButton:hover {"
//                "   background:#1565C0;"
//                "}"
//                );

//    connect(hyperspectralBtn,
//            &QToolButton::clicked,
//            this,
//            &MainWindow::onOpenHyperspectral);

//    toolBar->addWidget(hyperspectralBtn);

//    QWidget *spacer = new QWidget(this);
//    spacer->setFixedWidth(30);
//    spacer->setStyleSheet("background: transparent;");
//    toolBar->addWidget(spacer);

//    //-----------------------------------------
//    // ZoomIn, ZoomOut, FitView
//    //-----------------------------------------

//    toolBar->setStyleSheet(
//                "QToolBar {"
//                "    background-color: #0B4F6C;"
//                "    spacing: 5px;"
//                "}"
//                "QToolButton {"
//                "    background: transparent;"
//                "    border: none;"
//                "    padding: 5px;"
//                "}"
//                "QToolButton:hover {"
//                "    background-color: #1565C0;"
//                "    border-radius:4px;"
//                "}"
//                "QToolButton:pressed {"
//                "    background-color: #0A4A8A;"
//                "}"
//                );

//    QAction *zoomInAction = new QAction(
//                QIcon(":/file/WhatsApp Image 2026-07-04 at 12.34.03 PM (1).jpeg"),
//                "",
//                this);

//    zoomInAction->setToolTip("Zoom In");

//    connect(zoomInAction,
//            &QAction::triggered,
//            this,
//            [this]()
//    {
//        ui->graphicsView->zoomIn();
//    });

//    toolBar->addAction(zoomInAction);

//    toolBar->addSeparator();

//    QAction *zoomOutAction = new QAction(
//                QIcon(":/file/WhatsApp Image 2026-07-04 at 12.34.03 PM.jpeg"),
//                "",
//                this);

//    zoomOutAction->setToolTip("Zoom Out");

//    connect(zoomOutAction,
//            &QAction::triggered,
//            this,
//            [this]()
//    {
//        ui->graphicsView->zoomOut();
//    });

//    toolBar->addAction(zoomOutAction);

//    toolBar->addSeparator();

//    QAction *resetAction = new QAction(
//                QIcon(":/file/WhatsApp Image 2026-07-04 at 12.43.22 PM.jpeg"),
//                "",
//                this);

//    resetAction->setToolTip("Reset View");

//    connect(resetAction,
//            &QAction::triggered,
//            this,
//            [this]()
//    {
////        if(!mCurrentFile.isEmpty())
////            ui->graphicsView->loadBaseMap(mCurrentFile);
//        ui->graphicsView->resetView();
//    });

//    toolBar->addAction(resetAction);

//    toolBar->addSeparator();

//    QAction *layerAction = toolBar->addAction(
//        QIcon(":/file/WhatsApp Image 2026-07-04 at 12.34.04 PM.jpeg"),
//        "");

//    layerAction->setToolTip("Layers");

//    connect(layerAction, &QAction::triggered,
//            this,
//            [this]()
//    {
//        ui->layerDockWidget->setVisible(
//                    !ui->layerDockWidget->isVisible());
//    });

//}

//void MainWindow::initializeStatusBar()
//{
//    mStatusBarWidget = new StatusBarWidget(this);

//    statusBar()->addPermanentWidget(mStatusBarWidget, 1);

//    // Initial Values
//    mStatusBarWidget->setProjection("WGS84");
//    mStatusBarWidget->setLonLat("-- , --");
//    mStatusBarWidget->setCursorPosition("--");
//    mStatusBarWidget->setGridReference("--");

//    QRectF rect = ui->graphicsView->currentViewExtent();

//    mStatusBarWidget->setViewSize(
//                QString("%1 x %2")
//                .arg(rect.width(), 0, 'f', 2)
//                .arg(rect.height(), 0, 'f', 2));

//    mStatusBarWidget->setCurrentScale("1:1000000");
//    mStatusBarWidget->setRotation(0.0);
//}

//void MainWindow::initializeLayerDock()
//{
//    ui->treeWidgetLayers->clear();

//    ui->treeWidgetLayers->setHeaderLabel("Layers");

//    ui->layerDockWidget->setFloating(false);
//    ui->layerDockWidget->setAllowedAreas(Qt::LeftDockWidgetArea);
//    ui->layerDockWidget->setFeatures(QDockWidget::NoDockWidgetFeatures);

//    ui->layerDockWidget->setFeatures(QDockWidget::DockWidgetClosable);

//    ui->layerDockWidget->setMinimumWidth(300);
//    ui->layerDockWidget->setMaximumWidth(300);

//    qDebug() << ui->layerDockWidget->features();

//    ui->treeWidgetLayers->setContextMenuPolicy(
//                Qt::CustomContextMenu);

//    connect(ui->treeWidgetLayers,
//            &QTreeWidget::itemChanged,
//            mLayerManager,
//            &LayerManager::onLayerItemChanged);

//    connect(ui->treeWidgetLayers,
//            &QTreeWidget::customContextMenuRequested,
//            mLayerManager,
//            &LayerManager::showLayerContextMenu);
//}

//void MainWindow::connectSignals()
//{
//    connect(mStatusBarWidget,
//            &StatusBarWidget::rotationChanged,
//            this,
//            &MainWindow::onRotationChanged);

//    connect(mStatusBarWidget,
//            &StatusBarWidget::scaleChanged,
//            this,
//            &MainWindow::onScaleChanged);

//    connect(ui->graphicsView,
//            &MapCanvas::mouseCoordinateChanged,
//            this,
//            &MainWindow::updateMouseCoordinate);

//    connect(ui->graphicsView,
//            &MapCanvas::mouseGeoCoordinateChanged,
//            this,
//            &MainWindow::updateGeoCoordinate);

//    connect(ui->graphicsView,
//            &MapCanvas::scaleChanged,
//            this,
//            &MainWindow::updateScale);

//    connect(ui->graphicsView,
//            &MapCanvas::rotationChanged,
//            this,
//            &MainWindow::updateRotation);

//    connect(ui->graphicsView,
//            &MapCanvas::extentChanged,
//            this,
//            &MainWindow::updateExtent);
//}

//// --------------------------------
//// Slots
//// --------------------------------

//void MainWindow::onOpenClicked()
//{
//    QMessageBox::information(
//                this,
//                "Open",
//                "Import Dialog will open here.");
//}

//void MainWindow::onRotationChanged(double angle)
//{
//    qDebug() << "Rotation :" << angle;

//    ui->graphicsView->rotateMap(angle);
//}

//void MainWindow::onScaleChanged(const QString &scale)
//{
//    qDebug() << "Scale :" << scale;

//    ui->graphicsView->setScalePreset(scale);
//}

//void MainWindow::updateMouseCoordinate(const QPointF &scenePos)
//{
//    mStatusBarWidget->setCursorPosition(
//                QString("%1 , %2")
//                .arg(scenePos.x(), 0, 'f', 2)
//                .arg(scenePos.y(), 0, 'f', 2));
//}

//void MainWindow::updateExtent(const QRectF &rect)
//{
//    mStatusBarWidget->setViewSize(
//                QString("%1 x %2")
//                .arg(rect.width(), 0, 'f', 2)
//                .arg(rect.height(), 0, 'f', 2));
//}

//void MainWindow::updateScale(double scale)
//{
//    QString text;

//    if(scale >= 8.0)
//        text = "1:1000";
//    else if(scale >= 4.0)
//        text = "1:2500";
//    else if(scale >= 2.5)
//        text = "1:5000";
//    else if(scale >= 1.5)
//        text = "1:10000";
//    else if(scale >= 1.0)
//        text = "1:25000";
//    else if(scale >= 0.75)
//        text = "1:50000";
//    else
//        text = "1:100000";

//    mStatusBarWidget->setCurrentScale(text);
//}

//void MainWindow::updateRotation(double angle)
//{
//    mStatusBarWidget->setRotation(angle);
//}

//void MainWindow::clearLayers()
//{
//    ui->treeWidgetLayers->clear();
//}

//void MainWindow::keyPressEvent(QKeyEvent *event)
//{
//    if (event->key() == Qt::Key_Plus || event->key() == Qt::Key_Equal)
//    {
//        ui->graphicsView->zoomIn();
//        event->accept();
//    }
//    else if (event->key() == Qt::Key_Minus)
//    {
//        ui->graphicsView->zoomOut();
//        event->accept();
//    }
//    else
//    {
//        QMainWindow::keyPressEvent(event);
//    }
//}

//QString MainWindow::convertToUTM(
//        double lon,
//        double lat)
//{
//    if(lat < -80.0 || lat > 84.0)
//    {
//        return "Outside UTM";
//    }

//    int zone =
//            static_cast<int>(
//                (lon + 180.0) / 6.0) + 1;

//    OGRSpatialReference wgs84;
//    wgs84.SetWellKnownGeogCS("WGS84");

//    OGRSpatialReference utm;
//    utm.SetUTM(zone, lat >= 0);
//    utm.SetWellKnownGeogCS("WGS84");

//    wgs84.SetAxisMappingStrategy(
//                OAMS_TRADITIONAL_GIS_ORDER);

//    utm.SetAxisMappingStrategy(
//                OAMS_TRADITIONAL_GIS_ORDER);

//    OGRCoordinateTransformation *poCT =
//            OGRCreateCoordinateTransformation(
//                &wgs84,
//                &utm);
//    if(!poCT)
//        return "Invalid";

//    double x = lon;
//    double y = lat;

//    bool ok =
//            poCT->Transform(
//                1,
//                &x,
//                &y);

//    OCTDestroyCoordinateTransformation(
//                poCT);

//    if(!ok)
//        return "Invalid";

//    return QString("%1 , %2")
//            .arg(x,0,'f',2)
//            .arg(y,0,'f',2);
//}

//void MainWindow::updateGeoCoordinate(const QPointF &geoPos)
//{
//    double lon = geoPos.x();
//    double lat = geoPos.y();

//    mStatusBarWidget->setLonLat(QString("%1 , %2").arg(lon, 0, 'f', 6).arg(lat, 0, 'f', 6));

//    QString utm = convertToUTM(lon, lat);
//    mStatusBarWidget->setCursorPosition(utm);

//    int zone = static_cast<int>((lon + 180.0) / 6.0) + 1;
//    QString hemisphere = (lat >= 0) ? "N" : "S";
//    mStatusBarWidget->setGridReference(QString("%1%2").arg(zone).arg(hemisphere));
//}

//void MainWindow::on_btnOpenImport_clicked()
//{
//    importdialog *dialog = new importdialog(mES04Processor, nullptr);
//    dialog->show();
//}

//void MainWindow::onOpenCoregistration()

//{
//    co_bandstack *win = new co_bandstack(nullptr);
//        win->setAttribute(Qt::WA_DeleteOnClose);
//        win->setWindowTitle("Co-Registration");
//        win->show();

//}

//void MainWindow::onOpenHyperspectral()
//{
//    // Keep the existing left dock (layer panel) visible -- clicking
//    // Hyperspectral must not hide it.
//    if (ui->layerDockWidget)
//        ui->layerDockWidget->show();

//    // Toggle: if already built, just show/hide it instead of rebuilding.
//    if (hyperspectralDock) {
//        hyperspectralDock->setVisible(!hyperspectralDock->isVisible());
//        if (hyperspectralDock->isVisible())
//            hyperspectralDock->raise();
//        return;
//    }

//    // Dedicated, self-contained Hyperspectral UI (own step buttons +
//    // preview), built specifically for embedding -- not a reused/scraped
//    // top-level window.
//    hyperspectralPanel = new HyperspectralPanel(this);

//    hyperspectralDock = new QDockWidget("Hyperspectral Analytics", this);
//    hyperspectralDock->setObjectName("hyperspectralDockWidget");
//    hyperspectralDock->setWidget(hyperspectralPanel);
//    hyperspectralDock->setMinimumWidth(320);
//    hyperspectralDock->setAllowedAreas(Qt::RightDockWidgetArea | Qt::LeftDockWidgetArea);
//    hyperspectralDock->setFeatures(QDockWidget::DockWidgetClosable |
//                                    QDockWidget::DockWidgetMovable);

//    // Match WESEE's existing dark toolbar theme (#0B4F6C header).
//    hyperspectralDock->setStyleSheet(
//                "QDockWidget {"
//                "   color:white;"
//                "   font-weight:bold;"
//                "}"
//                "QDockWidget::title {"
//                "   background:#0B4F6C;"
//                "   padding:6px;"
//                "}"
//                );

//    addDockWidget(Qt::RightDockWidgetArea, hyperspectralDock);
//    hyperspectralDock->show();
//}


//void MainWindow::onProcessingFinished(const QString &folder)
//{
//    qDebug() << "===== Processing Finished =====";
//    qDebug() << "Folder =" << folder;

//    QSettings settings;
//    settings.setValue("PreProcessedFolder", folder);

//    qDebug() << "Loading layers from:" << folder;

//    ui->treeWidgetLayers->clear();

//    mLayerManager->clearLayers();

//    mLayerManager->addLayer(
//                mCurrentFile);

//    mLayerManager->loadFolder(folder);

//    mLayerManager->watchProject(folder);
//}


//void MainWindow::on_btnIHSFusion_clicked()
//{
//    QDialog dialog(this);

//    Ui::IHSFusion ui;
//    ui.setupUi(&dialog);

//    dialog.setWindowTitle("IHS Image Fusion");

//    //----------------------------------------------------
//    // Browse RED
//    //----------------------------------------------------

//    connect(ui.btnBrowseRED,
//            &QPushButton::clicked,
//            [&]()
//    {
//        QString file = QFileDialog::getOpenFileName(
//                    &dialog,
//                    "Open RED Image",
//                    "",
//                    "GeoTIFF (*.tif *.tiff)");

//        if(!file.isEmpty())
//            ui.txtRED->setText(file);
//    });

//    //----------------------------------------------------
//    // Browse NIR
//    //----------------------------------------------------

//    connect(ui.btnBrowseNIR,
//            &QPushButton::clicked,
//            [&]()
//    {
//        QString file = QFileDialog::getOpenFileName(
//                    &dialog,
//                    "Open NIR Image",
//                    "",
//                    "GeoTIFF (*.tif *.tiff)");

//        if(!file.isEmpty())
//            ui.txtNIR->setText(file);
//    });

//    //----------------------------------------------------
//    // Browse HH
//    //----------------------------------------------------

//    connect(ui.btnBrowseHH,
//            &QPushButton::clicked,
//            [&]()
//    {
//        QString file = QFileDialog::getOpenFileName(
//                    &dialog,
//                    "Open HH Image",
//                    "",
//                    "GeoTIFF (*.tif *.tiff)");

//        if(!file.isEmpty())
//            ui.txtHH->setText(file);
//    });

//    //----------------------------------------------------
//    // Reset
//    //----------------------------------------------------

//    connect(ui.btnReset,
//            &QPushButton::clicked,
//            [&]()
//    {
//        ui.txtRED->clear();
//        ui.txtNIR->clear();
//        ui.txtHH->clear();

//        ui.progressBar->setValue(0);
//    });

//    //----------------------------------------------------
//    // Cancel
//    //----------------------------------------------------

//    connect(ui.btnCancel,
//            &QPushButton::clicked,
//            &dialog,
//            &QDialog::reject);

//    //----------------------------------------------------
//    // Run
//    //----------------------------------------------------

//    connect(ui.btnRun,
//            &QPushButton::clicked,
//            [&]()
//    {
//        //------------------------------------------------
//        // Output Folder = PreProcessed/Fusion
//        //------------------------------------------------

//        QSettings settings("CAIR", "OpticalFusion");

//        QString workingFolder =
//                settings.value("WorkingFolder").toString();

//        if(workingFolder.isEmpty())
//        {
//            QMessageBox::warning(
//                        this,
//                        "Project Not Found",
//                        "Please select a valid project first.");

//            return;
//        }

//        QDir preProcessedDir(
//                    workingFolder + "/PreProcessed");

//        if(!preProcessedDir.exists())
//        {
//            preProcessedDir.mkpath(".");
//        }

//        QDir fusionDir(
//                    workingFolder + "/PreProcessed/Fusion");

//        if(!fusionDir.exists())
//        {
//            preProcessedDir.mkpath("Fusion");
//        }

//        QString outputFile =
//                fusionDir.filePath("IHSFusion.tif");

//        IHSFusion fusion;

//        fusion.progressCallback =
//                [&](int progress)
//        {
//            ui.progressBar->setValue(progress);
//            qApp->processEvents();
//        };

//        //------------------------------------------------
//        // Open Inputs
//        //------------------------------------------------

//        if(!fusion.openInputs(
//                    ui.txtRED->text(),
//                    ui.txtNIR->text(),
//                    ui.txtHH->text()))
//        {
//            QMessageBox::critical(
//                        &dialog,
//                        "Error",
//                        "Import all three bands.");

//            return;
//        }

//        //------------------------------------------------
//        // Create Output
//        //------------------------------------------------

//        if(!fusion.createOutput(outputFile))
//        {
//            QMessageBox::critical(
//                        &dialog,
//                        "Error",
//                        "Failed to create output image.");

//            return;
//        }

//        //------------------------------------------------
//        // Compute Statistics
//        //------------------------------------------------

//        if(!fusion.computeGlobalStatistics())
//        {
//            QMessageBox::critical(
//                        &dialog,
//                        "Error",
//                        "Failed to compute statistics.");

//            return;
//        }

//        //Build Histogram
//        if(!fusion.computeHistogramMatchingLUT())
//        {
//            QMessageBox::critical(&dialog, "Error", "Failed to build histogram matching LUT.");
//            return;
//        }

//        //------------------------------------------------
//        // Run Fusion
//        //------------------------------------------------

//        if(!fusion.processTiles())
//        {
//            QMessageBox::critical(
//                        &dialog,
//                        "Error",
//                        "Fusion failed.");

//            return;
//        }

//        fusion.close();

//        QMessageBox::information(
//                    &dialog,
//                    "Success",
//                    QString("Fusion completed successfully.\n\nOutput:\n%1")
//                    .arg(outputFile));


//    });

//    dialog.exec();
//}





#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "ui_IHSFusion.h"

#include "StatusBar/statusbarwidget.h"
#include "Layer/layerproperties.h"
#include "UI/ImportDialog/importdialog.h"
#include "../UI/co_bandstack/co_bandstack.h"

#include <QFileDialog>
#include <QFileDialog>
#include <QStatusBar>
#include <QMessageBox>
#include <QDebug>
#include <QToolBar>
#include <QKeyEvent>
#include <QToolBar>
#include <QLabel>
#include <QToolButton>
#include <QSettings>
#include <proj.h>

#include "database/rastermanager.h"
#include "Hyperspectral/ui/HyperspectralPanel.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow),
      mStatusBarWidget(nullptr)
{
    ui->setupUi(this);

    qDebug() << ui->layerDockWidget;
    qDebug() << ui->layerDockWidget->metaObject()->className();

    initializeUI();
    initializeStatusBar();

    mES04Processor = new es04processor(this);

    qDebug() << "MainWindow Processor =" << mES04Processor;

    mLayerManager = new LayerManager(this);

    mLayerManager->initialize(
                ui->graphicsView,
                ui->treeWidgetLayers);

    initializeLayerDock();

    connectSignals();

    QString filePath =
            "/home/pteck/Downloads/WESEE-Poc2-with-Hyperspectral (1)/WESEE-Poc2-staging/World.tif";

    qDebug() << "Loading :" << filePath;

    mCurrentFile = filePath;

    if(ui->graphicsView->loadBaseMap(filePath))
    {
        mLayerManager->addLayer(filePath);

        qDebug() << "Tree Count ="
                 << ui->treeWidgetLayers->topLevelItemCount();
    }
    qDebug() << "Current Path =" << QDir::currentPath();

    QSettings settings;

    QString folder =
            settings.value("PreProcessedFolder").toString();

    if(!folder.isEmpty() &&
            QDir(folder).exists())
    {
        qDebug() << "Loading Previous Folder :" << folder;

        mLayerManager->loadFolder(folder);

        mLayerManager->watchProject(folder);
    }

    connect(mES04Processor,
            &es04processor::processingFinished,
            this,
            &MainWindow::onProcessingFinished);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::initializeUI()
{
    showMaximized();

    //-----------------------------------------
    // Navigation Toolbar
    //-----------------------------------------
    QToolBar *toolBar = addToolBar("Navigation");
    toolBar->setMovable(false);
    toolBar->setIconSize(QSize(32,32));
    toolBar->setStyleSheet("background:#0B4F6C");

    //-----------------------------------------
    // Logo
    //-----------------------------------------
    QLabel *logoLabel = new QLabel(this);

    QPixmap logo(":/file/logo_dark-bg_vector.svg");   // Hardcoded path

    logoLabel->setPixmap(
                logo.scaled(250,200,
                            Qt::KeepAspectRatio,
                            Qt::SmoothTransformation));

    toolBar->addWidget(logoLabel);


    QWidget *logo_spacer = new QWidget(this);
    logo_spacer->setFixedWidth(60);
    logo_spacer->setStyleSheet("background: transparent;");
    toolBar->addWidget(logo_spacer);

    //-----------------------------------------
    // SAR Optical Fusion Menu
    //-----------------------------------------

    QToolButton *fusionMenu = new QToolButton(this);

    fusionMenu->setText("Maritime Geospatial Analytics");
    fusionMenu->setPopupMode(QToolButton::InstantPopup);

    fusionMenu->setStyleSheet(
                "QToolButton {"
                "background:#0B4F6C;"
                "color:white;"
                "font-weight:bold;"
                "border-radius:3px;"
                "padding:6px 10px;"
                "}"
                "QPushButton:hover{"
                "background:#1565C0;"
                "}"
                "QToolButton:hover {"
                "   background:#1565C0;"
                "}"
                );

    // Create Menu
    QMenu *menu = new QMenu(this);

    // Create Open Menu
    QAction *openAction = menu->addAction("Open");
    connect(openAction,
            &QAction::triggered,
            this,
            &MainWindow::on_btnOpenImport_clicked);

    // Create CoRegistration Menu
    QAction *coregAction = menu->addAction("Co-Registration");
    connect(coregAction,
            &QAction::triggered,
            this,
            &MainWindow::onOpenCoregistration);

    // Create Fusion Menu
    QAction *fusionAction = menu->addAction("Fusion");
    connect(fusionAction,
            &QAction::triggered,
            this,
            &MainWindow::on_btnIHSFusion_clicked);

    fusionMenu->setMenu(menu);

    toolBar->addWidget(fusionMenu);

    QWidget *fusionHsiSpacer = new QWidget(this);
    fusionHsiSpacer->setFixedWidth(12);
    fusionHsiSpacer->setStyleSheet("background: transparent;");
    toolBar->addWidget(fusionHsiSpacer);

    //-----------------------------------------
    // Hyperspectral Module Button
    //-----------------------------------------

    QToolButton *hyperspectralBtn = new QToolButton(this);

    hyperspectralBtn->setText("Hyperspectral");
    hyperspectralBtn->setToolTip("Open Hyperspectral Analytics Module");

    hyperspectralBtn->setStyleSheet(
                "QToolButton {"
                "background:#0B4F6C;"
                "color:white;"
                "font-weight:bold;"
                "border-radius:3px;"
                "padding:6px 10px;"
                "}"
                "QToolButton:hover {"
                "   background:#1565C0;"
                "}"
                );

    connect(hyperspectralBtn,
            &QToolButton::clicked,
            this,
            &MainWindow::onOpenHyperspectral);

    toolBar->addWidget(hyperspectralBtn);

    QWidget *spacer = new QWidget(this);
    spacer->setFixedWidth(30);
    spacer->setStyleSheet("background: transparent;");
    toolBar->addWidget(spacer);

    //-----------------------------------------
    // ZoomIn, ZoomOut, FitView
    //-----------------------------------------

    toolBar->setStyleSheet(
                "QToolBar {"
                "    background-color: #0B4F6C;"
                "    spacing: 5px;"
                "}"
                "QToolButton {"
                "    background: transparent;"
                "    border: none;"
                "    padding: 5px;"
                "}"
                "QToolButton:hover {"
                "    background-color: #1565C0;"
                "    border-radius:4px;"
                "}"
                "QToolButton:pressed {"
                "    background-color: #0A4A8A;"
                "}"
                );

    QAction *zoomInAction = new QAction(
                QIcon(":/file/WhatsApp Image 2026-07-04 at 12.34.03 PM (1).jpeg"),
                "",
                this);

    zoomInAction->setToolTip("Zoom In");

    connect(zoomInAction,
            &QAction::triggered,
            this,
            [this]()
    {
        ui->graphicsView->zoomIn();
    });

    toolBar->addAction(zoomInAction);

    toolBar->addSeparator();

    QAction *zoomOutAction = new QAction(
                QIcon(":/file/WhatsApp Image 2026-07-04 at 12.34.03 PM.jpeg"),
                "",
                this);

    zoomOutAction->setToolTip("Zoom Out");

    connect(zoomOutAction,
            &QAction::triggered,
            this,
            [this]()
    {
        ui->graphicsView->zoomOut();
    });

    toolBar->addAction(zoomOutAction);

    toolBar->addSeparator();

    QAction *resetAction = new QAction(
                QIcon(":/file/WhatsApp Image 2026-07-04 at 12.43.22 PM.jpeg"),
                "",
                this);

    resetAction->setToolTip("Reset View");

    connect(resetAction,
            &QAction::triggered,
            this,
            [this]()
    {
//        if(!mCurrentFile.isEmpty())
//            ui->graphicsView->loadBaseMap(mCurrentFile);
        ui->graphicsView->resetView();
    });

    toolBar->addAction(resetAction);

    toolBar->addSeparator();

    QAction *layerAction = toolBar->addAction(
        QIcon(":/file/WhatsApp Image 2026-07-04 at 12.34.04 PM.jpeg"),
        "");

    layerAction->setToolTip("Layers");

    connect(layerAction, &QAction::triggered,
            this,
            [this]()
    {
        ui->layerDockWidget->setVisible(
                    !ui->layerDockWidget->isVisible());
    });

}

void MainWindow::initializeStatusBar()
{
    mStatusBarWidget = new StatusBarWidget(this);

    statusBar()->addPermanentWidget(mStatusBarWidget, 1);

    // Initial Values
    mStatusBarWidget->setProjection("WGS84");
    mStatusBarWidget->setLonLat("-- , --");
    mStatusBarWidget->setCursorPosition("--");
    mStatusBarWidget->setGridReference("--");

    QRectF rect = ui->graphicsView->currentViewExtent();

    mStatusBarWidget->setViewSize(
                QString("%1 x %2")
                .arg(rect.width(), 0, 'f', 2)
                .arg(rect.height(), 0, 'f', 2));

    mStatusBarWidget->setCurrentScale("1:1000000");
    mStatusBarWidget->setRotation(0.0);
}

void MainWindow::initializeLayerDock()
{
    ui->treeWidgetLayers->clear();

    ui->treeWidgetLayers->setHeaderLabel("Layers");

    ui->layerDockWidget->setFloating(false);
    ui->layerDockWidget->setAllowedAreas(Qt::LeftDockWidgetArea);
    ui->layerDockWidget->setFeatures(QDockWidget::NoDockWidgetFeatures);

    ui->layerDockWidget->setFeatures(QDockWidget::DockWidgetClosable);

    ui->layerDockWidget->setMinimumWidth(300);
    ui->layerDockWidget->setMaximumWidth(300);

    qDebug() << ui->layerDockWidget->features();

    ui->treeWidgetLayers->setContextMenuPolicy(
                Qt::CustomContextMenu);

    connect(ui->treeWidgetLayers,
            &QTreeWidget::itemChanged,
            mLayerManager,
            &LayerManager::onLayerItemChanged);

    connect(ui->treeWidgetLayers,
            &QTreeWidget::customContextMenuRequested,
            mLayerManager,
            &LayerManager::showLayerContextMenu);
}

void MainWindow::connectSignals()
{
    connect(mStatusBarWidget,
            &StatusBarWidget::rotationChanged,
            this,
            &MainWindow::onRotationChanged);

    connect(mStatusBarWidget,
            &StatusBarWidget::scaleChanged,
            this,
            &MainWindow::onScaleChanged);

    connect(ui->graphicsView,
            &MapCanvas::mouseCoordinateChanged,
            this,
            &MainWindow::updateMouseCoordinate);

    connect(ui->graphicsView,
            &MapCanvas::mouseGeoCoordinateChanged,
            this,
            &MainWindow::updateGeoCoordinate);

    connect(ui->graphicsView,
            &MapCanvas::scaleChanged,
            this,
            &MainWindow::updateScale);

    connect(ui->graphicsView,
            &MapCanvas::rotationChanged,
            this,
            &MainWindow::updateRotation);

    connect(ui->graphicsView,
            &MapCanvas::extentChanged,
            this,
            &MainWindow::updateExtent);
}

// --------------------------------
// Slots
// --------------------------------

void MainWindow::onOpenClicked()
{
    QMessageBox::information(
                this,
                "Open",
                "Import Dialog will open here.");
}

void MainWindow::onRotationChanged(double angle)
{
    qDebug() << "Rotation :" << angle;

    ui->graphicsView->rotateMap(angle);
}

void MainWindow::onScaleChanged(const QString &scale)
{
    qDebug() << "Scale :" << scale;

    ui->graphicsView->setScalePreset(scale);
}

void MainWindow::updateMouseCoordinate(const QPointF &scenePos)
{
    mStatusBarWidget->setCursorPosition(
                QString("%1 , %2")
                .arg(scenePos.x(), 0, 'f', 2)
                .arg(scenePos.y(), 0, 'f', 2));
}

void MainWindow::updateExtent(const QRectF &rect)
{
    mStatusBarWidget->setViewSize(
                QString("%1 x %2")
                .arg(rect.width(), 0, 'f', 2)
                .arg(rect.height(), 0, 'f', 2));
}

void MainWindow::updateScale(double scale)
{
    QString text;

    if(scale >= 8.0)
        text = "1:1000";
    else if(scale >= 4.0)
        text = "1:2500";
    else if(scale >= 2.5)
        text = "1:5000";
    else if(scale >= 1.5)
        text = "1:10000";
    else if(scale >= 1.0)
        text = "1:25000";
    else if(scale >= 0.75)
        text = "1:50000";
    else
        text = "1:100000";

    mStatusBarWidget->setCurrentScale(text);
}

void MainWindow::updateRotation(double angle)
{
    mStatusBarWidget->setRotation(angle);
}

void MainWindow::clearLayers()
{
    ui->treeWidgetLayers->clear();
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Plus || event->key() == Qt::Key_Equal)
    {
        ui->graphicsView->zoomIn();
        event->accept();
    }
    else if (event->key() == Qt::Key_Minus)
    {
        ui->graphicsView->zoomOut();
        event->accept();
    }
    else
    {
        QMainWindow::keyPressEvent(event);
    }
}

QString MainWindow::convertToUTM(
        double lon,
        double lat)
{
    if(lat < -80.0 || lat > 84.0)
    {
        return "Outside UTM";
    }

    int zone =
            static_cast<int>(
                (lon + 180.0) / 6.0) + 1;

    OGRSpatialReference wgs84;
    wgs84.SetWellKnownGeogCS("WGS84");

    OGRSpatialReference utm;
    utm.SetUTM(zone, lat >= 0);
    utm.SetWellKnownGeogCS("WGS84");

    wgs84.SetAxisMappingStrategy(
                OAMS_TRADITIONAL_GIS_ORDER);

    utm.SetAxisMappingStrategy(
                OAMS_TRADITIONAL_GIS_ORDER);

    OGRCoordinateTransformation *poCT =
            OGRCreateCoordinateTransformation(
                &wgs84,
                &utm);
    if(!poCT)
        return "Invalid";

    double x = lon;
    double y = lat;

    bool ok =
            poCT->Transform(
                1,
                &x,
                &y);

    OCTDestroyCoordinateTransformation(
                poCT);

    if(!ok)
        return "Invalid";

    return QString("%1 , %2")
            .arg(x,0,'f',2)
            .arg(y,0,'f',2);
}

void MainWindow::updateGeoCoordinate(const QPointF &geoPos)
{
    double lon = geoPos.x();
    double lat = geoPos.y();

    mStatusBarWidget->setLonLat(QString("%1 , %2").arg(lon, 0, 'f', 6).arg(lat, 0, 'f', 6));

    QString utm = convertToUTM(lon, lat);
    mStatusBarWidget->setCursorPosition(utm);

    int zone = static_cast<int>((lon + 180.0) / 6.0) + 1;
    QString hemisphere = (lat >= 0) ? "N" : "S";
    mStatusBarWidget->setGridReference(QString("%1%2").arg(zone).arg(hemisphere));
}

void MainWindow::on_btnOpenImport_clicked()
{
    importdialog *dialog = new importdialog(mES04Processor, nullptr);
    dialog->show();
}

void MainWindow::onOpenCoregistration()

{
    co_bandstack *win = new co_bandstack(nullptr);
        win->setAttribute(Qt::WA_DeleteOnClose);
        win->setWindowTitle("Co-Registration");
        win->show();

}

void MainWindow::onOpenHyperspectral()
{
    // Keep the existing left dock (layer panel) visible -- clicking
    // Hyperspectral must not hide it.
    if (ui->layerDockWidget)
        ui->layerDockWidget->show();

    // Toggle: if already built, just show/hide it instead of rebuilding.
    if (hyperspectralDock) {
        hyperspectralDock->setVisible(!hyperspectralDock->isVisible());
        if (hyperspectralDock->isVisible())
            hyperspectralDock->raise();
        return;
    }

    // Dedicated, self-contained Hyperspectral UI (own step buttons +
    // preview), built specifically for embedding -- not a reused/scraped
    // top-level window.
    hyperspectralPanel = new HyperspectralPanel(this);

    hyperspectralDock = new QDockWidget("Hyperspectral Analytics", this);
    hyperspectralDock->setObjectName("hyperspectralDockWidget");
    hyperspectralDock->setWidget(hyperspectralPanel);
    hyperspectralDock->setMinimumWidth(320);
    hyperspectralDock->setAllowedAreas(Qt::RightDockWidgetArea | Qt::LeftDockWidgetArea);
    hyperspectralDock->setFeatures(QDockWidget::DockWidgetClosable |
                                    QDockWidget::DockWidgetMovable);

    // Match WESEE's existing dark toolbar theme (#0B4F6C header).
    hyperspectralDock->setStyleSheet(
                "QDockWidget {"
                "   color:white;"
                "   font-weight:bold;"
                "}"
                "QDockWidget::title {"
                "   background:#0B4F6C;"
                "   padding:6px;"
                "}"
                );

    addDockWidget(Qt::RightDockWidgetArea, hyperspectralDock);
    hyperspectralDock->show();
}

void MainWindow::addRasterLayerToMap(const QString &filePath)
{
    // Drops a Hyperspectral-module result (a georeferenced GeoTIFF) onto
    // the real MapCanvas: registers it in the Layers panel, ticks it,
    // draws it, and zooms to it -- same as if the user had imported the
    // file and double-clicked it themselves. (addRasterLayer() alone only
    // refreshes an *already-registered* layer's pixmap -- it never adds a
    // brand-new file to the Layers list, which is why nothing showed up.)
    if (!mLayerManager)
        return;

    mLayerManager->addAndDisplayLayer(filePath);
}


void MainWindow::onProcessingFinished(const QString &folder)
{
    qDebug() << "===== Processing Finished =====";
    qDebug() << "Folder =" << folder;

    QSettings settings;
    settings.setValue("PreProcessedFolder", folder);

    qDebug() << "Loading layers from:" << folder;

    ui->treeWidgetLayers->clear();

    mLayerManager->clearLayers();

    mLayerManager->addLayer(
                mCurrentFile);

    mLayerManager->loadFolder(folder);

    mLayerManager->watchProject(folder);
}


void MainWindow::on_btnIHSFusion_clicked()
{
    QDialog dialog(this);

    Ui::IHSFusion ui;
    ui.setupUi(&dialog);

    dialog.setWindowTitle("IHS Image Fusion");

    //----------------------------------------------------
    // Browse RED
    //----------------------------------------------------

    connect(ui.btnBrowseRED,
            &QPushButton::clicked,
            [&]()
    {
        QString file = QFileDialog::getOpenFileName(
                    &dialog,
                    "Open RED Image",
                    "",
                    "GeoTIFF (*.tif *.tiff)");

        if(!file.isEmpty())
            ui.txtRED->setText(file);
    });

    //----------------------------------------------------
    // Browse NIR
    //----------------------------------------------------

    connect(ui.btnBrowseNIR,
            &QPushButton::clicked,
            [&]()
    {
        QString file = QFileDialog::getOpenFileName(
                    &dialog,
                    "Open NIR Image",
                    "",
                    "GeoTIFF (*.tif *.tiff)");

        if(!file.isEmpty())
            ui.txtNIR->setText(file);
    });

    //----------------------------------------------------
    // Browse HH
    //----------------------------------------------------

    connect(ui.btnBrowseHH,
            &QPushButton::clicked,
            [&]()
    {
        QString file = QFileDialog::getOpenFileName(
                    &dialog,
                    "Open HH Image",
                    "",
                    "GeoTIFF (*.tif *.tiff)");

        if(!file.isEmpty())
            ui.txtHH->setText(file);
    });

    //----------------------------------------------------
    // Reset
    //----------------------------------------------------

    connect(ui.btnReset,
            &QPushButton::clicked,
            [&]()
    {
        ui.txtRED->clear();
        ui.txtNIR->clear();
        ui.txtHH->clear();

        ui.progressBar->setValue(0);
    });

    //----------------------------------------------------
    // Cancel
    //----------------------------------------------------

    connect(ui.btnCancel,
            &QPushButton::clicked,
            &dialog,
            &QDialog::reject);

    //----------------------------------------------------
    // Run
    //----------------------------------------------------

    connect(ui.btnRun,
            &QPushButton::clicked,
            [&]()
    {
        //------------------------------------------------
        // Output Folder = PreProcessed/Fusion
        //------------------------------------------------

        QSettings settings("CAIR", "OpticalFusion");

        QString workingFolder =
                settings.value("WorkingFolder").toString();

        if(workingFolder.isEmpty())
        {
            QMessageBox::warning(
                        this,
                        "Project Not Found",
                        "Please select a valid project first.");

            return;
        }

        QDir preProcessedDir(
                    workingFolder + "/PreProcessed");

        if(!preProcessedDir.exists())
        {
            preProcessedDir.mkpath(".");
        }

        QDir fusionDir(
                    workingFolder + "/PreProcessed/Fusion");

        if(!fusionDir.exists())
        {
            preProcessedDir.mkpath("Fusion");
        }

        QString outputFile =
                fusionDir.filePath("IHSFusion.tif");

        IHSFusion fusion;

        fusion.progressCallback =
                [&](int progress)
        {
            ui.progressBar->setValue(progress);
            qApp->processEvents();
        };

        //------------------------------------------------
        // Open Inputs
        //------------------------------------------------

        if(!fusion.openInputs(
                    ui.txtRED->text(),
                    ui.txtNIR->text(),
                    ui.txtHH->text()))
        {
            QMessageBox::critical(
                        &dialog,
                        "Error",
                        "Import all three bands.");

            return;
        }

        //------------------------------------------------
        // Create Output
        //------------------------------------------------

        if(!fusion.createOutput(outputFile))
        {
            QMessageBox::critical(
                        &dialog,
                        "Error",
                        "Failed to create output image.");

            return;
        }

        //------------------------------------------------
        // Compute Statistics
        //------------------------------------------------

        if(!fusion.computeGlobalStatistics())
        {
            QMessageBox::critical(
                        &dialog,
                        "Error",
                        "Failed to compute statistics.");

            return;
        }

        //Build Histogram
        if(!fusion.computeHistogramMatchingLUT())
        {
            QMessageBox::critical(&dialog, "Error", "Failed to build histogram matching LUT.");
            return;
        }

        //------------------------------------------------
        // Run Fusion
        //------------------------------------------------

        if(!fusion.processTiles())
        {
            QMessageBox::critical(
                        &dialog,
                        "Error",
                        "Fusion failed.");

            return;
        }

        fusion.close();

        QMessageBox::information(
                    &dialog,
                    "Success",
                    QString("Fusion completed successfully.\n\nOutput:\n%1")
                    .arg(outputFile));


    });

    dialog.exec();
}
