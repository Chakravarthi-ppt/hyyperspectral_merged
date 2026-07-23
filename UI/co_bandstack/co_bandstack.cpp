

/////////////////////modified//////////////////////////

#include "co_bandstack.h"

#include "ui_co_bandstack.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QDebug>
#include <QApplication>
#include <QFileInfo>
#include <cstdint>
#include <QSettings>
#include <QDir>

#include <opencv2/opencv.hpp>
#include <opencv2/features2d.hpp>
#include <opencv2/calib3d.hpp>

#include "gdal_priv.h"
#include "cpl_conv.h"
#include "gdal_utils.h"
#include <cmath>
#include <vector>

// ---------------------------------------------------------------------------
// UI validation helpers
// ---------------------------------------------------------------------------
static bool isValidMasterFile(const QString &filePath)
{
    QFileInfo info(filePath);
    QString ext = info.suffix().toLower();
    if (ext != "tif" && ext != "tiff")
        return false;
    QString name = info.fileName().toUpper();
    return name.contains("HH") || name.contains("HV") ||
           name.contains("VV") || name.contains("VH");
}

static bool isValidSlaveFile(const QString &filePath)
{
    QFileInfo info(filePath);
    QString ext = info.suffix().toLower();
    if (ext != "tif" && ext != "tiff")
        return false;
    QString name = info.fileName().toUpper();
    return name.contains("B2") || name.contains("B3") ||
           name.contains("B4") || name.contains("B8");
}

// ---------------------------------------------------------------------------
// Helper to generate synthetic GCPs (corners + centre) – used as fallback
// ---------------------------------------------------------------------------
static bool generateSyntheticGCPs(const double masterGT[6], const double slaveGT[6],
                                  int imgCols, int imgRows,
                                  QVector<GDAL_GCP> &gcps, double &rms)
{
    gcps.clear();
    double pixelPts[5][2] = {
        {0, 0},
        {double(imgCols), 0},
        {0, double(imgRows)},
        {double(imgCols), double(imgRows)},
        {double(imgCols)/2, double(imgRows)/2}
    };
    for (int i = 0; i < 5; ++i) {
        double wx, wy;
        // Convert master pixel to world
        wx = masterGT[0] + pixelPts[i][0] * masterGT[1] + pixelPts[i][1] * masterGT[2];
        wy = masterGT[3] + pixelPts[i][0] * masterGT[4] + pixelPts[i][1] * masterGT[5];

        // Inverse of slaveGT to find slave pixel
        double a = slaveGT[1], b = slaveGT[2];
        double d = slaveGT[4], e = slaveGT[5];
        double det = a * e - b * d;
        if (fabs(det) < 1e-12) continue;
        double dx = wx - slaveGT[0];
        double dy = wy - slaveGT[3];
        double slavePx = (dx * e - dy * b) / det;
        double slavePy = (dy * a - dx * d) / det;

        GDAL_GCP gcp;
        GDALInitGCPs(1, &gcp);
        gcp.dfGCPPixel = slavePx;
        gcp.dfGCPLine  = slavePy;
        gcp.dfGCPX     = wx;
        gcp.dfGCPY     = wy;
        gcp.dfGCPZ     = 0.0;
        gcps.push_back(gcp);
    }
    rms = 0.0;
    return true;
}

// ---------------------------------------------------------------------------
co_bandstack::co_bandstack(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::co_bandstack)
{
    ui->setupUi(this);
    setWindowTitle("Co-Registration");
    GDALAllRegister();
}

co_bandstack::~co_bandstack()
{
    delete ui;
}

// ---------------------------------------------------------------------------
void co_bandstack::reportProgress(int percent, const QString &message)
{
    ui->progressBar->setValue(percent);
//    qDebug().noquote() << QString("[%1%] %2").arg(percent, 3).arg(message);
    QApplication::processEvents();
}

// ---------------------------------------------------------------------------
void co_bandstack::on_master_clicked()
{
    QStringList files = QFileDialog::getOpenFileNames(
        this, "Select Master (SAR) Band(s)", QString(),
        "GeoTIFF (*.tif *.tiff);;All Files (*)");

    if (files.isEmpty())
        return;

    for (const QString &f : files) {
        if (!isValidMasterFile(f)) {
            QMessageBox::warning(this, "Invalid Selection",
                "Only SAR files with names containing HH, HV, VV, or VH "
                "and extension .tif/.tiff are allowed.\nSelection rejected.");
            return;
        }
    }

    m_masterBandPaths = files;
    QStringList names;
    for (const QString &f : files)
        names << QFileInfo(f).fileName();
    ui->lineEdit->setText(names.join(", "));
}

