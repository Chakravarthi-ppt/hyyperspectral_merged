
QT += core gui concurrent sql

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17


TARGET = WesseUpdatedProject
TEMPLATE = app

include(common.pri)

#-------------------------------------------------
# Project Include Paths
#-------------------------------------------------

INCLUDEPATH += \
    $$PWD \
    $$PWD/Layer \
    $$PWD/MapCanvas \
    $$PWD/StatusBar \
    $$PWD/UI/MainWindow \
    $$PWD/UI/ImportDialog \
    $$PWD/Processing \
    $$PWD/Processing/E0S-04 \
    $$PWD/Processing/Sentinel2 \
    $$PWD/Hyperspectral/core/include \
    $$PWD/Hyperspectral/ui

#-------------------------------------------------
# Hyperspectral (HSI) module - merged from hsi_work
#-------------------------------------------------

SOURCES += \
    Hyperspectral/core/src/AtmosphericCorrector.cpp \
    Hyperspectral/core/src/BandSelector.cpp \
    Hyperspectral/core/src/BandStacker.cpp \
    Hyperspectral/core/src/BuiltinSignatures.cpp \
    Hyperspectral/core/src/BuiltUpClassifier.cpp \
    Hyperspectral/core/src/ChangeDetector.cpp \
    Hyperspectral/core/src/Database.cpp \
    Hyperspectral/core/src/EsunTable.cpp \
    Hyperspectral/core/src/LulcClassifier.cpp \
    Hyperspectral/core/src/Orthorectifier.cpp \
    Hyperspectral/core/src/PcaReducer.cpp \
    Hyperspectral/core/src/Pipeline.cpp \
    Hyperspectral/core/src/RadiometricCalibrator.cpp \
    Hyperspectral/core/src/RasterIO.cpp \
    Hyperspectral/core/src/RasterToVector.cpp \
    Hyperspectral/core/src/SamClassifier.cpp \
    Hyperspectral/core/src/SpectralLibrary.cpp \
    Hyperspectral/core/src/SurfaceObjectMask.cpp \
    Hyperspectral/core/src/SvmModel.cpp \
    Hyperspectral/core/src/SpectralIndices.cpp \
    Hyperspectral/core/src/LandCoverMapper.cpp \
    Hyperspectral/core/src/RxDetector.cpp \
    Hyperspectral/core/src/ThematicClusterer.cpp \
    Hyperspectral/ui/dialogs/ThematicDialog.cpp \
    Hyperspectral/ui/MainWindow.cpp \
    Hyperspectral/ui/HyperspectralPanel.cpp \
    Hyperspectral/ui/RasterPreviewWidget.cpp \
    Hyperspectral/ui/ImageComparisonWidget.cpp \
    Hyperspectral/ui/Utils.cpp \
    Hyperspectral/ui/dialogs/PreprocessingDialog.cpp \
    Hyperspectral/ui/dialogs/SurfaceObjectMaskDialog.cpp \
    Hyperspectral/ui/dialogs/PcaStackDialog.cpp \
    Hyperspectral/ui/dialogs/BuiltUpClassificationDialog.cpp \
    Hyperspectral/ui/dialogs/LulcDialog.cpp \
    Hyperspectral/ui/dialogs/ChangeDetectionDialog.cpp \
    Hyperspectral/ui/dialogs/LandCoverMapperDialog.cpp \
    Hyperspectral/ui/dialogs/AnomalyDetectorDialog.cpp

HEADERS += \
    Hyperspectral/core/include/hsi/AtmosphericCorrector.h \
    Hyperspectral/core/include/hsi/BandSelector.h \
    Hyperspectral/core/include/hsi/BandStacker.h \
    Hyperspectral/core/include/hsi/BuiltinSignatures.h \
    Hyperspectral/core/include/hsi/BuiltUpClassifier.h \
    Hyperspectral/core/include/hsi/ChangeDetector.h \
    Hyperspectral/core/include/hsi/Database.h \
    Hyperspectral/core/include/hsi/EsunTable.h \
    Hyperspectral/core/include/hsi/Hsi.h \
    Hyperspectral/core/include/hsi/Logger.h \
    Hyperspectral/core/include/hsi/LulcClassifier.h \
    Hyperspectral/core/include/hsi/Orthorectifier.h \
    Hyperspectral/core/include/hsi/PcaReducer.h \
    Hyperspectral/core/include/hsi/Pipeline.h \
    Hyperspectral/core/include/hsi/RadiometricCalibrator.h \
    Hyperspectral/core/include/hsi/RasterIO.h \
    Hyperspectral/core/include/hsi/RasterToVector.h \
    Hyperspectral/core/include/hsi/SamClassifier.h \
    Hyperspectral/core/include/hsi/SpectralLibrary.h \
    Hyperspectral/core/include/hsi/SurfaceObjectMask.h \
    Hyperspectral/core/include/hsi/SvmModel.h \
    Hyperspectral/core/include/hsi/SpectralIndices.h \
    Hyperspectral/core/include/hsi/LandCoverMapper.h \
    Hyperspectral/core/include/hsi/RxDetector.h \
    Hyperspectral/core/include/hsi/Types.h \
    Hyperspectral/core/src/ThematicClusterer.h \
    Hyperspectral/ui/dialogs/DialogStyle.h \
    Hyperspectral/ui/dialogs/ThematicDialog.h \
    Hyperspectral/ui/MainWindow.h \
    Hyperspectral/ui/HyperspectralPanel.h \
    Hyperspectral/ui/PipelineWorker.h \
    Hyperspectral/ui/ProgressDialog.h \
    Hyperspectral/ui/RasterPreviewWidget.h \
    Hyperspectral/ui/ImageComparisonWidget.h \
    Hyperspectral/ui/Utils.h \
    Hyperspectral/ui/dialogs/PreprocessingDialog.h \
    Hyperspectral/ui/dialogs/SurfaceObjectMaskDialog.h \
    Hyperspectral/ui/dialogs/PcaStackDialog.h \
    Hyperspectral/ui/dialogs/BuiltUpClassificationDialog.h \
    Hyperspectral/ui/dialogs/LulcDialog.h \
    Hyperspectral/ui/dialogs/ChangeDetectionDialog.h \
    Hyperspectral/ui/dialogs/LandCoverMapperDialog.h \
    Hyperspectral/ui/dialogs/AnomalyDetectorDialog.h

