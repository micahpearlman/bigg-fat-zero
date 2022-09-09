/*
 * This is free and unencumbered software released into the public domain.
 */

#include <bigg.hpp>

#if BX_PLATFORM_LINUX || BX_PLATFORM_BSD
#define GLFW_EXPOSE_NATIVE_X11
#define GLFW_EXPOSE_NATIVE_GLX
#elif BX_PLATFORM_OSX
#define GLFW_EXPOSE_NATIVE_COCOA
#define GLFW_EXPOSE_NATIVE_NSGL
#elif BX_PLATFORM_WINDOWS
#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WGL
#endif // BX_PLATFORM_
#include <bx/math.h>
#include <bgfx/platform.h>
#include <GLFW/glfw3native.h>
#include <glm/glm.hpp>
#include <fstream>
#include <iostream>

#include "bigg_assets.h"
#include "bigg_shaders.hpp"
#include "bigg_imgui.hpp"

bx::DefaultAllocator bigg::Application::s_allocator;
bx::SpScUnboundedQueueT<bigg::Application::Event>
    bigg::Application::s_apiThreadEvents(&bigg::Application::s_allocator);

struct ApiThreadArgs {
    bgfx::Init         init;
    bigg::Application *app;
};

// bgfx utils

const bgfx::Memory *bigg::loadMemory(const char *filename) {
    std::ifstream   file(filename, std::ios::binary | std::ios::ate);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    const bgfx::Memory *mem = bgfx::alloc(uint32_t(size + 1));
    if (file.read((char *)mem->data, size)) {
        mem->data[mem->size - 1] = '\0';
        return mem;
    }
    return nullptr;
}

bgfx::ShaderHandle bigg::loadShader(const char *shader) {
    return bgfx::createShader(loadMemory(shader));
}

bgfx::ProgramHandle bigg::loadProgram(const char *vsName, const char *fsName) {
    bgfx::ShaderHandle vs = loadShader(vsName);
    bgfx::ShaderHandle fs = loadShader(fsName);
    return bgfx::createProgram(vs, fs, true);
}

// application

void bigg::Application::keyCallback(GLFWwindow *window, int key, int scancode,
                                    int action, int mods) {
    ImGuiIO &io = ImGui::GetIO();
    if (key >= 0 && key < IM_ARRAYSIZE(io.KeysDown)) {
        if (action == GLFW_PRESS) {
            io.KeysDown[key] = true;
        } else if (action == GLFW_RELEASE) {
            io.KeysDown[key] = false;
        }
    }

    io.KeyCtrl = io.KeysDown[GLFW_KEY_LEFT_CONTROL] ||
                 io.KeysDown[GLFW_KEY_RIGHT_CONTROL];
    io.KeyShift =
        io.KeysDown[GLFW_KEY_LEFT_SHIFT] || io.KeysDown[GLFW_KEY_RIGHT_SHIFT];
    io.KeyAlt =
        io.KeysDown[GLFW_KEY_LEFT_ALT] || io.KeysDown[GLFW_KEY_RIGHT_ALT];
    io.KeySuper =
        io.KeysDown[GLFW_KEY_LEFT_SUPER] || io.KeysDown[GLFW_KEY_RIGHT_SUPER];

    if (!io.WantCaptureKeyboard) {
        bigg::Application *app =
            (bigg::Application *)glfwGetWindowUserPointer(window);
        app->onKey(key, scancode, action, mods);
    }
}

void bigg::Application::charCallback(GLFWwindow  *window,
                                     unsigned int codepoint) {
    bigg::Application *app =
        (bigg::Application *)glfwGetWindowUserPointer(window);
    ImGuiIO &io = ImGui::GetIO();
    io.AddInputCharacter(codepoint);
    app->onChar(codepoint);
}

void bigg::Application::charModsCallback(GLFWwindow  *window,
                                         unsigned int codepoint, int mods) {
    bigg::Application *app =
        (bigg::Application *)glfwGetWindowUserPointer(window);
    app->onCharMods(codepoint, mods);
}

