/*
 * This is free and unencumbered software released into the public domain.
 */

#pragma once

#include <bgfx/bgfx.h>
#include <bx/bx.h>
#include <bx/allocator.h>
#include <bx/spscqueue.h>
#include <bx/thread.h>
#include <imgui.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <functional>
#include <any>

namespace bigg {
// bgfx utils
const bgfx::Memory *loadMemory(const char *filename);
bgfx::ShaderHandle  loadShader(const char *shader);
bgfx::ProgramHandle loadProgram(const char *vsName, const char *fsName);

// allocator
class Allocator : public bx::AllocatorI {
  public:
    void *realloc(void *_ptr, size_t _size, size_t _align, const char *_file,
                  uint32_t _line) {
        if (_size == 0) {
            free(_ptr);
            return nullptr;
        } else {
            return malloc(_size);
        }
    }
};

// application
class Application {
    static void keyCallback(GLFWwindow *window, int key, int scancode,
                            int action, int mods);
    static void charCallback(GLFWwindow *window, unsigned int codepoint);
    static void charModsCallback(GLFWwindow *window, unsigned int codepoint,
                                 int mods);
    static void mouseButtonCallback(GLFWwindow *window, int button, int action,
                                    int mods);
    static void cursorPosCallback(GLFWwindow *window, double xpos, double ypos);
    static void cursorEnterCallback(GLFWwindow *window, int entered);
    static void scrollCallback(GLFWwindow *window, double xoffset,
                               double yoffset);
    static void dropCallback(GLFWwindow *window, int count, const char **paths);
    static void windowSizeCallback(GLFWwindow *window, int width, int height);

    static int32_t runApiThread(bx::Thread *self, void *userData);

  public:
    struct Event {
        std::function<void(Event &)> delegate;
        Application                 *app;
        std::any                     user_data;
    };

  public:
    Application(const char *title = "", uint32_t width = 1280,
                uint32_t height = 768);

    int run(int argc, char **argv,
            bgfx::RendererType::Enum type = bgfx::RendererType::Count,
            uint16_t vendorId = BGFX_PCI_ID_NONE, uint16_t deviceId = 0,
            bgfx::CallbackI *callback = NULL, bx::AllocatorI *allocator = NULL);

    void         reset(uint32_t flags = 0);
    uint32_t     getWidth() const;
    uint32_t     getHeight() const;
    void         setSize(int width, int height);
    const char  *getTitle() const;
    void         setTitle(const char *title);
    bool         isKeyDown(int key) const;
    bool         isMouseButtonDown(int button) const;
    float        getMouseWheelH() const;
    float        getMouseWheel() const;
    bgfx::ViewId getMainDisplayViewId() const { return kMainDisplayViewId; }

    virtual void initialize(int _argc, char **_argv){};
    virtual void update(float dt){};
    virtual void render(float dt){};
    virtual void postRender(float dt) {}
    virtual int  shutdown() { return 0; };

    virtual void onReset(){};
    virtual void onKey(int key, int scancode, int action, int mods) {}
    virtual void onChar(unsigned int codepoint) {}
    virtual void onCharMods(int codepoint, unsigned int mods) {}
    virtual void onMouseButton(int button, int action, int mods) {}
    virtual void onCursorPos(double xpos, double ypos) {}
    virtual void onCursorEnter(int entered) {}
    virtual void onScroll(double xoffset, double yoffset) {}
    virtual void onDrop(int count, const char **paths) {}
    virtual void onWindowSize(int width, int height) {}

    bool isRunning() const { return mIsRunning; }

    uint32_t currentFrame() const { return mCurrentFrame; }

    void addRenderFunction(std::function<void(Event &)> func,
                           std::any                     user_data = nullptr) {
        auto event = new Event();
        event->delegate = func;
        event->user_data = user_data;
        event->app = this;
        s_apiThreadEvents.push(event);
    }

  protected:
    GLFWwindow     *mWindow;
    bigg::Allocator mAllocator;

    void        setIsRunning(bool is_running) { mIsRunning = is_running; }
    GLFWwindow *window() { return mWindow; }

  private:
    uint32_t           mReset;
    uint32_t           mWidth;
    uint32_t           mHeight;
    const char        *mTitle;
    float              mMouseWheelH = 0.0f;
    float              mMouseWheel = 0.0f;
    bool               mIsRunning = true;
    const bgfx::ViewId kMainDisplayViewId = 0;
    uint32_t           mCurrentFrame = -1;

    static bx::DefaultAllocator                        s_allocator;
    static bx::SpScUnboundedQueueT<Application::Event> s_apiThreadEvents;
};
} // namespace bigg
