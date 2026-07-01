#include "collectionstylefile.h"

#include <QDir>
#include <QFile>
#include <QTextStream>

namespace {
QString valueAfterColon(const QString &line)
{
    const int separator = line.indexOf(':');
    return separator >= 0 ? line.mid(separator + 1).trimmed() : QString();
}

QString symbolName(PointSymbol symbol)
{
    return symbol == PointSymbol::Cross ? "cross" : "circle";
}

PointSymbol parseSymbol(const QString &value)
{
    return value.compare("cross", Qt::CaseInsensitive) == 0
        ? PointSymbol::Cross : PointSymbol::Circle;
}

QString modeName(RasterRenderMode mode)
{
    return mode == RasterRenderMode::Rgb ? "rgb" : "single_band";
}

RasterRenderMode parseMode(const QString &value)
{
    return value.compare("rgb", Qt::CaseInsensitive) == 0
        ? RasterRenderMode::Rgb : RasterRenderMode::SingleBand;
}

QString rampName(ColorRamp ramp)
{
    switch (ramp) {
    case ColorRamp::RedBlue: return "red_blue";
    case ColorRamp::GreenMagenta: return "green_magenta";
    case ColorRamp::Rainbow: return "rainbow";
    case ColorRamp::Categorical: return "categorical";
    default: return "gray";
    }
}

ColorRamp parseRamp(const QString &value)
{
    if (value == "red_blue") return ColorRamp::RedBlue;
    if (value == "green_magenta") return ColorRamp::GreenMagenta;
    if (value == "rainbow") return ColorRamp::Rainbow;
    if (value == "categorical") return ColorRamp::Categorical;
    return ColorRamp::Gray;
}

QString stretchName(StretchMode mode)
{
    switch (mode) {
    case StretchMode::Gamma: return "gamma";
    case StretchMode::Linear: return "linear";
    default: return "none";
    }
}

StretchMode parseStretch(const QString &value)
{
    if (value == "gamma") return StretchMode::Gamma;
    if (value == "linear") return StretchMode::Linear;
    return StretchMode::None;
}

bool parseBool(const QString &value)
{
    return value.compare("true", Qt::CaseInsensitive) == 0
        || value == "1";
}
}

QString CollectionStyleFile::filePath(const QString &collectionDirectory)
{
    return QDir(collectionDirectory).filePath("style.yaml");
}

CollectionStyleData CollectionStyleFile::load(
    const QString &collectionDirectory)
{
    CollectionStyleData data;
    QFile file(filePath(collectionDirectory));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return data;

    QTextStream stream(&file);
    while (!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#'))
            continue;

        const QString key = line.section(':', 0, 0).trimmed();
        const QString value = valueAfterColon(line);
        if (key == "layer") {
            data.kind = value == "point"
                ? CollectionStyleData::Kind::Point
                : CollectionStyleData::Kind::Image;
        } else if (key == "color") {
            const QColor color(value);
            if (color.isValid())
                data.pointStyle.color = color;
        } else if (key == "size") {
            data.pointStyle.size = value.toDouble();
        } else if (key == "symbol") {
            data.pointStyle.symbol = parseSymbol(value);
        } else if (key == "field") {
            data.pointStyle.labelField =
                value == "none" ? QString() : value;
        } else if (key == "label_size") {
            data.pointStyle.labelSize = value.toDouble();
        } else if (key == "opacity") {
            data.imageStyle.opacity = value.toDouble();
        } else if (key == "mode") {
            data.imageStyle.render.mode = parseMode(value);
        } else if (key == "band") {
            data.imageStyle.render.band = qMax(0, value.toInt() - 1);
        } else if (key == "red_band") {
            data.imageStyle.render.redBand = qMax(0, value.toInt() - 1);
        } else if (key == "green_band") {
            data.imageStyle.render.greenBand = qMax(0, value.toInt() - 1);
        } else if (key == "blue_band") {
            data.imageStyle.render.blueBand = qMax(0, value.toInt() - 1);
        } else if (key == "ramp") {
            data.imageStyle.render.colorRamp = parseRamp(value);
        } else if (key == "stretch") {
            data.imageStyle.render.stretchMode = parseStretch(value);
        } else if (key == "gamma") {
            data.imageStyle.render.gamma = value.toDouble();
        } else if (key == "tail_trim") {
            data.imageStyle.render.tailTrim = parseBool(value);
        } else if (key == "trim_left_tail") {
            data.imageStyle.render.trimLeftTail = parseBool(value);
        } else if (key == "trim_right_tail") {
            data.imageStyle.render.trimRightTail = parseBool(value);
        } else if (key == "trim_percent") {
            data.imageStyle.render.trimPercent = value.toDouble();
        } else if (key == "manual_range_enabled") {
            data.imageStyle.render.manualRangeEnabled = parseBool(value);
        } else if (key == "manual_min") {
            data.imageStyle.render.manualMin = value.toDouble();
        } else if (key == "manual_max") {
            data.imageStyle.render.manualMax = value.toDouble();
        }
    }
    return data;
}

