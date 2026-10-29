#ifndef CANVAS_H
#define CANVAS_H

#include <QWidget>
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QToolTip>
#include "uav_model.h"

enum class ViewMode {
    DualSideBySide = 0, // 左右双屏对比视图 (推荐)
    KMeansOnly = 1,     // 仅查看 K-Means 基准
    SAOnly = 2          // 仅查看 模拟退火 Minimax 优化
};

class UAVCanvas : public QWidget {
    Q_OBJECT
public:
    explicit UAVCanvas(QWidget *parent = nullptr);

    void setModel(UAVSystemModel *model);
    void setViewMode(ViewMode mode);
    ViewMode getViewMode() const { return m_viewMode; }

    void setSelectedUserId(int id);
    void resetView();
    bool saveImage(const QString &filepath);

    // 显示控制开关
    void setShowCircles(bool show) { m_showCircles = show; update(); }
    void setShowLinks(bool show) { m_showLinks = show; update(); }
    void setShowPowerTags(bool show) { m_showPowerTags = show; update(); }

signals:
    void userClicked(int userId);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    void drawSingleScenario(QPainter &p, const QRectF &rect, const SchemeResult &scheme,
                            const QString &title, bool isSA);
    void drawGrid(QPainter &p, const QRectF &rect);
    QPointF worldToScreen(const QPointF &wPos, const QRectF &rect) const;
    QPointF screenToWorld(const QPointF &sPos, const QRectF &rect) const;

    UAVSystemModel *m_model;
    ViewMode m_viewMode;

    bool m_showCircles;
    bool m_showLinks;
    bool m_showPowerTags;

    int m_selectedUserId;
    int m_hoverUserId;
    int m_hoverUavId;
    int m_hoverSchemeIdx; // 0 for KM, 1 for SA
    QPoint m_hoverPos;

    // 缩放和平移参数
    double m_zoom;
    QPointF m_panOffset;
    bool m_isPanning;
    QPoint m_lastMousePos;
};

#endif // CANVAS_H