// ---------------------------------------------------------------------------
void co_bandstack::on_slave_clicked()
{
    QStringList files = QFileDialog::getOpenFileNames(
        this, "Select Slave (Optical) Band(s)", QString(),
        "GeoTIFF (*.tif *.tiff);;All Files (*)");

    if (files.isEmpty())
        return;

    // No validation – accept any file
    m_slaveBandPaths = files;

    QStringList names;
    for (const QString &f : files)
        names << QFileInfo(f).fileName();
    ui->lineEditInputImage->setText(names.join(", "));
}

// ---------------------------------------------------------------------------
void co_bandstack::on_reset_clicked()
{
    m_masterBandPaths.clear();
    m_slaveBandPaths.clear();
    ui->lineEdit->clear();
    ui->lineEditInputImage->clear();
    ui->progressBar->setValue(0);
}

void co_bandstack::on_cancel_clicked()
{
    close();
}

// ---------------------------------------------------------------------------
void co_bandstack::on_Run_clicked()
{
    if (m_masterBandPaths.isEmpty() || m_slaveBandPaths.isEmpty()) {
        QMessageBox::warning(this, "Missing input",
            "Please select at least one master band and one slave band.");
        return;
    }

//    QString outPath = QFileDialog::getSaveFileName(this, "Save Stacked Output (base name)",
//                                                     "", "GeoTIFF (*.tif)");
//    if (outPath.isEmpty())
//        return;
    QSettings settings;

    // Read PreProcessedFolder from settings
    QString preprocessFolder =
            settings.value("PreProcessedFolder").toString();

    if (preprocessFolder.isEmpty())
    {
        QMessageBox::warning(this,
                             "Error",
                             "PreProcessedFolder is not set.");
        return;
    }
    QDir dir(preprocessFolder);

    if (!dir.exists("Co-Registration"))
    {
        dir.mkdir("Co-Registration");
    }

    // Base output path
    QString outPath =
            dir.filePath("Co-Registration/CoRegistration.tif");

    ui->progressBar->setValue(0);
    PipelineResult result = runPipeline(m_masterBandPaths, m_slaveBandPaths, outPath);

    if (!result.success) {
        QMessageBox::critical(this, "Pipeline failed", result.errorMessage);
        return;
    }

    QString summary = QString(
        "Bands Stacked : %1 (%2 Master + %3 Optical)\n\n"
//        "RMS Error     : %4 pixels\n\n"
        "Output File   :\n%5"
    )
    .arg(result.numBandsStacked)
    .arg(m_masterBandPaths.size())
    .arg(m_slaveBandPaths.size())
//    .arg(result.rmsError, 0, 'f', 4)
    .arg(result.stackedOutputPath);
    QMessageBox::information(this, "Band Stacking Complete", summary);
}

// ---------------------------------------------------------------------------
bool co_bandstack::readRasterInfo(const QString &path, QString &projWkt, double gt[6],
                                   int &xSize, int &ySize, QString &errMsg)
{
    GDALDatasetH ds = GDALOpen(path.toUtf8().constData(), GA_ReadOnly);
    if (!ds) {
        errMsg = "Could not open raster: " + path;
        return false;
    }
    const char *proj = GDALGetProjectionRef(ds);
    projWkt = QString::fromUtf8(proj);
    if (GDALGetGeoTransform(ds, gt) != CE_None) {
        errMsg = "Could not read geotransform for: " + path;
        GDALClose(ds);
        return false;
    }
    xSize = GDALGetRasterXSize(ds);
    ySize = GDALGetRasterYSize(ds);
    GDALClose(ds);
    return true;
}

// ---------------------------------------------------------------------------
bool co_bandstack::sameCRS(const QString &wktA, const QString &wktB)
{
    OGRSpatialReferenceH srsA = OSRNewSpatialReference(nullptr);
    OGRSpatialReferenceH srsB = OSRNewSpatialReference(nullptr);
    QByteArray wktABytes = wktA.toUtf8();
    QByteArray wktBBytes = wktB.toUtf8();
    char *wktAPtr = wktABytes.data();
    char *wktBPtr = wktBBytes.data();
    OSRImportFromWkt(srsA, &wktAPtr);
    OSRImportFromWkt(srsB, &wktBPtr);
    bool result = OSRIsSame(srsA, srsB) != 0;
    OSRDestroySpatialReference(srsA);
    OSRDestroySpatialReference(srsB);
    return result;
}

// ---------------------------------------------------------------------------
void co_bandstack::pixelSize(const double gt[6], double &xRes, double &yRes)
{
    xRes = std::fabs(gt[1]);
    yRes = std::fabs(gt[5]);
}