unix:!macx {
    # --- GDAL ---
    GDAL_CFLAGS = $$system(gdal-config --cflags 2>/dev/null)
    isEmpty(GDAL_CFLAGS) {
        INCLUDEPATH += /usr/include/gdal
        LIBS += -lgdal
    } else {
        QMAKE_CXXFLAGS += $$GDAL_CFLAGS
        LIBS += $$system(gdal-config --libs)
    }

    # --- OpenCV ---
    OPENCV_HAS_PC = $$system(pkg-config --exists opencv4 2>/dev/null && echo yes)
    isEmpty(OPENCV_HAS_PC) {
        INCLUDEPATH += /usr/include/opencv4
        LIBS += -lopencv_core -lopencv_ml
    } else {
        QMAKE_CXXFLAGS += $$system(pkg-config --cflags opencv4)
        LIBS += $$system(pkg-config --libs opencv4)
    }

    # --- Eigen3 (header-only) ---
    EIGEN_CFLAGS = $$system(pkg-config --cflags eigen3 2>/dev/null)
    isEmpty(EIGEN_CFLAGS) {
        INCLUDEPATH += /usr/include/eigen3
    } else {
        QMAKE_CXXFLAGS += $$EIGEN_CFLAGS
    }

    # --- PostgreSQL (libpq), used by hsi::Database ---
    PG_CONFIG_BIN = $$system(which pg_config 2>/dev/null)
    isEmpty(PG_CONFIG_BIN) {
        INCLUDEPATH += /usr/local/pgsql/include
        LIBS += -L/usr/local/pgsql/lib -lpq
    } else {
        INCLUDEPATH += $$system(pg_config --includedir)
        LIBS += -L$$system(pg_config --libdir) -lpq
    }

    LIBS += -lpthread -lstdc++fs
}

#-------------------------------------------------
# Sources
#-------------------------------------------------


SOURCES += \
    Common/threadmanager.cpp \
    Fusion/IHSFusion.cpp \
    Layer/layermanager.cpp \
    Layer/rgbbanddialog.cpp \
    MapCanvas/northarrow.cpp \
    Processing/Manager/preprocessingmanager.cpp \
    Processing/Sentinel2/sentinel2processor.cpp \
    TilesFile/tileprocessor.cpp \
    UI/SuccessDialog/successdialog.cpp \
    UI/co_bandstack/co_bandstack.cpp \
    database/rastermanager.cpp \
    main.cpp \
    UI/MainWindow/mainwindow.cpp \
    UI/ImportDialog/importdialog.cpp \
    MapCanvas/mapcanvas.cpp \
    StatusBar/statusbarwidget.cpp \
    Layer/layerproperties.cpp \
    Processing/E0S-04/es04processor.cpp

#-------------------------------------------------
# Headers
#-------------------------------------------------

HEADERS += \
    Common/processorbase.h \
    Common/threadmanager.h \
    Fusion/IHSFusion.h \
    Layer/layermanager.h \
    Layer/rgbbanddialog.h \
    MapCanvas/northarrow.h \
    Processing/Manager/preprocessingmanager.h \
    Processing/Sentinel2/sentinel2processor.h \
    TilesFile/tileprocessor.h \
    UI/MainWindow/mainwindow.h \
    UI/ImportDialog/importdialog.h \
    MapCanvas/mapcanvas.h \
    StatusBar/statusbarwidget.h \
    Layer/layerproperties.h \
    Processing/E0S-04/es04processor.h \
    UI/SuccessDialog/successdialog.h \
    UI/co_bandstack/co_bandstack.h \
    database/rastermanager.h

#-------------------------------------------------
# Forms
#-------------------------------------------------

FORMS += \
    Fusion/IHSFusion.ui \
    Layer/rgbbanddialog.ui \
    UI/MainWindow/mainwindow.ui \
    UI/ImportDialog/importdialog.ui \
    StatusBar/statusbarwidget.ui \
    Layer/layerproperties.ui \
    UI/SuccessDialog/successdialog.ui \
    UI/co_bandstack/co_bandstack.ui

RESOURCES += \
    resource.qrc

DISTFILES += \
    Data/world.tif \

    OBJECTS_DIR = $$PWD/build/obj
    MOC_DIR     = $$PWD/build/moc
    RCC_DIR     = $$PWD/build/rcc
    UI_DIR      = $$PWD/build/ui
