// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.

#include "slideio/drivers/ndpi/ndpiscene.hpp"

#include <opencv2/imgproc.hpp>

#include "ndpifile.hpp"
#include "slideio/core/tools/tools.hpp"
#include "slideio/drivers/ndpi/ndpitiffmessagehandler.hpp"
#include "slideio/imagetools/imagetools.hpp"

using namespace slideio;

class NDPIUserData
{
public:
    NDPIUserData(const NDPITiffDirectory* dir, const std::string& filePath) : m_dir(dir),
                                                                              m_file(nullptr), m_rowsPerStrip(0),
                                                                              m_filePath(filePath)
    {
        if ((!dir->tiled) && (dir->rowsPerStrip == dir->height) && dir->slideioCompression==Compression::Jpeg) {
#if defined(WIN32)
            m_file = _wfopen(Tools::toWstring(filePath).c_str(), L"rb");
#else
            m_file = fopen(filePath.c_str(), "rb");
#endif
            if (!m_file) {
                RAISE_RUNTIME_ERROR << "NDPI Image Driver: Cannot open file " << filePath;
            }
            const int MAX_BUFFER_SIZE = 10 * 1024 * 1024;
            int strideSize = dir->width * dir->channels * ImageTools::dataTypeSize(dir->dataType);
            int rowsPerStrip = MAX_BUFFER_SIZE / strideSize;
            m_rowsPerStrip = std::min(rowsPerStrip, dir->height);
        } else {
            m_rowsPerStrip = dir->rowsPerStrip;
        }
    }

    ~NDPIUserData()
    {
        if (m_file) {
            fclose(m_file);
        }
    }

    const NDPITiffDirectory* dir() const
    {
        return m_dir;
    }

    FILE* file() const
    {
        return m_file;
    }

    int rowsPerStrip() const
    {
        return m_rowsPerStrip;
    }

    const std::string& filePath() const
    {
        return m_filePath;
    }

private:
    const NDPITiffDirectory* m_dir;
    FILE* m_file;
    int m_rowsPerStrip;
    std::string m_filePath;
};

NDPIScene::NDPIScene() : m_pfile(nullptr), m_startDir(-1), m_endDir(-1), m_rect(0, 0, 0, 0)
{
    m_cacheManager = std::make_shared<CacheManager>();
}

NDPIScene::~NDPIScene()
{
}

void NDPIScene::init(const std::string& name, NDPIFile* file, int32_t startDirIndex, int32_t endDirIndex)
{
    NDPITIFFMessageHandler mh;

    m_sceneName = name;
    m_pfile = file;
    m_startDir = startDirIndex;
    m_endDir = endDirIndex;

    if (!m_pfile) {
        RAISE_RUNTIME_ERROR << "NDPIImageDriver: Invalid file handle.";
    }
    const std::vector<NDPITiffDirectory>& directories = m_pfile->directories();
    if (m_startDir < 0 || m_startDir >= directories.size()) {
        RAISE_RUNTIME_ERROR << "NDPIImageDriver: Invalid directory index: " << m_startDir << ". File:" << m_pfile->
            getFilePath();
    }
    const NDPITiffDirectory& dir = m_pfile->directories()[m_startDir];
    m_rect.width = dir.width;
    m_rect.height = dir.height;
}

cv::Rect NDPIScene::getRect() const
{
    return m_rect;
}

int NDPIScene::getNumChannels() const
{
    const std::vector<NDPITiffDirectory>& directories = m_pfile->directories();
    if (m_startDir < 0 || m_startDir >= directories.size()) {
        RAISE_RUNTIME_ERROR << "NDPIImageDriver: Invalid directory index: " << m_startDir << ". File:" << m_pfile->
            getFilePath();
    }
    const auto& dir = directories[m_startDir];
    return dir.channels;
}


std::string NDPIScene::getFilePath() const
{
    if (!m_pfile) {
        throw std::runtime_error(std::string("NDPIScene: Invalid file pointer"));
    }
    return m_pfile->getFilePath();
}

slideio::DataType NDPIScene::getChannelDataType(int channel) const
{
    const std::vector<NDPITiffDirectory>& directories = m_pfile->directories();
    if (m_startDir < 0 || m_startDir >= directories.size()) {
        RAISE_RUNTIME_ERROR << "NDPIImageDriver: Invalid directory index: " << m_startDir << ". File:" << m_pfile->
            getFilePath();
    }
    const auto& dir = directories[m_startDir];
    return dir.dataType;
}