// ---------------------------------------------------------------------------
bool co_bandstack::reprojectToMatch(const QString &srcPath, const QString &dstSRS,
                                     const QString &outPath, QString &errMsg)
{
    GDALDatasetH srcDS = GDALOpen(srcPath.toUtf8().constData(), GA_ReadOnly);
    if (!srcDS) {
        errMsg = "Could not open source for reprojection: " + srcPath;
        return false;
    }
    QByteArray dstSRSBytes = dstSRS.toUtf8();
    std::vector<char *> args;
    args.push_back(const_cast<char *>("-t_srs"));
    args.push_back(dstSRSBytes.data());
    args.push_back(const_cast<char *>("-r"));
    args.push_back(const_cast<char *>("near"));
    args.push_back(nullptr);
    GDALWarpAppOptions *warpOpts = GDALWarpAppOptionsNew(args.data(), nullptr);
    GDALDatasetH datasets[1] = { srcDS };
    int usageErr = 0;
    GDALDatasetH outDS = GDALWarp(outPath.toUtf8().constData(), nullptr, 1,
                                   datasets, warpOpts, &usageErr);
    GDALWarpAppOptionsFree(warpOpts);
    GDALClose(srcDS);
    if (!outDS || usageErr != 0) {
        errMsg = "Reprojection failed for: " + srcPath;
        return false;
    }
    GDALClose(outDS);
    return true;
}

// ---------------------------------------------------------------------------
bool co_bandstack::resampleToResolution(const QString &srcPath, double xRes, double yRes,
                                         const QString &outPath, QString &errMsg)
{
    GDALDatasetH srcDS = GDALOpen(srcPath.toUtf8().constData(), GA_ReadOnly);
    if (!srcDS) {
        errMsg = "Could not open source for resampling: " + srcPath;
        return false;
    }
    QString xResStr = QString::number(xRes, 'f', 10);
    QString yResStr = QString::number(yRes, 'f', 10);
    QByteArray xResBytes = xResStr.toUtf8();
    QByteArray yResBytes = yResStr.toUtf8();
    std::vector<char *> args;
    args.push_back(const_cast<char *>("-tr"));
    args.push_back(xResBytes.data());
    args.push_back(yResBytes.data());
    args.push_back(const_cast<char *>("-r"));
    args.push_back(const_cast<char *>("near"));
    args.push_back(nullptr);
    GDALWarpAppOptions *warpOpts = GDALWarpAppOptionsNew(args.data(), nullptr);
    GDALDatasetH datasets[1] = { srcDS };
    int usageErr = 0;
    GDALDatasetH outDS = GDALWarp(outPath.toUtf8().constData(), nullptr, 1,
                                   datasets, warpOpts, &usageErr);
    GDALWarpAppOptionsFree(warpOpts);
    GDALClose(srcDS);
    if (!outDS || usageErr != 0) {
        errMsg = "Resampling failed for: " + srcPath;
        return false;
    }
    GDALClose(outDS);
    return true;
}


