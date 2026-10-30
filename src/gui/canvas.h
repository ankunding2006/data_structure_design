#ifndef CANVAS_H
#define CANVAS_H

#include <QWidget>
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QTimer>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QVariantAnimation>
#include <QEasingCurve>
#include <QPointF>
#include <QVector>
#include <QMap>
#include "core/uav_model.h"

enum class ViewMode {
    DualSideBySide = 0, // Left/right dual-screen comparison view (recommended)
    KMeansOnly     = 1, // K-Means baseline only
    SAOnly         = 2  // Simulated Annealing Minimax optimization only
};

class UAVCanvas : public QWidget {
    Q_OBJECT

    // Animatable property: pan offset for reset-view animation
    Q_PROPERTY(QPointF panOffset READ panOffset WRITE setPanOffset)
    // Animatable property: zoom for reset-view animation
    Q_PROPERTY(double zoom READ zoom WRITE setZoom)

public:
    explicit UAVCanvas(QWidget *parent = nullptr);
    ~UAVCanvas() override;

    void setModel(UAVSystemModel *model);
    void setViewMode(ViewMode mode);
    ViewMode getViewMode() const { return m_viewMode; }

    void setSelectedUserId(int id);
    void resetView();
    bool saveImage(const QString &filepath);

    // Display toggle switches
    void setShowCircles(bool show)   { m_showCircles   = show; update(); }
    void setShowLinks(bool show)     { m_showLinks     = show; update(); }
    void setShowPowerTags(bool show) { m_showPowerTags = show; update(); }

    // Entrance animation: call after solve is complete
    void playEntranceAnimation();

    // Q_PROPERTY accessors
    QPointF panOffset() const { return m_panOffset; }
    void    setPanOffset(const QPointF &v) { m_panOffset = v; update(); }
    double  zoom() const { return m_zoom; }
    void    setZoom(double v) { m_zoom = v; update(); }

signals:
    void userClicked(int userId);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    // Drawing helpers
    void drawSingleScenario(QPainter &p, const QRectF &rect, const SchemeResult &scheme,
                            const QString &title, bool isSA);
    void drawGrid(QPainter &p, const QRectF &rect);
    void drawUAVIcon(QPainter &p, const QPointF &center, const QColor &color, double size);
    void drawFiveStar(QPainter &p, const QPointF &center, double radius, const QColor &color);
    void drawCoverCircle(QPainter &p, const QPointF &center, double rScreen,
                         const QColor &color, double radiusProgress);
    void drawConnectionLine(QPainter &p, const QPointF &from, const QPointF &to,
                            const QColor &color, double opacity);
    void drawTooltipCard(QPainter &p);
    void drawLegend(QPainter &p, const QRectF &rect, bool isSA);
    void drawDivider(QPainter &p, double x);

    QPointF worldToScreen(const QPointF &wPos, const QRectF &rect) const;
    QPointF screenToWorld(const QPointF &sPos, const QRectF &rect) const;

    // Animation state
    // Per-user animated screen positions (for entrance scatter animation)
    // Key: user id, Value: current animated world position (interpolated from center)
    QMap<int, QPointF> m_animUserWorldPos; // animated world position per user id
    double             m_circleProgress;   // 0.0 -> 1.0: coverage circle radius progress
    double             m_linkOpacity;      // 0.0 -> 1.0: connection line opacity
    bool               m_animRunning;

    // Pulse animation for selected user halo
    QTimer  *m_pulseTimer;
    double   m_pulsePhase;  // radians, driven by timer
    double   m_hoverScale;  // scale factor for hovered user node (0 -> 1 on hover enter)

    // Animation groups / animations
    QParallelAnimationGroup *m_entranceGroup;
    QVariantAnimation       *m_circleAnim;
    QVariantAnimation       *m_linkAnim;

    // Per-user position animations (stored in the parallel group)
    // We keep target world positions here for painting
    QMap<int, QPointF> m_targetUserWorldPos; // final world position

    UAVSystemModel *m_model;
    ViewMode        m_viewMode;

    bool m_showCircles;
    bool m_showLinks;
    bool m_showPowerTags;

    int    m_selectedUserId;
    int    m_hoverUserId;
    int    m_hoverUavId;
    int    m_hoverSchemeIdx; // 0 = KM, 1 = SA
    QPoint m_hoverPos;

    // Pan & zoom
    double  m_zoom;
    QPointF m_panOffset;
    bool    m_isPanning;
    QPoint  m_lastMousePos;
};

#endif // CANVAS_H
