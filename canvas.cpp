#include "canvas.h"
#include <QPainterPath>
#include <QFontMetrics>
#include <QDateTime>
#include <cmath>

UAVCanvas::UAVCanvas(QWidget *parent)
    : QWidget(parent), m_model(nullptr), m_viewMode(ViewMode::DualSideBySide),
      m_showCircles(true), m_showLinks(true), m_showPowerTags(true),
      m_selectedUserId(-1), m_hoverUserId(-1), m_hoverUavId(-1), m_hoverSchemeIdx(-1),
      m_zoom(1.0), m_panOffset(0, 0), m_isPanning(false) {
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(600, 450);
}

void UAVCanvas::setModel(UAVSystemModel *model) {
    m_model = model;
    update();
}

void UAVCanvas::setViewMode(ViewMode mode) {
    m_viewMode = mode;
    update();
}

void UAVCanvas::setSelectedUserId(int id) {
    m_selectedUserId = id;
    update();
}

void UAVCanvas::resetView() {
    m_zoom = 1.0;
    m_panOffset = QPointF(0, 0);
    update();
}

bool UAVCanvas::saveImage(const QString &filepath) {
    QPixmap pixmap(size() * 2); // 2x 超分辨率采样导出
    pixmap.setDevicePixelRatio(2.0);
    render(&pixmap);
    return pixmap.save(filepath, "PNG", 95);
}

QPointF UAVCanvas::worldToScreen(const QPointF &wPos, const QRectF &rect) const {
    double area = (m_model ? m_model->getAreaSize() : 100.0);
    double topMargin = 48.0;    // 预留顶部标题空间
    double bottomMargin = 80.0; // 预留底部 HUD 卡片空间
    double sideMargin = 38.0;

    double drawW = rect.width() - 2 * sideMargin;
    double drawH = rect.height() - topMargin - bottomMargin;
    double scale = std::min(drawW, drawH) / area * m_zoom;

    double originX = rect.left() + sideMargin + (drawW - area * scale) / 2.0 + m_panOffset.x();
    double originY = rect.top() + topMargin + (drawH - area * scale) / 2.0 + area * scale + m_panOffset.y();

    double sx = originX + wPos.x() * scale;
    double sy = originY - wPos.y() * scale; // 屏幕 Y 轴向下，笛卡尔 Y 轴向上
    return QPointF(sx, sy);
}

QPointF UAVCanvas::screenToWorld(const QPointF &sPos, const QRectF &rect) const {
    double area = (m_model ? m_model->getAreaSize() : 100.0);
    double topMargin = 48.0;
    double bottomMargin = 80.0;
    double sideMargin = 38.0;

    double drawW = rect.width() - 2 * sideMargin;
    double drawH = rect.height() - topMargin - bottomMargin;
    double scale = std::min(drawW, drawH) / area * m_zoom;

    double originX = rect.left() + sideMargin + (drawW - area * scale) / 2.0 + m_panOffset.x();
    double originY = rect.top() + topMargin + (drawH - area * scale) / 2.0 + area * scale + m_panOffset.y();

    double wx = (sPos.x() - originX) / scale;
    double wy = (originY - sPos.y()) / scale;
    return QPointF(wx, wy);
}

