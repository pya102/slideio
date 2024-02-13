// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/vsi/etsfile.hpp"
#include "slideio/drivers/vsi/vsistream.hpp"
#include "slideio/drivers/vsi/vsistruct.hpp"
#include "slideio/drivers/vsi/vsitools.hpp"
#include "slideio/core/tools/tools.hpp"
#include "slideio/imagetools/cvtools.hpp"

using namespace slideio;

slideio::vsi::EtsFile::EtsFile(const std::string& filePath) : m_filePath(filePath) {
}

void slideio::vsi::EtsFile::read(std::list<std::shared_ptr<Volume>>& volumes) {
    // Open the file
    m_etsStream = std::make_unique<vsi::VSIStream>(m_filePath);
    vsi::EtsVolumeHeader header = {0};
    m_etsStream->read<vsi::EtsVolumeHeader>(header);
    if (strncmp((char*)header.magic, "SIS", 3) != 0) {
        RAISE_RUNTIME_ERROR << "VSI driver: invalid ETS file header. Expected first tree bytes: 'SIS', got: "
            << header.magic;
    }
    if (header.headerSize != 64) {
        RAISE_RUNTIME_ERROR << "VSI driver: invalid file header. Expected header size: 64, got: "
            << header.headerSize;
    }
    m_etsStream->setPos(header.additionalHeaderPos);
    ETSAdditionalHeader additionalHeader = {0};
    m_etsStream->read<vsi::ETSAdditionalHeader>(additionalHeader);
    if (strncmp((char*)additionalHeader.magic, "ETS", 3) != 0) {
        RAISE_RUNTIME_ERROR << "VSI driver: invalid ETS file header. Expected first tree bytes: 'ETS', got: "
            << header.magic;
    }
    m_numDimensions = static_cast<int>(header.numDimensions);
    m_dataType = VSITools::toSlideioPixelType(additionalHeader.componentType);
    m_numChannels = static_cast<int>(additionalHeader.componentCount);
    m_colorSpace = static_cast<ColorSpace>(additionalHeader.colorSpace);
    m_compression = VSITools::toSlideioCompression(static_cast<vsi::Compression>(additionalHeader.format));
    m_compressionQuality = static_cast<int>(additionalHeader.quality);
    m_tileSize.width = static_cast<int>(additionalHeader.sizeX);
    m_tileSize.height = static_cast<int>(additionalHeader.sizeY);
    m_numZSlices = static_cast<int>(additionalHeader.sizeZ);
    std::memcpy(m_pixelInfoHints, additionalHeader.pixInfoHints, sizeof(m_pixelInfoHints));
    std::memcpy(m_backgroundColor, additionalHeader.background, sizeof(m_backgroundColor));
    m_usePyramid = additionalHeader.usePyramid != 0;

    m_etsStream->setPos(header.usedChunksPos);
    std::vector<TileInfo> tiles;
    tiles.resize(header.numUsedChunks);
    std::vector<int> maxCoordinates(m_numDimensions);
    for (uint chunk = 0; chunk < header.numUsedChunks; ++chunk) {
        TileInfo& tileInfo = tiles[chunk];
        m_etsStream->skipBytes(4);
        tileInfo.coordinates.resize(m_numDimensions);
        for (int i = 0; i < m_numDimensions; ++i) {
            tileInfo.coordinates[i] = m_etsStream->readValue<int32_t>();
            maxCoordinates[i] = std::max(maxCoordinates[i], tileInfo.coordinates[i]);
        }
        tileInfo.offset = m_etsStream->readValue<int64_t>();
        tileInfo.size = m_etsStream->readValue<uint32_t>();
        m_etsStream->skipBytes(4);
    }

    const int minWidth = maxCoordinates[0] * m_tileSize.width;
    const int minHeight = maxCoordinates[1] * m_tileSize.height;
    const int maxWidth = minWidth + m_tileSize.width;
    const int maxHeight = minHeight + m_tileSize.height;

    m_sizeWithCompleteTiles = cv::Size(maxWidth, maxHeight);

    for (auto it = volumes.begin(); it != volumes.end(); ++it) {
        const std::shared_ptr<Volume> volume = *it;
        if (volume->getType() != StackType::DEFAULT_IMAGE && volume->getType() != StackType::OVERVIEW_IMAGE) {
            continue;
        }
        const cv::Size volumeSize = volume->getSize();
        const int volumeWidth = volumeSize.width;
        const int volumeHeight = volumeSize.height;
        if (volumeWidth >= minWidth && volumeWidth <= maxWidth && volumeHeight >= minHeight && volumeHeight <=
            maxHeight) {
            volumes.erase(it);
            assignVolume(volume);
            break;
        }
    }
    if (m_volume) {
        m_size.width = m_volume->getSize().width;
        m_size.height = m_volume->getSize().height;
    }
    else {
        RAISE_RUNTIME_ERROR << "VSI driver: no volume with size " << m_size.width << "x" << m_size.height << " found";
    }

    const int zIndex = m_volume->getDimensionOrder(Dimensions::Z);
    if (zIndex > 1 && zIndex < maxCoordinates.size()) {
        m_numZSlices = maxCoordinates[m_volume->getDimensionOrder(Dimensions::Z)] + 1;
    }
    const int tIndex = m_volume->getDimensionOrder(Dimensions::T);
    if (tIndex > 1 && tIndex < maxCoordinates.size()) {
        m_numTFrames = maxCoordinates[m_volume->getDimensionOrder(Dimensions::T)] + 1;
    }
    const int lambdaIndex = m_volume->getDimensionOrder(Dimensions::L);
    if (lambdaIndex > 1 && lambdaIndex < maxCoordinates.size()) {
        m_numLambdas = maxCoordinates[m_volume->getDimensionOrder(Dimensions::L)] + 1;
    }
    const int channelIndex = m_volume->getDimensionOrder(Dimensions::C);
    if (channelIndex > 1 && channelIndex < maxCoordinates.size()) {
        m_numChannels = maxCoordinates[m_volume->getDimensionOrder(Dimensions::C)] + 1;
    }

    m_pyramid.init(tiles, cv::Size(maxWidth, maxHeight), m_tileSize, m_volume.get());

    const int numChannelIndices = m_pyramid.getNumChannelIndices();
    if (numChannelIndices > 1 && numChannelIndices != getNumChannels()) {
        RAISE_RUNTIME_ERROR << "VSIImageDriver: init: Unexpected number of channel indices "
            << numChannelIndices << ". Expected 1 or " << getNumChannels();
    }
}

