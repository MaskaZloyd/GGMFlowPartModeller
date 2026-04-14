from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMakeDeps


class GGMFlowPartModellerConan(ConanFile):
    settings = "os", "compiler", "build_type", "arch"

    def requirements(self):
        self.requires("imgui/1.92.6-docking")

        self.requires("glfw/3.4")
        self.requires("eigen/3.4.0")
        self.requires("opengl/system")
        self.requires("nlohmann_json/3.11.3")
        self.requires("onetbb/2021.12.0")
        self.requires("spdlog/1.14.1")

    def configure(self):
        self.options["onetbb"].shared = False
        self.options["hwloc"].shared = True
        # Use std::format instead of fmt — sidesteps a clang/C++23/fmt 10
        # consteval-checking incompatibility that rejects some of spdlog's
        # internal format strings.
        self.options["spdlog"].use_std_fmt = True

    def generate(self):
        tc = CMakeToolchain(self)
        tc.generate()
        deps = CMakeDeps(self)
        deps.generate()
