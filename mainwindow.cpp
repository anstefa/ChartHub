#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QtCharts/QCandlestickSet>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QJsonArray>
#include <QDebug>

#include <algorithm>
#include <QMouseEvent>
#include <QGraphicsTextItem>
#include <QVBoxLayout>
#include <QSharedPointer>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    chartView(new QChartView(this)),
    chart(new QChart()),
    series(new QCandlestickSeries()),
    networkManager(new QNetworkAccessManager(this)),
    axisX(new QDateTimeAxis()),
    axisY(new QValueAxis()),
    verticalLine(new QGraphicsLineItem(chart)),
    horizontalLine(new QGraphicsLineItem(chart)),
    priceText(new QGraphicsTextItem(chart)),
    isPanning(false),
    visibleCandlesticks(30) // Startwert für sichtbare Candlesticks
{
    ui->setupUi(this);

    // Aktien-Symbol und Zeiteinheit festlegen
    symbol = "IBM"; // Sie können hier das gewünschte Symbol eintragen
    timeInterval = "Daily"; // Zeiteinheit: "Daily", "Weekly", "Monthly"

    // Chart-Titel anpassen
    chart->setTitle(QString("Candlestick Chart: %1 (%2)").arg(symbol).arg(timeInterval));

    // Candlestick-Serie konfigurieren
    series->setName("Aktienkurs");
    series->setIncreasingColor(QColor(Qt::green));
    series->setDecreasingColor(QColor(Qt::red));

    // Chart konfigurieren
    chart->addSeries(series);
    chart->setAnimationOptions(QChart::NoAnimation); // Animationen deaktivieren für flüssigeres Zoomen und Panning

    // Achsen konfigurieren
    axisX->setFormat("dd.MM");
    axisX->setTitleText("Datum");
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);

    axisY->setTitleText("Preis");
    axisY->setLabelFormat("%.2f"); // Zwei Nachkommastellen
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);

    // ChartView konfigurieren
    chartView->setChart(chart);

    // Mouse Tracking aktivieren und Event-Filter installieren
    chartView->setMouseTracking(true);
    chartView->viewport()->installEventFilter(this);

    // Crosshair-Linien initialisieren
    QPen linePen(Qt::DashLine);
    linePen.setColor(Qt::gray);
    verticalLine->setPen(linePen);
    horizontalLine->setPen(linePen);

    verticalLine->setZValue(10);
    horizontalLine->setZValue(10);

    // Preisanzeige initialisieren
    priceText->setZValue(11); // Über den Linien anzeigen
    priceText->setDefaultTextColor(Qt::black);

    // Layout anpassen, um nur den ChartView anzuzeigen
    setCentralWidget(chartView);

    // Netzwerkverbindung herstellen
    connect(networkManager, &QNetworkAccessManager::finished,
            this, &MainWindow::onDataReceived);

    // Daten abrufen
    fetchData();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::fetchData()
{
    QString apiKey = "BI09BQV1KKMT4E44";
    QString function;

    if (timeInterval == "Daily") {
        function = "TIME_SERIES_DAILY";
    } else if (timeInterval == "Weekly") {
        function = "TIME_SERIES_WEEKLY";
    } else if (timeInterval == "Monthly") {
        function = "TIME_SERIES_MONTHLY";
    } else {
        qDebug() << "Unbekannte Zeiteinheit:" << timeInterval;
        return;
    }

    QString url = QString("https://www.alphavantage.co/query?function=%1&symbol=%2&apikey=%3")
                  .arg(function, symbol, apiKey);
    networkManager->get(QNetworkRequest(QUrl(url)));
}

