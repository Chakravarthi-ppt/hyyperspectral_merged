#include "IHSFusion.h"

#include <QDebug>
#include <cmath>
#include <algorithm>

constexpr int IHSFusion::TILE_SIZE;

IHSFusion::IHSFusion()
{
    GDALAllRegister();
    m_redDataset = nullptr;
    m_nirDataset = nullptr;
    m_hhDataset = nullptr;
    m_hhResampledDataset = nullptr;
    m_outputDataset = nullptr;
    m_rows = 0;
    m_cols = 0;
    for (int i = 0; i < 6; i++) m_geoTransform[i] = 0.0;
    m_redMin = m_redMax = m_nirMin = m_nirMax = m_hhMin = m_hhMax = 0.0;
    m_hhLUTMin = 0.0f;
    m_hhLUTMax = 1.0f;
}

IHSFusion::~IHSFusion() { close(); }

void IHSFusion::close()
{
    if (m_redDataset) { GDALClose(m_redDataset); m_redDataset = nullptr; }
    if (m_nirDataset) { GDALClose(m_nirDataset); m_nirDataset = nullptr; }
    if (m_hhResampledDataset && m_hhResampledDataset != m_hhDataset)
        GDALClose(m_hhResampledDataset);
    m_hhResampledDataset = nullptr;
    if (m_hhDataset) { GDALClose(m_hhDataset); m_hhDataset = nullptr; }
    if (m_outputDataset) { GDALClose(m_outputDataset); m_outputDataset = nullptr; }
    m_rows = 0; m_cols = 0;
    m_projection.clear();
    qDebug() << "IHSFusion resources released.";
}

bool IHSFusion::openInputs(const QString &redFile, const QString &nirFile, const QString &hhFile)
{
    close();

    m_redDataset = (GDALDataset*)GDALOpen(redFile.toStdString().c_str(), GA_ReadOnly);
    if (!m_redDataset) { qDebug() << "Failed to open RED image."; return false; }

    m_nirDataset = (GDALDataset*)GDALOpen(nirFile.toStdString().c_str(), GA_ReadOnly);
    if (!m_nirDataset) { qDebug() << "Failed to open NIR image."; return false; }

    m_hhDataset = (GDALDataset*)GDALOpen(hhFile.toStdString().c_str(), GA_ReadOnly);
    if (!m_hhDataset) { qDebug() << "Failed to open HH image."; return false; }

    int redCols = m_redDataset->GetRasterXSize();
    int redRows = m_redDataset->GetRasterYSize();
    int nirCols = m_nirDataset->GetRasterXSize();
    int nirRows = m_nirDataset->GetRasterYSize();

    if (redCols != nirCols || redRows != nirRows)
    {
        qDebug() << "RED and NIR images have different dimensions.";
        return false;
    }

    m_cols = redCols;
    m_rows = redRows;
    m_projection = QString(m_redDataset->GetProjectionRef());

    if (m_redDataset->GetGeoTransform(m_geoTransform) != CE_None)
    {
        qDebug() << "Failed to read GeoTransform.";
        return false;
    }

    bool needResample = false;
    if (m_hhDataset->GetRasterXSize() != m_cols) needResample = true;
    if (m_hhDataset->GetRasterYSize() != m_rows) needResample = true;
    if (QString(m_hhDataset->GetProjectionRef()) != m_projection) needResample = true;

    if (needResample)
    {
        qDebug() << "Resampling HH image to RED image geometry...";
        if (!resampleHHToReference())
        {
            qDebug() << "HH resampling failed.";
            return false;
        }
    }
    else
    {
        m_hhResampledDataset = m_hhDataset;
    }

    if (!computeCommonExtent())
    {
        qDebug() << "Failed to compute common extent.";
        return false;
    }

    qDebug() << "--------------------------------";
    qDebug() << "Input Images Loaded";
    qDebug() << "Rows :" << m_rows << " Cols :" << m_cols;
    qDebug() << "--------------------------------";

    return true;
}

