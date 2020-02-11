// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/core/imagedriver.hpp"
#include "slideio/drivers/gdal/gdalimagedriver.hpp"
#include "slideio/drivers/gdal/gdalscene.hpp"
#include "slideio/drivers/gdal/gdalslide.hpp"
#include <boost/algorithm/string.hpp>
#include <set>
#include "slideio/gdal_lib.hpp"



slideio::GDALImageDriver::GDALImageDriver()
{
	GDALAllRegister();
}

slideio::GDALImageDriver::~GDALImageDriver()
{
}

std::string slideio::GDALImageDriver::getID() const
{
	return std::string("GDAL");
}


std::shared_ptr<slideio::Slide> slideio::GDALImageDriver::openFile(const std::string& filePath)
{
	GDALDatasetH ds = GDALScene::openFile(filePath);
	slideio::Slide* slide = new GDALSlide(ds, filePath);
	std::shared_ptr<slideio::Slide> ptr(slide);
	return ptr;
}

std::string slideio::GDALImageDriver::getFileSpecs() const
{
	static std::string pattern("*.png;*.jpeg;*.jpg;*.tif;*.tiff;*.bmp;*.gif;*.gtiff;*.gtif;*.ntif;*.jp2");
    return pattern;
}