void UAVCanvas::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    // 工业级深色背景
    p.fillRect(rect(), QColor(24, 26, 32));

    if (!m_model || !m_model->hasSolved()) {
        p.setPen(QColor(160, 165, 180));
        p.setFont(QFont("Microsoft YaHei", 12));
        p.drawText(rect(), Qt::AlignCenter, "系统就绪，请在左侧面板点击【一键全流程求解】生成无人机仿真部署");
        return;
    }

    if (m_viewMode == ViewMode::DualSideBySide) {
        double w = width() / 2.0;
        QRectF leftRect(0, 0, w, height());
        QRectF rightRect(w, 0, w, height());

        drawSingleScenario(p, leftRect, m_model->getKMeansResult(), "方案 A：K-Means (K=2) 基准聚类划分", false);
        drawSingleScenario(p, rightRect, m_model->getSAResult(), "方案 B：模拟退火 (SA) Minimax 创新优化", true);

        // 分割线
        p.setPen(QPen(QColor(50, 54, 66), 1.5));
        p.drawLine(QPointF(w, 0), QPointF(w, height()));
    } else if (m_viewMode == ViewMode::KMeansOnly) {
        drawSingleScenario(p, rect(), m_model->getKMeansResult(), "方案 A：K-Means (K=2) 基准聚类划分", false);
    } else {
        drawSingleScenario(p, rect(), m_model->getSAResult(), "方案 B：模拟退火 (SA) Minimax 创新优化", true);
    }

    // 悬浮 HUD 详情弹窗
    if (m_hoverUserId != -1 && m_model) {
        UserNode *cur = m_model->getUserListHead();
        while (cur && cur->id != m_hoverUserId) cur = cur->next;
        if (cur) {
            const SchemeResult &res = (m_hoverSchemeIdx == 1 ? m_model->getSAResult() : m_model->getKMeansResult());
            int did = res.assign[cur->id];
            double d = res.dist_3d[cur->id];
            double h = res.channel[cur->id];
            double power = res.power[cur->id];

            QString hudText = QString("用户 U%1 节点详情\n"
                                      "──────────────────\n"
                                      "坐标: (%2, %3, 0) m\n"
                                      "容量需求 Rn: %4 bps/Hz\n"
                                      "服务基站: UAV %5\n"
                                      "空间距离 dn: %6 m\n"
                                      "信道系数 hn: %7\n"
                                      "发射功率 pn: %8 W")
                                  .arg(cur->id)
                                  .arg(cur->x, 0, 'f', 2)
                                  .arg(cur->y, 0, 'f', 2)
                                  .arg(cur->R, 0, 'f', 2)
                                  .arg(did + 1)
                                  .arg(d, 0, 'f', 2)
                                  .arg(h, 0, 'e', 3)
                                  .arg(power, 0, 'f', 2);

            QFont f("Microsoft YaHei", 9);
            p.setFont(f);
            QFontMetrics fm(f);
            QRect hudRect = fm.boundingRect(QRect(0, 0, 240, 200), Qt::AlignLeft, hudText).adjusted(-10, -8, 10, 8);

            QPoint popupPos = m_hoverPos + QPoint(15, 15);
            if (popupPos.x() + hudRect.width() > width()) popupPos.setX(m_hoverPos.x() - hudRect.width() - 10);
            if (popupPos.y() + hudRect.height() > height()) popupPos.setY(m_hoverPos.y() - hudRect.height() - 10);
            hudRect.moveTo(popupPos);

            p.setBrush(QColor(18, 20, 26, 235));
            p.setPen(QPen(QColor(0, 168, 255), 1.2));
            p.drawRoundedRect(hudRect, 6, 6);

            p.setPen(QColor(235, 240, 255));
            p.drawText(hudRect.adjusted(10, 8, -10, -8), Qt::AlignLeft, hudText);
        }
    }
}

