#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtCharts/QChartView>
#include <QtCharts/QChart>
#include <QtCharts/QCandlestickSeries>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QValueAxis>
#include <QNetworkAccessManager>
#include <QSharedPointer>
#include <QNetworkReply>
#include <QGraphicsLineItem>
#include <QGraphicsTextItem>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onDataReceived(QNetworkReply *reply);

private:
    Ui::MainWindow *ui;
    QChartView *chartView;
    QChart *chart;
    QCandlestickSeries *series;
    QNetworkAccessManager *networkManager;

    // Achsen
    QDateTimeAxis *axisX;
    QValueAxis *axisY;

    // Neue Mitglieder
    QString symbol;
    QString timeInterval;

    // Crosshair
    QGraphicsLineItem *verticalLine;
    QGraphicsLineItem *horizontalLine;
    QGraphicsTextItem *priceText;

    // Panning
    QPoint lastMousePos;
    bool isPanning;

    int visibleCandlesticks;
    QList<QPair<QDateTime, QSharedPointer<QCandlestickSet>>> allDataList;

    void fetchData();
    void updateChart();
};

#endif // MAINWINDOW_H
