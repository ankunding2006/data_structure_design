// canvas.cpp — Industrial-grade Qt6 UAV visualization canvas
// Dark aerospace telemetry simulation theme, full animation-driven.

#include "canvas.h"
#include <QPainterPath>
#include <QFontMetrics>
#include <QRadialGradient>
#include <QLinearGradient>
#include <QPixmap>
#include <QScreen>
#include <QApplication>
#include <cmath>
#include <algorithm>

// ── Color palette ──────────────────────────────────────────────────────────
static const QColor BG_DARK(13, 17, 23);          // #0D1117
static const QColor GRID_LINE(22, 27, 34);         // #161B22
static const QColor AXIS_COLOR(70, 80, 100);
static const QColor AXIS_TICK(100, 110, 130);

static const QColor UAV_COLOR[2] = {
    QColor(255, 85,  85),   // UAV1: coral-red
    QColor(0,   168, 255),  // UAV2: sky-blue
};
static const QColor UAV_FILL[2] = {
    QColor(255, 85,  85,  28),
    QColor(0,   168, 255, 28),
};
static const QColor STAR_COLOR(255, 200, 0);       // gold reassigned star
static const QColor SELECT_HALO(255, 215, 0);      // selection halo
static const QColor TOOLTIP_BG(14, 18, 26, 238);
static const QColor TOOLTIP_BORDER(0, 168, 255);

// ── Constructor / Destructor ───────────────────────────────────────────────
UAVCanvas::UAVCanvas(QWidget *parent)
    : QWidget(parent),
      m_model(nullptr),
      m_viewMode(ViewMode::DualSideBySide),
      m_showCircles(true),
      m_showLinks(true),
      m_showPowerTags(true),
      m_selectedUserId(-1),
      m_hoverUserId(-1),
      m_hoverUavId(-1),
      m_hoverSchemeIdx(-1),
      m_zoom(1.0),
      m_panOffset(0.0, 0.0),
      m_isPanning(false),
      m_circleProgress(1.0),
      m_linkOpacity(1.0),
      m_animRunning(false),
      m_pulsePhase(0.0),
      m_hoverScale(1.0),
      m_entranceGroup(nullptr),
      m_circleAnim(nullptr),
      m_linkAnim(nullptr)
{
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(640, 480);

    // Pulse timer for selected-user halo (sinusoidal bounce)
    m_pulseTimer = new QTimer(this);
    m_pulseTimer->setInterval(16); // ~60 fps
    connect(m_pulseTimer, &QTimer::timeout, this, [this]() {
        m_pulsePhase += 0.08; // ~60fps * 0.08 ≈ 5 rad/s
        if (m_pulsePhase > 6.2832) m_pulsePhase -= 6.2832;
        update();
    });
}

UAVCanvas::~UAVCanvas() {
    if (m_entranceGroup) {
        m_entranceGroup->stop();
        delete m_entranceGroup;
    }
}

// ── Public API ─────────────────────────────────────────────────────────────
void UAVCanvas::setModel(UAVSystemModel *model) {
    m_model = model;
    // Reset animation state when model changes
    m_animUserWorldPos.clear();
    m_targetUserWorldPos.clear();
    m_circleProgress = 1.0;
    m_linkOpacity    = 1.0;
    m_animRunning    = false;
    update();
}

void UAVCanvas::setViewMode(ViewMode mode) {
    m_viewMode = mode;
    update();
}

void UAVCanvas::setSelectedUserId(int id) {
    m_selectedUserId = id;
    if (id >= 0) {
        m_pulsePhase = 0.0;
        if (!m_pulseTimer->isActive()) m_pulseTimer->start();
    } else {
        m_pulseTimer->stop();
    }
    update();
}

void UAVCanvas::resetView() {
    // Animated reset using QPropertyAnimation
    if (m_entranceGroup && m_entranceGroup->state() == QAbstractAnimation::Running)
        m_entranceGroup->stop();

    auto *panAnim = new QPropertyAnimation(this, "panOffset", this);
    panAnim->setDuration(400);
    panAnim->setStartValue(m_panOffset);
    panAnim->setEndValue(QPointF(0.0, 0.0));
    panAnim->setEasingCurve(QEasingCurve::OutCubic);

    auto *zoomAnim = new QPropertyAnimation(this, "zoom", this);
    zoomAnim->setDuration(400);
    zoomAnim->setStartValue(m_zoom);
    zoomAnim->setEndValue(1.0);
    zoomAnim->setEasingCurve(QEasingCurve::OutCubic);

    auto *group = new QParallelAnimationGroup(this);
    group->addAnimation(panAnim);
    group->addAnimation(zoomAnim);
    connect(group, &QParallelAnimationGroup::finished, group, &QObject::deleteLater);
    group->start();
}

bool UAVCanvas::saveImage(const QString &filepath) {
    QPixmap pixmap(size() * 2);
    pixmap.setDevicePixelRatio(2.0);
    render(&pixmap);
    return pixmap.save(filepath, "PNG", 95);
}

