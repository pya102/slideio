// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.

#include "etsfilescene.hpp"

using namespace slideio;

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
    
}

int EtsFileScene::getTileCount(void* userData)
{
    return 0;
}

bool EtsFileScene::getTileRect(int tileIndex, cv::Rect& tileRect, void* userData)
{
    return false;
}

bool EtsFileScene::readTile(int tileIndex, const std::vector<int>& channelIndices, cv::OutputArray tileRaster,
    void* userData)
{
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