bool co_bandstack::findGCPs(const QString &masterPath, const QString &slavePath,
                            const double masterGT[6], const double slaveGT[6],
                            QVector<GDAL_GCP> &gcps, double &rms, QString &errMsg,
                            int maxDim)
{
    if (maxDim <= 0) maxDim = 1500;

    // Helper: read a GDAL band, normalize to 8‑bit, optionally equalize, and resize
    auto readAndResize = [&](const QString &path, cv::Mat &outImg,
                              int &origW, int &origH, double &scale,
                              bool equalize = false) -> bool {
        GDALDatasetH ds = GDALOpen(path.toUtf8().constData(), GA_ReadOnly);
        if (!ds) {
            errMsg = "Cannot open: " + path;
            return false;
        }
        int xSize = GDALGetRasterXSize(ds);
        int ySize = GDALGetRasterYSize(ds);
        origW = xSize; origH = ySize;
        std::vector<float> data(xSize * ySize);
        GDALRasterBandH band = GDALGetRasterBand(ds, 1);
        if (GDALRasterIO(band, GF_Read, 0, 0, xSize, ySize,
                         data.data(), xSize, ySize, GDT_Float32, 0, 0) != CE_None) {
            errMsg = "Failed to read raster data from: " + path;
            GDALClose(ds);
            return false;
        }
        GDALClose(ds);

        float minVal = *std::min_element(data.begin(), data.end());
        float maxVal = *std::max_element(data.begin(), data.end());
        cv::Mat floatMat(ySize, xSize, CV_32FC1, data.data());
        cv::Mat normFloat;
        if (maxVal - minVal < 1e-6) {
            outImg = cv::Mat(ySize, xSize, CV_8UC1, cv::Scalar(128));
        } else {
            cv::normalize(floatMat, normFloat, 0, 255, cv::NORM_MINMAX);
            normFloat.convertTo(outImg, CV_8UC1);
        }

        // Optional histogram equalisation (helps low‑contrast bands like B8)
        if (equalize) {
            cv::equalizeHist(outImg, outImg);
        }

        // Downsample if the largest dimension exceeds maxDim
        scale = 1.0;
        int maxSize = std::max(xSize, ySize);
        if (maxSize > maxDim) {
            scale = double(maxDim) / maxSize;
            cv::Mat resized;
            cv::resize(outImg, resized, cv::Size(), scale, scale, cv::INTER_AREA);
            outImg = resized;
        }
        return true;
    };

    cv::Mat masterImg, slaveImg;
    int masterW, masterH, slaveW, slaveH;
    double masterScale, slaveScale;
    // Equalize only the slave (optical) image to boost contrast
    if (!readAndResize(masterPath, masterImg, masterW, masterH, masterScale, false) ||
        !readAndResize(slavePath, slaveImg, slaveW, slaveH, slaveScale, true))
        return false;

    // SIFT with 3000 features (more keypoints for sub‑pixel accuracy)
    cv::Ptr<cv::SIFT> detector = cv::SIFT::create(3000, 3, 0.01, 10, 1.6);
    std::vector<cv::KeyPoint> keypoints1, keypoints2;
    cv::Mat descriptors1, descriptors2;
    detector->detectAndCompute(masterImg, cv::noArray(), keypoints1, descriptors1);
    detector->detectAndCompute(slaveImg, cv::noArray(), keypoints2, descriptors2);

    if (keypoints1.empty() || keypoints2.empty()) {
//        qDebug() << "WARNING: No SIFT features – falling back to synthetic GCPs (RMS=0).";
        return generateSyntheticGCPs(masterGT, slaveGT, masterW, masterH, gcps, rms);
    }

    // FLANN matcher
    cv::FlannBasedMatcher matcher;
    std::vector<cv::DMatch> matches;
    matcher.match(descriptors1, descriptors2, matches);

    double minDist = std::min_element(matches.begin(), matches.end(),
                                      [](const cv::DMatch &a, const cv::DMatch &b) {
                                          return a.distance < b.distance;
                                      })->distance;
    std::vector<cv::DMatch> goodMatches;
    for (const auto &m : matches) {
        if (m.distance < 3.0 * minDist)
            goodMatches.push_back(m);
    }

    if (goodMatches.size() < 4) {
//        qDebug() << "WARNING: SIFT found only" << goodMatches.size()
//                 << "good matches – falling back to synthetic GCPs (RMS will be 0).";
        return generateSyntheticGCPs(masterGT, slaveGT, masterW, masterH, gcps, rms);
    }

    std::vector<cv::Point2f> pts1, pts2;
    for (const auto &m : goodMatches) {
        pts1.push_back(keypoints1[m.queryIdx].pt);
        pts2.push_back(keypoints2[m.trainIdx].pt);
    }

    // RANSAC with threshold 1.0 (tighter for sub‑pixel)
    std::vector<uchar> inliers;
    cv::Mat affine = cv::estimateAffinePartial2D(pts2, pts1, inliers,
                                                  cv::RANSAC, 0.5, 3000);
    if (affine.empty()) {
//        qDebug() << "WARNING: RANSAC failed – falling back to synthetic GCPs.";
        return generateSyntheticGCPs(masterGT, slaveGT, masterW, masterH, gcps, rms);
    }

    gcps.clear();
    gcps.reserve(inliers.size());
    double sumSq = 0.0;
    int validCount = 0;

    for (size_t i = 0; i < inliers.size(); ++i) {
        if (!inliers[i]) continue;
        // Coordinates in resized image space
        cv::Point2f masterPxResized = pts1[i];
        cv::Point2f slavePxResized  = pts2[i];

        // Scale back to original pixel space
        double masterPx = masterPxResized.x / masterScale;
        double masterPy = masterPxResized.y / masterScale;
        double slavePx  = slavePxResized.x / slaveScale;
        double slavePy  = slavePxResized.y / slaveScale;

        // World coordinates from master pixel
        double wx = masterGT[0] + masterPx * masterGT[1] + masterPy * masterGT[2];
        double wy = masterGT[3] + masterPx * masterGT[4] + masterPy * masterGT[5];

        GDAL_GCP gcp;
        GDALInitGCPs(1, &gcp);
        gcp.dfGCPPixel = slavePx;
        gcp.dfGCPLine  = slavePy;
        gcp.dfGCPX     = wx;
        gcp.dfGCPY     = wy;
        gcp.dfGCPZ     = 0.0;
        gcps.push_back(gcp);

        // Compute residual in original pixel space (using the affine on resized coords)
        cv::Mat predictedResized = affine * cv::Mat(cv::Vec3d(slavePxResized.x, slavePxResized.y, 1.0));
        double predXResized = predictedResized.at<double>(0);
        double predYResized = predictedResized.at<double>(1);
        double predX = predXResized / masterScale;
        double predY = predYResized / masterScale;
        double dx = predX - masterPx;
        double dy = predY - masterPy;
        sumSq += dx*dx + dy*dy;
        validCount++;
    }

    if (validCount < 2) {
//        qDebug() << "WARNING: RANSAC gave too few inliers – falling back to synthetic GCPs.";
        return generateSyntheticGCPs(masterGT, slaveGT, masterW, masterH, gcps, rms);
    }

    rms = std::sqrt(sumSq / validCount);
    return true;
}