void UAVCanvas::drawSingleScenario(QPainter &p, const QRectF &rect, const SchemeResult &scheme,
                                   const QString &title, bool isSA) {
    p.save();
    p.setClipRect(rect);

    // 绘制坐标网格
    drawGrid(p, rect);

    // 标题条
    p.setPen(QColor(220, 225, 235));
    p.setFont(QFont("Microsoft YaHei", 10, QFont::Bold));
    p.drawText(rect.adjusted(16, 12, -16, 0), Qt::AlignTop | Qt::AlignLeft, title);

    double area = m_model->getAreaSize();
    double scale = std::min(rect.width() - 56.0, rect.height() - 56.0) / area * m_zoom;

    // 调色盘：UAV1 (珊瑚红), UAV2 (青蓝)
    QColor uavColors[2] = {QColor(255, 75, 75), QColor(0, 168, 255)};
    QColor uavFills[2] = {QColor(255, 75, 75, 30), QColor(0, 168, 255, 30)};

    // 1. 绘制无人机覆盖圆与中心点
    for (int i = 0; i < 2; ++i) {
        const Drone &d = scheme.drones[i];
        QPointF centerScreen = worldToScreen(QPointF(d.x, d.y), rect);
        double rScreen = d.max_r * scale;

        if (m_showCircles) {
            p.setBrush(uavFills[i]);
            p.setPen(QPen(uavColors[i], 1.6, Qt::DashLine));
            p.drawEllipse(centerScreen, rScreen, rScreen);
        }

        // 标签牌 (置于无人机上方)
        QString tag = QString("UAV %1\n(%2, %3)\nP=%4W")
                          .arg(i + 1)
                          .arg(d.x, 0, 'f', 1)
                          .arg(d.y, 0, 'f', 1)
                          .arg(d.total_power, 0, 'f', 0);
        p.setFont(QFont("Microsoft YaHei", 7.5, QFont::Bold));
        QRectF tagRect(centerScreen.x() - 46, centerScreen.y() - 52, 92, 40);
        p.setBrush(QColor(18, 20, 26, 220));
        p.setPen(QPen(uavColors[i], 1.0));
        p.drawRoundedRect(tagRect, 4, 4);
        p.setPen(uavColors[i]);
        p.drawText(tagRect, Qt::AlignCenter, tag);

        // 无人机方块图标 (位于中心)
        double boxSize = 12.0;
        QRectF uavBox(centerScreen.x() - boxSize / 2, centerScreen.y() - boxSize / 2, boxSize, boxSize);
        p.setBrush(uavColors[i]);
        p.setPen(QPen(Qt::white, 1.5));
        p.drawRect(uavBox);
    }

    // 2. 绘制用户散点与连线
    const auto &assign = scheme.assign;
    const SchemeResult &kmRes = m_model->getKMeansResult();
    UserNode *cur = m_model->getUserListHead();
    int idx = 0;

    while (cur && idx < static_cast<int>(assign.size())) {
        int did = assign[idx];
        const Drone &dr = scheme.drones[did];

        QPointF uScreen = worldToScreen(QPointF(cur->x, cur->y), rect);
        QPointF dScreen = worldToScreen(QPointF(dr.x, dr.y), rect);

        // 连线
        if (m_showLinks) {
            p.setPen(QPen(QColor(uavColors[did].red(), uavColors[did].green(), uavColors[did].blue(), 90),
                          1.0, Qt::DotLine));
            p.drawLine(uScreen, dScreen);
        }

        // 判断是否为 SA 中被智能重新划分的用户
        bool isReassigned = (isSA && !kmRes.assign.empty() && kmRes.assign[idx] != assign[idx]);
        bool isSelected = (cur->id == m_selectedUserId);

        // 选中高亮光环
        if (isSelected) {
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(QColor(255, 215, 0), 2.5));
            p.drawEllipse(uScreen, 10, 10);
        }

        // 用户节点散点
        if (isReassigned) {
            // 金色五角星/高亮菱形标记
            p.setBrush(QColor(255, 193, 7));
            p.setPen(QPen(Qt::white, 1.2));
            QPolygonF star;
            double s = 6.0;
            star << QPointF(uScreen.x(), uScreen.y() - s)
                 << QPointF(uScreen.x() + s * 0.7, uScreen.y())
                 << QPointF(uScreen.x(), uScreen.y() + s)
                 << QPointF(uScreen.x() - s * 0.7, uScreen.y());
            p.drawPolygon(star);
        } else {
            p.setBrush(uavColors[did]);
            p.setPen(QPen(Qt::white, 1.0));
            p.drawEllipse(uScreen, 4.5, 4.5);
        }

        // 用户 ID 标注
        p.setFont(QFont("Microsoft YaHei", 7.5));
        p.setPen(isReassigned ? QColor(255, 215, 0) : QColor(200, 205, 215));
        QString uLabel = QString("U%1%2").arg(cur->id).arg(isReassigned ? "★" : "");
        p.drawText(uScreen + QPointF(6, -2), uLabel);

        cur = cur->next;
        idx++;
    }

    // 3. 底部信息面板卡片 (HUD Summary)
    QRectF hudBox(rect.left() + 16, rect.bottom() - 72, 220, 56);
    p.setBrush(QColor(18, 20, 26, 215));
    p.setPen(QPen(QColor(60, 65, 80), 1.0));
    p.drawRoundedRect(hudBox, 6, 6);

    p.setFont(QFont("Microsoft YaHei", 8));
    p.setPen(QColor(200, 210, 225));
    QString hudText = QString("瓶颈覆盖半径 max(r): %1 m\n"
                              "最远用户空间距离: %2 m\n"
                              "两机总发射功率: %3 W")
                          .arg(scheme.metrics.max_horizontal_r, 0, 'f', 2)
                          .arg(scheme.metrics.max_3d_dist, 0, 'f', 2)
                          .arg(scheme.metrics.total_power, 0, 'f', 1);
    p.drawText(hudBox.adjusted(8, 6, -8, -6), Qt::AlignLeft | Qt::AlignVCenter, hudText);

    p.restore();
}

