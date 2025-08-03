#include <QApplication>
#include <QImage>
#include <QImageWriter>
#include <QPainter>
#include <QColorSpace>
#include <QtMath>
#include <QFileDialog>
#include <QMessageBox>
#include <QVector>
#include <iostream>

// 表盘配置参数
struct DialConfig {
    int imageWidth = 2778;         // 图片宽度
    int imageHeight = 2363;        // 图片高度
    int imageDPI = 2400;           // 图片DPI
    int imageBitDepth = 40;        // 图片位深度
    int dialRadius = 1000;         // 表盘半径（根据新尺寸调整）
    double startAngle = -135.0;    // 起始角度（度）
    double endAngle = 135.0;       // 结束角度（度）
    double maxPressure = 50.0;     // 最大压力值
    QColor backgroundColor = Qt::white;
    QColor dialColor = Qt::black;
    QColor scaleColor = Qt::black;
    QColor numberColor = Qt::black;
};

static inline int dpi_to_dpm(double dpi) { return qRound(dpi / 0.0254); } // = dpi*39.370079

// 极坐标/角度工具函数
static inline int deg16(double deg){ return int(std::round(deg*16.0)); }
static inline QPointF pol2(const QPointF& c, double angDeg, double r){
    const double a = qDegreesToRadians(angDeg);
    return QPointF(c.x() + r*std::cos(a), c.y() - r*std::sin(a)); // Qt 坐标 y 向下
}
static inline double v2ang(double v, double vmax, double startDeg, double spanDeg){
    return startDeg + (v/vmax)*spanDeg;
}

// 绘制刻度和数字
void drawTicksAndNumbers(QPainter& p, const QPointF& C, double outerR,
                        double startDeg, double spanDeg,
                        double vmax, double majorStep, int minorPerMajor) {
    // outerR 就是图纸中 R17.1 对应的像素半径，直接按比例计算其他半径
    const double r17_1 = outerR;                        // R17.1：最外圆
    const double r16_1 = outerR * (16.1 / 17.1);       // R16.1：彩色带内圈
    const double r15_1 = outerR * (15.1 / 17.1);       // R15.1：小刻度线外沿
    const double r14_6 = outerR * (14.6 / 17.1);       // R14.6：大刻度线内沿
    const double r13_0 = outerR * (13.0 / 17.1);       // R14.0：数字位置
    // 刻度线宽：根据实际半径调整，保持合理比例
    const double lineScale = outerR / 300.0;  // 基准缩放
    QPen penMinor(Qt::black, std::max(1.0, 0.2 * lineScale * 10), Qt::SolidLine, Qt::RoundCap);    // 小刻度：平直端点
    QPen penMajor(Qt::black, std::max(2.0, 0.4 * lineScale * 10), Qt::SolidLine, Qt::RoundCap);   // 大刻度：圆形端点

    // 次刻度（1 MPa间隔）
    p.setPen(penMinor);
    const double minorStep = 1.0;
    for (double v = 0; v <= vmax + 1e-6; v += minorStep) {
        const double ang = v2ang(v, vmax, startDeg, spanDeg);
        p.drawLine(pol2(C, ang, r16_1), pol2(C, ang, r15_1));
    }

    // 主刻度（5 MPa间隔）
    p.setPen(penMajor);
    const double majorTickStep = 5.0;
    for (double v = 0; v <= vmax + 1e-6; v += majorTickStep) {
        const double ang = v2ang(v, vmax, startDeg, spanDeg);
        p.drawLine(pol2(C, ang, r16_1), pol2(C, ang, r14_6));
    }

    // 数字字体：直接使用合理的固定字体大小，不要基于比例计算
    const int fontNumberPx = 3;  // 固定合理字体大小
    QFont numFont("Arial", fontNumberPx, QFont::Bold);
    p.setFont(numFont);
    p.setPen(QPen(Qt::black, 1));

    // 数字：0,10,20,30,40,50（每10MPa一个数字）
    for (double v = 0; v <= vmax + 1e-6; v += majorStep) {
        const double ang = v2ang(v, vmax, startDeg, spanDeg);
        const QPointF pos = pol2(C, ang, r13_0);
        const QString t = QString::number(int(v));
        QRectF br = p.fontMetrics().boundingRect(t);
        br.moveCenter(pos);
        p.drawText(br, Qt::AlignCenter, t);
    }
}

