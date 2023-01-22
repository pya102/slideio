// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_converter_HPP
#define OPENCV_slideio_converter_HPP

#include "slideio/converter/converter_def.hpp"
#include "slideio/core/cvstructs.hpp"
#include "slideio/core/structs.hpp"
#include "slideio/core/slideio_enums.hpp"
#include "slideio/core/cvscene.hpp"

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
   void SLIDEIO_CONVERTER_EXPORTS convertScene(std::shared_ptr<slideio::CVScene> inputScene,
                                                const std::string& driverId,
                                                const std::string& outPath,
                                                const std::map<std::string, std::string>& parameters);
}

#endif