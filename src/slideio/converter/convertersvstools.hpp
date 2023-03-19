// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_convertersvstools_HPP
#define OPENCV_slideio_convertersvstools_HPP

#include "slideio/converter/converter_def.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/imagetools/tiffkeeper.hpp"

namespace slideio
{
    struct ConverterParameters;

    class SLIDEIO_CONVERTER_EXPORTS ConverterSVSTools
    {
    public:
        static void checkSVSRequirements(const CVScenePtr& scene);
        static std::string createDescription(const CVScenePtr& scene, const ConverterParameters& parameters);
        static void createZoomLevel(TIFFKeeperPtr& file, int zoomLevel, const CVScenePtr& scene, ConverterParameters& parameters);
        static void createSVS(TIFFKeeperPtr& file, const CVScenePtr& scene, ConverterParameters& parameters);
    };
}
#endif