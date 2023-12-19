#include <QApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QScopedPointer>
#include <QSettings>
#include <QUuid>
#include <QDebug>
#include <QDir>

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

static const quint64 LOWEST_PORT {49152}, HIGHEST_PORT {65535};

static QColor generateRandomQColor()
{
    return QColor
    {
        static_cast<quint8>(QRandomGenerator::global()->generate() % 255),
        static_cast<quint8>(QRandomGenerator::global()->generate() % 255),
        static_cast<quint8>(QRandomGenerator::global()->generate() % 255)
    };
};

static QVector<QPointF> mergeCoordinates(const QVector<qreal> &xPoints, const QVector<qreal> &yPoints)
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

static QVector<qreal> convertFromArrayToRealsVector(const QJsonArray &jsonArray)
{
    QVector<qreal> points;

    for (const QJsonValueConstRef jsonValueRef : jsonArray)
        points << jsonValueRef.toVariant().toReal();

    return points;
};

int main(int argc, char *argv[])
{
    QApplication app {argc, argv};

    QCoreApplication::setApplicationName("LineChart-Microservice");
    QCoreApplication::setApplicationVersion("1.0.0");

    QCommandLineParser commandlineParser;
    commandlineParser.addHelpOption();
    commandlineParser.addVersionOption();

    commandlineParser.setApplicationDescription(QString
    {
        "Microservice for LineChart-Plotting."
    });

    commandlineParser.process(app);

    if (!QFile::exists(QApplication::applicationDirPath() + QDir::separator() + "settings.ini"))
        commandlineParser.showHelp(-100);

    const QSettings settings {QApplication::applicationDirPath() + QDir::separator() + "settings.ini", QSettings::Format::IniFormat, &app};

    if (!settings.allKeys().contains("server/port"))
        commandlineParser.showHelp(-101);

    const quint64 port {settings.value(QString{"server/port"}).toULongLong()};

    if (port > HIGHEST_PORT || port < LOWEST_PORT)
        commandlineParser.showHelp(-102);

    if (!settings.allKeys().contains("main/imagepath"))
        commandlineParser.showHelp(-103);

    static const QString imagepath {settings.value(QString{"main/imagepath"}).toString()};

    if (imagepath.isEmpty())
        commandlineParser.showHelp(-104);

    if (!QFile::exists(imagepath))
        commandlineParser.showHelp(-105);

    if (QDir::isRelativePath(imagepath))
        commandlineParser.showHelp(-106);

    const QScopedPointer<QHttpServer> httpServer {new QHttpServer {&app}};

    httpServer->route("/charts/line", QHttpServerRequest::Method::Post,
    [](const QHttpServerRequest &request) -> QFuture<QHttpServerResponse>
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

            for (const QString &key : {"X_Start", "X_End", "Points"})
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

            if (!jsonObject.value("Points").isArray())
                return QHttpServerResponse
                {
                    QJsonObject
                    {
                        {"Message", "Invalid data sent. JSON-Key 'Points' is not an array. Please send a valid JSON-Object."}
                    }
                };

            const qreal xStart {jsonObject.value("X_Start").toDouble()};
            const qreal xEnd   {jsonObject.value("X_End").toDouble()};

            const QJsonArray jsonArray {jsonObject.value("Points").toArray()};

            if (jsonArray.isEmpty())
                return QHttpServerResponse
                {
                    QJsonObject
                    {
                        {"Message", "Invalid data sent. JSON-Key 'Points' is empty. Please send a valid JSON-Object."}
                    }
                };

             if (jsonArray.size() > 1)
                return QHttpServerResponse
                {
                    QJsonObject
                    {
                        {"Message", "Invalid data sent. JSON-Key 'Points' contains more than one array. Please send a valid JSON-Object."}
                    }
                };

            if (jsonArray.first().isNull())
                return QHttpServerResponse
                {
                    QJsonObject
                    {
                        {"Message", "Invalid data sent. Array in JSON-Key 'Points' contains no JSON subobjects. Please send a valid JSON-Object."}
                    }
                };

            for (const QJsonValueConstRef arrayValue : jsonArray.first().toArray())
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
                            {"Message", "Invalid data sent. A caption of one sub-object in array 'Points' is empty. Please send a valid JSON-Object."}
                        }
                    };

                if (!arrayObject.value("X_Points").isArray())
                    return QHttpServerResponse
                    {
                        QJsonObject
                        {
                            {"Message", "Invalid data sent. JSON-Key 'X_Points' of one sub-object in array 'Points' is not an array. Please send a valid JSON-Object."}
                        }
                    };

                if (!arrayObject.value("Y_Points").isArray())
                    return QHttpServerResponse
                    {
                        QJsonObject
                        {
                            {"Message", "Invalid data sent. JSON-Key 'Y_Points' of one sub-object in array 'Points' is not an array. Please send a valid JSON-Object."}
                        }
                    };

                for (const QJsonValueConstRef subObject : arrayObject.value("Y_Points").toArray())
                {
                    if (!subObject.isDouble())
                        return QHttpServerResponse
                        {
                            QJsonObject
                            {
                                {"Message", "Invalid data sent. A point in JSON-Key 'Y_Points' in one sub-object of 'Points' is not a double value. Please send a valid JSON-Object."}
                            }
                        };
                }
            }

            const QVector<QJsonObject> pointsObjects = [](const QJsonArray &pointsArray) -> QVector<QJsonObject>
            {
                QVector<QJsonObject> pointsObjects;

                for (const QJsonValueConstRef value : pointsArray)
                {
                    for (const QJsonValueConstRef &arrayValue : value.toArray())
                        pointsObjects << arrayValue.toObject();
                }

                return pointsObjects;

            }(jsonArray);

            const QMap<QString, QPair<QVector<qreal>, QVector<qreal> > > captionToPoints = [](const QVector<QJsonObject> &yPointsObjects) -> QMap<QString, QPair<QVector<qreal>, QVector<qreal> > >
            {
                QMap<QString, QPair<QVector<qreal>, QVector<qreal> > > captionToPoints;

                for (const QJsonObject &object : yPointsObjects)
                {
                    const QString caption {object.value("Caption").toString()};

                    const QVector<qreal> xPoints {convertFromArrayToRealsVector(object.value("X_Points").toArray())};
                    const QVector<qreal> yPoints {convertFromArrayToRealsVector(object.value("Y_Points").toArray())};

                    captionToPoints.insert(caption, {xPoints, yPoints});
                }

                return captionToPoints;

            }(pointsObjects);

            const QVector<qreal> generalXPointsRange = [](const qreal &xStart, const qreal &xEnd)
            {
                QVector<qreal> points;
                points.resize(static_cast<int>(std::abs(xStart) + std::abs(xEnd)));

                std::iota(points.begin(), points.end(), xStart);
                return points;

            }(xStart, xEnd);

            const qreal yStart = [](const QMap<QString, QPair<QVector<qreal>, QVector<qreal> > > &captionToPoints) -> qreal
            {
                QVector<qreal> allYPoints;

                for (const QString &caption : captionToPoints.keys())
                    allYPoints << captionToPoints.value(caption).second;

                if (allYPoints.size() > 1)
                    return *std::min_element(allYPoints.begin(), allYPoints.end());

                return 0;

            }(captionToPoints);

            const qreal yEnd = [](const QMap<QString, QPair<QVector<qreal>, QVector<qreal> > > &captionToPoints) -> qreal
            {
                QVector<qreal> allYPoints;

                for (const QString &caption : captionToPoints.keys())
                    allYPoints << captionToPoints.value(caption).second;

                if (allYPoints.size() > 1)
                    return *std::max_element(allYPoints.begin(), allYPoints.end());

                return 0;

            }(captionToPoints);

            const QScopedPointer<QWidget>     chartWidget {new QWidget};
            const QScopedPointer<QChartView>  chartView   {new QChartView};
            const QScopedPointer<QChart>      chart       {new QChart};
            const QScopedPointer<QGridLayout> gridLayout  {new QGridLayout};

            QValueAxis * const axisX {new QValueAxis};
            axisX->setRange(xStart, xEnd);
            axisX->setTickCount(static_cast<int>(axisX->max() + 1));
            chart->addAxis(axisX, Qt::AlignBottom);

            QValueAxis * const axisY {new QValueAxis};
            axisY->setRange(yStart, yEnd);
            axisY->setTickCount(static_cast<int>(axisY->max() + 1));
            chart->addAxis(axisY, Qt::AlignLeft);

            for (const QString &caption : captionToPoints.keys())
            {
                const QVector<QPointF> coordinates {mergeCoordinates(captionToPoints.value(caption).first, captionToPoints.value(caption).second)};

                QLineSeries * const lineSeries {new QLineSeries {chart.data()}};
                lineSeries->append(coordinates);
                lineSeries->setColor(generateRandomQColor());
                lineSeries->setName(caption);

                chart->addSeries(lineSeries);

                lineSeries->attachAxis(axisX);
                lineSeries->attachAxis(axisY);
            }

            chartView->setChart(chart.data());
            chartView->setRenderHint(QPainter::Antialiasing);
            gridLayout->addWidget(chartView.data(), 0, 0);
            chartWidget->setLayout(gridLayout.data());
            chartWidget->resize(QSize{1024, 768});

            const QString uuid {QUuid::createUuid().toString(QUuid::StringFormat::WithoutBraces)};
            const QString imageFilename {uuid + ".png"};
            chartWidget->grab().save(imagepath + QDir::separator() + imageFilename);

            return QHttpServerResponse
            {
                QJsonObject
                {
                    {"Link",    QString{"http://127.0.0.1:50001/charts/line/img/%0"}.arg(uuid)},
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
    [](const QHttpServerRequest &request) -> QFuture<QHttpServerResponse>
    {
        Q_UNUSED(request)

        return QtConcurrent::run([]()
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

    httpServer->route("/charts/line/result/<arg>", QHttpServerRequest::Method::Get     |
                                                   QHttpServerRequest::Method::Put     |
                                                   QHttpServerRequest::Method::Head    |
                                                   QHttpServerRequest::Method::Trace   |
                                                   QHttpServerRequest::Method::Patch   |
                                                   QHttpServerRequest::Method::Delete  |
                                                   QHttpServerRequest::Method::Options |
                                                   QHttpServerRequest::Method::Connect |
                                                   QHttpServerRequest::Method::Unknown,
    [](const QString &argument) -> QFuture<QHttpServerResponse>
    {
        static std::function<QHttpServerResponse(const QString &)> responseFunction = [](const QString &argument)
        {
            //see, if it is a correct uuid
            const QUuid uuid {QUuid::fromString(argument)};

            if (uuid.isNull())
                return QHttpServerResponse
                {
                    QJsonObject
                    {
                        {"Message", "The submitted argument is not an UUID. Please send a valid UUID."}
                    }
                };

            if (!QFile::exists(imagepath + QDir::separator() + uuid.toString(QUuid::StringFormat::WithoutBraces) + ".png"))
                return QHttpServerResponse
                {
                    QJsonObject
                    {
                        {"Message", "The submitted UUID is either not linked to any chart or already expired. Please contact our support via our e-mail %0 ."}
                    }
                };

            QFile imageFile {imagepath + QDir::separator() + uuid.toString(QUuid::StringFormat::WithoutBraces) + ".png"};

            if (!imageFile.open(QFile::OpenModeFlag::ReadOnly))
                return QHttpServerResponse
                {
                    QJsonObject
                    {
                        {"Message", "An internal error (errorcode 100) has occured. Please contact our support via our e-mail %0 ."}
                    }
                };

            const QByteArray imageFileBytes {imageFile.readAll()};

            return QHttpServerResponse
            {
                QJsonObject
                {
                    {"Message", "The 'Data' entry of this JSON-object contains the base64-encoded png-file data of your chart-plot."},
                    {"Data",    QString{QString{imageFileBytes.toBase64()}.toUtf8()}}
                }
            };
        };

        return QtConcurrent::run(responseFunction, argument);
    });

    if (httpServer->listen(QHostAddress::Any, static_cast<quint16>(port)) == 0)
        commandlineParser.showHelp(-99);

    qDebug() << QCoreApplication::applicationName() << " is running on port: " << port;
    return app.exec();
}