bool IHSFusion::resampleHHToReference()
{
    if (!m_redDataset || !m_hhDataset)
    {
        qDebug() << "Invalid datasets for resampling.";
        return false;
    }

    GDALDriver *memDriver = GetGDALDriverManager()->GetDriverByName("MEM");
    if (!memDriver) { qDebug() << "Failed to get MEM driver."; return false; }

    m_hhResampledDataset = memDriver->Create("", m_cols, m_rows, 1, GDT_Float32, nullptr);
    if (!m_hhResampledDataset) { qDebug() << "Failed to create memory dataset."; return false; }

    m_hhResampledDataset->SetProjection(m_redDataset->GetProjectionRef());

    double gt[6];
    if (m_redDataset->GetGeoTransform(gt) != CE_None)
    {
        qDebug() << "Failed to read RGB geotransform.";
        return false;
    }
    m_hhResampledDataset->SetGeoTransform(gt);

    int hasNoData = 0;
    double hhNoData = m_hhDataset->GetRasterBand(1)->GetNoDataValue(&hasNoData);
    if (!hasNoData)
    {
        hhNoData = 0.0;
        m_hhDataset->GetRasterBand(1)->SetNoDataValue(hhNoData);
    }

    m_hhResampledDataset->GetRasterBand(1)->Fill(hhNoData);
    m_hhResampledDataset->GetRasterBand(1)->SetNoDataValue(hhNoData);

    GDALWarpOptions *warpOptions = GDALCreateWarpOptions();
    warpOptions->hSrcDS = m_hhDataset;
    warpOptions->hDstDS = m_hhResampledDataset;
    warpOptions->nBandCount = 1;
    warpOptions->panSrcBands = (int*)CPLMalloc(sizeof(int));
    warpOptions->panSrcBands[0] = 1;
    warpOptions->panDstBands = (int*)CPLMalloc(sizeof(int));
    warpOptions->panDstBands[0] = 1;
    warpOptions->padfSrcNoDataReal = (double*)CPLMalloc(sizeof(double));
    warpOptions->padfSrcNoDataReal[0] = hhNoData;
    warpOptions->padfDstNoDataReal = (double*)CPLMalloc(sizeof(double));
    warpOptions->padfDstNoDataReal[0] = hhNoData;
    warpOptions->eResampleAlg = GRA_Bilinear;

    CPLErr err = GDALReprojectImage(
                m_hhDataset, m_hhDataset->GetProjectionRef(),
                m_hhResampledDataset, m_redDataset->GetProjectionRef(),
                GRA_Bilinear, 0.0, 0.0, nullptr, nullptr, warpOptions);

    GDALDestroyWarpOptions(warpOptions);

    if (err != CE_None)
    {
        qDebug() << "GDALReprojectImage failed:" << CPLGetLastErrorMsg();
        return false;
    }

    qDebug() << "HH resampled successfully. NoData =" << hhNoData;
    return true;
}

bool IHSFusion::createOutput(const QString &outputFile)
{
    if (!m_redDataset) { qDebug() << "Reference image not loaded."; return false; }

    GDALDriver *driver = GetGDALDriverManager()->GetDriverByName("GTiff");
    if (!driver) { qDebug() << "Failed to load GTiff driver."; return false; }

    char **options = nullptr;
    options = CSLSetNameValue(options, "COMPRESS", "LZW");
    options = CSLSetNameValue(options, "PREDICTOR", "3");
    options = CSLSetNameValue(options, "BIGTIFF", "IF_NEEDED");
    options = CSLSetNameValue(options, "TILED", "YES");
    options = CSLSetNameValue(options, "BLOCKXSIZE", "512");
    options = CSLSetNameValue(options, "BLOCKYSIZE", "512");

    m_outputDataset = driver->Create(outputFile.toStdString().c_str(),
                                      m_commonCols, m_commonRows, 3, GDT_Float32, options);
    CSLDestroy(options);

    if (!m_outputDataset) { qDebug() << "Failed to create output image."; return false; }

    m_outputDataset->SetProjection(m_redDataset->GetProjectionRef());
    m_outputDataset->SetGeoTransform(m_commonGeoTransform);

    GDALRasterBand *band1 = m_outputDataset->GetRasterBand(1);
    GDALRasterBand *band2 = m_outputDataset->GetRasterBand(2);
    GDALRasterBand *band3 = m_outputDataset->GetRasterBand(3);

    // band1 = R_Fused (Intensity slot, from HH)
    // band2 = G_Fused
    // band3 = B_Fused
    band1->SetDescription("Fused_R_HH");
    band2->SetDescription("Fused_G_NIR");
    band3->SetDescription("Fused_B_RED");

    band1->SetNoDataValue(0.0);
    band2->SetNoDataValue(0.0);
    band3->SetNoDataValue(0.0);

    qDebug() << "--------------------------------";
    qDebug() << "Output Dataset Created. Rows :" << m_commonRows << " Cols :" << m_commonCols;
    qDebug() << "--------------------------------";

    return true;
}

