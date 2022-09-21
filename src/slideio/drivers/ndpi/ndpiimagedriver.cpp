// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/ndpi/ndpiimagedriver.hpp"
#include "slideio/drivers/ndpi/ndpislide.hpp"
#include <boost/filesystem.hpp>
#include "slideio/core/tools/log.hpp"

slideio::NDPIImageDriver::NDPIImageDriver()
{
}

slideio::NDPIImageDriver::~NDPIImageDriver()
{
}

std::string slideio::NDPIImageDriver::getID() const
{
	return std::string("NDPI");
}

std::shared_ptr<slideio::CVSlide> slideio::NDPIImageDriver::openFile(const std::string& filePath)
{
	std::shared_ptr<NDPISlide> slide(new NDPISlide);
	slide->init(filePath);
	return slide;
}

std::string slideio::NDPIImageDriver::getFileSpecs() const
{
	static std::string pattern("*.ndpi");
    return pattern;
}