void bigg::Application::mouseButtonCallback(GLFWwindow *window, int button,
                                            int action, int mods) {
    ImGuiIO &io = ImGui::GetIO();
    if (button >= 0 && button < IM_ARRAYSIZE(io.MouseDown)) {
        if (action == GLFW_PRESS) {
            io.MouseDown[button] = true;
        } else if (action == GLFW_RELEASE) {
            io.MouseDown[button] = false;
        }
    }

    if (!io.WantCaptureMouse) {
        bigg::Application *app =
            (bigg::Application *)glfwGetWindowUserPointer(window);
        app->onMouseButton(button, action, mods);
    }
}

void bigg::Application::cursorPosCallback(GLFWwindow *window, double xpos,
                                          double ypos) {
    bigg::Application *app =
        (bigg::Application *)glfwGetWindowUserPointer(window);
    app->onCursorPos(xpos, ypos);
}

void bigg::Application::cursorEnterCallback(GLFWwindow *window, int entered) {
    bigg::Application *app =
        (bigg::Application *)glfwGetWindowUserPointer(window);
    app->onCursorEnter(entered);
}

void bigg::Application::scrollCallback(GLFWwindow *window, double xoffset,
                                       double yoffset) {
    ImGuiIO &io = ImGui::GetIO();
    io.MouseWheelH += (float)xoffset;
    io.MouseWheel += (float)yoffset;

    if (!io.WantCaptureMouse) {
        bigg::Application *app =
            (bigg::Application *)glfwGetWindowUserPointer(window);
        app->mMouseWheelH += (float)xoffset;
        app->mMouseWheel += (float)yoffset;
        app->onScroll(xoffset, yoffset);
    }
}

void bigg::Application::dropCallback(GLFWwindow *window, int count,
                                     const char **paths) {
    bigg::Application *app =
        (bigg::Application *)glfwGetWindowUserPointer(window);
    app->onDrop(count, paths);
}

void bigg::Application::windowSizeCallback(GLFWwindow *window, int width,
                                           int height) {
    bigg::Application *app =
        (bigg::Application *)glfwGetWindowUserPointer(window);
    app->mWidth = width;
    app->mHeight = height;
    app->reset(app->mReset);
    app->onWindowSize(width, height);
}

bigg::Application::Application(const char *title, uint32_t width,
                               uint32_t height) {
    mWidth = width;
    mHeight = height;
    mTitle = title;
}

int32_t bigg::Application::runApiThread(bx::Thread *self, void *userData) {
    ApiThreadArgs     *args = (ApiThreadArgs *)userData;
    bigg::Application *app = args->app;
    if (!bgfx::init(args->init)) {
        throw std::runtime_error("bgfx::init failed");
    }
    // Setup ImGui
    imguiInit(app->window());

    float lastTime = 0;
    float dt;
    float time;
    app->setIsRunning(true);
    while (app->isRunning() == true) {
        time = (float)glfwGetTime();
        dt = time - lastTime;
        lastTime = time;

        // Handle events from the main thread.
        while (auto event = bigg::Application::s_apiThreadEvents.pop()) {
            if (event->delegate != nullptr) {
                event->user_data = app;
                event->delegate(*event);
            }
            delete event;
        }

        // This dummy draw call is here to make sure that view 0 is cleared if
        // no other draw calls are submitted to view 0.
        bgfx::touch(0);

        // Advance to next frame. Main thread will be kicked to process
        // submitted rendering primitives.
        imguiEvents(dt);
        ImGui::NewFrame();

        app->render(dt);

        ImGui::Render();
        imguiRender(ImGui::GetDrawData());        
        bgfx::frame();
    }
    int ret = app->shutdown();
    imguiShutdown();
    bgfx::shutdown();
    return ret;
}