Resolution NDPIScene::getResolution() const
{
    const std::vector<NDPITiffDirectory>& directories = m_pfile->directories();
    if (m_startDir < 0 || m_startDir >= directories.size()) {
        RAISE_RUNTIME_ERROR << "NDPIImageDriver: Invalid directory index: " << m_startDir << ". File:" << m_pfile->
            getFilePath();
    }
    const auto& dir = directories[m_startDir];
    return dir.res;
}

double NDPIScene::getMagnification() const
{
    const std::vector<NDPITiffDirectory>& directories = m_pfile->directories();
    if (m_startDir < 0 || m_startDir >= directories.size()) {
        RAISE_RUNTIME_ERROR << "NDPIImageDriver: Invalid directory index: " << m_startDir << ". File:" << m_pfile->
            getFilePath();
    }
    const auto& dir = directories[m_startDir];
    return dir.magnification;
}

Compression NDPIScene::getCompression() const
{
    const std::vector<NDPITiffDirectory>& directories = m_pfile->directories();
    if (m_startDir < 0 || m_startDir >= directories.size()) {
        RAISE_RUNTIME_ERROR << "NDPIImageDriver: Invalid directory index: " << m_startDir << ". File:" << m_pfile->
            getFilePath();
    }
    const auto& dir = directories[m_startDir];
    return dir.slideioCompression;
}


void NDPIScene::readBlockFromCache(const cv::Rect& imageBlockRect, const std::vector<int>& channelIndices,
                            const cv::Size& requiredBlockSize, cv::OutputArray output)
{
    const NDPITiffDirectory& dir = findZoomDirectory(imageBlockRect, requiredBlockSize);
    const double dirZoomX = static_cast<double>(dir.width) / static_cast<double>(m_rect.width);
    const double dirZoomY = static_cast<double>(dir.height) / static_cast<double>(m_rect.height);
    const cv::Size tileSize(1000, 1000);
    if(!isDirectoryCached(dir)) {
        NDPITIFFMessageHandler mh;
        NDPITiffTools::cacheScanlines(m_pfile, dir, tileSize, m_cacheManager.get());
        markDirectoryCached(dir);
    }
    CacheManagerTiler tiler(m_cacheManager, tileSize, dir.dirIndex);
    cv::Rect dirBlockRect;
    scaleBlockToDirectory(imageBlockRect, dir, dirBlockRect);
    TileComposer::composeRect(&tiler, channelIndices, dirBlockRect, requiredBlockSize, output);
}


const NDPITiffDirectory& NDPIScene::findZoomDirectory(const cv::Rect& imageBlockRect, const cv::Size& requiredBlockSize) const
{
    auto directories = m_pfile->directories();

    const double zoomImageToBlockX = static_cast<double>(requiredBlockSize.width) / static_cast<double>(imageBlockRect.width);
    const double zoomImageToBlockY = static_cast<double>(requiredBlockSize.height) / static_cast<double>(imageBlockRect.height);

    const double zoom = std::max(zoomImageToBlockX, zoomImageToBlockY);
    const NDPITiffDirectory& dir = m_pfile->findZoomDirectory(zoom, m_rect.width, m_startDir, m_endDir);
    return dir;
}



bool NDPIScene::isDirectoryCached(const NDPITiffDirectory& dir)
{
    return m_cachedDirectory.find(dir.dirIndex) != m_cachedDirectory.end();
}

void NDPIScene::markDirectoryCached(const NDPITiffDirectory& dir)
{
    m_cachedDirectory.insert(dir.dirIndex);
}

void NDPIScene::scaleBlockToDirectory(const cv::Rect& imageBlockRect, const slideio::NDPITiffDirectory& dir, cv::Rect& dirBlockRect) const
{
    // scale coefficients to scale original image to the directory image
    const double zoomImageToDirX = static_cast<double>(dir.width) / static_cast<double>(m_rect.width);
    const double zoomImageToDirY = static_cast<double>(dir.height) / static_cast<double>(m_rect.height);

    // rectangle on the directory zoom level
    Tools::scaleRect(imageBlockRect, zoomImageToDirX, zoomImageToDirY, dirBlockRect);
}