// 绘制彩色带
void drawColorBandsOverTicks(QPainter& p, const QPointF& C, double outerR,
                             double startDeg, double spanDeg, double vmax)
{
    // ① 彩带半径 = R16.1 ~ R15.1（图纸要求）
    const double r17_1 = outerR;   // 彩带外沿
    const double r16_1 = outerR * (16.1 / 17.1);   // 彩带内沿
    const double r_mid = 0.5 * (r16_1 + r17_1);    // 以中径+粗笔画
    const double bandWidth = (r17_1 - r16_1);

    QRectF arcRect(C.x() - r_mid, C.y() - r_mid, 2*r_mid, 2*r_mid);

    // ② 与刻度一致的“主刻度线宽” —— 与 drawTicksAndNumbers 里的 penMajor 保持一致
    const double lineScale = outerR / 300.0;
    const double w_major   = std::max(2.0, 0.4 * lineScale * 10); // 你的主刻度宽逻辑
    const double deltaDeg  = qRadiansToDegrees(std::atan((w_major * 0.5) / r_mid));
    const bool   clockwise = (spanDeg < 0);

    // ③ 颜色分段
    const QColor Y07("#D8B64C"), G02("#2E5E36"), R03("#6A2A2A");
    struct Seg { double v0, v1; QColor c; };
    const QVector<Seg> segs = {
        { 0.0,  5.9, Y07},
        { 5.9, 21.0, G02},
        {21.0, vmax, R03}
    };

    QPen pen; pen.setWidthF(bandWidth); pen.setCapStyle(Qt::FlatCap);

    // ④ 只在两端做角度补偿；内部边界不补偿（避免断开）
    const double eps = 1e-9;
    for (const auto& s : segs) {
        double a0 = v2ang(s.v0, vmax, startDeg, spanDeg);
        double a1 = v2ang(s.v1, vmax, startDeg, spanDeg);

        if (std::abs(s.v0 - 0.0) < eps) {
            // 左端（0MPa）：让彩带端头贴住 0 的主刻度“外边缘”
            a0 -= (clockwise ? -deltaDeg : +deltaDeg);
        }
        if (std::abs(s.v1 - vmax) < eps) {
            // 右端（Vmax）：让彩带端头贴住 Vmax 主刻度“外边缘”
            a1 -= (clockwise ? +deltaDeg : -deltaDeg);
        }

        pen.setColor(s.c);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawArc(arcRect, deg16(a0), deg16(a1 - a0));
    }
}

// 绘制中央单位
void drawUnitMPa(QPainter& p, const QPointF& C, double outerR) {
    // 直接使用合理的固定字体大小
    double r4_0 = outerR * (4.0 / 17.1);
    double r2_0 = outerR * (2.0 / 17.1);
    double fontPixelHeight = (r4_0 - r2_0) * 2;


    QFont font("DIN Rounded");
    font.setPixelSize(std::round(fontPixelHeight)); // 按像素高度设置
    font.setBold(true); // 若图纸要求粗体，可设置
    p.setFont(font);

    p.setPen(QPen(Qt::black, 1));

    const QString MPa = "MPa";
    QRectF textRect = p.fontMetrics().boundingRect(MPa);

    double r3_0 = (r4_0 + r2_0) / 2.0;
    QPointF pos = QPointF(C.x() - textRect.width() / 2.0,
                          C.y() - r3_0 - textRect.height() / 2.0);

    p.drawText(pos, MPa);
}