// ---------------------------------------------------------------------------
// Warp a single band using the given GCPs (order 1 = affine).
// ---------------------------------------------------------------------------
bool co_bandstack::warpBandWithGCPs(const QString &srcPath, const QVector<GDAL_GCP> &gcps,
                                     const QString &dstSRSWkt, int xSize, int ySize,
                                     const double masterGT[6], const QString &outPath,
                                     QString &errMsg)
{
    GDALDatasetH srcDS = GDALOpen(srcPath.toUtf8().constData(), GA_ReadOnly);
    if (!srcDS) {
        errMsg = "Could not open source for warping: " + srcPath;
        return false;
    }
    GDALDriverH memDriver = GDALGetDriverByName("MEM");
    GDALDatasetH memDS = GDALCreateCopy(memDriver, "", srcDS, FALSE, nullptr, nullptr, nullptr);
    GDALSetGCPs(memDS, gcps.size(), gcps.data(), dstSRSWkt.toUtf8().constData());

    double minX = masterGT[0];
    double maxY = masterGT[3];
    double maxX = masterGT[0] + xSize * masterGT[1];
    double minY = masterGT[3] + ySize * masterGT[5];

    QString minXStr = QString::number(minX, 'f', 10);
    QString minYStr = QString::number(minY, 'f', 10);
    QString maxXStr = QString::number(maxX, 'f', 10);
    QString maxYStr = QString::number(maxY, 'f', 10);
    QString widthStr = QString::number(xSize);
    QString heightStr = QString::number(ySize);
    QByteArray minXBytes = minXStr.toUtf8();
    QByteArray minYBytes = minYStr.toUtf8();
    QByteArray maxXBytes = maxXStr.toUtf8();
    QByteArray maxYBytes = maxYStr.toUtf8();
    QByteArray widthBytes = widthStr.toUtf8();
    QByteArray heightBytes = heightStr.toUtf8();
    QByteArray dstSRSBytes = dstSRSWkt.toUtf8();

    std::vector<char *> args;
    args.push_back(const_cast<char *>("-t_srs"));
    args.push_back(dstSRSBytes.data());
    args.push_back(const_cast<char *>("-order"));
    args.push_back(const_cast<char *>("1"));
    args.push_back(const_cast<char *>("-te"));
    args.push_back(minXBytes.data());
    args.push_back(minYBytes.data());
    args.push_back(maxXBytes.data());
    args.push_back(maxYBytes.data());
    args.push_back(const_cast<char *>("-ts"));
    args.push_back(widthBytes.data());
    args.push_back(heightBytes.data());
    args.push_back(const_cast<char *>("-r"));
    args.push_back(const_cast<char *>("near"));
    args.push_back(nullptr);

    GDALWarpAppOptions *warpOpts = GDALWarpAppOptionsNew(args.data(), nullptr);
    GDALDatasetH datasets[1] = { memDS };
    int usageErr = 0;
    GDALDatasetH outDS = GDALWarp(outPath.toUtf8().constData(), nullptr, 1,
                                   datasets, warpOpts, &usageErr);
    GDALWarpAppOptionsFree(warpOpts);
    GDALClose(memDS);
    GDALClose(srcDS);
    if (!outDS || usageErr != 0) {
        errMsg = "Warp failed for: " + srcPath;
        return false;
    }
    GDALClose(outDS);
    return true;
}

