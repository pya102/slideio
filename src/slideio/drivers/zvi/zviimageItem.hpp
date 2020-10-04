// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_zviimagedriver_imageitem_HPP
#define OPENCV_slideio_zviimagedriver_imageitem_HPP
#include <pole/storage.hpp>
#include "slideio/drivers/zvi/zvipixelformat.hpp"
#include "slideio/structs.hpp"

namespace slideio
{
        class   ZVIImageItem
        {
        public:
            ZVIImageItem() = default;
            void setItemIndex(int itemIndex) { m_ItemIndex = itemIndex; }
            int getZIndex() const { return m_ZIndex; }
            int getCIndex() const { return m_CIndex; }
            int getTIndex() const { return m_TIndex; }
            int getPositionIndex() const { return m_PositionIndex; }
            int getSceneIndex() const { return m_SceneIndex; }
            int getItemIndex() const { return m_ItemIndex; }
            std::streamoff getDataOffset() { return m_DataPos; }
            std::string getChannelName() const { return m_ChannelName; }
            ZVIPixelFormat getPixelFormat() const { return m_PixelFormat; }
            int getWidth() const { return m_Width; }
            int getHeight() const { return m_Height; }
            int getZSliceCount() const { return m_ZSliceCount; }
            void setZSliceCount(int depth) { m_ZSliceCount = depth; }
            DataType getDataType() const { return m_DataType; }
            int getChannelCount() const { return m_ChannelCount; }
            void readItemInfo(ole::compound_document& doc);
        private:
            void setWidth(int width) { m_Width = width; }
            void setHeight(int height) { m_Height = height; }
            void setChannelCount(int channels) { m_ChannelCount = channels; }
            void setDataType(DataType dt) { m_DataType = dt; }
            void setPixelFormat(ZVIPixelFormat pixelFormat) { m_PixelFormat = pixelFormat; }
            void setZIndex(int zIndex) { m_ZIndex = zIndex; }
            void setCIndex(int cIndex) { m_CIndex = cIndex; }
            void setTIndex(int tIndex) { m_TIndex = tIndex; }
            void setPositionIndex(int positionIndex) { m_PositionIndex = positionIndex; }
            void setSceneIndex(int sceneIndex) { m_SceneIndex = sceneIndex; }
            void setDataOffset(std::streamoff pos) { m_DataPos = pos; }
            void setChannelName(const std::string& name) { m_ChannelName = name; }
            void readContents(ole::compound_document& doc);
            void readTags(ole::compound_document& doc);
        private:
            int m_ChannelCount = 0;
            int m_Width = 0;
            int m_Height = 0;
            int m_ItemIndex = -1;
            int m_ZIndex = -1;
            int m_CIndex = -1;
            int m_TIndex = -1;
            int m_PositionIndex = -1;
            int m_SceneIndex = -1;
            std::streamoff m_DataPos = 0;
            std::string m_ChannelName;
            ZVIPixelFormat m_PixelFormat = ZVIPixelFormat::PF_UNKNOWN;
            int m_ZSliceCount = 1;
            DataType m_DataType = DataType::DT_Unknown;
        };

}
#endif