bool IHSFusion::readTile(GDALRasterBand *band, int x, int y, int width, int height, cv::Mat &tile)
{
    if (!band) return false;
    tile = cv::Mat(height, width, CV_32F);

    CPLErr err = band->RasterIO(GF_Read, x, y, width, height,
                                tile.data, width, height, GDT_Float32, 0, 0);
    if (err != CE_None)
    {
        qDebug() << "Failed to read tile:" << x << y << "-" << CPLGetLastErrorMsg();
        return false;
    }
    return true;
}

bool IHSFusion::writeTile(GDALRasterBand *band, int x, int y, const cv::Mat &tile)
{
    if (!band || tile.empty()) return false;
    CV_Assert(tile.type() == CV_32F);

    CPLErr err = band->RasterIO(GF_Write, x, y, tile.cols, tile.rows,
                                const_cast<float*>(tile.ptr<float>()),
                                tile.cols, tile.rows, GDT_Float32, 0, 0);
    if (err != CE_None)
    {
        qDebug() << "Failed to write tile:" << x << y << "-" << CPLGetLastErrorMsg();
        return false;
    }
    return true;
}

bool IHSFusion::computeGlobalStatistics()
{
    if (!m_redDataset || !m_nirDataset || !m_hhDataset || !m_hhResampledDataset)
    {
        qDebug() << "Datasets are not initialized.";
        return false;
    }

    GDALRasterBand *redBand = m_redDataset->GetRasterBand(1);
    GDALRasterBand *nirBand = m_nirDataset->GetRasterBand(1);
    GDALRasterBand *hhBand  = m_hhResampledDataset->GetRasterBand(1);

    double minMax[2];

    redBand->ComputeRasterMinMax(TRUE, minMax);
    m_redMin = minMax[0]; m_redMax = minMax[1];

    nirBand->ComputeRasterMinMax(TRUE, minMax);
    m_nirMin = minMax[0]; m_nirMax = minMax[1];

    hhBand->ComputeRasterMinMax(TRUE, minMax);
    m_hhMin = minMax[0]; m_hhMax = minMax[1];

    qDebug() << "--------------------------------";
    qDebug() << "RED :" << m_redMin << "to" << m_redMax;
    qDebug() << "NIR :" << m_nirMin << "to" << m_nirMax;
    qDebug() << "HH(dB):" << m_hhMin << "to" << m_hhMax;
    qDebug() << "--------------------------------";

    return true;
}

void IHSFusion::normalizeOpticalBand(const cv::Mat& src, cv::Mat& dst, double minVal, double maxVal)
{
    src.convertTo(dst, CV_32F);
    float range = static_cast<float>(maxVal - minVal);
    if (std::fabs(range) < 1e-6f) range = 1.0f;
    dst = (dst - static_cast<float>(minVal)) * (1.0f / range);
    cv::max(dst, 0.0f, dst);
    cv::min(dst, 1.0f, dst);
}