void MainWindow::onDataReceived(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "Netzwerkfehler:" << reply->errorString();
        reply->deleteLater();
        return;
    }

    QByteArray response = reply->readAll();
    QJsonDocument jsonDoc = QJsonDocument::fromJson(response);

    if (!jsonDoc.isNull())
    {
        QJsonObject jsonObj = jsonDoc.object();

        // Überprüfen auf API-Fehlermeldungen
        if (jsonObj.contains("Error Message")) {
            qDebug() << "API Fehler:" << jsonObj.value("Error Message").toString();
            reply->deleteLater();
            return;
        }

        if (jsonObj.contains("Note")) {
            qDebug() << "API Hinweis:" << jsonObj.value("Note").toString();
            reply->deleteLater();
            return;
        }

        QString timeSeriesKey;
        if (timeInterval == "Daily") {
            timeSeriesKey = "Time Series (Daily)";
        } else if (timeInterval == "Weekly") {
            timeSeriesKey = "Weekly Time Series";
        } else if (timeInterval == "Monthly") {
            timeSeriesKey = "Monthly Time Series";
        }

        QJsonObject timeSeries = jsonObj.value(timeSeriesKey).toObject();

        if (timeSeries.isEmpty()) {
            qDebug() << "Keine Zeitreihendaten erhalten.";
            reply->deleteLater();
            return;
        }

        QList<QPair<QDateTime, QSharedPointer<QCandlestickSet>>> dataList;

        // Daten parsen
        for (const QString& dateStr : timeSeries.keys())
        {
            QJsonObject dayData = timeSeries.value(dateStr).toObject();
            double open = dayData.value("1. open").toString().toDouble();
            double high = dayData.value("2. high").toString().toDouble();
            double low = dayData.value("3. low").toString().toDouble();
            double close = dayData.value("4. close").toString().toDouble();

            QDateTime dateTime = QDateTime::fromString(dateStr, "yyyy-MM-dd");
            dateTime.setTimeSpec(Qt::UTC);

            QSharedPointer<QCandlestickSet> set(new QCandlestickSet());
            set->setTimestamp(dateTime.toMSecsSinceEpoch());
            set->setOpen(open);
            set->setHigh(high);
            set->setLow(low);
            set->setClose(close);

            dataList.append(qMakePair(dateTime, set));
        }

        // Daten sortieren
        std::sort(dataList.begin(), dataList.end(), [](const QPair<QDateTime, QSharedPointer<QCandlestickSet>> &a, const QPair<QDateTime, QSharedPointer<QCandlestickSet>> &b){
            return a.first < b.first;
        });

        // Vorhandene Datenliste leeren und neue Daten speichern
        allDataList.clear();
        allDataList = dataList;

        // Achsen initial einstellen
        if (!allDataList.isEmpty())
        {
            int totalCandlesticks = allDataList.size();
            int startIndex = qMax(0, totalCandlesticks - visibleCandlesticks);
            int endIndex = totalCandlesticks;

            QDateTime minDate = allDataList[startIndex].first;
            QDateTime maxDate = allDataList[endIndex - 1].first;

            axisX->setMin(minDate);
            axisX->setMax(maxDate);
        }

        // Chart initial aktualisieren
        updateChart();
    }
    else
    {
        qDebug() << "Fehler beim Parsen der JSON-Daten";
    }

    reply->deleteLater();
}

