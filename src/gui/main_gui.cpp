#include <QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[]) {
    // 适配高分屏缩放
    QApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);
    app.setApplicationName("UAVCoverageOptimizer");
    app.setApplicationDisplayName("无人机地面基站覆盖与功率优化系统 [工业仿真平台]");
    app.setOrganizationName("CUMT");

    MainWindow w;

    if (argc >= 3 && std::string(argv[1]) == "--headless-save") {
        w.resize(1360, 900);
        w.show();
        qApp->processEvents();
        w.exportDirectImage(QString::fromLocal8Bit(argv[2]));
        return 0;
    }

    w.show();
    return app.exec();
}