void IHSFusion::normalizeSARBand(const cv::Mat& src, cv::Mat& dst, double minVal, double maxVal)
{
    src.convertTo(dst, CV_32F);
    float range = static_cast<float>(maxVal - minVal);
    if (std::fabs(range) < 1e-6f) range = 1.0f;
    dst = (dst - static_cast<float>(minVal)) / range;
    cv::max(dst, 0.0f, dst);
    cv::min(dst, 1.0f, dst);
}

// Kept for interface compatibility. NOT used in the Option B
// fusion path - Hue/Saturation are now computed directly from
// Red/NIR (see processTiles), not via a 3-channel forward call.
void IHSFusion::rgbToIHS(const cv::Mat& R, const cv::Mat& G, const cv::Mat& B,
                         cv::Mat& I, cv::Mat& H, cv::Mat& S)
{
    CV_Assert(R.type() == CV_32F);
    CV_Assert(G.type() == CV_32F);
    CV_Assert(B.type() == CV_32F);

    I.create(R.size(), CV_32F);
    H.create(R.size(), CV_32F);
    S.create(R.size(), CV_32F);

    const float PI = 3.14159265358979323846f;

    for (int y = 0; y < R.rows; y++)
    {
        const float* rPtr = R.ptr<float>(y);
        const float* gPtr = G.ptr<float>(y);
        const float* bPtr = B.ptr<float>(y);
        float* iPtr = I.ptr<float>(y);
        float* hPtr = H.ptr<float>(y);
        float* sPtr = S.ptr<float>(y);

        for (int x = 0; x < R.cols; x++)
        {
            float r = rPtr[x], g = gPtr[x], b = bPtr[x];
            float intensity = (r + g + b) / 3.0f;
            iPtr[x] = intensity;

            float minRGB = std::min(r, std::min(g, b));
            sPtr[x] = (intensity <= 1e-6f) ? 0.0f : 1.0f - (minRGB / intensity);

            float numerator = 0.5f * ((r - g) + (r - b));
            float denominator = sqrt((r - g) * (r - g) + (r - b) * (g - b));
            float theta = (denominator < 1e-6f) ? 0.0f
                          : acos(std::max(-1.0f, std::min(1.0f, numerator / denominator)));

            hPtr[x] = (b <= g) ? theta : (2.0f * PI - theta);
        }
    }
}

void IHSFusion::ihsToRGB(const cv::Mat& I, const cv::Mat& H, const cv::Mat& S,
                         cv::Mat& R_Fused, cv::Mat& G_Fused, cv::Mat& B_Fused)
{
    CV_Assert(I.type() == CV_32F);
    CV_Assert(H.type() == CV_32F);
    CV_Assert(S.type() == CV_32F);

    R_Fused.create(I.size(), CV_32F);
    G_Fused.create(I.size(), CV_32F);
    B_Fused.create(I.size(), CV_32F);

    const float PI = 3.14159265358979323846f;
    const float TWO_PI_OVER_THREE = 2.0f * PI / 3.0f;
    const float FOUR_PI_OVER_THREE = 4.0f * PI / 3.0f;
    const float PI_OVER_THREE = PI / 3.0f;
    const float EPS = 1e-6f;

    for (int y = 0; y < I.rows; y++)
    {
        const float* iPtr = I.ptr<float>(y);
        const float* hPtr = H.ptr<float>(y);
        const float* sPtr = S.ptr<float>(y);
        float* rPtr = R_Fused.ptr<float>(y);
        float* gPtr = G_Fused.ptr<float>(y);
        float* bPtr = B_Fused.ptr<float>(y);

        for (int x = 0; x < I.cols; x++)
        {
            float intensity = iPtr[x];
            float hue = hPtr[x];
            float sat = sPtr[x];
            float r, g, b;

            if (hue < TWO_PI_OVER_THREE)
            {
                float denom = std::cos(PI_OVER_THREE - hue);
                if (std::fabs(denom) < EPS) denom = EPS;
                b = intensity * (1.0f - sat);
                r = intensity * (1.0f + sat * std::cos(hue) / denom);
                g = 3.0f * intensity - (r + b);
            }
            else if (hue < FOUR_PI_OVER_THREE)
            {
                hue -= TWO_PI_OVER_THREE;
                float denom = std::cos(PI_OVER_THREE - hue);
                if (std::fabs(denom) < EPS) denom = EPS;
                r = intensity * (1.0f - sat);
                g = intensity * (1.0f + sat * std::cos(hue) / denom);
                b = 3.0f * intensity - (r + g);
            }
            else
            {
                hue -= FOUR_PI_OVER_THREE;
                float denom = std::cos(PI_OVER_THREE - hue);
                if (std::fabs(denom) < EPS) denom = EPS;
                g = intensity * (1.0f - sat);
                b = intensity * (1.0f + sat * std::cos(hue) / denom);
                r = 3.0f * intensity - (g + b);
            }

            rPtr[x] = r; gPtr[x] = g; bPtr[x] = b;
        }
    }
}