// ── Entrance animation ─────────────────────────────────────────────────────
void UAVCanvas::playEntranceAnimation() {
    if (!m_model || !m_model->hasSolved()) return;

    // Stop any previous entrance animation
    if (m_entranceGroup) {
        m_entranceGroup->stop();
        m_entranceGroup->deleteLater();
        m_entranceGroup = nullptr;
    }

    m_circleProgress = 1.0;
    m_linkOpacity    = 1.0;
    m_animRunning    = false;

    // Build target positions from model
    m_targetUserWorldPos.clear();
    m_animUserWorldPos.clear();
    UserNode *cur = m_model->getUserListHead();
    while (cur) {
        m_targetUserWorldPos[cur->id] = QPointF(cur->x, cur->y);
        m_animUserWorldPos[cur->id]   = QPointF(cur->x, cur->y);
        cur = cur->next;
    }
    update();

    // Start all users at world center (area/2, area/2)
    double area = m_model->getAreaSize();
    QPointF center(area / 2.0, area / 2.0);
    m_animUserWorldPos.clear();
    for (auto it = m_targetUserWorldPos.cbegin(); it != m_targetUserWorldPos.cend(); ++it) {
        m_animUserWorldPos[it.key()] = center;
    }

    m_entranceGroup = new QParallelAnimationGroup(this);

    // ── Per-user position animations ──
    // We use a QVariantAnimation per user to tween world position
    // Collect user ids first to avoid issues with lambda captures
    QVector<int> userIds;
    for (auto it = m_targetUserWorldPos.cbegin(); it != m_targetUserWorldPos.cend(); ++it) {
        userIds.append(it.key());
    }

    for (int uid : userIds) {
        QPointF target = m_targetUserWorldPos[uid];
        auto *anim = new QVariantAnimation(m_entranceGroup);
        anim->setDuration(700 + (uid % 5) * 40); // slight stagger per user
        anim->setStartValue(QVariant::fromValue(center));
        anim->setEndValue(QVariant::fromValue(target));
        anim->setEasingCurve(QEasingCurve::OutCubic);
        connect(anim, &QVariantAnimation::valueChanged, this, [this, uid](const QVariant &val) {
            m_animUserWorldPos[uid] = val.value<QPointF>();
            update();
        });
        m_entranceGroup->addAnimation(anim);
    }

    // ── Circle radius expansion animation ──
    m_circleAnim = new QVariantAnimation(m_entranceGroup);
    m_circleAnim->setDuration(900);
    m_circleAnim->setStartValue(0.0);
    m_circleAnim->setEndValue(1.0);
    m_circleAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_circleAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant &val) {
        m_circleProgress = val.toDouble();
        update();
    });
    m_entranceGroup->addAnimation(m_circleAnim);

    // ── Link opacity animation (starts after 400ms via delay trick) ──
    m_linkAnim = new QVariantAnimation(m_entranceGroup);
    m_linkAnim->setDuration(600);
    m_linkAnim->setStartValue(0.0);
    m_linkAnim->setEndValue(1.0);
    m_linkAnim->setEasingCurve(QEasingCurve::InOutQuad);
    connect(m_linkAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant &val) {
        m_linkOpacity = val.toDouble();
        update();
    });
    m_entranceGroup->addAnimation(m_linkAnim);

    connect(m_entranceGroup, &QParallelAnimationGroup::finished, this, [this]() {
        m_animRunning    = false;
        m_circleProgress = 1.0;
        m_linkOpacity    = 1.0;
        // Copy final positions
        for (auto it = m_targetUserWorldPos.cbegin(); it != m_targetUserWorldPos.cend(); ++it) {
            m_animUserWorldPos[it.key()] = it.value();
        }
        update();
    });

    m_entranceGroup->start();
}

// ── Coordinate transform ───────────────────────────────────────────────────
QPointF UAVCanvas::worldToScreen(const QPointF &wPos, const QRectF &rect) const {
    double area       = (m_model ? m_model->getAreaSize() : 100.0);
    double topMargin  = 50.0;
    double botMargin  = 88.0;
    double sideMargin = 42.0;

    double drawW = rect.width()  - 2.0 * sideMargin;
    double drawH = rect.height() - topMargin - botMargin;
    double scale = std::min(drawW, drawH) / area * m_zoom;

    double originX = rect.left() + sideMargin + (drawW - area * scale) / 2.0 + m_panOffset.x();
    double originY = rect.top()  + topMargin  + (drawH - area * scale) / 2.0 + area * scale + m_panOffset.y();

    return QPointF(originX + wPos.x() * scale,
                   originY - wPos.y() * scale);
}

QPointF UAVCanvas::screenToWorld(const QPointF &sPos, const QRectF &rect) const {
    double area       = (m_model ? m_model->getAreaSize() : 100.0);
    double topMargin  = 50.0;
    double botMargin  = 88.0;
    double sideMargin = 42.0;

    double drawW = rect.width()  - 2.0 * sideMargin;
    double drawH = rect.height() - topMargin - botMargin;
    double scale = std::min(drawW, drawH) / area * m_zoom;

    double originX = rect.left() + sideMargin + (drawW - area * scale) / 2.0 + m_panOffset.x();
    double originY = rect.top()  + topMargin  + (drawH - area * scale) / 2.0 + area * scale + m_panOffset.y();

    return QPointF((sPos.x() - originX) / scale,
                   (originY  - sPos.y()) / scale);
}

