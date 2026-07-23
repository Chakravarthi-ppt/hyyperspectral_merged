#ifndef IHSFUSION_H
#define IHSFUSION_H

#include <QString>
#include <functional>

#include <opencv2/opencv.hpp>

#include "gdal_priv.h"
#include "gdalwarper.h"
#include "gdal_alg.h"

class IHSFusion
{
public:

    IHSFusion();
    ~IHSFusion();

    //------------------------------------
    // Main Functions
    //------------------------------------

    bool openInputs(const QString &redFile,
                    const QString &nirFile,

                    const QString &hhFile);

    bool createOutput(const QString &outputFile);

    bool computeGlobalStatistics();

    bool processFullImage();

    bool processTiles();

    bool computeHistogramMatchingLUT();

    void close();

    //------------------------------------
    // Progress Callback
    //------------------------------------

    std::function<void(int)> progressCallback;

private:

    //------------------------------------
    // GDAL Datasets
    //------------------------------------

    GDALDataset *m_redDataset;
    GDALDataset *m_nirDataset;
    GDALDataset *m_hhDataset;
    GDALDataset *m_hhResampledDataset;
    GDALDataset *m_outputDataset;

    //------------------------------------
    // Image Information
    //------------------------------------

    int m_rows;
    int m_cols;

    double m_geoTransform[6];

    QString m_projection;

    //------------------------------------
    // Statistics
    //------------------------------------

    double m_redMin;
    double m_redMax;

    double m_nirMin;
    double m_nirMax;

    double m_hhMin;
    double m_hhMax;

    //------------------------------------
    // Helper Functions
    //------------------------------------

    bool readTile(GDALRasterBand *band,
                             int x,
                             int y,
                             int width,
                             int height,
                             cv::Mat &tile);

    bool writeTile(GDALRasterBand *band,
                              int x,
                              int y,
                              const cv::Mat &tile);

    //------------------------------------
    // Normalization
    //------------------------------------

    void normalizeOpticalBand(const cv::Mat& src,
                              cv::Mat& dst,
                              double minVal,
                              double maxVal);

    void normalizeSARBand(const cv::Mat& src,
                          cv::Mat& dst,
                          double minVal,
                          double maxVal);

    //------------------------------------
    // IHS
    //------------------------------------

    void rgbToIHS(const cv::Mat& R,
                  const cv::Mat& G,
                  const cv::Mat& B,
                  cv::Mat& I,
                  cv::Mat& H,
                  cv::Mat& S);

    void ihsToRGB(const cv::Mat& I,
                  const cv::Mat& H,
                  const cv::Mat& S,
                  cv::Mat& HH,
                  cv::Mat& NIR,
                  cv::Mat& R);

    //------------------------------------
    // Resampling
    //------------------------------------

    bool resampleHHToReference();

    //------------------------------------
    // Image Buffers
    //------------------------------------

    cv::Mat m_red;
    cv::Mat m_nir;
    cv::Mat m_hh;

    cv::Mat m_intensity;
    cv::Mat m_hue;
    cv::Mat m_saturation;

    cv::Mat m_fusedRed;
    cv::Mat m_fusednir;
    cv::Mat m_fusedHH;

    //------------------------------------
    // Common Extent
    //------------------------------------

    int m_commonRows;
    int m_commonCols;

    double m_commonGeoTransform[6];

    int m_redOffsetX;
    int m_redOffsetY;

    int m_nirOffsetX;
    int m_nirOffsetY;

    int m_hhOffsetX;
    int m_hhOffsetY;

    bool computeCommonExtent();

    //histo

    void applyHistogramMatch(const cv::Mat &src, cv::Mat &dst);

    std::vector<float> m_hhLUT;
    float m_hhLUTMin, m_hhLUTMax;


private:
    static constexpr int TILE_SIZE = 512;
};

#endif