//------------------------------------------------------------
// Histogram Matching LUT (Option B: reference Intensity is
// built directly from Red/NIR, no duplicated channel)
//------------------------------------------------------------

bool IHSFusion::computeHistogramMatchingLUT()
{
    if (m_commonRows <= 0 || m_commonCols <= 0)
    {
        qDebug() << "Common extent not computed yet.";
        return false;
    }

    GDALRasterBand *redBand = m_redDataset->GetRasterBand(1);
    GDALRasterBand *nirBand = m_nirDataset->GetRasterBand(1);
    GDALRasterBand *hhBand  = m_hhResampledDataset->GetRasterBand(1);

    cv::Mat redFull(m_commonRows, m_commonCols, CV_32F);
    cv::Mat nirFull(m_commonRows, m_commonCols, CV_32F);
    cv::Mat hhFull(m_commonRows, m_commonCols, CV_32F);

    if (redBand->RasterIO(GF_Read, m_redOffsetX, m_redOffsetY, m_commonCols, m_commonRows,
                           redFull.data, m_commonCols, m_commonRows, GDT_Float32, 0, 0) != CE_None)
    { qDebug() << "Failed to read full RED band:" << CPLGetLastErrorMsg(); return false; }

    if (nirBand->RasterIO(GF_Read, m_nirOffsetX, m_nirOffsetY, m_commonCols, m_commonRows,
                           nirFull.data, m_commonCols, m_commonRows, GDT_Float32, 0, 0) != CE_None)
    { qDebug() << "Failed to read full NIR band:" << CPLGetLastErrorMsg(); return false; }

    if (hhBand->RasterIO(GF_Read, m_hhOffsetX, m_hhOffsetY, m_commonCols, m_commonRows,
                          hhFull.data, m_commonCols, m_commonRows, GDT_Float32, 0, 0) != CE_None)
    { qDebug() << "Failed to read full HH band:" << CPLGetLastErrorMsg(); return false; }

    int redHasNoData=0, nirHasNoData=0, hhHasNoData=0;
    double redNoData = redBand->GetNoDataValue(&redHasNoData);
    double nirNoData = nirBand->GetNoDataValue(&nirHasNoData);
    double hhNoData  = hhBand->GetNoDataValue(&hhHasNoData);
    if (!redHasNoData) redNoData = 0.0;
    if (!nirHasNoData) nirNoData = 0.0;
    if (!hhHasNoData)  hhNoData  = 0.0;

    cv::Mat validRed, validNir, validHh, validMask;
    cv::compare(redFull, (float)redNoData, validRed, cv::CMP_NE);
    cv::compare(nirFull, (float)nirNoData, validNir, cv::CMP_NE);
    cv::compare(hhFull,  (float)hhNoData,  validHh,  cv::CMP_NE);
    cv::bitwise_and(validRed, validNir, validMask);
    cv::bitwise_and(validMask, validHh, validMask);

    // Optical Intensity reference, direct 2-band average (no
    // duplicated channel, no rgbToIHS call)
    cv::Mat redNorm, nirNorm;
    normalizeOpticalBand(redFull, redNorm, m_redMin, m_redMax);
    normalizeOpticalBand(nirFull, nirNorm, m_nirMin, m_nirMax);

    cv::Mat I_optical = (redNorm + nirNorm) * 0.5f;
    redFull.release();
    nirFull.release();
    redNorm.release();
    nirNorm.release();

    cv::Mat sarNorm;
    normalizeSARBand(hhFull, sarNorm, -30.0, 10.0);
    hhFull.release();

    const int SAMPLE_STRIDE = 4;
    std::vector<float> sarVals, optVals;
    sarVals.reserve((m_commonRows / SAMPLE_STRIDE + 1) * (m_commonCols / SAMPLE_STRIDE + 1));
    optVals.reserve(sarVals.capacity());

    for (int y = 0; y < m_commonRows; y += SAMPLE_STRIDE)
    {
        const uchar* mrow = validMask.ptr<uchar>(y);
        const float* srow = sarNorm.ptr<float>(y);
        const float* irow = I_optical.ptr<float>(y);
        for (int x = 0; x < m_commonCols; x += SAMPLE_STRIDE)
        {
            if (mrow[x])
            {
                sarVals.push_back(srow[x]);
                optVals.push_back(irow[x]);
            }
        }
    }

    if (sarVals.empty())
    {
        qDebug() << "No valid overlap pixels for histogram matching.";
        return false;
    }

    std::sort(sarVals.begin(), sarVals.end());
    std::sort(optVals.begin(), optVals.end());

    m_hhLUTMin = 0.0f;
    m_hhLUTMax = 1.0f;

    const int NBINS = 2048;
    m_hhLUT.assign(NBINS, 0.0f);

    for (int i = 0; i < NBINS; i++)
    {
        float sarValue = static_cast<float>(i) / (NBINS - 1);
        auto it = std::lower_bound(sarVals.begin(), sarVals.end(), sarValue);
        double percentile = (double)(it - sarVals.begin()) / sarVals.size();
        size_t optIdx = static_cast<size_t>(percentile * (optVals.size() - 1));
        m_hhLUT[i] = optVals[optIdx];
    }

    qDebug() << "--------------------------------";
    qDebug() << "Histogram Matching LUT built (Option B, direct Red+NIR average)";
    qDebug() << "SAR range:" << sarVals.front() << "to" << sarVals.back();
    qDebug() << "Optical I range:" << optVals.front() << "to" << optVals.back();
    qDebug() << "--------------------------------";

    return true;
}