// ── paintEvent ────────────────────────────────────────────────────────────
void UAVCanvas::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);

    // Deep-space background
    p.fillRect(rect(), BG_DARK);

    if (!m_model || !m_model->hasSolved()) {
        // Centered placeholder text
        p.setPen(QColor(130, 140, 165));
        p.setFont(QFont("Microsoft YaHei", 12));
        p.drawText(rect(), Qt::AlignCenter,
                   "系统就绪 —— 请在左侧控制面板点击【一键全流程求解与对比】启动仿真");
        return;
    }

    if (m_viewMode == ViewMode::DualSideBySide) {
        double w = width() / 2.0;
        QRectF leftRect (0, 0, w, height());
        QRectF rightRect(w, 0, w, height());

        drawSingleScenario(p, leftRect,  m_model->getKMeansResult(),
                           "方案 A：K-Means (K=2) 基准聚类划分", false);
        drawSingleScenario(p, rightRect, m_model->getSAResult(),
                           "方案 B：模拟退火 (SA) Minimax 创新优化", true);

        // Gradient vertical divider
        drawDivider(p, w);

    } else if (m_viewMode == ViewMode::KMeansOnly) {
        drawSingleScenario(p, QRectF(rect()), m_model->getKMeansResult(),
                           "方案 A：K-Means (K=2) 基准聚类划分", false);
    } else {
        drawSingleScenario(p, QRectF(rect()), m_model->getSAResult(),
                           "方案 B：模拟退火 (SA) Minimax 创新优化", true);
    }

    // Floating tooltip card (drawn above everything)
    if (m_hoverUserId != -1) drawTooltipCard(p);
}

// ── drawDivider: gradient vertical line ───────────────────────────────────
void UAVCanvas::drawDivider(QPainter &p, double x) {
    QLinearGradient grad(x, 0, x, height());
    grad.setColorAt(0.0,  QColor(0,  168, 255, 0));
    grad.setColorAt(0.25, QColor(0,  168, 255, 180));
    grad.setColorAt(0.5,  QColor(0,  220, 255, 255));
    grad.setColorAt(0.75, QColor(0,  168, 255, 180));
    grad.setColorAt(1.0,  QColor(0,  168, 255, 0));

    QPen pen(QBrush(grad), 1.5);
    p.setPen(pen);
    p.drawLine(QPointF(x, 0), QPointF(x, height()));
}

