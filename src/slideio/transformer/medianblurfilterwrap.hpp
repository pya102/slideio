// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/transformer/transformer_def.hpp"
#include <memory>

namespace slideio
{
    class MedianBlurFilter;
    enum class TransformationType;
    class SLIDEIO_TRANSFORMER_EXPORTS MedianBlurFilterWrap
    {
    public:
        MedianBlurFilterWrap();
        int getKernelSize() const;
        void setKernelSize(int kernelSize);
        TransformationType getType() const;
    private:
        std::shared_ptr<MedianBlurFilter> m_filter;
};

}