// ---------------------------------------------------------------------------
// Write cropped bands with given data type
// ---------------------------------------------------------------------------
bool co_bandstack::writeCroppedBands(const std::vector<std::vector<float>> &bandData,
                                     const double gt[6], const QString &proj,
                                     const QString &outPath,
                                     GDALDataType dataType,
                                     double noDataValue,
                                     int cropXSize, int cropYSize,
                                     QString &errMsg)
{
    int numBands = bandData.size();
    if (numBands == 0) {
        errMsg = "No bands to write.";
        return false;
    }

    GDALDriverH driver = GDALGetDriverByName("GTiff");
    GDALDatasetH outDS = GDALCreate(driver, outPath.toUtf8().constData(),
                                     cropXSize, cropYSize, numBands,
                                     dataType, nullptr);
    if (!outDS) {
        errMsg = "Could not create output: " + outPath;
        return false;
    }

    GDALSetGeoTransform(outDS, const_cast<double*>(gt));
    GDALSetProjection(outDS, proj.toUtf8().constData());

    for (int i = 0; i < numBands; ++i) {
        GDALRasterBandH dstBand = GDALGetRasterBand(outDS, i + 1);
        if (dataType == GDT_UInt16) {
            std::vector<uint16_t> uintData(cropXSize * cropYSize);
            const std::vector<float> &src = bandData[i];
            for (size_t j = 0; j < uintData.size(); ++j) {
                float val = src[j];
                if (val == 0.0f) {
                    uintData[j] = static_cast<uint16_t>(noDataValue);
                } else {
                    if (val < 0) val = 0;
                    else if (val > 65535) val = 65535;
                    uintData[j] = static_cast<uint16_t>(val);
                }
            }
            GDALRasterIO(dstBand, GF_Write, 0, 0, cropXSize, cropYSize,
                         static_cast<void*>(uintData.data()), cropXSize, cropYSize,
                         dataType, 0, 0);
        } else {
            GDALRasterIO(dstBand, GF_Write, 0, 0, cropXSize, cropYSize,
                         const_cast<float*>(bandData[i].data()), cropXSize, cropYSize,
                         dataType, 0, 0);
        }
        GDALSetRasterNoDataValue(dstBand, noDataValue);
    }

    GDALClose(outDS);
    return true;
}