// ── drawSingleScenario ────────────────────────────────────────────────────
void UAVCanvas::drawSingleScenario(QPainter &p, const QRectF &rect,
                                   const SchemeResult &scheme,
                                   const QString &title, bool isSA)
{
    p.save();
    p.setClipRect(rect);

    drawGrid(p, rect);

    // ── Title bar ──
    {
        QLinearGradient titleGrad(rect.left(), rect.top(), rect.right(), rect.top());
        titleGrad.setColorAt(0, QColor(0, 30, 60, 200));
        titleGrad.setColorAt(1, QColor(0,  0,  0,   0));
        p.fillRect(QRectF(rect.left(), rect.top(), rect.width(), 44), titleGrad);

        QColor titleAccent = isSA ? UAV_COLOR[1] : UAV_COLOR[0];
        p.setPen(titleAccent);
        p.setFont(QFont("Segoe UI", 9, QFont::Bold));
        p.drawText(rect.adjusted(16, 12, -16, 0), Qt::AlignTop | Qt::AlignLeft, title);

        // Accent underline
        p.setPen(QPen(titleAccent, 1.5));
        p.drawLine(QPointF(rect.left() + 14, rect.top() + 34),
                   QPointF(rect.left() + 14 + 160, rect.top() + 34));
    }

    double area  = m_model->getAreaSize();
    double drawW = rect.width()  - 84.0;
    double drawH = rect.height() - 50.0 - 88.0;
    double scale = std::min(drawW, drawH) / area * m_zoom;

    // ── Coverage circles ──
    if (m_showCircles) {
        for (int i = 0; i < 2; ++i) {
            const Drone &d     = scheme.drones[i];
            QPointF     cScr   = worldToScreen(QPointF(d.x, d.y), rect);
            double      rScr   = d.max_r * scale * m_circleProgress;
            drawCoverCircle(p, cScr, rScr, UAV_COLOR[i], m_circleProgress);
        }
    }

    // ── Connection lines ──
    if (m_showLinks) {
        const auto &assign = scheme.assign;
        UserNode *cur = m_model->getUserListHead();
        int idx = 0;
        while (cur && idx < static_cast<int>(assign.size())) {
            int did = assign[idx];
            const Drone &dr = scheme.drones[did];

            // Determine user screen position (animated or final)
            QPointF uWorld = (m_animRunning && m_animUserWorldPos.contains(cur->id))
                             ? m_animUserWorldPos[cur->id]
                             : QPointF(cur->x, cur->y);

            QPointF uScr = worldToScreen(uWorld, rect);
            QPointF dScr = worldToScreen(QPointF(dr.x, dr.y), rect);
            drawConnectionLine(p, uScr, dScr, UAV_COLOR[did], m_linkOpacity);

            cur = cur->next;
            ++idx;
        }
    }

    // ── User nodes ──
    {
        const auto &assign = scheme.assign;
        const SchemeResult &kmRes = m_model->getKMeansResult();
        UserNode *cur = m_model->getUserListHead();
        int idx = 0;

        while (cur && idx < static_cast<int>(assign.size())) {
            int did = assign[idx];

            QPointF uWorld = (m_animRunning && m_animUserWorldPos.contains(cur->id))
                             ? m_animUserWorldPos[cur->id]
                             : QPointF(cur->x, cur->y);

            QPointF uScr = worldToScreen(uWorld, rect);

            bool isReassigned = isSA && !kmRes.assign.empty() && kmRes.assign[idx] != assign[idx];
            bool isSelected   = (cur->id == m_selectedUserId);
            bool isHovered    = (cur->id == m_hoverUserId);

            // Selected user: sinusoidal pulsing halo
            if (isSelected) {
                double pulse  = 10.0 + 4.0 * std::sin(m_pulsePhase);
                double alphaf = 0.55 + 0.45 * std::sin(m_pulsePhase * 1.3);
                int    alpha  = static_cast<int>(alphaf * 255);
                p.setBrush(Qt::NoBrush);
                p.setPen(QPen(QColor(255, 215, 0, alpha), 2.2));
                p.drawEllipse(uScr, pulse, pulse);

                // Outer glow ring
                p.setPen(QPen(QColor(255, 215, 0, alpha / 3), 4.0));
                p.drawEllipse(uScr, pulse + 4.0, pulse + 4.0);
            }

            // Hover enlargement
            double nodeR = isHovered ? 7.0 : 5.0;

            if (isReassigned) {
                // Gold five-pointed star
                drawFiveStar(p, uScr, nodeR + 2.0, STAR_COLOR);
            } else {
                // Glowing circle node with shadow
                // Shadow
                p.setBrush(QColor(0, 0, 0, 80));
                p.setPen(Qt::NoPen);
                p.drawEllipse(uScr + QPointF(1.5, 2.0), nodeR, nodeR);

                // Node body
                QRadialGradient rg(uScr - QPointF(nodeR * 0.3, nodeR * 0.3), nodeR * 2.0);
                rg.setColorAt(0.0, UAV_COLOR[did].lighter(140));
                rg.setColorAt(1.0, UAV_COLOR[did]);
                p.setBrush(rg);
                p.setPen(QPen(Qt::white, 1.0));
                p.drawEllipse(uScr, nodeR, nodeR);
            }

            // User ID label
            p.setFont(QFont("Segoe UI", 7));
            p.setPen(isReassigned ? QColor(255, 210, 60) : QColor(200, 210, 228));
            QString label = QString("U%1").arg(cur->id);
            p.drawText(uScr + QPointF(nodeR + 2.0, 4.0), label);

            cur = cur->next;
            ++idx;
        }
    }

    // ── UAV icons ──
    for (int i = 0; i < 2; ++i) {
        const Drone &d   = scheme.drones[i];
        QPointF     cScr = worldToScreen(QPointF(d.x, d.y), rect);
        drawUAVIcon(p, cScr, UAV_COLOR[i], 18.0);

        // Label badge
        QString tag = QString("UAV %1\n(%2, %3)\nP=%4 W")
                          .arg(i + 1)
                          .arg(d.x, 0, 'f', 1)
                          .arg(d.y, 0, 'f', 1)
                          .arg(d.total_power, 0, 'f', 1);
        p.setFont(QFont("Segoe UI", 7, QFont::Bold));
        QFontMetrics fm(p.font());
        QRectF tagRect = fm.boundingRect(QRect(0, 0, 200, 120),
                                         Qt::AlignCenter | Qt::TextWordWrap, tag).toRectF();
        tagRect.setWidth(tagRect.width() + 14);
        tagRect.setHeight(tagRect.height() + 10);
        tagRect.moveCenter(cScr - QPointF(0, tagRect.height() / 2.0 + 16.0));

        // Badge background
        QLinearGradient bg(tagRect.topLeft(), tagRect.bottomRight());
        bg.setColorAt(0, QColor(18, 22, 32, 230));
        bg.setColorAt(1, QColor(10, 14, 22, 210));
        p.setBrush(bg);
        p.setPen(QPen(UAV_COLOR[i], 1.0));
        p.drawRoundedRect(tagRect, 5, 5);
        p.setPen(UAV_COLOR[i]);
        p.drawText(tagRect, Qt::AlignCenter | Qt::TextWordWrap, tag);
    }

    // ── Bottom HUD card ──
    {
        QRectF hudBox(rect.left() + 14, rect.bottom() - 80, 248, 66);
        QLinearGradient hudGrad(hudBox.topLeft(), hudBox.bottomLeft());
        hudGrad.setColorAt(0, QColor(14, 18, 26, 220));
        hudGrad.setColorAt(1, QColor(10, 14, 20, 200));
        p.setBrush(hudGrad);
        p.setPen(QPen(QColor(40, 50, 70), 1.0));
        p.drawRoundedRect(hudBox, 6, 6);

        // Accent left bar
        p.fillRect(QRectF(hudBox.left(), hudBox.top() + 4,
                          3, hudBox.height() - 8),
                   isSA ? UAV_COLOR[1] : UAV_COLOR[0]);

        p.setFont(QFont("Microsoft YaHei", 8));
        p.setPen(QColor(190, 200, 220));
        QString hudText =
            QString("瓶颈覆盖半径 max(r): %1 m\n"
                    "最远用户三维距离: %2 m\n"
                    "总发射功率: %3 W")
                .arg(scheme.metrics.max_horizontal_r, 0, 'f', 2)
                .arg(scheme.metrics.max_3d_dist,      0, 'f', 2)
                .arg(scheme.metrics.total_power,       0, 'f', 2);
        p.drawText(hudBox.adjusted(12, 8, -8, -6),
                   Qt::AlignLeft | Qt::AlignVCenter, hudText);
    }

    // ── Legend (bottom-right) ──
    drawLegend(p, rect, isSA);

    p.restore();
}

