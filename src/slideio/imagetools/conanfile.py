from conan import ConanFile

class ImageToolsRecipe(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    default_options = {"glog/*:shared": True}
    def requirements(self):
        self.requires("boost/1.81.0")
        self.requires("sqlite3/3.44.2")
        self.requires("libxml2/2.11.4")
        self.requires("glog/0.6.0")
        self.requires("opencv/4.1.2")
        self.requires("zlib/1.2.13")
        self.requires("xz_utils/5.4.5")
        self.requires("libtiff/4.6.0")
        self.requires("libjpeg/9e")
        self.requires("libwebp/1.3.2")
        self.requires("libpng/1.6.38")
        self.requires("openjpeg/2.5.2")
        self.requires("jpegxrcodec/1.0.3")
        self.requires("libiconv/1.17")
        self.requires("libdeflate/1.17")
        if self.settings.os == "Windows":
            self.requires("gdal/3.5.2")
        else:
            self.requires("gdal/3.4.3")
            self.requires("openssl/3.1.0")
