#include <QApplication>
#include <QScopedPointer>
#include <QUuid>

#include <QtConcurrent/QtConcurrent>
#include <QFutureInterface>
#include <QFuture>

#include <QJsonDocument>
#include <QJsonObject>

#include <QtHttpServer>
#include <QHostAddress>

#include <QChart>
#include <QChartView>
#include <QLineSeries>
#include <QValueAxis>
#include <QBarCategoryAxis>

#include <QWidget>
#include <QGridLayout>

#include <functional>

int main(int argc, char *argv[])
{
    QApplication a {argc, argv};

    QScopedPointer<QHttpServer> httpServer {new QHttpServer {&a}};

    httpServer->route("/charts/line", QHttpServerRequest::Method::Post,
    [&](const QHttpServerRequest &request) -> QFuture<QHttpServerResponse>
    {
        return QtConcurrent::run([&]()
        {
            const QJsonDocument jsonDocument {QJsonDocument::fromJson(request.body())};

            if (jsonDocument.isNull())
            {
                return QHttpServerResponse
                {
                    QJsonObject
                    {
                        {"Message", "Invalid data sent. Please send a valid JSON-Object."}
                    }
                };
            }

            const QJsonObject jsonObject {jsonDocument.object()};

            if (jsonObject.isEmpty())
            {
                return QHttpServerResponse
                {
                    QJsonObject
                    {
                        {"Message", "Invalid data sent. Please send a valid JSON-Object."}
                    }
                };
            }

            for (const QString &key : {"X_Start", "X_End", "Y_Points"})
            {
                if (!jsonObject.contains(key))
                {
                    return QHttpServerResponse
                    {
                        QJsonObject
                        {
                            {"Message", QString{"Invalid data sent. Missing JSON-Key '%0'. Please send a valid JSON-Object."}.arg(key)}
                        }
                    };
                }
            }

            if (!jsonObject.value("X_Start").isDouble())
                return QHttpServerResponse
                {
                    QJsonObject
                    {
                        {"Message", "Invalid data sent. JSON-Key 'X_Start' is not a double value. Please send a valid JSON-Object."}
                    }
                };

            if (!jsonObject.value("X_End").isDouble())
                return QHttpServerResponse
                {
                    QJsonObject
                    {
                        {"Message", "Invalid data sent. JSON-Key 'X_End' is not a double value. Please send a valid JSON-Object."}
                    }
                };

            if (!jsonObject.value("Y_Points").isArray())
                return QHttpServerResponse
                {
                    QJsonObject
                    {
                        {"Message", "Invalid data sent. JSON-Key 'Y_Points' is not an array. Please send a valid JSON-Object."}
                    }
                };

            const qreal xStart {jsonObject.value("X_Start").toDouble()};
            const qreal xEnd   {jsonObject.value("X_End").toDouble()};

            const QJsonArray jsonArray {jsonObject.value("Y_Points").toArray()};

            if (jsonArray.isEmpty())
                return QHttpServerResponse
                {
                    QJsonObject
                    {
                        {"Message", "Invalid data sent. JSON-Key 'Y_Points' is empty. Please send a valid JSON-Object."}
                    }
                };

             if (jsonArray.size() > 1)
                return QHttpServerResponse
                {
                    QJsonObject
                    {
                        {"Message", "Invalid data sent. JSON-Key 'Y_Points' contains more than one array. Please send a valid JSON-Object."}
                    }
                };

            if (jsonArray.first().isNull())
                return QHttpServerResponse
                {
                    QJsonObject
                    {
                        {"Message", "Invalid data sent. Array in JSON-Key 'Y_Points' contains no JSON subobjects. Please send a valid JSON-Object."}
                    }
                };

            for (const QJsonValueConstRef &arrayValue : jsonArray.first().toArray())
            {
                if (!arrayValue.isObject())
                    return QHttpServerResponse
                    {
                        QJsonObject
                        {
                            {"Message", "Invalid data sent. A sub-object in array 'Y_Points' is not a proper JSON-object. Please send a valid JSON-Object."}
                        }
                    };

                const QJsonObject arrayObject {arrayValue.toObject()};

                if (arrayObject.value("Caption").toString().isEmpty())
                    return QHttpServerResponse
                    {
                        QJsonObject
                        {
                            {"Message", "Invalid data sent. A caption of one sub-object in array 'Y_Points' is empty. Please send a valid JSON-Object."}
                        }
                    };

                if (!arrayObject.value("Points").isArray())
                    return QHttpServerResponse
                    {
                        QJsonObject
                        {
                            {"Message", "Invalid data sent. JSON-Key 'Points' of one sub-object in array 'Y_Points' is not an array. Please send a valid JSON-Object."}
                        }
                    };

                for (const QJsonValueConstRef &subObject : arrayObject.value("Points").toArray())
                {
                    if (!subObject.isDouble())
                        return QHttpServerResponse
                        {
                            QJsonObject
                            {
                                {"Message", "Invalid data sent. A point in JSON-Key 'Points' in one sub-object of 'Y_Points' is not a double value. Please send a valid JSON-Object."}
                            }
                        };
                }
            }

            const QVector<QJsonObject> yPointsObjects = [](const QJsonArray &yPointsArray) -> QVector<QJsonObject>
            {
                QVector<QJsonObject> pointsObjects;

                for (const QJsonValueConstRef value : yPointsArray)
                {
                    for (const QJsonValueConstRef &arrayValue : value.toArray())
                        pointsObjects << arrayValue.toObject();
                }

                return pointsObjects;

            }(jsonArray);

            std::function<QVector<qreal>(const QJsonArray &)> convertFromArrayToRealsVector = [](const QJsonArray & jsonArray) -> QVector<qreal>
            {
                QVector<qreal> points;

                for (const QJsonValueConstRef &jsonValueRef : jsonArray)
                    points << jsonValueRef.toVariant().toReal();

                return points;
            };

            const QMap<QString, QVector<qreal> > captionToYPoints = [&convertFromArrayToRealsVector](const QVector<QJsonObject> &yPointsObjects) -> QMap<QString, QVector<qreal> >
            {
                QMap<QString, QVector<qreal> > captionToYPoints;

                for (const QJsonObject &object : yPointsObjects)
                {
                    const QString caption {object.value("Caption").toString()};
                    const QVector<qreal> points {convertFromArrayToRealsVector(object.value("Points").toArray())};

                    captionToYPoints.insert(caption, points);
                }

                return captionToYPoints;

            }(yPointsObjects);

            const QVector<qreal> xPoints = [](const qreal &xStart, const qreal &xEnd)
            {
                QVector<qreal> points;
                points.resize(static_cast<int>(std::abs(xStart) + std::abs(xEnd)));

                std::iota(points.begin(), points.end(), xStart);
                return points;

            }(xStart, xEnd);

            const QScopedPointer<QWidget>     chartWidget {new QWidget};
            const QScopedPointer<QChartView>  chartView   {new QChartView};
            const QScopedPointer<QChart>      chart       {new QChart};
            const QScopedPointer<QGridLayout> gridLayout  {new QGridLayout};

            std::function<QVector<QPointF>(const QVector<qreal>&, const QVector<qreal>&)> mergeCoordinates = [](const QVector<qreal> &xPoints, const QVector<qreal> &yPoints)
            {
                QVector<QPointF> points;

                for (const qreal &x : xPoints)
                    points << QPointF {x, 0};

                QVector<qreal> y {yPoints};
                std::reverse(y.begin(), y.end());

                for (QPointF &point : points)
                {
                    if (!y.isEmpty())
                        point.setY(y.takeLast());
                }

                return points;
            };

            std::function<QColor()> generateNewQColor = []()
            {
                return QColor {static_cast<quint8>(QRandomGenerator::global()->generate() % 255),
                               static_cast<quint8>(QRandomGenerator::global()->generate() % 255),
                               static_cast<quint8>(QRandomGenerator::global()->generate() % 255)};
            };

            for (const QString &caption : captionToYPoints.keys())
            {
                QLineSeries * const lineSeries {new QLineSeries {chart.data()}};

                const QVector<QPointF> coordinates {mergeCoordinates(xPoints, captionToYPoints.value(caption))};
                lineSeries->append(coordinates);
                lineSeries->setColor(generateNewQColor());

                lineSeries->setName(caption);
                chart->addSeries(lineSeries);
            }

            chart->createDefaultAxes();
            chartView->setChart(chart.data());
            gridLayout->addWidget(chartView.data(), 0, 0);
            chartWidget->setLayout(gridLayout.data());

            const QString imageFilename {QUuid::createUuid().toString(QUuid::StringFormat::WithoutBraces) + ".jpg"};
            chartWidget->grab().save(imageFilename);

            return QHttpServerResponse
            {
                QJsonObject
                {
                    {"Link",    QString{"http://127.0.0.1:50001/%0"}.arg(imageFilename)},
                    {"Message", "The provided url will expire in 24 hours."}
                }
            };
        });
    });

    httpServer->route("/charts/line", QHttpServerRequest::Method::Get     |
                                      QHttpServerRequest::Method::Put     |
                                      QHttpServerRequest::Method::Head    |
                                      QHttpServerRequest::Method::Trace   |
                                      QHttpServerRequest::Method::Patch   |
                                      QHttpServerRequest::Method::Delete  |
                                      QHttpServerRequest::Method::Options |
                                      QHttpServerRequest::Method::Connect |
                                      QHttpServerRequest::Method::Unknown,
    [&](const QHttpServerRequest &request) -> QFuture<QHttpServerResponse>
    {
        return QtConcurrent::run([&]()
        {
            return QHttpServerResponse
            {
                QJsonObject
                {
                    {"Message", "The used HTTP-Method is not implemented."}
                }
            };
        });
    });

    if (httpServer->listen(QHostAddress::LocalHost, quint16{50001}) == 0)
        return -100;

    return a.exec();
}