// ── drawGrid ──────────────────────────────────────────────────────────────
void UAVCanvas::drawGrid(QPainter &p, const QRectF &rect) {
    p.save();

    double area = m_model ? m_model->getAreaSize() : 100.0;

    QPointF orig = worldToScreen(QPointF(0,    0),    rect);
    QPointF topR = worldToScreen(QPointF(area, area), rect);

    QRectF gridRect(orig.x(), topR.y(), topR.x() - orig.x(), orig.y() - topR.y());

    // Subtle dark background for plot area
    p.fillRect(gridRect, QColor(16, 20, 28));
    p.setPen(QPen(QColor(40, 48, 64), 1.0));
    p.drawRect(gridRect);

    // Grid lines
    int steps = 5;
    p.setFont(QFont("Segoe UI", 7));
    for (int i = 0; i <= steps; ++i) {
        double val = area / steps * i;
        QPointF vBot = worldToScreen(QPointF(val, 0),    rect);
        QPointF vTop = worldToScreen(QPointF(val, area), rect);
        QPointF hLft = worldToScreen(QPointF(0,   val),  rect);
        QPointF hRgt = worldToScreen(QPointF(area, val), rect);

        p.setPen(QPen(GRID_LINE, 0.8));
        p.drawLine(vBot, vTop);
        p.drawLine(hLft, hRgt);

        // Tick labels
        p.setPen(AXIS_TICK);
        p.drawText(QPointF(vBot.x() - 12, vBot.y() + 14),
                   QString::number(static_cast<int>(val)));
        if (i > 0)
            p.drawText(QPointF(hLft.x() - 30, hLft.y() + 4),
                       QString::number(static_cast<int>(val)));
    }

    // Coordinate axes with arrows
    p.setPen(QPen(AXIS_COLOR, 1.5));
    // X-axis arrow
    QPointF axisOrig = worldToScreen(QPointF(0, 0), rect);
    QPointF axisXEnd = worldToScreen(QPointF(area * 1.04, 0), rect);
    QPointF axisYEnd = worldToScreen(QPointF(0, area * 1.04), rect);

    p.drawLine(axisOrig, axisXEnd);
    p.drawLine(axisOrig, axisYEnd);

    // Arrowheads
    double aw = 6.0, ah = 4.0;
    // X arrow
    {
        QPolygonF arr;
        arr << axisXEnd
            << axisXEnd - QPointF(aw, -ah)
            << axisXEnd - QPointF(aw,  ah);
        p.setBrush(AXIS_COLOR);
        p.setPen(Qt::NoPen);
        p.drawPolygon(arr);
    }
    // Y arrow
    {
        QPolygonF arr;
        arr << axisYEnd
            << axisYEnd - QPointF(-ah, -aw)
            << axisYEnd - QPointF( ah, -aw);
        p.setBrush(AXIS_COLOR);
        p.setPen(Qt::NoPen);
        p.drawPolygon(arr);
    }

    // Axis labels
    p.setPen(AXIS_TICK);
    p.setFont(QFont("Segoe UI", 8, QFont::Bold));
    p.drawText(axisXEnd + QPointF(4, 5), "X");
    p.drawText(axisYEnd + QPointF(4, -4), "Y");

    p.restore();
}

// ── drawUAVIcon: quadrotor shape using QPainterPath ───────────────────────
void UAVCanvas::drawUAVIcon(QPainter &p, const QPointF &center, const QColor &color, double size) {
    p.save();
    p.translate(center);

    double arm  = size * 0.55;  // arm half-length
    double prop = size * 0.30;  // propeller radius
    double body = size * 0.22;  // body half-size

    // Glow underneath
    QRadialGradient glow(QPointF(0, 0), size * 1.4);
    glow.setColorAt(0.0, QColor(color.red(), color.green(), color.blue(), 80));
    glow.setColorAt(1.0, QColor(color.red(), color.green(), color.blue(), 0));
    p.setBrush(glow);
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPointF(0, 0), size * 1.4, size * 1.4);

    // Four arms (diagonal cross at 45 deg)
    QPainterPath arms;
    static const double dirs[4][2] = {{1,1},{-1,1},{-1,-1},{1,-1}};
    p.setPen(QPen(color, 2.0));
    for (auto &dir : dirs) {
        p.drawLine(QPointF(0, 0),
                   QPointF(dir[0] * arm * 0.707, dir[1] * arm * 0.707));
    }

    // Propeller discs at each arm tip
    p.setBrush(QColor(color.red(), color.green(), color.blue(), 100));
    p.setPen(QPen(color.lighter(130), 1.0));
    for (auto &dir : dirs) {
        QPointF tip(dir[0] * arm * 0.707, dir[1] * arm * 0.707);
        p.drawEllipse(tip, prop, prop);
    }

    // Body center (hexagon-ish — drawn as two overlaid rects rotated 30 deg)
    p.setBrush(QColor(color.red(), color.green(), color.blue(), 200));
    p.setPen(QPen(Qt::white, 1.0));
    p.drawEllipse(QPointF(0, 0), body, body);

    // Center dot
    p.setBrush(Qt::white);
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPointF(0, 0), body * 0.35, body * 0.35);

    p.restore();
}