void UAVCanvas::drawGrid(QPainter &p, const QRectF &rect) {
    p.save();
    double area = (m_model ? m_model->getAreaSize() : 100.0);

    QPointF p0 = worldToScreen(QPointF(0, 0), rect);
    QPointF p1 = worldToScreen(QPointF(area, area), rect);
    QRectF gridRect(p0.x(), p1.y(), p1.x() - p0.x(), p0.y() - p1.y());

    // 绘制坐标背景
    p.fillRect(gridRect, QColor(28, 31, 38));
    p.setPen(QPen(QColor(65, 70, 85), 1.2));
    p.drawRect(gridRect);

    // 栅格线
    p.setPen(QPen(QColor(42, 46, 56), 0.8, Qt::DashLine));
    p.setFont(QFont("Microsoft YaHei", 7));
    p.setPen(QColor(110, 115, 130));

    int steps = 5;
    for (int i = 0; i <= steps; ++i) {
        double val = (area / steps) * i;
        QPointF spX0 = worldToScreen(QPointF(val, 0), rect);
        QPointF spX1 = worldToScreen(QPointF(val, area), rect);
        p.drawLine(spX0, spX1);
        p.drawText(QPointF(spX0.x() - 10, spX0.y() + 14), QString::number(static_cast<int>(val)));

        QPointF spY0 = worldToScreen(QPointF(0, val), rect);
        QPointF spY1 = worldToScreen(QPointF(area, val), rect);
        p.drawLine(spY0, spY1);
        p.drawText(QPointF(spY0.x() - 22, spY0.y() + 4), QString::number(static_cast<int>(val)));
    }

    p.restore();
}

void UAVCanvas::mouseMoveEvent(QMouseEvent *event) {
    m_hoverPos = event->pos();

    if (m_isPanning) {
        m_panOffset += (event->pos() - m_lastMousePos);
        m_lastMousePos = event->pos();
        update();
        return;
    }

    if (!m_model || !m_model->hasSolved()) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    int oldUser = m_hoverUserId;
    m_hoverUserId = -1;
    m_hoverSchemeIdx = -1;

    auto checkScenario = [&](const QRectF &r, const SchemeResult &res, int sIdx) {
        UserNode *cur = m_model->getUserListHead();
        while (cur) {
            QPointF sp = worldToScreen(QPointF(cur->x, cur->y), r);
            double dist = std::hypot(sp.x() - event->pos().x(), sp.y() - event->pos().y());
            if (dist < 10.0) {
                m_hoverUserId = cur->id;
                m_hoverSchemeIdx = sIdx;
                break;
            }
            cur = cur->next;
        }
    };

    if (m_viewMode == ViewMode::DualSideBySide) {
        double w = width() / 2.0;
        if (event->pos().x() < w) {
            checkScenario(QRectF(0, 0, w, height()), m_model->getKMeansResult(), 0);
        } else {
            checkScenario(QRectF(w, 0, w, height()), m_model->getSAResult(), 1);
        }
    } else if (m_viewMode == ViewMode::KMeansOnly) {
        checkScenario(rect(), m_model->getKMeansResult(), 0);
    } else {
        checkScenario(rect(), m_model->getSAResult(), 1);
    }

    if (m_hoverUserId != oldUser) update();
    QWidget::mouseMoveEvent(event);
}

void UAVCanvas::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::RightButton || event->button() == Qt::MiddleButton) {
        m_isPanning = true;
        m_lastMousePos = event->pos();
        setCursor(Qt::ClosedHandCursor);
    } else if (event->button() == Qt::LeftButton) {
        if (m_hoverUserId != -1) {
            m_selectedUserId = m_hoverUserId;
            emit userClicked(m_selectedUserId);
            update();
        }
    }
    QWidget::mousePressEvent(event);
}

void UAVCanvas::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::RightButton || event->button() == Qt::MiddleButton) {
        m_isPanning = false;
        setCursor(Qt::ArrowCursor);
    }
    QWidget::mouseReleaseEvent(event);
}

void UAVCanvas::wheelEvent(QWheelEvent *event) {
    double factor = (event->angleDelta().y() > 0) ? 1.12 : 0.89;
    m_zoom = std::clamp(m_zoom * factor, 0.5, 4.0);
    update();
    QWidget::wheelEvent(event);
}