bool CollectionStyleFile::save(const QString &collectionDirectory,
                               const CollectionStyleData &data,
                               QString *errorMessage)
{
    QFile file(filePath(collectionDirectory));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        if (errorMessage)
            *errorMessage = "Could not write style.yaml.";
        return false;
    }

    QTextStream stream(&file);
    stream << "schema_version: 1\n";
    if (data.kind == CollectionStyleData::Kind::Point) {
        stream << "layer: point\n";
        stream << "color: " << data.pointStyle.color.name(QColor::HexRgb) << "\n";
        stream << "size: " << data.pointStyle.size << "\n";
        stream << "symbol: " << symbolName(data.pointStyle.symbol) << "\n";
        stream << "label:\n";
        stream << "  field: "
               << (data.pointStyle.labelField.isEmpty()
                       ? "none" : data.pointStyle.labelField)
               << "\n";
        stream << "  label_size: " << data.pointStyle.labelSize << "\n";
    } else if (data.kind == CollectionStyleData::Kind::Image) {
        const RasterRenderStyle &render = data.imageStyle.render;
        stream << "layer: image\n";
        stream << "opacity: " << data.imageStyle.opacity << "\n";
        stream << "render:\n";
        stream << "  mode: " << modeName(render.mode) << "\n";
        stream << "  band: " << render.band + 1 << "\n";
        stream << "  red_band: " << render.redBand + 1 << "\n";
        stream << "  green_band: " << render.greenBand + 1 << "\n";
        stream << "  blue_band: " << render.blueBand + 1 << "\n";
        stream << "  ramp: " << rampName(render.colorRamp) << "\n";
        stream << "  stretch: " << stretchName(render.stretchMode) << "\n";
        stream << "  gamma: " << render.gamma << "\n";
        stream << "  tail_trim: "
               << (render.tailTrim ? "true" : "false") << "\n";
        stream << "  trim_left_tail: "
               << (render.trimLeftTail ? "true" : "false") << "\n";
        stream << "  trim_right_tail: "
               << (render.trimRightTail ? "true" : "false") << "\n";
        stream << "  trim_percent: " << render.trimPercent << "\n";
        stream << "  manual_range_enabled: "
               << (render.manualRangeEnabled ? "true" : "false") << "\n";
        stream << "  manual_min: " << render.manualMin << "\n";
        stream << "  manual_max: " << render.manualMax << "\n";
    } else {
        if (errorMessage)
            *errorMessage = "Unsupported collection style.";
        return false;
    }

    return true;
}

bool CollectionStyleFile::remove(const QString &collectionDirectory,
                                 QString *errorMessage)
{
    const QString path = filePath(collectionDirectory);
    if (!QFile::exists(path) || QFile::remove(path))
        return true;
    if (errorMessage)
        *errorMessage = "Could not remove style.yaml.";
    return false;
}