// ── drawFiveStar: gold filled five-pointed star ───────────────────────────
void UAVCanvas::drawFiveStar(QPainter &p, const QPointF &center, double radius, const QColor &color) {
    QPainterPath path;
    const double outerR = radius;
    const double innerR = radius * 0.42;
    const double startAngle = -M_PI / 2.0; // point up

    for (int i = 0; i < 10; ++i) {
        double angle = startAngle + i * M_PI / 5.0;
        double r     = (i % 2 == 0) ? outerR : innerR;
        QPointF pt(center.x() + r * std::cos(angle),
                   center.y() + r * std::sin(angle));
        if (i == 0) path.moveTo(pt);
        else         path.lineTo(pt);
    }
    path.closeSubpath();

    // Outer glow
    p.setBrush(QColor(color.red(), color.green(), color.blue(), 60));
    p.setPen(Qt::NoPen);
    p.drawEllipse(center, radius * 1.6, radius * 1.6);

    // Star fill with gradient
    QRadialGradient rg(center - QPointF(outerR * 0.3, outerR * 0.4), outerR * 1.5);
    rg.setColorAt(0.0, color.lighter(140));
    rg.setColorAt(1.0, color);
    p.setBrush(rg);
    p.setPen(QPen(Qt::white, 0.8));
    p.drawPath(path);
}

// ── drawCoverCircle: radial-gradient filled, dual-pass glowing dashed ─────
void UAVCanvas::drawCoverCircle(QPainter &p, const QPointF &center, double rScreen,
                                const QColor &color, double /*progress*/)
{
    if (rScreen < 1.0) return;

    // Radial gradient fill
    QRadialGradient rg(center, rScreen);
    rg.setColorAt(0.0, QColor(color.red(), color.green(), color.blue(), 10));
    rg.setColorAt(0.7, QColor(color.red(), color.green(), color.blue(), 18));
    rg.setColorAt(1.0, QColor(color.red(), color.green(), color.blue(), 45));
    p.setBrush(rg);
    p.setPen(Qt::NoPen);
    p.drawEllipse(center, rScreen, rScreen);

    // Outer glow ring (low-opacity, thicker)
    QPen outerPen(QColor(color.red(), color.green(), color.blue(), 55), 4.0, Qt::DashLine);
    outerPen.setDashPattern({4.0, 6.0});
    p.setBrush(Qt::NoBrush);
    p.setPen(outerPen);
    p.drawEllipse(center, rScreen, rScreen);

    // Inner solid ring
    p.setPen(QPen(QColor(color.red(), color.green(), color.blue(), 160), 1.2, Qt::DashLine));
    p.drawEllipse(center, rScreen, rScreen);
}

// ── drawConnectionLine: directional dot-dash with arrowhead ───────────────
void UAVCanvas::drawConnectionLine(QPainter &p, const QPointF &from, const QPointF &to,
                                   const QColor &color, double opacity)
{
    if (opacity < 0.01) return;

    int a = static_cast<int>(opacity * 90);
    QPen pen(QColor(color.red(), color.green(), color.blue(), a), 1.0, Qt::DotLine);
    pen.setDashPattern({2.0, 5.0});
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawLine(from, to);

    // Small arrowhead near UAV end
    if (opacity > 0.5) {
        QPointF dir = to - from;
        double  len = std::sqrt(dir.x() * dir.x() + dir.y() * dir.y());
        if (len < 1.0) return;
        dir /= len;
        QPointF arrowTip = to - dir * 10.0;
        QPointF perp(-dir.y(), dir.x());
        QPolygonF arr;
        arr << to - dir * 12.0 + perp * 3.5
            << arrowTip
            << to - dir * 12.0 - perp * 3.5;
        p.setBrush(QColor(color.red(), color.green(), color.blue(),
                          static_cast<int>(opacity * 120)));
        p.setPen(Qt::NoPen);
        p.drawPolygon(arr);
    }
}