PipelineResult co_bandstack::runPipeline(const QStringList &masterBands,
                                          const QStringList &slaveBands,
                                          const QString &outPath)
{
    PipelineResult result;
    QString errMsg;

    if (masterBands.isEmpty() || slaveBands.isEmpty()) {
        result.errorMessage = "At least one master band and one slave band are required.";
        return result;
    }

    // 1. Read master reference
    reportProgress(5, "Reading master reference...");
    QString masterProj;
    double masterGT[6];
    int mx, my;
    if (!readRasterInfo(masterBands.first(), masterProj, masterGT, mx, my, errMsg)) {
        result.errorMessage = errMsg;
        return result;
    }
    for (int i = 1; i < masterBands.size(); ++i) {
        QString p; double g[6]; int x, y;
        if (!readRasterInfo(masterBands[i], p, g, x, y, errMsg)) {
            result.errorMessage = errMsg;
            return result;
        }
        if (x != mx || y != my) {
            result.errorMessage = "Master band size mismatch.";
            return result;
        }
    }

    // 2. Read slave info
    reportProgress(10, "Reading slave info...");
    QString slaveProj;
    double slaveGT[6];
    int sx, sy;
    if (!readRasterInfo(slaveBands.first(), slaveProj, slaveGT, sx, sy, errMsg)) {
        result.errorMessage = errMsg;
        return result;
    }

    // 3. CRS check
    reportProgress(20, "Checking CRS...");
    QStringList workingSlavePaths = slaveBands;
    if (!sameCRS(masterProj, slaveProj)) {
        reportProgress(30, "Reprojecting slave...");
        QStringList reprojected;
        for (const QString &p : workingSlavePaths) {
            QString outP = QString(p).replace(".tif", "_reproj.tif");
            if (!reprojectToMatch(p, masterProj, outP, errMsg)) {
                result.errorMessage = errMsg;
                return result;
            }
            reprojected << outP;
        }
        workingSlavePaths = reprojected;
    }

    // 4. Resolution check
    reportProgress(50, "Checking pixel size...");
    double masterXRes, masterYRes;
    pixelSize(masterGT, masterXRes, masterYRes);
    double sampleGT[6];
    int dummyX, dummyY;
    if (!readRasterInfo(workingSlavePaths.first(), slaveProj, sampleGT, dummyX, dummyY, errMsg)) {
        result.errorMessage = errMsg;
        return result;
    }
    double slaveXRes, slaveYRes;
    pixelSize(sampleGT, slaveXRes, slaveYRes);
    if (!qFuzzyCompare(masterXRes, slaveXRes) || !qFuzzyCompare(masterYRes, slaveYRes)) {
        reportProgress(75, "Resampling slave...");
        QStringList resampled;
        for (const QString &p : workingSlavePaths) {
//            QString outP = QString(p).replace(".tif", "_resampled.tif");
//            if (!resampleToResolution(p, masterXRes, masterYRes, outP, errMsg)) {
//                result.errorMessage = errMsg;
//                return result;
//            }
//            resampled << outP;
        }
//        workingSlavePaths = resampled;
    }

    // 5. Warp each slave band (using larger maxDim=1500 for sub‑pixel accuracy)
//    reportProgress(55, "Finding tie points with SIFT+RANSAC (sub‑pixel mode)...");
    QStringList warpedPaths;

    // --- MODIFIED: Accumulate RMS and GCP count over all slave bands ---
    double totalRms = 0.0;
    int totalGcpCount = 0;
    int numSlaveBands = workingSlavePaths.size();

    for (int idx = 0; idx < numSlaveBands; ++idx) {
        const QString &slavePath = workingSlavePaths[idx];
        QString slaveProjTmp;
        double slaveGTtmp[6];
        int sxTmp, syTmp;
        if (!readRasterInfo(slavePath, slaveProjTmp, slaveGTtmp, sxTmp, syTmp, errMsg)) {
            result.errorMessage = errMsg;
            return result;
        }
        QVector<GDAL_GCP> gcps;
        double rms;
        // Use maxDim = 1500 for better accuracy
        if (!findGCPs(masterBands.first(), slavePath, masterGT, slaveGTtmp, gcps, rms, errMsg, 1500)) {
            result.errorMessage = errMsg;
            return result;
        }
        totalRms += rms;
        totalGcpCount += gcps.size();

        QString memPath = "/vsimem/temp_warp_" + QString::number(idx) + ".tif";

        if (!warpBandWithGCPs(slavePath, gcps, masterProj, mx, my, masterGT, memPath, errMsg)) {
            result.errorMessage = errMsg;
            return result;
        }
        warpedPaths << memPath;   // we'll read from this virtual file later
    }

    // Compute average RMS over all slave bands
    double avgRms = totalRms / numSlaveBands;
    result.rmsError = avgRms;
    result.numGCPs = totalGcpCount;   // total across all slave bands

    // 6. Read all bands, compute common overlap, crop and write three outputs
//    reportProgress(85);
    QStringList allBandPaths = masterBands;
    allBandPaths << warpedPaths;
    int fullXSize = mx, fullYSize = my;
    const size_t nPixels = size_t(fullXSize) * size_t(fullYSize);
    const double OUT_NODATA = 0.0;

    std::vector<std::vector<float>> allData(allBandPaths.size());
    std::vector<double> allNoData(allBandPaths.size());

    for (int i = 0; i < allBandPaths.size(); ++i) {
        GDALDatasetH ds = GDALOpen(allBandPaths[i].toUtf8().constData(), GA_ReadOnly);
        if (!ds) {
            result.errorMessage = "Could not open: " + allBandPaths[i];
            return result;
        }
        int bx = GDALGetRasterXSize(ds);
        int by = GDALGetRasterYSize(ds);
        if (bx != fullXSize || by != fullYSize) {
            GDALClose(ds);
            result.errorMessage = "Size mismatch in " + allBandPaths[i];
            return result;
        }
        GDALRasterBandH band = GDALGetRasterBand(ds, 1);
        int hasNoData = 0;
        double nd = GDALGetRasterNoDataValue(band, &hasNoData);
        allNoData[i] = hasNoData ? nd : 0.0;
        allData[i].resize(nPixels);
        GDALRasterIO(band, GF_Read, 0, 0, fullXSize, fullYSize,
                     static_cast<void*>(allData[i].data()), fullXSize, fullYSize,
                     GDT_Float32, 0, 0);
        GDALClose(ds);
    }

    // Mask
    std::vector<bool> valid(nPixels, true);
    for (int i = 0; i < allBandPaths.size(); ++i) {
        const double nd = allNoData[i];
        const std::vector<float> &data = allData[i];
        for (size_t p = 0; p < nPixels; ++p) {
            if (valid[p] && double(data[p]) == nd)
                valid[p] = false;
        }
    }

    // Bounding box
    int minRow = fullYSize, maxRow = -1, minCol = fullXSize, maxCol = -1;
    for (int row = 0; row < fullYSize; ++row) {
        for (int col = 0; col < fullXSize; ++col) {
            if (valid[size_t(row) * fullXSize + col]) {
                if (row < minRow) minRow = row;
                if (row > maxRow) maxRow = row;
                if (col < minCol) minCol = col;
                if (col > maxCol) maxCol = col;
            }
        }
    }
    if (minRow > maxRow || minCol > maxCol) {
        result.errorMessage = "No valid overlapping pixels found.";
        return result;
    }

    int cropXSize = maxCol - minCol + 1;
    int cropYSize = maxRow - minRow + 1;
    size_t cropPixels = size_t(cropXSize) * size_t(cropYSize);

    double cropGT[6];
    cropGT[0] = masterGT[0] + minCol * masterGT[1] + minRow * masterGT[2];
    cropGT[1] = masterGT[1];
    cropGT[2] = masterGT[2];
    cropGT[3] = masterGT[3] + minCol * masterGT[4] + minRow * masterGT[5];
    cropGT[4] = masterGT[4];
    cropGT[5] = masterGT[5];

    // Crop master and slave
    std::vector<std::vector<float>> masterCropped(masterBands.size());
    std::vector<std::vector<float>> slaveCropped(warpedPaths.size());

    for (int i = 0; i < masterBands.size(); ++i) {
        const std::vector<float> &src = allData[i];
        masterCropped[i].resize(cropPixels);
        for (int row = 0; row < cropYSize; ++row) {
            for (int col = 0; col < cropXSize; ++col) {
                int srcRow = minRow + row;
                int srcCol = minCol + col;
                size_t srcIdx = size_t(srcRow) * fullXSize + srcCol;
                if (valid[srcIdx])
                    masterCropped[i][size_t(row) * cropXSize + col] = src[srcIdx];
                else
                    masterCropped[i][size_t(row) * cropXSize + col] = float(OUT_NODATA);
            }
        }
    }

    for (int i = 0; i < warpedPaths.size(); ++i) {
        const std::vector<float> &src = allData[masterBands.size() + i];
        slaveCropped[i].resize(cropPixels);
        for (int row = 0; row < cropYSize; ++row) {
            for (int col = 0; col < cropXSize; ++col) {
                int srcRow = minRow + row;
                int srcCol = minCol + col;
                size_t srcIdx = size_t(srcRow) * fullXSize + srcCol;
                if (valid[srcIdx])
                    slaveCropped[i][size_t(row) * cropXSize + col] = src[srcIdx];
                else
                    slaveCropped[i][size_t(row) * cropXSize + col] = float(OUT_NODATA);
            }
        }
    }

    // --------------------------------------------------------------------
    // Generate robust file names using QFileInfo
    // --------------------------------------------------------------------
    QFileInfo fi(outPath);
    QString baseName = fi.absolutePath() + "/" + fi.baseName(); // no extension
    QString sarPath = baseName + "_SAR.tif";
    QString opticalPath = baseName + "_Optical.tif";
    QString stackedPath = baseName + "_Stacked.tif";

//    qDebug() << "Writing SAR to       :" << sarPath;
//    qDebug() << "Writing Optical to   :" << opticalPath;
//    qDebug() << "Writing Stacked to   :" << stackedPath;

    // Write SAR (Float32)
    if (!writeCroppedBands(masterCropped, cropGT, masterProj, sarPath,
                           GDT_Float32, OUT_NODATA,
                           cropXSize, cropYSize, errMsg)) {
        result.errorMessage = errMsg;
        return result;
    }

    // Write Optical (UInt16)
    if (!writeCroppedBands(slaveCropped, cropGT, masterProj, opticalPath,
                           GDT_UInt16, 0,   // NoData = 0
                           cropXSize, cropYSize, errMsg)) {
        result.errorMessage = errMsg;
        return result;
    }

    // Write Combined Stack (Float32, all bands)
    std::vector<std::vector<float>> allCropped;
    allCropped.reserve(masterCropped.size() + slaveCropped.size());
    for (auto &v : masterCropped) allCropped.push_back(std::move(v));
    for (auto &v : slaveCropped)  allCropped.push_back(std::move(v));

    if (!writeCroppedBands(allCropped, cropGT, masterProj, stackedPath,
                           GDT_Float32, OUT_NODATA,
                           cropXSize, cropYSize, errMsg)) {
        result.errorMessage = errMsg;
        return result;
    }

    reportProgress(100, "Done.");

    result.success = true;
    result.transformation = "Affine";
    result.interpolation = "Nearest Neighbour";
    result.sarOutputPath = sarPath;
    result.opticalOutputPath = opticalPath;
    result.stackedOutputPath = stackedPath;
    result.numBandsStacked = allBandPaths.size();
    return result;
}




