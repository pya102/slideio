// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.

#include "slideio/drivers/pke//pketiledscene.hpp"
#include "slideio/imagetools/tifftools.hpp"
#include "slideio/drivers/pke/pketools.hpp"
#include "slideio/drivers/pke/pkescene.hpp"
#include "slideio/core/tools/tools.hpp"
#include "slideio/imagetools/cvtools.hpp"
#include <tinyxml2.h>

using namespace slideio;


PKETiledScene::PKETiledScene(const std::string& filePath, const std::string& name, 
    const std::vector<TiffDirectory>& dirs): PKEScene(filePath, name), m_directories(dirs)
{
    initialize();
}

PKETiledScene::PKETiledScene(const std::string& filePath, libtiff::TIFF* hFile, const std::string& name,
    const std::vector<slideio::TiffDirectory>& dirs) : PKEScene(filePath, hFile, name), m_directories(dirs)
{
    initialize();
}

void PKETiledScene::initialize()
{
    const auto& directory = m_directories[0];
    const auto& description = directory.description;
    if (!description.empty()) {
        tinyxml2::XMLDocument doc;
        tinyxml2::XMLError error = doc.Parse(description.c_str(), description.size());
        if (error != tinyxml2::XML_SUCCESS) {
            RAISE_RUNTIME_ERROR << "PKEImageDriver: Error parsing image description xml: " << static_cast<int>(error);
        }
        tinyxml2::XMLElement* root = doc.RootElement();
        if (!root) {
            RAISE_RUNTIME_ERROR << "PKEImageDriver: Error parsing image description xml: root element is null";
        }
        // Extract magnification
        double magnification = -1.;
        auto xmlScanResolution = root->FirstChildElement("ScanResolution");
        if (xmlScanResolution) {
            auto xmlMagnification = xmlScanResolution->FirstChildElement("Magnification");
            if (xmlMagnification) {
                magnification = xmlMagnification->DoubleText();
            }
        }
        if(magnification < 0) {
            auto xmlObjective = root->FirstChildElement("Objective");
            if (xmlObjective) {
                magnification = xmlObjective->DoubleText();
            }
        }
        if (magnification > 0) {
            m_magnification = magnification;
        }
        // Extract name
        auto xmlName = root->FirstChildElement("SlideID");
        if (xmlName) {
            m_name = xmlName->GetText();
        }
        // Extract isUnmixed
        auto xmlUnmixed = root->FirstChildElement("IsUnmixedComponent");
        if (xmlUnmixed) {
            m_isUnmixed = xmlUnmixed->BoolText();
        }
    }

    auto& dir = m_directories[0];
    m_dataType = dir.dataType;
    m_resolution = dir.res;

    if (m_dataType == DataType::DT_None || m_dataType == DataType::DT_Unknown)
    {
        switch (dir.bitsPerSample)
        {
        case 8:
            m_dataType = dir.dataType = DataType::DT_Byte;
            break;
        case 16:
            m_dataType = dir.dataType = DataType::DT_UInt16;
            break;
        default:
            m_dataType = DataType::DT_Unknown;
        }
    }

    if (!m_directories.empty())
    {
        const auto& dir = m_directories.front();
        m_compression = dir.slideioCompression;
        if (m_compression == Compression::Unknown &&
            (dir.compression == 33003 || dir.compression == 3305))
        {
            m_compression = Compression::Jpeg2000;
        }
        const int fullResolutionWidth = m_directories[0].width;
        const int fullResolutionHeight = m_directories[0].height;

        int numFullResolutions = 0;
        int lastWidth = 0;
        int index = 0;
        for (const auto& dir : m_directories) {
            if(dir.width == fullResolutionWidth && dir.height == fullResolutionHeight) {
                numFullResolutions++;
            }
            if (lastWidth != dir.width && dir.width > 0 && dir.height > 0) {
                lastWidth = dir.width;
                m_zoomDirectoryIndices.push_back(index);
            }
            ++index;
        }

        m_numChannels = numFullResolutions* m_directories.front().channels;

        const int numLevels = static_cast<int>(m_zoomDirectoryIndices.size());
        m_levels.resize(numLevels);
        for (int lv = 0; lv < numLevels; ++lv) {
            int directoryIndex = m_zoomDirectoryIndices[lv];
            const TiffDirectory& directory = m_directories[directoryIndex];
            LevelInfo& level = m_levels[lv];
            const double scale = static_cast<double>(directory.width) / static_cast<double>(fullResolutionWidth);
            level.setLevel(lv);
            level.setScale(scale);
            level.setSize({ directory.width, directory.height });
            level.setTileSize({ directory.tileWidth, directory.tileHeight });
            level.setMagnification(m_magnification * scale);
        }
    }
    initializeChannelNames();
}

