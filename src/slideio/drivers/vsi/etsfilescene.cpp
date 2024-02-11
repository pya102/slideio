// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.

#include "etsfilescene.hpp"

#include "slideio/base/exceptions.hpp"
#include "slideio/core/tools/tools.hpp"
#include "slideio/drivers/vsi/etsfile.hpp"
#include "slideio/drivers/vsi/vsifile.hpp"

using namespace slideio;
using namespace slideio::vsi;

struct TileComposerUserData
{
    int levelIndex = -1;
};

EtsFileScene::EtsFileScene(const std::string& filePath,
                            std::shared_ptr<vsi::VSIFile>& vsiFile,
                            int etsIndex) :
                            VSIScene(filePath, vsiFile),
                            m_etsIndex(etsIndex)
{
       init();
}

void EtsFileScene::readResampledBlockChannels(const cv::Rect& blockRect, const cv::Size& blockSize,
    const std::vector<int>& channelIndices, cv::OutputArray output)
{
    const auto etsFile = getEtsFile();
    if(!etsFile) {
        RAISE_RUNTIME_ERROR << "VSIImageDriver: ETS file is not initialized";
    }
    const auto volume = etsFile->getVolume();
    if(!volume) {
        RAISE_RUNTIME_ERROR << "VSIImageDriver: ETS file does not contain volume";
    }

    double zoomX = static_cast<double>(blockSize.width) / static_cast<double>(blockRect.width);
    double zoomY = static_cast<double>(blockSize.height) / static_cast<double>(blockRect.height);
    double zoom = std::max(zoomX, zoomY);
    const int levelIndex = findZoomLevelIndex(zoom);
    if(levelIndex < 0 || levelIndex >= etsFile->getNumPyramidLevels()) {
        RAISE_RUNTIME_ERROR << "VSIImageDriver: Unexpected zoom level index: "
            << levelIndex << " Expected: " << "0 - " << etsFile->getNumPyramidLevels();
    }
    const EtsFile::PyramidLevel& level = etsFile->getPyramidLevel(levelIndex);
    const double levelZoom = 1. / level.scaleLevel;
    cv::Rect resizedBlock;
    Tools::scaleRect(blockRect, levelZoom, levelZoom, resizedBlock);
    TileComposerUserData userData;
    userData.levelIndex = levelIndex;
    TileComposer::composeRect(this, channelIndices, resizedBlock, blockSize, output, (void*)&userData);
}

int EtsFileScene::getTileCount(void* userData)
{
    const TileComposerUserData* tileComposerUserData = static_cast<TileComposerUserData*>(userData);
    const int levelIndex = tileComposerUserData->levelIndex;
    const std::shared_ptr<EtsFile> etsFile = getEtsFile();
    if(!etsFile) {
        RAISE_RUNTIME_ERROR << "VSIImageDriver: ETS file is not initialized";
    }
    if(levelIndex < 0 || levelIndex >= etsFile->getNumPyramidLevels()) {
        RAISE_RUNTIME_ERROR << "VSIImageDriver: Pyramid level is not initialized";
    }
    const EtsFile::PyramidLevel& level = etsFile->getPyramidLevel(levelIndex);
    return static_cast<int>(level.tiles.size());
}

bool EtsFileScene::getTileRect(int tileIndex, cv::Rect& tileRect, void* userData)
{
    const TileComposerUserData* tileComposerUserData = static_cast<TileComposerUserData*>(userData);
    const int levelIndex = tileComposerUserData->levelIndex;
    const std::shared_ptr<EtsFile> etsFile = getEtsFile();
    if (!etsFile) {
        RAISE_RUNTIME_ERROR << "VSIImageDriver: ETS file is not initialized";
    }
    if (levelIndex < 0 || levelIndex >= etsFile->getNumPyramidLevels()) {
        RAISE_RUNTIME_ERROR << "VSIImageDriver: Pyramid level is not initialized";
    }
    const EtsFile::PyramidLevel& level = etsFile->getPyramidLevel(levelIndex);
    if(tileIndex < 0 || tileIndex >= level.tiles.size()) {
        RAISE_RUNTIME_ERROR << "VSIImageDriver: Tile index " << tileIndex
            << " is out of range (0 - " << level.tiles.size() << ")";
    }
    const EtsFile::TileInfo tileInfo = level.tiles[tileIndex];
    tileRect.x = tileInfo.coordinates[0];
    tileRect.y = tileInfo.coordinates[1];
    tileRect.width = etsFile->getTileSize().width;
    tileRect.height = etsFile->getTileSize().height;
    return true;
}

