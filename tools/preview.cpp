// SPDX-FileCopyrightText: 2026 haoyn231
// SPDX-License-Identifier: MIT
//
// 开发工具：把 QML 离屏渲染成 PNG。
//
// 为什么不直接用 `qml` 命令行：plasmoid 里的 i18n() 是 Plasma 注入的全局函数，
// 裸 QML 环境里根本不存在，组件一引用就抛 ReferenceError。这里往引擎的全局对象
// 上塞一个恒等实现，组件代码就不用为了"能在预览里跑"而写一堆 typeof 判断。
//
// 用法：
//   lyrics-preview tools/preview.qml -o /tmp/out.png --width 380 --height 430 --wait 3000

#include <QGuiApplication>
#include <QIcon>
#include <QImage>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QDir>
#include <QQuickItem>
#include <QQuickView>
#include <QTimer>
#include <QCommandLineParser>
#include <QTextStream>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Render a QML file to PNG"));
    parser.addHelpOption();
    parser.addPositionalArgument(QStringLiteral("qml"), QStringLiteral("QML file to render"));
    const QCommandLineOption outOpt({QStringLiteral("o"), QStringLiteral("output")},
                                    QStringLiteral("Output PNG"), QStringLiteral("path"));
    const QCommandLineOption widthOpt(QStringLiteral("width"), QStringLiteral("Window width"),
                                      QStringLiteral("px"), QStringLiteral("380"));
    const QCommandLineOption heightOpt(QStringLiteral("height"), QStringLiteral("Window height"),
                                       QStringLiteral("px"), QStringLiteral("430"));
    const QCommandLineOption waitOpt(QStringLiteral("wait"),
                                     QStringLiteral("Wait before grabbing (ms)"),
                                     QStringLiteral("ms"), QStringLiteral("2000"));
    const QCommandLineOption twoLinesOpt(QStringLiteral("two-lines"),
                                         QStringLiteral("Set previewTwoLines=true"));
    const QCommandLineOption scaleOpt(QStringLiteral("scale"),
                                      QStringLiteral("Device pixel ratio"),
                                      QStringLiteral("factor"), QStringLiteral("1"));
    const QCommandLineOption darkOpt(QStringLiteral("dark"),
                                     QStringLiteral("Use a dark panel background (compact preview)"));
    parser.addOptions({outOpt, widthOpt, heightOpt, waitOpt, twoLinesOpt, scaleOpt, darkOpt});
    parser.process(app);

    const QStringList files = parser.positionalArguments();
    if (files.isEmpty()) {
        QTextStream(stderr) << "missing QML file\n";
        return 2;
    }

    const QString qmlFile = QDir::current().absoluteFilePath(files.first());
    const QString output = parser.value(outOpt);
    if (output.isEmpty()) {
        QTextStream(stderr) << "missing -o output.png\n";
        return 2;
    }

    // 没有平台主题时 QIcon::fromTheme 找不到图标主题，Kirigami.Icon 会画出空白，
    // 预览里的图标就全没了。显式指定一次就好。
    if (QIcon::themeName().isEmpty()) {
        QIcon::setThemeName(QStringLiteral("breeze"));
    }

    QQuickView view;
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.resize(parser.value(widthOpt).toInt(), parser.value(heightOpt).toInt());

    QQmlEngine *engine = view.engine();
    engine->addImportPath(QDir::current().absoluteFilePath(QStringLiteral("package/contents/lib")));

    // 预览里不需要翻译：字符串原样返回，和中文界面一致
    engine->globalObject().setProperty(QStringLiteral("i18n"),
                                       engine->evaluate(QStringLiteral("(function(text){return text;})")));
    view.rootContext()->setContextProperty(QStringLiteral("previewTwoLines"),
                                           parser.isSet(twoLinesOpt));
    view.rootContext()->setContextProperty(QStringLiteral("previewDark"),
                                           parser.isSet(darkOpt));

    view.setSource(QUrl::fromLocalFile(qmlFile));
    if (view.status() != QQuickView::Ready) {
        for (const QQmlError &error : view.errors()) {
            QTextStream(stderr) << error.toString() << "\n";
        }
        return 1;
    }

    view.setColor(Qt::transparent);
    view.show();

    const int waitMs = parser.value(waitOpt).toInt();
    QTimer::singleShot(waitMs, &app, [&view, output, scale = parser.value(scaleOpt).toDouble()]() {
        QImage image = view.grabWindow();
        if (scale > 1.0) {
            image = image.scaled(image.size() * scale, Qt::IgnoreAspectRatio,
                                 Qt::SmoothTransformation);
        }
        if (!image.save(output)) {
            QTextStream(stderr) << "failed to save " << output << "\n";
            QGuiApplication::exit(1);
            return;
        }
        QTextStream(stdout) << "saved " << output << " (" << image.width() << "x"
                           << image.height() << ")\n";
        QGuiApplication::quit();
    });

    return app.exec();
}