void IHSFusion::applyHistogramMatch(const cv::Mat &src, cv::Mat &dst)
{
    dst.create(src.size(), CV_32F);
    int nbins = static_cast<int>(m_hhLUT.size());

    for (int y = 0; y < src.rows; y++)
    {
        const float* srcPtr = src.ptr<float>(y);
        float* dstPtr = dst.ptr<float>(y);
        for (int x = 0; x < src.cols; x++)
        {
            float t = std::max(0.0f, std::min(1.0f, srcPtr[x]));
            float pos = t * (nbins - 1);
            int idx0 = static_cast<int>(pos);
            int idx1 = std::min(idx0 + 1, nbins - 1);
            float frac = pos - idx0;
            dstPtr[x] = m_hhLUT[idx0] * (1.0f - frac) + m_hhLUT[idx1] * frac;
        }
    }
}

//------------------------------------------------------------
// Main Tile Processing Loop — Option B:
// Hue <- NIR directly, Saturation <- Red directly,
// Intensity <- histogram-matched SAR. No duplicated channel,
// no rgbToIHS() call, no HH leaking into Hue/Saturation.
//------------------------------------------------------------

bool IHSFusion::processTiles()
{
    if (!m_redDataset || !m_nirDataset || !m_hhResampledDataset || !m_outputDataset)
        return false;

    GDALRasterBand *redBand = m_redDataset->GetRasterBand(1);
    GDALRasterBand *nirBand = m_nirDataset->GetRasterBand(1);
    GDALRasterBand *hhBand  = m_hhResampledDataset->GetRasterBand(1);

    GDALRasterBand *outB = m_outputDataset->GetRasterBand(1);
    GDALRasterBand *outG = m_outputDataset->GetRasterBand(2);
    GDALRasterBand *outR = m_outputDataset->GetRasterBand(3);

    const int totalTilesX = (m_commonCols + TILE_SIZE - 1) / TILE_SIZE;
    const int totalTilesY = (m_commonRows + TILE_SIZE - 1) / TILE_SIZE;
    const int totalTiles  = totalTilesX * totalTilesY;
    int processedTiles = 0;

    for (int y = 0; y < m_commonRows; y += TILE_SIZE)
    {
        int tileHeight = std::min(TILE_SIZE, m_commonRows - y);

        for (int x = 0; x < m_commonCols; x += TILE_SIZE)
        {
            int tileWidth = std::min(TILE_SIZE, m_commonCols - x);

            cv::Mat redTile, nirTile, hhTile;

            if (!readTile(redBand, x + m_redOffsetX, y + m_redOffsetY, tileWidth, tileHeight, redTile))
                return false;
            if (!readTile(nirBand, x + m_nirOffsetX, y + m_nirOffsetY, tileWidth, tileHeight, nirTile))
                return false;
            if (!readTile(hhBand, x + m_hhOffsetX, y + m_hhOffsetY, tileWidth, tileHeight, hhTile))
                return false;

            // Validity mask
            int redHasNoData=0, nirHasNoData=0, hhHasNoData=0;
            double redNoData = redBand->GetNoDataValue(&redHasNoData);
            double nirNoData = nirBand->GetNoDataValue(&nirHasNoData);
            double hhNoData  = hhBand->GetNoDataValue(&hhHasNoData);
            if (!redHasNoData) redNoData = 0.0;
            if (!nirHasNoData) nirNoData = 0.0;
            if (!hhHasNoData)  hhNoData  = 0.0;

            cv::Mat validRed, validNir, validHh, validMask, invalidMask;
            cv::compare(redTile, static_cast<float>(redNoData), validRed, cv::CMP_NE);
            cv::compare(nirTile, static_cast<float>(nirNoData), validNir, cv::CMP_NE);
            cv::compare(hhTile,  static_cast<float>(hhNoData),  validHh,  cv::CMP_NE);
            cv::bitwise_and(validRed, validNir, validMask);
            cv::bitwise_and(validMask, validHh, validMask);
            cv::bitwise_not(validMask, invalidMask);

            //------------------------------------------------
            // Hue <- NIR, Saturation <- Red (direct, no duplicate)
            //------------------------------------------------

            cv::Mat nirScaled, redScaled;
            normalizeOpticalBand(nirTile, nirScaled, m_nirMin, m_nirMax);
            normalizeOpticalBand(redTile, redScaled, m_redMin, m_redMax);

            cv::Mat H = nirScaled * (2.0f * CV_PI);
            cv::Mat S = redScaled;

            //------------------------------------------------
            // Intensity <- histogram-matched SAR
            //------------------------------------------------

            cv::Mat sarNorm;
            normalizeSARBand(hhTile, sarNorm, -30.0, 10.0);

            cv::Mat I_Fused;
            applyHistogramMatch(sarNorm, I_Fused);

            //------------------------------------------------
            // Inverse IHS
            //------------------------------------------------

            cv::Mat R_Fused, G_Fused, B_Fused;
            ihsToRGB(I_Fused, H, S, R_Fused, G_Fused, B_Fused);

            cv::max(R_Fused, 0.0f, R_Fused); cv::min(R_Fused, 1.0f, R_Fused);
            cv::max(G_Fused, 0.0f, G_Fused); cv::min(G_Fused, 1.0f, G_Fused);
            cv::max(B_Fused, 0.0f, B_Fused); cv::min(B_Fused, 1.0f, B_Fused);

            R_Fused.setTo(0.0f, invalidMask);
            G_Fused.setTo(0.0f, invalidMask);
            B_Fused.setTo(0.0f, invalidMask);

            if (!writeTile(outB, x, y, B_Fused)) { qDebug() << "Failed to write R_Fused tile."; return false; }
            if (!writeTile(outG, x, y, G_Fused)) { qDebug() << "Failed to write G_Fused tile."; return false; }
            if (!writeTile(outR, x, y, R_Fused)) { qDebug() << "Failed to write B_Fused tile."; return false; }

            processedTiles++;
            if (progressCallback)
            {
                int progress = static_cast<int>(100.0 * processedTiles / totalTiles);
                progressCallback(progress);
            }
        }
    }

    for (int i = 1; i <= 3; i++)
        m_outputDataset->GetRasterBand(i)->FlushCache();
    m_outputDataset->FlushCache();

    qDebug() << "--------------------------------------";
    qDebug() << "IHS Fusion Completed (Option B: direct Hue/Sat from Red/NIR)";
    qDebug() << "--------------------------------------";

    return true;
}