void PKETiledScene::initializeChannelNames() {
    const auto& dir0 = m_directories.front();
    if(dir0.channels == 1) {
        for (int channel = 0; channel < m_numChannels; ++channel) {
            const auto& dir = m_directories[channel];
            std::string name;
            const auto& description = dir.description;
            tinyxml2::XMLDocument doc;
            tinyxml2::XMLError error = doc.Parse(description.c_str(), description.size());
            if (error != tinyxml2::XML_SUCCESS) {
                RAISE_RUNTIME_ERROR << "PKEImageDriver: Error parsing image description xml: " << static_cast<int>(error);
            }
            tinyxml2::XMLElement* root = doc.RootElement();
            if (!root) {
                RAISE_RUNTIME_ERROR << "PKEImageDriver: Error parsing image description xml: root element is null";
            }
            auto xmlName = root->FirstChildElement("Name");
            if(xmlName) {
                name = xmlName->GetText();
            }
            m_channelNames.push_back(name);
        }
    }
}


cv::Rect PKETiledScene::getRect() const
{
    cv::Rect rect = { 0,0,  m_directories[0].width,  m_directories[0].height };
    return rect;
}

int PKETiledScene::getNumChannels() const
{
    return m_numChannels;
}


void PKETiledScene::readResampledBlockChannels(const cv::Rect& blockRect, const cv::Size& blockSize,
    const std::vector<int>& channelIndices, cv::OutputArray output)
{
    auto hFile = getFileHandle();
    if (hFile == nullptr)
        throw std::runtime_error("PKEDriver: Invalid file header by raster reading operation");
    double zoomX = static_cast<double>(blockSize.width) / static_cast<double>(blockRect.width);
    double zoomY = static_cast<double>(blockSize.height) / static_cast<double>(blockRect.height);
    double zoom = std::max(zoomX, zoomY);
    const slideio::TiffDirectory& dir = findZoomDirectory(zoom);
    double zoomDirX = static_cast<double>(dir.width) / static_cast<double>(m_directories[0].width); 
    double zoomDirY = static_cast<double>(dir.height) / static_cast<double>(m_directories[0].height);
    cv::Rect resizedBlock;
    Tools::scaleRect(blockRect, zoomDirX, zoomDirY, resizedBlock);
    TileComposer::composeRect(this, channelIndices, resizedBlock, blockSize, output, (void*)&dir);
}

const TiffDirectory& PKETiledScene::findZoomDirectory(double zoom) const
{
    const cv::Rect sceneRect = getRect();
    const double sceneWidth = static_cast<double>(sceneRect.width);
    const auto& directories = m_directories;
    int index = Tools::findZoomLevel(zoom, (int)m_directories.size(), [&directories, sceneWidth](int index){
        return directories[index].width/sceneWidth;
    });
    return m_directories[index];
}

int PKETiledScene::getTileCount(void* userData)
{
    const TiffDirectory* dir = (const TiffDirectory*)userData;
    int tilesX = (dir->width-1)/dir->tileWidth + 1;
    int tilesY = (dir->height-1)/dir->tileHeight + 1;
    return tilesX * tilesY;
}

bool PKETiledScene::getTileRect(int tileIndex, cv::Rect& tileRect, void* userData)
{
    const TiffDirectory* dir = (const TiffDirectory*)userData;
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

bool PKETiledScene::readTile(int tileIndex, const std::vector<int>& channelIndices, cv::OutputArray tileRaster,
    void* userData)
{
    const TiffDirectory* dir = static_cast<const TiffDirectory*>(userData);
    bool ret = false;
    try
    {
        TiffTools::readTile(getFileHandle(), *dir, tileIndex, channelIndices, tileRaster);
        ret = true;
    }
    catch(std::runtime_error&){
        const cv::Size tileSize = { dir->tileWidth, dir->tileHeight };
        const slideio::DataType dt = dir->dataType;
        tileRaster.create(tileSize, CV_MAKETYPE(slideio::CVTools::toOpencvType(dt), dir->channels));
        tileRaster.setTo(0);
    }

    return ret;
}

void PKETiledScene::initializeBlock(const cv::Size& blockSize, const std::vector<int>& channelIndices, cv::OutputArray output)
{
    initializeSceneBlock(blockSize, channelIndices, output);
}

std::string PKETiledScene::getChannelName(int channel) const {
    return m_channelNames.empty()?"":m_channelNames[channel];
}