void vsi::EtsFile::readTilePart(const vsi::TileInfo& tileInfo, cv::OutputArray tileRaster) {
    const int64_t offset = tileInfo.offset;
    const uint32_t tileCompressedSize = tileInfo.size;
    m_etsStream->setPos(offset);
    m_buffer.resize(tileCompressedSize);
    m_etsStream->readBytes(m_buffer.data(), static_cast<int>(m_buffer.size()));
    tileRaster.create(m_tileSize, CV_MAKETYPE(CVTools::cvTypeFromDataType(m_dataType), m_numChannels));
    if (m_compression == slideio::Compression::Uncompressed) {
        const int tileSize = m_tileSize.width * m_tileSize.height * m_numChannels;
        std::memcpy(tileRaster.getMat().data, m_buffer.data(), tileSize);
    }
    else if (m_compression == slideio::Compression::Jpeg) {
        ImageTools::decodeJpegStream(m_buffer.data(), m_buffer.size(), tileRaster);
    }
    else if (m_compression == slideio::Compression::Jpeg2000) {
        ImageTools::decodeJp2KStream(m_buffer.data(), m_buffer.size(), tileRaster);
    }
    else {
        RAISE_RUNTIME_ERROR << "VSIImageDriver: readTile: Compression " << static_cast<int>(m_compression)
            << " is not supported";
    }
}

void vsi::EtsFile::readTile(int levelIndex,
                            int tileIndex,
                            const std::vector<int>& channelIndices,
                            int zSlice,
                            int tFrame,
                            cv::OutputArray output) {
    if (levelIndex < 0 || levelIndex >= m_pyramid.getNumLevels()) {
        RAISE_RUNTIME_ERROR << "VSIImageDriver: readTile: Pyramid level "
            << levelIndex << " is out of range (0 - " << m_pyramid.getNumLevels() << " )";
    }
    const PyramidLevel& pyramidLevel = m_pyramid.getLevel(levelIndex);

    if (tileIndex < 0 || tileIndex >= pyramidLevel.getNumTiles()) {
        RAISE_RUNTIME_ERROR << "VSIImageDriver: readTile: Tile index "
            << tileIndex << " is out of range (0 - " << pyramidLevel.getNumTiles() << " )";
    }
    const int numChannelIndices = m_pyramid.getNumChannelIndices();

    if (numChannelIndices > 1) {
        std::list<int> channelList(channelIndices.begin(), channelIndices.end());
        if (channelList.empty()) {
            for (int i = 0; i < getNumChannels(); ++i) {
                channelList.push_back(i);
            }
        }
        std::vector<cv::Mat> channelRasters(channelList.size());
        for (const int channelIndex : channelList) {
            if (channelIndex < 0 || channelIndex >= getNumChannels()) {
                RAISE_RUNTIME_ERROR << "VSIImageDriver: readTile: Channel index "
                    << channelIndex << " is out of range (0 - " << numChannelIndices << " )";
            }
            const TileInfo& tileInfo = pyramidLevel.getTile(tileIndex, channelIndex, zSlice, tFrame);
            readTilePart(tileInfo, channelRasters[channelIndex]);
        }
        if (channelRasters.size() == 1) {
            channelRasters[0].copyTo(output);
        }
        else {
            cv::merge(channelRasters, output);
        }
    }
    else {
        cv::Mat tileRaster;
        const TileInfo& tileInfo = pyramidLevel.getTile(tileIndex, 0, zSlice, tFrame);
        readTilePart(tileInfo, tileRaster);
        Tools::extractChannels(tileRaster, channelIndices, output);
    }
}