void MainWindow::updateChart()
{
    if (allDataList.isEmpty())
        return;

    // Aktuellen Achsenbereich erhalten
    QDateTime xMin = axisX->min();
    QDateTime xMax = axisX->max();

    // Start- und Endindex für den sichtbaren Bereich finden
    int startIndex = 0;
    int endIndex = allDataList.size();

    // Finden des Startindex
    for (int i = 0; i < allDataList.size(); ++i)
    {
        if (allDataList[i].first >= xMin)
        {
            startIndex = i;
            break;
        }
    }

    // Finden des Endindex
    for (int i = startIndex; i < allDataList.size(); ++i)
    {
        if (allDataList[i].first > xMax)
        {
            endIndex = i;
            break;
        }
    }

    // Alte Daten aus der Serie entfernen
    series->clear();

    // Sichtbare Daten zur Serie hinzufügen
    for (int i = startIndex; i < endIndex; ++i)
    {
        if (allDataList[i].second)
        {
            QSharedPointer<QCandlestickSet> originalSet = allDataList[i].second;
            QCandlestickSet *newSet = new QCandlestickSet();
            newSet->setTimestamp(originalSet->timestamp());
            newSet->setOpen(originalSet->open());
            newSet->setHigh(originalSet->high());
            newSet->setLow(originalSet->low());
            newSet->setClose(originalSet->close());
            series->append(newSet);
        }
    }

    // Y-Achsenbereich aktualisieren
    if (endIndex > startIndex)
    {
        double minPrice = std::numeric_limits<double>::max();
        double maxPrice = std::numeric_limits<double>::min();

        for (int i = startIndex; i < endIndex; ++i)
        {
            QSharedPointer<QCandlestickSet> set = allDataList[i].second;
            minPrice = std::min(minPrice, set->low());
            maxPrice = std::max(maxPrice, set->high());
        }

        axisY->setMin(minPrice);
        axisY->setMax(maxPrice);
    }
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == chartView->viewport())
    {
        if (event->type() == QEvent::MouseMove)
        {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);

            if (isPanning && mouseEvent->buttons() & Qt::LeftButton)
            {
                // Panning-Logik
                QPointF deltaPixels = mouseEvent->pos() - lastMousePos;
                lastMousePos = mouseEvent->pos();

                QRectF plotArea = chart->plotArea();
                double dx = deltaPixels.x() * (axisX->max().toMSecsSinceEpoch() - axisX->min().toMSecsSinceEpoch()) / plotArea.width();

                QDateTime newMin = axisX->min().addMSecs(-dx);
                QDateTime newMax = axisX->max().addMSecs(-dx);

                // Grenzen setzen, um übermäßiges Panning zu verhindern
                if (newMin < allDataList.first().first)
                {
                    newMin = allDataList.first().first;
                    newMax = newMin.addMSecs(axisX->max().toMSecsSinceEpoch() - axisX->min().toMSecsSinceEpoch());
                }
                else if (newMax > allDataList.last().first)
                {
                    newMax = allDataList.last().first;
                    newMin = newMax.addMSecs(-(axisX->max().toMSecsSinceEpoch() - axisX->min().toMSecsSinceEpoch()));
                }

                axisX->setMin(newMin);
                axisX->setMax(newMax);

                // Chart aktualisieren
                updateChart();

                return true;
            }
            else
            {
                // Crosshair-Code
                QPointF point = chartView->mapToScene(mouseEvent->pos());
                QPointF chartPoint = chart->mapFromScene(point);

                // Überprüfen, ob der Punkt innerhalb des Plotbereichs liegt
                if (chart->plotArea().contains(chartPoint))
                {
                    QPointF value = chart->mapToValue(chartPoint, series);

                    // Crosshair-Linien aktualisieren
                    verticalLine->setLine(chartPoint.x(), chart->plotArea().top(), chartPoint.x(), chart->plotArea().bottom());
                    horizontalLine->setLine(chart->plotArea().left(), chartPoint.y(), chart->plotArea().right(), chartPoint.y());

                    // Preis anzeigen
                    priceText->setPlainText(QString::number(value.y(), 'f', 2));
                    priceText->setPos(chart->plotArea().left(), chartPoint.y() - priceText->boundingRect().height());

                    verticalLine->show();
                    horizontalLine->show();
                    priceText->show();
                }
                else
                {
                    verticalLine->hide();
                    horizontalLine->hide();
                    priceText->hide();
                }
                return true;
            }
        }
        else if (event->type() == QEvent::MouseButtonPress)
        {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton)
            {
                isPanning = true;
                lastMousePos = mouseEvent->pos();
                chartView->setCursor(Qt::ClosedHandCursor);
                return true;
            }
        }
        else if (event->type() == QEvent::MouseButtonRelease)
        {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton)
            {
                isPanning = false;
                chartView->setCursor(Qt::ArrowCursor);
                return true;
            }
        }
        else if (event->type() == QEvent::Wheel)
        {
            QWheelEvent *wheelEvent = static_cast<QWheelEvent *>(event);

            // Mausposition in Chart-Koordinaten erhalten
            QPointF point = chartView->mapToScene(wheelEvent->position().toPoint());
            QPointF chartPoint = chart->mapFromScene(point);

            // Überprüfen, ob der Punkt innerhalb des Plotbereichs liegt
            if (chart->plotArea().contains(chartPoint))
            {
                // Mausposition in Werte umrechnen
                QPointF value = chart->mapToValue(chartPoint);

                // Zoomfaktor bestimmen
                qreal factor;
                if (wheelEvent->angleDelta().y() > 0) // Reinzoomen
                {
                    factor = 0.9; // 10% Reinzoomen
                }
                else // Rauszoomen
                {
                    factor = 1.1; // 10% Rauszoomen
                }

                // Aktuelle Achsenbereiche erhalten
                double xMin = axisX->min().toMSecsSinceEpoch();
                double xMax = axisX->max().toMSecsSinceEpoch();
                double yMin = axisY->min();
                double yMax = axisY->max();

                // Mausposition in Achsenwerten
                double x0 = value.x();
                double y0 = value.y();

                // Neue Achsenbereiche berechnen
                double newXMin = x0 - (x0 - xMin) / factor;
                double newXMax = x0 + (xMax - x0) / factor;
                double newYMin = y0 - (y0 - yMin) / factor;
                double newYMax = y0 + (yMax - y0) / factor;

                // Grenzen setzen, um übermäßiges Zoomen zu verhindern
                double dataXMin = allDataList.first().first.toMSecsSinceEpoch();
                double dataXMax = allDataList.last().first.toMSecsSinceEpoch();

                if (newXMin < dataXMin)
                    newXMin = dataXMin;
                if (newXMax > dataXMax)
                    newXMax = dataXMax;

                // Achsenbereiche nicht invertieren
                if (newXMax - newXMin < 1)
                {
                    newXMin = xMin;
                    newXMax = xMax;
                }

                axisX->setMin(QDateTime::fromMSecsSinceEpoch(newXMin));
                axisX->setMax(QDateTime::fromMSecsSinceEpoch(newXMax));
                axisY->setMin(newYMin);
                axisY->setMax(newYMax);

                // Candlesticks aktualisieren
                updateChart();
            }

            return true; // Ereignis wurde verarbeitet
        }
    }
    return QMainWindow::eventFilter(obj, event);
}