int bigg::Application::run(int argc, char **argv, bgfx::RendererType::Enum type,
                           uint16_t vendorId, uint16_t deviceId,
                           bgfx::CallbackI *callback,
                           bx::AllocatorI  *allocator) {
    // Initialize the glfw
    if (!glfwInit()) {
        std::cerr << "glfwInit failed" << std::endl;
        return -1;
    }

    // Create a window
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    mWindow = glfwCreateWindow(getWidth(), getHeight(), getTitle(), NULL, NULL);
    if (!mWindow) {
        glfwTerminate();
        return -1;
    }

    // Setup input callbacks
    glfwSetWindowUserPointer(mWindow, this);
    glfwSetKeyCallback(mWindow, keyCallback);
    glfwSetCharCallback(mWindow, charCallback);
    glfwSetCharModsCallback(mWindow, charModsCallback);
    glfwSetMouseButtonCallback(mWindow, mouseButtonCallback);
    glfwSetCursorPosCallback(mWindow, cursorPosCallback);
    glfwSetCursorEnterCallback(mWindow, cursorEnterCallback);
    glfwSetScrollCallback(mWindow, scrollCallback);
    glfwSetDropCallback(mWindow, dropCallback);
    glfwSetWindowSizeCallback(mWindow, windowSizeCallback);

    bgfx::renderFrame(); // need this before starting api thread

    // Setup bgfx
    bgfx::PlatformData platformData;
    memset(&platformData, 0, sizeof(platformData));
#if BX_PLATFORM_LINUX || BX_PLATFORM_BSD
    platformData.nwh = (void *)(uintptr_t)glfwGetX11Window(mWindow);
    platformData.ndt = glfwGetX11Display();
#elif BX_PLATFORM_OSX
    platformData.nwh = glfwGetCocoaWindow(mWindow);
#elif BX_PLATFORM_WINDOWS
    platformData.nwh = glfwGetWin32Window(mWindow);
#endif // BX_PLATFORM_
    // bgfx::setPlatformData(platformData);

    // Init bgfx
    bgfx::Init init;
    init.type = type;
    init.vendorId = vendorId;
    init.deviceId = deviceId;
    init.callback = callback;
    init.allocator = allocator;
    init.platformData = platformData;
    init.resolution.width = getWidth();
    init.resolution.height = getHeight();
    init.resolution.reset = BGFX_RESET_VSYNC;

    bx::Thread    apiThread;
    ApiThreadArgs args;
    args.init = init;
    args.app = this;
    apiThread.init(runApiThread, &args);

    // bgfx::init(init);

    // Setup ImGui
    // imguiInit(mWindow);

    // Initialize the application on the render thread   
    reset();
    addRenderFunction([argc,argv](Application::Event& event){
        event.app->initialize(argc, argv);
    });
    

    // Loop until the user closes the window
    float lastTime = 0;
    float dt;
    float time;
    while (!glfwWindowShouldClose(mWindow)) {
        time = (float)glfwGetTime();
        dt = time - lastTime;
        lastTime = time;

        glfwPollEvents();
        // imguiEvents(dt);
        // ImGui::NewFrame();
        update(dt);
        // ImGui::Render();
        // imguiRender(ImGui::GetDrawData());
        // bgfx::frame();

        bgfx::renderFrame();
    }

    // Shutdown application and glfw
    // int ret = shutdown();
    setIsRunning(false);
    // imguiShutdown();
    // bgfx::shutdown();
    glfwTerminate();
    return 0;
}

void bigg::Application::reset(uint32_t flags) {
    mReset = flags;

    addRenderFunction([](bigg::Application::Event &event) {
        bgfx::reset(event.app->getWidth(), event.app->getHeight(),
                    event.app->mReset);
        imguiReset(uint16_t(event.app->getWidth()),
                   uint16_t(event.app->getHeight()));
        event.app->onReset();
    });
}

uint32_t bigg::Application::getWidth() const { return mWidth; }

uint32_t bigg::Application::getHeight() const { return mHeight; }

void bigg::Application::setSize(int width, int height) {
    glfwSetWindowSize(mWindow, width, height);
}

const char *bigg::Application::getTitle() const { return mTitle; }

void bigg::Application::setTitle(const char *title) {
    mTitle = title;
    glfwSetWindowTitle(mWindow, title);
}

bool bigg::Application::isKeyDown(int key) const {
    ImGuiIO &io = ImGui::GetIO();
    if (key < GLFW_KEY_SPACE || key > GLFW_KEY_LAST || io.WantCaptureKeyboard) {
        return false;
    }

    return glfwGetKey(mWindow, key) == GLFW_PRESS;
}

bool bigg::Application::isMouseButtonDown(int button) const {
    ImGuiIO &io = ImGui::GetIO();
    if (button < GLFW_MOUSE_BUTTON_1 || button > GLFW_MOUSE_BUTTON_LAST ||
        io.WantCaptureMouse) {
        return false;
    }

    return glfwGetMouseButton(mWindow, button) == GLFW_PRESS;
}

float bigg::Application::getMouseWheelH() const { return mMouseWheelH; }

float bigg::Application::getMouseWheel() const { return mMouseWheel; }