bool EtsFileScene::readTile(int tileIndex, const std::vector<int>& channelIndices, cv::OutputArray tileRaster,
    void* userData)
{
    const TileComposerUserData* tileComposerUserData = static_cast<TileComposerUserData*>(userData);
    const int levelIndex = tileComposerUserData->levelIndex;
    const std::shared_ptr<EtsFile> etsFile = getEtsFile();
    etsFile->readTile(levelIndex, tileIndex, tileRaster);
    return false;
}

void EtsFileScene::addAuxImage(const std::string& name, std::shared_ptr<CVScene> scene) {
	m_auxScenes[name] = scene;
    m_auxNames.push_back(name);
}

std::shared_ptr<CVScene> EtsFileScene::getAuxImage(const std::string& imageName) const {
    if(m_auxScenes.count(imageName)) {
        return m_auxScenes.at(imageName);
    }
    RAISE_RUNTIME_ERROR << "VSIImageDriver: ETS file does not contain auxiliary image with name " << imageName;
}

int EtsFileScene::getNumZSlices() const {
    return getEtsFile()->getNumZSlices();
}

int EtsFileScene::getNumTFrames() const {
    return getEtsFile()->getNumTFrames();
}

int EtsFileScene::getNumLambdas() const {
	return getEtsFile()->getNumLambdas();
}

int EtsFileScene::getNumPyramidLevels() const {
	return getEtsFile()->getNumPyramidLevels();
}

DataType EtsFileScene::getChannelDataType(int) const {
    return getEtsFile()->getDataType();
}

Resolution EtsFileScene::getResolution() const {
    return getEtsFile()->getVolume()->getResolution();
}

double EtsFileScene::getZSliceResolution() const {
    if(getEtsFile() && getEtsFile()->getVolume()) {
        return getEtsFile()->getVolume()->getZResolution();
    }
    return 0.;
}

double EtsFileScene::getTFrameResolution() const {
    if (getEtsFile() && getEtsFile()->getVolume()) {
        return getEtsFile()->getVolume()->getTResolution();
    }
    return 0.;
}

int EtsFileScene::getNumChannels() const {
    if(getEtsFile()) {
        return getEtsFile()->getNumChannels();
    }
    return 0;
}

void EtsFileScene::init()
{
    if(!m_vsiFile) {
        RAISE_RUNTIME_ERROR << "VSIImageDriver: VSI file is not initialized";
    }
    const int etsFileCount = m_vsiFile->getNumEtsFiles();
    const std::shared_ptr<vsi::EtsFile> etsFile = getEtsFile();
    const auto& volume = etsFile->getVolume();
    m_rect = cv::Rect(cv::Point2i(0, 0), etsFile->getSize());
    m_numChannels = etsFile->getNumChannels();
    if(volume) {
        m_name = volume->getName();
        m_magnification = volume->getMagnification();
    }
}

std::shared_ptr<vsi::EtsFile> EtsFileScene::getEtsFile() const
{
    if (m_etsIndex < 0 || m_etsIndex >= m_vsiFile->getNumEtsFiles()) {
        RAISE_RUNTIME_ERROR << "VSIImageDriver: ETS index " << m_etsIndex
            << " is out of range (0-" << m_vsiFile->getNumEtsFiles() << ")";
    }
	return m_vsiFile->getEtsFile(m_etsIndex);
}

int EtsFileScene::findZoomLevelIndex(double zoom) const{
    std::shared_ptr<EtsFile> etsFile = getEtsFile();
    const int levelCount = etsFile->getNumPyramidLevels();
    const int index = Tools::findZoomLevel(zoom, levelCount, [&etsFile](int levelIndex) {
            const EtsFile::PyramidLevel& level = etsFile->getPyramidLevel(levelIndex);
            const double levelZoom = 1./level.scaleLevel;
            return static_cast<double>(levelZoom);
        });
    return index;
}