//=== 生成指定像素尺寸与 DPI 的 TIFF 画布，并绘制表盘 ===//
QImage generateDialImageTIFF(const DialConfig& config)
{
    // 目标规格
    const int OUT_W = config.imageWidth;
    const int OUT_H = config.imageHeight;
    const double OUT_DPI = config.imageDPI;

    // 1) 用 QImage，选择 RGBA64（16bpc/通道），兼容 TIFF
    QImage img(OUT_W, OUT_H, QImage::Format_RGBA64);
    img.fill(config.backgroundColor);

    // 2) 写入 DPI 元数据（TIFF 会映射为 X/YResolution）
    const int dpm = dpi_to_dpm(OUT_DPI);
    img.setDotsPerMeterX(dpm);
    img.setDotsPerMeterY(dpm);

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    // 3) 可选：嵌入 sRGB 色彩空间（印前/跨软件显示更一致）
    img.setColorSpace(QColorSpace::SRgb);
#endif

    // 4) 开始绘制——根据工程图纸精确定位
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    // 根据图纸：圆心位置应该在整个圆形的中心
    // 图纸显示的是一个完整圆形的上半部分，所以圆心应该在画布下半部分
    const QPointF C(OUT_W/2.0, OUT_H * 0.54);           // 圆心位置调整
    
    // 重要：Rpx 就是图纸中 R17.1 对应的像素半径，不需要再做比例换算
    const double Rpx = std::min(OUT_W, OUT_H) * 0.4;    // R17.1的实际像素半径

    // 角度系统：根据图纸，弧形从左上约160°到右上约20°
    const double alpha    = 100.0;                      // 总角度范围140°
    const double startDeg = 90.0 + alpha/2.0;           // 起始角度160°（左上）
    const double spanDeg  = -alpha;                     // 角度跨度-140°（顺时针）

    // 量程与刻度
    const double vmax      = config.maxPressure;
    const double majorStep = 10.0; // 数字：0,10,20,30,40,50
    const int    minorPer  = 10;   // 1 MPa 一格的次刻度

    // ① 先绘制刻度与数字
    drawTicksAndNumbers     (p, C, Rpx, startDeg, spanDeg, vmax, majorStep, minorPer);
    // ② 然后绘制彩色带（覆盖在刻度上层）
    drawColorBandsOverTicks (p, C, Rpx, startDeg, spanDeg, vmax);
    // ③ 最后绘制单位
    drawUnitMPa             (p, C, Rpx);

    p.end();
    return img;
}

//=== 保存为 TIFF ===//
bool saveGeneratedDialTIFF(const QImage& img, const QString& fileName)
{
    QImageWriter w(fileName, "tiff");
    // 压缩：LZW（无损，兼容性好）；也可改 "deflate"
    w.setCompression(1); // 0=无压缩，1=LZW，2=PackBits，32946=Deflate（按 Qt/平台实现而定）
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    // Qt6.5+ 可设置 ICC（如有自定义）
    // w.setColorSpace(QColorSpace::SRgb);
#endif
    if (!w.write(img)) {
        std::cout << "错误：保存失败: " << w.errorString().toStdString() << std::endl;
        return false;
    } else {
        std::cout << "成功：已保存 TIFF: " << fileName.toStdString() << std::endl;
        return true;
    }
}

// 测试函数
int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    // 创建表盘配置
    DialConfig config;
    
    // 生成表盘
    QImage dialImage = generateDialImageTIFF(config);
    
    if (dialImage.isNull()) {
        std::cout << "错误：表盘生成失败" << std::endl;
        return -1;
    }
     
    // 保存表盘
    QString fileName = "test_generated_dial.tiff";
    if (saveGeneratedDialTIFF(dialImage, fileName)) {
        std::cout << "测试完成：表盘已保存为 " << fileName.toStdString() << std::endl;
        return 0;
    } else {
        std::cout << "测试失败：保存表盘时出错" << std::endl;
        return -1;
    }
} 