void NDPIScene::readResampledBlockChannels(const cv::Rect& imageBlockRect, const cv::Size& requiredBlockSize,
                                           const std::vector<int>& channelIndices, cv::OutputArray output)
{

    const slideio::NDPITiffDirectory& dir = findZoomDirectory(imageBlockRect, requiredBlockSize);

    const auto& directories = m_pfile->directories();

    cv::Rect dirBlockRect;
    scaleBlockToDirectory(imageBlockRect, dir, dirBlockRect);


    if (!dir.tiled && dir.rowsPerStrip == dir.height && dir.slideioCompression == Compression::Jpeg) {
        readBlockFromCache(imageBlockRect,channelIndices, requiredBlockSize, output);
    }
    else {
        NDPIUserData data(&dir, getFilePath());
        TileComposer::composeRect(this, channelIndices, dirBlockRect, requiredBlockSize, output, (void*)&data);
    }
}

int NDPIScene::getTileCount(void* userData)
{
    NDPIUserData* data = (NDPIUserData*)userData;
    const NDPITiffDirectory* dir = data->dir();
    if (dir->tiled) {
        int tilesX = (dir->width - 1) / dir->tileWidth + 1;
        int tilesY = (dir->height - 1) / dir->tileHeight + 1;
        return tilesX * tilesY;
    }
    else {
        const int rowsPerStrip = (dir->rowsPerStrip == dir->height) ? data->rowsPerStrip() : dir->rowsPerStrip;
        const int stripes = (dir->height - 1) / rowsPerStrip + 1;
        return stripes;
    }
}

bool NDPIScene::getTileRect(int tileIndex, cv::Rect& tileRect, void* userData)
{
    NDPITIFFMessageHandler mh;

    NDPIUserData* data = (NDPIUserData*)userData;
    const NDPITiffDirectory* dir = data->dir();
    if (dir->tiled) {
        const int tilesX = (dir->width - 1) / dir->tileWidth + 1;
        const int tilesY = (dir->height - 1) / dir->tileHeight + 1;
        const int tileY = tileIndex / tilesX;
        const int tileX = tileIndex % tilesX;
        tileRect.x = tileX * dir->tileWidth;
        tileRect.y = tileY * dir->tileHeight;
        tileRect.width = dir->tileWidth;
        tileRect.height = dir->tileHeight;
        return true;
    }
    else {
        const int rowsPerStrip = (dir->rowsPerStrip == dir->height) ? data->rowsPerStrip() : dir->rowsPerStrip;
        const int y = tileIndex * rowsPerStrip;
        tileRect.x = 0;
        tileRect.y = y;
        tileRect.width = dir->width;
        tileRect.height = NDPITiffTools::computeStripHeight(dir->height, rowsPerStrip, tileIndex);
        return true;
    }
}

bool NDPIScene::readTile(int tileIndex, const std::vector<int>& channelIndices, cv::OutputArray tileRaster,
                         void* userData)
{
    NDPITIFFMessageHandler mh;

    NDPIUserData* data = (NDPIUserData*)userData;
    const NDPITiffDirectory* dir = data->dir();
    bool ret = false;

    if (dir->tiled) {
        try {
            NDPITiffTools::readTile(m_pfile->getTiffHandle(), *dir, tileIndex, channelIndices, tileRaster);
            ret = true;
        }
        catch (std::runtime_error&) {
        }
    }
    else if (dir->rowsPerStrip == dir->height && dir->slideioCompression == Compression::Jpeg) {
        try {
            const int firstScanline = tileIndex * data->rowsPerStrip();
            const int numberScanlines = data->rowsPerStrip();
            NDPITiffTools::readScanlines(m_pfile->getTiffHandle(), data->file(), *dir, firstScanline, numberScanlines,
                                         channelIndices, tileRaster);
            ret = true;
        }
        catch (std::runtime_error&) {

        }
    } else {
        try {
            NDPITiffTools::readStrip(m_pfile->getTiffHandle(), *dir, tileIndex, channelIndices, tileRaster);
            ret = true;
        }
        catch (std::runtime_error&) {
        }
    }
    return ret;
}

void NDPIScene::initializeBlock(const cv::Size& blockSize, const std::vector<int>& channelIndices, cv::OutputArray output)
{
    initializeSceneBlock(blockSize, channelIndices, output);
}