bool IHSFusion::processFullImage()
{
    qDebug() << "--------------------------------";
    qDebug() << "Starting IHS Fusion";
    qDebug() << "--------------------------------";

    if (!computeGlobalStatistics()) return false;
    if (!computeHistogramMatchingLUT()) return false;
    if (!processTiles()) return false;

    qDebug() << "--------------------------------";
    qDebug() << "Fusion Completed Successfully";
    qDebug() << "--------------------------------";
    return true;
}

bool IHSFusion::computeCommonExtent()
{
    double redGT[6], hhGT[6], nirGT[6];

    if (m_redDataset->GetGeoTransform(redGT) != CE_None) return false;
    if (m_hhResampledDataset->GetGeoTransform(hhGT) != CE_None) return false;
    if (m_nirDataset->GetGeoTransform(nirGT) != CE_None) return false;

    double redLeft = redGT[0], redTop = redGT[3];
    double redRight = redLeft + m_redDataset->GetRasterXSize() * redGT[1];
    double redBottom = redTop + m_redDataset->GetRasterYSize() * redGT[5];

    double hhLeft = hhGT[0], hhTop = hhGT[3];
    double hhRight = hhLeft + m_hhResampledDataset->GetRasterXSize() * hhGT[1];
    double hhBottom = hhTop + m_hhResampledDataset->GetRasterYSize() * hhGT[5];

    double nirLeft = nirGT[0], nirTop = nirGT[3];
    double nirRight = nirLeft + m_nirDataset->GetRasterXSize() * nirGT[1];
    double nirBottom = nirTop + m_nirDataset->GetRasterYSize() * nirGT[5];

    double left = std::max({redLeft, nirLeft, hhLeft});
    double right = std::min({redRight, nirRight, hhRight});
    double top = std::min({redTop, nirTop, hhTop});
    double bottom = std::max({redBottom, nirBottom, hhBottom});

    if (left >= right || bottom >= top)
    {
        qDebug() << "No overlapping area.";
        return false;
    }

    double pixelSize = redGT[1];
    m_commonCols = static_cast<int>((right - left) / pixelSize);
    m_commonRows = static_cast<int>((top - bottom) / pixelSize);

    m_redOffsetX = static_cast<int>((left - redLeft) / pixelSize);
    m_redOffsetY = static_cast<int>((redTop - top) / pixelSize);
    m_nirOffsetX = static_cast<int>((left - nirLeft) / pixelSize);
    m_nirOffsetY = static_cast<int>((nirTop - top) / pixelSize);
    m_hhOffsetX = static_cast<int>((left - hhLeft) / pixelSize);
    m_hhOffsetY = static_cast<int>((hhTop - top) / pixelSize);

    memcpy(m_commonGeoTransform, redGT, sizeof(double) * 6);
    m_commonGeoTransform[0] = left;
    m_commonGeoTransform[3] = top;

    qDebug() << "--------------------------------";
    qDebug() << "Common Area. Rows :" << m_commonRows << " Cols :" << m_commonCols;
    qDebug() << "--------------------------------";

    return true;
}