// ── drawTooltipCard: self-drawn floating tooltip ──────────────────────────
void UAVCanvas::drawTooltipCard(QPainter &p) {
    if (!m_model) return;

    UserNode *cur = m_model->getUserListHead();
    while (cur && cur->id != m_hoverUserId) cur = cur->next;
    if (!cur) return;

    const SchemeResult &res = (m_hoverSchemeIdx == 1)
                              ? m_model->getSAResult()
                              : m_model->getKMeansResult();

    if (cur->id >= static_cast<int>(res.assign.size())) return;

    int    did   = res.assign[cur->id];
    double d3    = res.dist_3d[cur->id];
    double chan  = res.channel[cur->id];
    double pwr   = res.power[cur->id];

    // Check if reassigned in SA
    bool isReas = false;
    if (m_hoverSchemeIdx == 1) {
        const SchemeResult &km = m_model->getKMeansResult();
        if (!km.assign.empty() && cur->id < static_cast<int>(km.assign.size()))
            isReas = (km.assign[cur->id] != did);
    }

    // Build rich text lines
    struct Line { QString label; QString value; };
    QVector<Line> lines = {
        {"用户编号",    QString("U%1").arg(cur->id)},
        {"平面坐标",   QString("(%1, %2, 0) m").arg(cur->x, 0,'f',2).arg(cur->y, 0,'f',2)},
        {"容量需求 Rn", QString("%1 bps/Hz").arg(cur->R, 0,'f',2)},
        {"服务基站",   QString("无人机 UAV %1").arg(did + 1)},
        {"三维距离 dn", QString("%1 m").arg(d3, 0,'f',2)},
        {"信道衰减 hn", QString("%1").arg(chan, 0,'e',3)},
        {"最小发射功率 pn", QString("%1 W").arg(pwr, 0,'f',3)},
    };
    if (isReas) {
        Line reassLine;
        reassLine.label = "归属优化";
        reassLine.value = "SA 智能重分配 (离群点收敛)";
        lines.append(reassLine);
    }

    QFont labelFont("Microsoft YaHei", 8, QFont::Bold);
    QFont valueFont("Microsoft YaHei", 8);

    QFontMetrics lfm(labelFont);
    QFontMetrics vfm(valueFont);

    int lineH     = 18;
    int cardW     = 250;
    int headerH   = 26;
    int cardH     = headerH + static_cast<int>(lines.size()) * lineH + 12;

    QPoint popupPos = m_hoverPos + QPoint(14, 14);
    if (popupPos.x() + cardW > width())
        popupPos.setX(m_hoverPos.x() - cardW - 8);
    if (popupPos.y() + cardH > height())
        popupPos.setY(m_hoverPos.y() - cardH - 8);

    QRectF card(popupPos, QSize(cardW, cardH));

    // Drop shadow
    p.setBrush(QColor(0, 0, 0, 60));
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(card.adjusted(3, 4, 3, 4), 7, 7);

    // Card background
    QLinearGradient cardBg(card.topLeft(), card.bottomLeft());
    cardBg.setColorAt(0, QColor(18, 24, 36, 245));
    cardBg.setColorAt(1, QColor(12, 16, 26, 240));
    p.setBrush(cardBg);
    p.setPen(QPen(TOOLTIP_BORDER, 1.2));
    p.drawRoundedRect(card, 7, 7);

    // Header strip
    QColor headerColor = UAV_COLOR[
        (cur->id < static_cast<int>(
            (m_hoverSchemeIdx == 1 ? m_model->getSAResult() : m_model->getKMeansResult())
            .assign.size()))
        ? (m_hoverSchemeIdx == 1 ? m_model->getSAResult() : m_model->getKMeansResult())
              .assign[cur->id]
        : 0];
    QLinearGradient hdrGrad(card.topLeft(), QPointF(card.right(), card.top()));
    hdrGrad.setColorAt(0, headerColor.darker(150));
    hdrGrad.setColorAt(1, QColor(18, 24, 36, 0));
    p.fillRect(QRectF(card.left() + 1, card.top() + 1, card.width() - 2, headerH - 1), hdrGrad);

    // Header text
    p.setFont(QFont("Microsoft YaHei", 9, QFont::Bold));
    p.setPen(Qt::white);
    p.drawText(QRectF(card.left() + 10, card.top() + 4, card.width() - 14, headerH - 4),
               Qt::AlignVCenter | Qt::AlignLeft,
               QString("地面用户 U%1 遥测详情").arg(cur->id));

    // Separator
    p.setPen(QPen(QColor(50, 60, 80), 1));
    p.drawLine(QPointF(card.left() + 8, card.top() + headerH),
               QPointF(card.right() - 8, card.top() + headerH));

    // Data rows
    int y0 = static_cast<int>(card.top()) + headerH + 6;
    for (const auto &line : lines) {
        p.setFont(labelFont);
        p.setPen(QColor(120, 140, 180));
        p.drawText(QPointF(card.left() + 10, y0 + 12), line.label + ":");

        p.setFont(valueFont);
        p.setPen(QColor(210, 220, 240));
        p.drawText(QPointF(card.left() + 115, y0 + 12), line.value);
        y0 += lineH;
    }
}

