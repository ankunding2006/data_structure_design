#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QTableWidget>
#include <QTabWidget>
#include <QTextEdit>
#include <QLabel>
#include <QTimer>
#include <QAction>
#include <QToolBar>
#include <QMenuBar>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QSplitter>
#include <QFrame>
#include <QScrollArea>
#include <QGroupBox>
#include "canvas.h"
#include "core/uav_model.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    // 公开方法：右侧用户信息面板更新
    void updateUserInfoPanel(int userId);

    bool exportDirectImage(const QString &filepath) {
        return m_canvas->saveImage(filepath);
    }

    UAVSystemModel m_model;
    UAVCanvas     *m_canvas;

    // 参数控件
    QSpinBox       *m_spinN;
    QDoubleSpinBox *m_spinG;
    QDoubleSpinBox *m_spinArea;
    QSpinBox       *m_spinSeed;
    QDoubleSpinBox *m_spinRMin;
    QDoubleSpinBox *m_spinRMax;

    // 视图与渲染
    QComboBox *m_comboViewMode;
    QCheckBox *m_chkCircles;
    QCheckBox *m_chkLinks;

    // 底部面板
    QTabWidget   *m_bottomTabs;
    QTableWidget *m_tableMetrics;
    QTableWidget *m_tableUsers;
    QTextEdit    *m_textTheory;

    // 状态栏标签
    QLabel *m_statusLabel;
    QLabel *m_statusCoord;
    QLabel *m_statusAlgo;
    QLabel *m_statusVersion;

private slots:
    // 算法动作
    void onRunAllClicked();
    void onGenerateClicked();
    void onKMeansClicked();
    void onSAClicked();
    void onRandomSeedClicked();

    // 视图动作
    void onViewModeChanged(int index);
    void onResetViewClicked();

    // 导出动作
    void onExportImageClicked();
    void onExportCSVClicked();
    void onExportJSONClicked();

    // 联动动作
    void onTableUserSelected(int row, int column);
    void onCanvasUserClicked(int userId);

    // 面板收起/展开
    void onToggleLeftPanel();

    // 帮助菜单
    void onAboutClicked();
    void onMathPrincipleClicked();
    void onUsageClicked();

    // 状态栏动画 ticker
    void onStatusTick();

    // 底部面板弹出动画结束
    void onBottomPanelAnimFinished();

private:
    void setupMenuBar();
    void setupToolBar();
    void setupUI();
    void applyIndustrialTheme();

    void updateTables();
    void updateMetricsTab();
    void updateUsersTab();
    void updateTheoryTab();

    void startSolvingAnimation();
    void stopSolvingAnimation();

    void animateLeftPanelIn();
    void animateBottomPanelUp();

    // ── 左侧面板 ──
    QWidget   *m_leftPanel;
    QPushButton *m_btnToggleLeft;
    bool        m_leftPanelExpanded;
    QPropertyAnimation *m_leftPanelAnim;

    // ── 右侧用户信息面板 ──
    QWidget  *m_rightInfoPanel;
    QLabel   *m_infoUserId;
    QLabel   *m_infoCoord;
    QLabel   *m_infoCapacity;
    QLabel   *m_infoKmDist;
    QLabel   *m_infoKmPower;
    QLabel   *m_infoSaDist;
    QLabel   *m_infoSaPower;
    QLabel   *m_infoReassign;
    QGraphicsOpacityEffect *m_infoPanelOpacity;
    QPropertyAnimation     *m_infoPanelFadeAnim;

    // ── 底部面板动画 ──
    QWidget            *m_bottomContainer;
    QPropertyAnimation *m_bottomAnim;
    int                 m_bottomTargetHeight;

    // ── 工具栏按钮引用（供状态管理） ──
    QPushButton *m_btnRunAll;

    // ── 状态栏动画 ──
    QTimer *m_statusTimer;
    int     m_statusDots;
    bool    m_isSolving;

    // ── 菜单动作 ──
    QAction *m_actNewSim;
    QAction *m_actSaveImage;
    QAction *m_actExportCSV;
    QAction *m_actQuit;
    QAction *m_actViewDual;
    QAction *m_actViewKM;
    QAction *m_actViewSA;
    QAction *m_actFullscreen;
    QAction *m_actGenUsers;
    QAction *m_actRunKMeans;
    QAction *m_actRunSA;
    QAction *m_actRunAll;
    QAction *m_actAbout;
    QAction *m_actMath;
    QAction *m_actUsage;
};

#endif // MAINWINDOW_H
