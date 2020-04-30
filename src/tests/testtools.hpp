// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include <string>

class TestTools
{
public:
	static std::string getTestImageDirectory();
	static std::string getTestImagePath(const std::string& subfolder, const std::string& image);
};
