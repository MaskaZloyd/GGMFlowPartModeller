#include "ggmfpm_mainwindow.hpp"
#include "../out/build/win/ui_ggmfpm_mainwindow.h"
#include <algorithm>
#include <iterator>
#include <qcustomplot.h>

GGMFPM::MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow) {
  ui->setupUi(this);
  setupPlotWidget();
}

GGMFPM::MainWindow::~MainWindow() { delete ui; }

void GGMFPM::MainWindow::setupPlotWidget() {
  std::vector<QCustomPlot *> plotWidgets{
      ui->meridianLinesPlot, ui->meridianAreaPlot, ui->meridianLinesSpeedPlot};

  for (auto widget : plotWidgets) {
    widget->setInteraction(QCP::iRangeDrag, true);
    widget->setInteraction(QCP::iRangeZoom, true);
    widget->setOpenGl(true);
  }
}