// ── drawLegend ────────────────────────────────────────────────────────────
void UAVCanvas::drawLegend(QPainter &p, const QRectF &rect, bool isSA) {
    struct Entry {
        QString label;
        int     kind; // 0=circle, 1=star, 2=dash-line
        QColor  color;
    };
    QVector<Entry> entries = {
        {"无人机 1 覆盖圆", 0, UAV_COLOR[0]},
        {"无人机 2 覆盖圆", 0, UAV_COLOR[1]},
        {"无人机 1 归属用户", 3, UAV_COLOR[0]},
        {"无人机 2 归属用户", 3, UAV_COLOR[1]},
    };
    if (isSA)
        entries.append({"智能重分配 (SA优化)", 1, STAR_COLOR});

    QFont f("Microsoft YaHei", 7.5);
    QFontMetrics fm(f);

    int lineH  = 18;
    int padX   = 10;
    int padY   = 8;
    int iconW  = 22;
    int cardW  = 175;
    int cardH  = padY * 2 + static_cast<int>(entries.size()) * lineH;

    QRectF leg(rect.right() - cardW - 14,
               rect.bottom() - cardH - 14,
               cardW, cardH);

    // Background
    p.setBrush(QColor(14, 18, 26, 210));
    p.setPen(QPen(QColor(40, 55, 80), 1.0));
    p.drawRoundedRect(leg, 5, 5);

    // Title
    p.setFont(QFont("Microsoft YaHei", 7.5, QFont::Bold));
    p.setPen(QColor(150, 170, 210));
    p.drawText(QRectF(leg.left() + padX, leg.top() + 2, cardW - padX * 2, 16),
               Qt::AlignLeft | Qt::AlignVCenter, "图例说明");

    p.setFont(f);
    int iy = static_cast<int>(leg.top()) + padY + 10;

    for (const auto &e : entries) {
        QPointF iconCenter(leg.left() + padX + iconW / 2.0, iy + lineH / 2.0);

        if (e.kind == 0) {
            // Dashed circle sample
            p.setBrush(QColor(e.color.red(), e.color.green(), e.color.blue(), 40));
            p.setPen(QPen(e.color, 1.0, Qt::DashLine));
            p.drawEllipse(iconCenter, 7.0, 7.0);
        } else if (e.kind == 1) {
            // Five-star sample
            drawFiveStar(p, iconCenter, 6.0, e.color);
        } else if (e.kind == 2) {
            // Dash line sample
            p.setPen(QPen(e.color, 1.0, Qt::DotLine));
            p.drawLine(iconCenter - QPointF(8, 0), iconCenter + QPointF(8, 0));
        } else {
            // Filled circle node sample
            QRadialGradient rg(iconCenter - QPointF(2, 2), 10.0);
            rg.setColorAt(0, e.color.lighter(140));
            rg.setColorAt(1, e.color);
            p.setBrush(rg);
            p.setPen(QPen(Qt::white, 0.8));
            p.drawEllipse(iconCenter, 5.0, 5.0);
        }

        p.setPen(QColor(190, 200, 220));
        p.setFont(f);
        p.drawText(QPointF(leg.left() + padX + iconW + 4, iy + lineH / 2.0 + 4),
                   e.label);
        iy += lineH;
    }
}

// ── Mouse events ──────────────────────────────────────────────────────────
void UAVCanvas::mouseMoveEvent(QMouseEvent *event) {
    m_hoverPos = event->pos();

    if (m_isPanning) {
        QPointF delta = event->pos() - m_lastMousePos;
        m_panOffset  += delta;
        m_lastMousePos = event->pos();
        update();
        QWidget::mouseMoveEvent(event);
        return;
    }

    if (!m_model || !m_model->hasSolved()) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    int oldUser = m_hoverUserId;
    m_hoverUserId   = -1;
    m_hoverSchemeIdx = -1;

    auto checkScenario = [&](const QRectF &r, int sIdx) {
        UserNode *cur = m_model->getUserListHead();
        while (cur) {
            QPointF uWorld = (m_animRunning && m_animUserWorldPos.contains(cur->id))
                             ? m_animUserWorldPos[cur->id]
                             : QPointF(cur->x, cur->y);
            QPointF sp = worldToScreen(uWorld, r);
            double dist = std::hypot(sp.x() - event->pos().x(),
                                     sp.y() - event->pos().y());
            if (dist < 11.0) {
                m_hoverUserId    = cur->id;
                m_hoverSchemeIdx = sIdx;
                return;
            }
            cur = cur->next;
        }
    };

    if (m_viewMode == ViewMode::DualSideBySide) {
        double w = width() / 2.0;
        if (event->pos().x() < w)
            checkScenario(QRectF(0, 0, w, height()), 0);
        else
            checkScenario(QRectF(w, 0, w, height()), 1);
    } else if (m_viewMode == ViewMode::KMeansOnly) {
        checkScenario(QRectF(rect()), 0);
    } else {
        checkScenario(QRectF(rect()), 1);
    }

    if (m_hoverUserId != oldUser) update();

    // Update cursor
    setCursor(m_hoverUserId != -1 ? Qt::PointingHandCursor : Qt::ArrowCursor);

    QWidget::mouseMoveEvent(event);
}

void UAVCanvas::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::RightButton || event->button() == Qt::MiddleButton) {
        m_isPanning    = true;
        m_lastMousePos = event->pos();
        setCursor(Qt::ClosedHandCursor);
    } else if (event->button() == Qt::LeftButton) {
        if (m_hoverUserId != -1) {
            m_selectedUserId = m_hoverUserId;
            m_pulsePhase     = 0.0;
            if (!m_pulseTimer->isActive()) m_pulseTimer->start();
            emit userClicked(m_selectedUserId);
            update();
        }
    }
    QWidget::mousePressEvent(event);
}

void UAVCanvas::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::RightButton || event->button() == Qt::MiddleButton) {
        m_isPanning = false;
        setCursor(m_hoverUserId != -1 ? Qt::PointingHandCursor : Qt::ArrowCursor);
    }
    QWidget::mouseReleaseEvent(event);
}

void UAVCanvas::wheelEvent(QWheelEvent *event) {
    double factor = (event->angleDelta().y() > 0) ? 1.12 : 0.893;
    m_zoom = std::clamp(m_zoom * factor, 0.3, 5.0);
    update();
    QWidget::wheelEvent(event);
}

void UAVCanvas::mouseDoubleClickEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        resetView();
    }
    QWidget::mouseDoubleClickEvent(event);
}
