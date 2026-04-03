#include "Logger.h"
#include "MainWindow.h"

#include <QApplication>
#include <QStyleFactory>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>

// Windows subsystem entry point (no console window in release builds)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR lpCmdLine, int)
{
    int    argc = __argc;
    char** argv = __argv;
#else
int main(int argc, char* argv[])
{
#endif
    QApplication app(argc, argv);
    QApplication::setApplicationName("NDI Capture");
    QApplication::setApplicationVersion("0.1.0");
    QApplication::setOrganizationName("lqx3089");

    // Use Fusion style for consistent look on Windows
    QApplication::setStyle(QStyleFactory::create("Fusion"));

    // Open log file
    Logger::instance().open("ndi-capture");
    LOG_INFO("NDI Capture starting (v0.1.0)");

    MainWindow w;
    w.show();

    int ret = app.exec();
    LOG_INFO("NDI Capture exiting (code " + std::to_string(ret) + ")");
    return ret;
}
