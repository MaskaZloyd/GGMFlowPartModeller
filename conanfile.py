from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMakeDeps


class GGMFlowPartModellerConan(ConanFile):
    settings = "os", "compiler", "build_type", "arch"

    def requirements(self):
        self.requires("imgui/1.92.6-docking")
        self.requires("glfw/3.4")
        self.requires("eigen/3.4.0")
        self.requires("opengl/system")

    def generate(self):
        tc = CMakeToolchain(self)
        tc.generate()
        deps = CMakeDeps(self)
        deps.generate()
