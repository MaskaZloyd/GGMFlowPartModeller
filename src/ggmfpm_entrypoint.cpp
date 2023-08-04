#include "ggmfpm_mainwindow.hpp"
#include <QApplication>

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);
  GGMFPM::MainWindow window;
  window.show();

  return app.exec();
}
