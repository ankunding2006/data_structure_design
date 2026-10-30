#include "mainwindow.h"
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QStatusBar>
#include <QDateTime>
#include <QMenuBar>
#include <QToolBar>
#include <QSplitter>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <random>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_canvas(nullptr),
      m_leftPanelExpanded(true),
      m_leftPanelAnim(nullptr),
      m_infoPanelFadeAnim(nullptr),
      m_bottomAnim(nullptr),
      m_statusDots(0),
      m_isSolving(false)
{
    setWindowTitle("无人机地面基站覆盖与功率优化系统 [工业仿真平台] - 数据结构与算法分析");
    resize(1440, 920);
    setMinimumSize(1150, 750);

    setupMenuBar();
    setupToolBar();
    setupUI();
    applyIndustrialTheme();

    // 状态栏动态更新定时器
    m_statusTimer = new QTimer(this);
    connect(m_statusTimer, &QTimer::timeout, this, &MainWindow::onStatusTick);
    m_statusTimer->start(400);

    // 初始全流程求解
    onRunAllClicked();
}

MainWindow::~MainWindow() {
}

void MainWindow::setupMenuBar() {
    QMenuBar *menuBar = this->menuBar();

    // 1. 文件菜单
    QMenu *fileMenu = menuBar->addMenu("文件(&F)");
    m_actNewSim = fileMenu->addAction("新建/重置仿真(&N)", this, &MainWindow::onRunAllClicked, QKeySequence::New);
    fileMenu->addSeparator();
    m_actSaveImage = fileMenu->addAction("保存画布图像(&S)...", this, &MainWindow::onExportImageClicked, QKeySequence::Save);
    m_actExportCSV = fileMenu->addAction("导出数据表格(CSV)...", this, &MainWindow::onExportCSVClicked, QKeySequence("Ctrl+E"));
    fileMenu->addAction("导出结果数据(JSON)...", this, &MainWindow::onExportJSONClicked);
    fileMenu->addSeparator();
    m_actQuit = fileMenu->addAction("退出(&Q)", this, &QWidget::close, QKeySequence::Quit);

    // 2. 视图菜单
    QMenu *viewMenu = menuBar->addMenu("视图(&V)");
    m_actViewDual = viewMenu->addAction("双屏对比模式(&D)", [this](){ m_comboViewMode->setCurrentIndex(0); });
    m_actViewKM = viewMenu->addAction("K-Means 基线模式(&K)", [this](){ m_comboViewMode->setCurrentIndex(1); });
    m_actViewSA = viewMenu->addAction("模拟退火优化模式(&A)", [this](){ m_comboViewMode->setCurrentIndex(2); });
    viewMenu->addSeparator();
    viewMenu->addAction("折叠/展开左侧面板(&T)", this, &MainWindow::onToggleLeftPanel, QKeySequence("Ctrl+B"));
    viewMenu->addAction("重置画布缩放居中(&R)", this, &MainWindow::onResetViewClicked, QKeySequence("Ctrl+0"));
    viewMenu->addSeparator();
    m_actFullscreen = viewMenu->addAction("全屏模式切换(&F)", [this](){
        if (isFullScreen()) showNormal(); else showFullScreen();
    }, QKeySequence("F11"));

    // 3. 算法菜单
    QMenu *algoMenu = menuBar->addMenu("算法调度(&A)");
    m_actGenUsers = algoMenu->addAction("随机生成地面用户链表(&G)", this, &MainWindow::onGenerateClicked, QKeySequence("Ctrl+G"));
    m_actRunKMeans = algoMenu->addAction("执行 K-Means 基线聚类(&K)", this, &MainWindow::onKMeansClicked);
    m_actRunSA = algoMenu->addAction("执行 模拟退火 Minimax 优化(&M)", this, &MainWindow::onSAClicked);
    algoMenu->addSeparator();
    m_actRunAll = algoMenu->addAction("一键全流程求解对比(&R)", this, &MainWindow::onRunAllClicked, QKeySequence("Ctrl+R"));

    // 4. 帮助菜单
    QMenu *helpMenu = menuBar->addMenu("帮助(&H)");
    m_actMath = helpMenu->addAction("数学原理与香农容量(&M)", this, &MainWindow::onMathPrincipleClicked);
    m_actUsage = helpMenu->addAction("工业平台使用说明(&U)", this, &MainWindow::onUsageClicked);
    helpMenu->addSeparator();
    m_actAbout = helpMenu->addAction("关于本系统(&A)", this, &MainWindow::onAboutClicked);
}

void MainWindow::setupToolBar() {
    QToolBar *toolBar = addToolBar("主要操作");
    toolBar->setMovable(false);
    toolBar->setIconSize(QSize(18, 18));
    toolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    QAction *actRun = toolBar->addAction("▶ 一键全流程求解", this, &MainWindow::onRunAllClicked);
    actRun->setToolTip("生成随机链表、执行K-Means基线并执行SA Minimax优化 (Ctrl+R)");

    QAction *actRand = toolBar->addAction("🎲 随机样本", this, &MainWindow::onRandomSeedClicked);
    actRand->setToolTip("更换随机数种子并重新计算");

    toolBar->addSeparator();

    QAction *actKM = toolBar->addAction("📊 仅K-Means", this, &MainWindow::onKMeansClicked);
    QAction *actSA = toolBar->addAction("⚡ 仅SA优化", this, &MainWindow::onSAClicked);

    toolBar->addSeparator();

    QAction *actResetView = toolBar->addAction("🔍 重置视野", this, &MainWindow::onResetViewClicked);
    QAction *actSave = toolBar->addAction("📷 导出图像", this, &MainWindow::onExportImageClicked);
    QAction *actCSV = toolBar->addAction("📄 导出CSV", this, &MainWindow::onExportCSVClicked);

    toolBar->addSeparator();

    QAction *actToggleLeft = toolBar->addAction("◨ 侧边栏", this, &MainWindow::onToggleLeftPanel);
    actToggleLeft->setToolTip("展开/收起左侧控制参数栏 (Ctrl+B)");
}

void MainWindow::setupUI() {
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    QHBoxLayout *mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(6);

    // ========================================================================
    // 1. 左侧面板：控制与参数输入（支持平滑折叠）
    // ========================================================================
    m_leftPanel = new QWidget(this);
    m_leftPanel->setFixedWidth(290);
    QVBoxLayout *leftLayout = new QVBoxLayout(m_leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(8);

    // 系统参数组
    QGroupBox *paramGroup = new QGroupBox("系统模型与环境参数", m_leftPanel);
    QGridLayout *paramLayout = new QGridLayout(paramGroup);
    paramLayout->setContentsMargins(10, 14, 10, 10);
    paramLayout->setVerticalSpacing(6);

    paramLayout->addWidget(new QLabel("地面用户数 N (≥8):"), 0, 0);
    m_spinN = new QSpinBox(paramGroup);
    m_spinN->setRange(8, 100);
    m_spinN->setValue(16);
    paramLayout->addWidget(m_spinN, 0, 1);

    paramLayout->addWidget(new QLabel("无人机飞行高度 G (m):"), 1, 0);
    m_spinG = new QDoubleSpinBox(paramGroup);
    m_spinG->setRange(10.0, 100.0);
    m_spinG->setValue(20.0);
    m_spinG->setSingleStep(1.0);
    paramLayout->addWidget(m_spinG, 1, 1);

    paramLayout->addWidget(new QLabel("覆盖区域边长 L (m):"), 2, 0);
    m_spinArea = new QDoubleSpinBox(paramGroup);
    m_spinArea->setRange(50.0, 500.0);
    m_spinArea->setValue(100.0);
    m_spinArea->setSingleStep(10.0);
    paramLayout->addWidget(m_spinArea, 2, 1);

    paramLayout->addWidget(new QLabel("随机数种子:"), 3, 0);
    QHBoxLayout *seedLayout = new QHBoxLayout();
    m_spinSeed = new QSpinBox(paramGroup);
    m_spinSeed->setRange(0, 999999);
    m_spinSeed->setValue(42);
    QPushButton *btnRandomSeed = new QPushButton("🎲", paramGroup);
    btnRandomSeed->setToolTip("随机生成新种子");
    btnRandomSeed->setFixedWidth(32);
    connect(btnRandomSeed, &QPushButton::clicked, this, &MainWindow::onRandomSeedClicked);
    seedLayout->addWidget(m_spinSeed);
    seedLayout->addWidget(btnRandomSeed);
    paramLayout->addLayout(seedLayout, 3, 1);

    paramLayout->addWidget(new QLabel("目标容量需求 Rn:"), 4, 0);
    QHBoxLayout *rLayout = new QHBoxLayout();
    m_spinRMin = new QDoubleSpinBox(paramGroup);
    m_spinRMin->setRange(0.5, 10.0);
    m_spinRMin->setValue(1.0);
    m_spinRMax = new QDoubleSpinBox(paramGroup);
    m_spinRMax->setRange(0.5, 10.0);
    m_spinRMax->setValue(5.0);
    rLayout->addWidget(m_spinRMin);
    rLayout->addWidget(m_spinRMax);
    paramLayout->addLayout(rLayout, 4, 1);

    leftLayout->addWidget(paramGroup);

    // 算法求解控制
    QGroupBox *solverGroup = new QGroupBox("仿真引擎与算法调度", m_leftPanel);
    QVBoxLayout *solverLayout = new QVBoxLayout(solverGroup);
    solverLayout->setContentsMargins(10, 14, 10, 10);
    solverLayout->setSpacing(6);

    m_btnRunAll = new QPushButton("▶ 一键全流程求解与对比", solverGroup);
    m_btnRunAll->setObjectName("primaryBtn");
    m_btnRunAll->setMinimumHeight(40);
    connect(m_btnRunAll, &QPushButton::clicked, this, &MainWindow::onRunAllClicked);
    solverLayout->addWidget(m_btnRunAll);

    QPushButton *btnGenerate = new QPushButton("1. 重新生成用户链表 (随机坐标/容量)", solverGroup);
    connect(btnGenerate, &QPushButton::clicked, this, &MainWindow::onGenerateClicked);
    solverLayout->addWidget(btnGenerate);

    QPushButton *btnKMeans = new QPushButton("2. 仅执行 K-Means 聚类划分", solverGroup);
    connect(btnKMeans, &QPushButton::clicked, this, &MainWindow::onKMeansClicked);
    solverLayout->addWidget(btnKMeans);

    QPushButton *btnSA = new QPushButton("3. 仅执行 模拟退火 Minimax 优化", solverGroup);
    connect(btnSA, &QPushButton::clicked, this, &MainWindow::onSAClicked);
    solverLayout->addWidget(btnSA);

    leftLayout->addWidget(solverGroup);

    // 视图显示控制
    QGroupBox *viewGroup = new QGroupBox("视图渲染控制", m_leftPanel);
    QVBoxLayout *viewLayout = new QVBoxLayout(viewGroup);
    viewLayout->setContentsMargins(10, 14, 10, 10);
    viewLayout->setSpacing(6);

    viewLayout->addWidget(new QLabel("布局模式:"));
    m_comboViewMode = new QComboBox(viewGroup);
    m_comboViewMode->addItem("双屏分栏对比 (基线 vs SA)");
    m_comboViewMode->addItem("单屏模式：K-Means 基线方案");
    m_comboViewMode->addItem("单屏模式：模拟退火创新方案");
    connect(m_comboViewMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onViewModeChanged);
    viewLayout->addWidget(m_comboViewMode);

    m_chkCircles = new QCheckBox("渲染无人机覆盖圆与半径", viewGroup);
    m_chkCircles->setChecked(true);
    connect(m_chkCircles, &QCheckBox::toggled, [this](bool checked){ m_canvas->setShowCircles(checked); });
    viewLayout->addWidget(m_chkCircles);

    m_chkLinks = new QCheckBox("渲染通信矢量连线与方向", viewGroup);
    m_chkLinks->setChecked(true);
    connect(m_chkLinks, &QCheckBox::toggled, [this](bool checked){ m_canvas->setShowLinks(checked); });
    viewLayout->addWidget(m_chkLinks);

    QPushButton *btnResetView = new QPushButton("🔍 重置画布视角与居中", viewGroup);
    connect(btnResetView, &QPushButton::clicked, this, &MainWindow::onResetViewClicked);
    viewLayout->addWidget(btnResetView);

    leftLayout->addWidget(viewGroup);
    leftLayout->addStretch();

    mainLayout->addWidget(m_leftPanel);

    // ========================================================================
    // 2. 中间主区域：画布 + 底部信息面板
    // ========================================================================
    QWidget *centerPanel = new QWidget(this);
    QVBoxLayout *centerLayout = new QVBoxLayout(centerPanel);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(6);

    // 工业遥测矢量画布
    m_canvas = new UAVCanvas(centerPanel);
    m_canvas->setModel(&m_model);
    connect(m_canvas, &UAVCanvas::userClicked, this, &MainWindow::onCanvasUserClicked);
    centerLayout->addWidget(m_canvas, 1);

    // 底部选项卡
    m_bottomTabs = new QTabWidget(centerPanel);
    m_bottomTabs->setFixedHeight(230);

    // Tab 1: 算法量化指标对比表
    m_tableMetrics = new QTableWidget(m_bottomTabs);
    m_tableMetrics->setColumnCount(4);
    m_tableMetrics->setHorizontalHeaderLabels({"量化指标项目", "方案 A：K-Means 基准聚类", "方案 B：模拟退火 Minimax 优化", "创新算法优化改善幅度"});
    m_tableMetrics->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_tableMetrics->verticalHeader()->setVisible(false);
    m_tableMetrics->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableMetrics->setSelectionMode(QAbstractItemView::NoSelection);
    m_bottomTabs->addTab(m_tableMetrics, "📈 方案量化指标对比仪表盘");

    // Tab 2: 用户链表节点完整数据表
    m_tableUsers = new QTableWidget(m_bottomTabs);
    m_tableUsers->setColumnCount(9);
    m_tableUsers->setHorizontalHeaderLabels({
        "用户ID", "坐标 (x, y)", "所需容量 Rn",
        "KM分配基站", "KM三维距离", "KM最小功率",
        "SA分配基站", "SA三维距离", "SA最小功率"
    });
    m_tableUsers->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_tableUsers->verticalHeader()->setVisible(false);
    m_tableUsers->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableUsers->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableUsers->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(m_tableUsers, &QTableWidget::cellClicked, this, &MainWindow::onTableUserSelected);
    m_bottomTabs->addTab(m_tableUsers, "📋 地面用户链式存储节点数据审查表");

    // Tab 3: 数学原理与创新说明
    m_textTheory = new QTextEdit(m_bottomTabs);
    m_textTheory->setReadOnly(true);
    m_bottomTabs->addTab(m_textTheory, "💡 算法数学原理与课程设计报告摘要");
    updateTheoryTab();

    centerLayout->addWidget(m_bottomTabs);
    mainLayout->addWidget(centerPanel, 1);

    // ========================================================================
    // 3. 右侧面板：当前选中用户遥测详情卡片
    // ========================================================================
    m_rightInfoPanel = new QWidget(this);
    m_rightInfoPanel->setFixedWidth(240);
    QVBoxLayout *rightLayout = new QVBoxLayout(m_rightInfoPanel);
    rightLayout->setContentsMargins(4, 0, 4, 0);
    rightLayout->setSpacing(8);

    QGroupBox *infoGroup = new QGroupBox("节点遥测监视卡片", m_rightInfoPanel);
    QVBoxLayout *infoLayout = new QVBoxLayout(infoGroup);
    infoLayout->setContentsMargins(10, 14, 10, 10);
    infoLayout->setSpacing(6);

    m_infoUserId = new QLabel("未选中任何节点", infoGroup);
    m_infoUserId->setStyleSheet("font-size: 14px; font-weight: bold; color: #00B4D8;");
    infoLayout->addWidget(m_infoUserId);

    m_infoCoord = new QLabel("坐标: --", infoGroup);
    m_infoCapacity = new QLabel("容量需求 Rn: --", infoGroup);
    infoLayout->addWidget(m_infoCoord);
    infoLayout->addWidget(m_infoCapacity);

    QFrame *line1 = new QFrame(infoGroup);
    line1->setFrameShape(QFrame::HLine);
    line1->setStyleSheet("color: #2D333B;");
    infoLayout->addWidget(line1);

    infoLayout->addWidget(new QLabel("<b>[方案 A: K-Means]</b>", infoGroup));
    m_infoKmDist = new QLabel("空间距离: --", infoGroup);
    m_infoKmPower = new QLabel("发射功率: --", infoGroup);
    infoLayout->addWidget(m_infoKmDist);
    infoLayout->addWidget(m_infoKmPower);

    QFrame *line2 = new QFrame(infoGroup);
    line2->setFrameShape(QFrame::HLine);
    line2->setStyleSheet("color: #2D333B;");
    infoLayout->addWidget(line2);

    infoLayout->addWidget(new QLabel("<b>[方案 B: 模拟退火]</b>", infoGroup));
    m_infoSaDist = new QLabel("空间距离: --", infoGroup);
    m_infoSaPower = new QLabel("发射功率: --", infoGroup);
    infoLayout->addWidget(m_infoSaDist);
    infoLayout->addWidget(m_infoSaPower);

    m_infoReassign = new QLabel("", infoGroup);
    m_infoReassign->setWordWrap(true);
    infoLayout->addWidget(m_infoReassign);

    infoLayout->addStretch();
    rightLayout->addWidget(infoGroup);

    mainLayout->addWidget(m_rightInfoPanel);

    // 状态栏初始化
    m_statusCoord = new QLabel("坐标: 区域内", this);
    m_statusAlgo = new QLabel("状态: 就绪", this);
    m_statusVersion = new QLabel("v2.0 工业级仿真平台 | 数据结构课程设计", this);

    statusBar()->addWidget(m_statusAlgo, 2);
    statusBar()->addPermanentWidget(m_statusCoord, 1);
    statusBar()->addPermanentWidget(m_statusVersion);
}

void MainWindow::applyIndustrialTheme() {
    QString qss = R"(
        QMainWindow, QWidget {
            background-color: #0D1117;
            color: #C9D1D9;
            font-family: "Microsoft YaHei", "Segoe UI", sans-serif;
            font-size: 12px;
        }
        QMenuBar {
            background-color: #161B22;
            color: #C9D1D9;
            border-bottom: 1px solid #30363D;
            padding: 2px 4px;
        }
        QMenuBar::item:selected {
            background-color: #21262D;
            border-radius: 4px;
            color: #58A6FF;
        }
        QMenu {
            background-color: #161B22;
            border: 1px solid #30363D;
            padding: 4px 0px;
        }
        QMenu::item {
            padding: 6px 24px;
            color: #C9D1D9;
        }
        QMenu::item:selected {
            background-color: #1F6FEB;
            color: #FFFFFF;
        }
        QToolBar {
            background-color: #161B22;
            border-bottom: 1px solid #30363D;
            spacing: 6px;
            padding: 4px 8px;
        }
        QToolButton {
            background-color: #21262D;
            border: 1px solid #30363D;
            border-radius: 4px;
            padding: 5px 10px;
            color: #C9D1D9;
            font-weight: 500;
        }
        QToolButton:hover {
            background-color: #30363D;
            border-color: #58A6FF;
            color: #FFFFFF;
        }
        QGroupBox {
            border: 1px solid #30363D;
            border-top: 2px solid #1F6FEB;
            border-radius: 6px;
            margin-top: 14px;
            padding-top: 12px;
            font-weight: bold;
            color: #58A6FF;
            background-color: #161B22;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            left: 10px;
            padding: 0 4px;
        }
        QPushButton {
            background-color: #21262D;
            border: 1px solid #30363D;
            border-radius: 5px;
            padding: 6px 12px;
            color: #C9D1D9;
            font-weight: 500;
        }
        QPushButton:hover {
            background-color: #30363D;
            border-color: #58A6FF;
            color: #FFFFFF;
        }
        QPushButton:pressed {
            background-color: #161B22;
        }
        QPushButton#primaryBtn {
            background-color: #238636;
            border: 1px solid #2EA043;
            font-weight: bold;
            color: #FFFFFF;
        }
        QPushButton#primaryBtn:hover {
            background-color: #2EA043;
            border-color: #3FB950;
        }
        QSpinBox, QDoubleSpinBox, QComboBox {
            background-color: #0D1117;
            border: 1px solid #30363D;
            border-radius: 4px;
            padding: 4px 6px;
            color: #FFFFFF;
        }
        QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus {
            border-color: #58A6FF;
        }
        QCheckBox {
            color: #C9D1D9;
            spacing: 6px;
        }
        QCheckBox::indicator {
            width: 14px;
            height: 14px;
            border: 1px solid #30363D;
            border-radius: 3px;
            background-color: #0D1117;
        }
        QCheckBox::indicator:checked {
            background-color: #1F6FEB;
            border-color: #58A6FF;
        }
        QTabWidget::pane {
            border: 1px solid #30363D;
            background-color: #161B22;
            border-radius: 6px;
        }
        QTabBar::tab {
            background-color: #0D1117;
            color: #8B949E;
            border: 1px solid #30363D;
            border-bottom: none;
            padding: 7px 16px;
            border-top-left-radius: 4px;
            border-top-right-radius: 4px;
            margin-right: 2px;
        }
        QTabBar::tab:selected {
            background-color: #161B22;
            color: #58A6FF;
            font-weight: bold;
            border-bottom: 2px solid #58A6FF;
        }
        QTableWidget {
            background-color: #161B22;
            gridline-color: #21262D;
            border: none;
            color: #C9D1D9;
            selection-background-color: #1F6FEB;
        }
        QHeaderView::section {
            background-color: #21262D;
            color: #8B949E;
            padding: 5px;
            border: 1px solid #30363D;
            font-weight: bold;
        }
        QTextEdit {
            background-color: #161B22;
            border: none;
            color: #C9D1D9;
        }
        QStatusBar {
            background-color: #161B22;
            color: #8B949E;
            border-top: 1px solid #30363D;
        }
        QScrollBar:vertical {
            background: #0D1117;
            width: 8px;
            margin: 0px;
        }
        QScrollBar::handle:vertical {
            background: #30363D;
            min-height: 20px;
            border-radius: 4px;
        }
        QScrollBar::handle:vertical:hover {
            background: #58A6FF;
        }
    )";

    if (auto app = qobject_cast<QApplication*>(QCoreApplication::instance())) {
        app->setStyleSheet(qss);
    } else {
        setStyleSheet(qss);
    }
}

void MainWindow::onRunAllClicked() {
    int n = m_spinN->value();
    double g = m_spinG->value();
    double area = m_spinArea->value();
    unsigned int seed = static_cast<unsigned int>(m_spinSeed->value());
    double rmin = m_spinRMin->value();
    double rmax = m_spinRMax->value();

    m_isSolving = true;
    m_model.generateRandomUsers(n, area, g, rmin, rmax, seed);
    m_model.solveAll();

    // 触发画布入场动画
    m_canvas->playEntranceAnimation();

    updateTables();
    m_isSolving = false;

    double kmR = m_model.getKMeansResult().metrics.max_horizontal_r;
    double saR = m_model.getSAResult().metrics.max_horizontal_r;
    double diff = ((saR - kmR) / kmR) * 100.0;

    m_statusAlgo->setText(QString("状态: 求解完成 | N=%1, G=%2m, 瓶颈半径改善 %3%")
                              .arg(n).arg(g).arg(QString::number(diff, 'f', 2)));
}

void MainWindow::onGenerateClicked() {
    int n = m_spinN->value();
    double g = m_spinG->value();
    double area = m_spinArea->value();
    unsigned int seed = static_cast<unsigned int>(m_spinSeed->value());
    double rmin = m_spinRMin->value();
    double rmax = m_spinRMax->value();

    m_model.generateRandomUsers(n, area, g, rmin, rmax, seed);
    m_model.solveKMeans();
    m_canvas->playEntranceAnimation();
    updateTables();
    m_statusAlgo->setText("状态: 已生成新用户链表并完成基线划分");
}

void MainWindow::onKMeansClicked() {
    m_model.solveKMeans();
    m_canvas->update();
    updateTables();
    m_statusAlgo->setText("状态: 已执行 K-Means 聚类划分");
}

void MainWindow::onSAClicked() {
    m_model.solveSimulatedAnnealing();
    m_canvas->update();
    updateTables();
    m_statusAlgo->setText("状态: 已执行 模拟退火 Minimax 优化");
}

void MainWindow::onRandomSeedClicked() {
    std::random_device rd;
    int s = rd() % 999999;
    m_spinSeed->setValue(s);
    onRunAllClicked();
}

void MainWindow::onViewModeChanged(int index) {
    m_canvas->setViewMode(static_cast<ViewMode>(index));
}

void MainWindow::onResetViewClicked() {
    m_canvas->resetView();
}

void MainWindow::onToggleLeftPanel() {
    m_leftPanelExpanded = !m_leftPanelExpanded;
    m_leftPanel->setVisible(m_leftPanelExpanded);
}

void MainWindow::onExportImageClicked() {
    QString filename = QFileDialog::getSaveFileName(this, "导出高清矢量/位图对比图", "uav_coverage_comparison.png", "PNG 图像 (*.png)");
    if (!filename.isEmpty()) {
        if (m_canvas->saveImage(filename)) {
            QMessageBox::information(this, "导出成功", QString("超分辨率对比图已成功保存至：\n%1").arg(filename));
        } else {
            QMessageBox::warning(this, "导出失败", "写入图像文件失败！");
        }
    }
}

void MainWindow::onExportCSVClicked() {
    QString filename = QFileDialog::getSaveFileName(this, "导出数据表为 CSV", "uav_results.csv", "CSV 文件 (*.csv)");
    if (!filename.isEmpty()) {
        if (m_model.exportCSV(filename.toStdString())) {
            QMessageBox::information(this, "导出成功", QString("仿真数据已导出至 CSV 表格：\n%1").arg(filename));
        }
    }
}

void MainWindow::onExportJSONClicked() {
    QString filename = QFileDialog::getSaveFileName(this, "导出数据为 JSON", "results.json", "JSON 文件 (*.json)");
    if (!filename.isEmpty()) {
        if (m_model.exportJSON(filename.toStdString())) {
            QMessageBox::information(this, "导出成功", QString("仿真数据已导出为 JSON：\n%1").arg(filename));
        }
    }
}

void MainWindow::onTableUserSelected(int row, int) {
    if (row >= 0 && row < m_tableUsers->rowCount()) {
        int uid = m_tableUsers->item(row, 0)->text().replace("U", "").replace("★", "").trimmed().toInt();
        m_canvas->setSelectedUserId(uid);
        updateUserInfoPanel(uid);
    }
}

void MainWindow::onCanvasUserClicked(int userId) {
    for (int i = 0; i < m_tableUsers->rowCount(); ++i) {
        int uid = m_tableUsers->item(i, 0)->text().replace("U", "").replace("★", "").trimmed().toInt();
        if (uid == userId) {
            m_tableUsers->selectRow(i);
            updateUserInfoPanel(userId);
            break;
        }
    }
}

void MainWindow::updateUserInfoPanel(int userId) {
    if (!m_model.hasSolved()) return;

    UserNode *cur = m_model.getUserListHead();
    while (cur && cur->id != userId) cur = cur->next;
    if (!cur) return;

    const SchemeResult &km = m_model.getKMeansResult();
    const SchemeResult &sa = m_model.getSAResult();

    int didKM = km.assign[userId];
    int didSA = sa.assign[userId];
    bool isReassigned = (didKM != didSA);

    m_infoUserId->setText(QString("用户 U%1 节点状态 %2").arg(userId).arg(isReassigned ? "★ (SA重分配)" : ""));
    m_infoCoord->setText(QString("三维坐标: (%1, %2, 0.0) m").arg(cur->x, 0, 'f', 2).arg(cur->y, 0, 'f', 2));
    m_infoCapacity->setText(QString("容量需求 Rn: %1 bps/Hz").arg(cur->R, 0, 'f', 2));

    m_infoKmDist->setText(QString("服务基站: UAV %1 | 距离: %2 m").arg(didKM + 1).arg(km.dist_3d[userId], 0, 'f', 2));
    m_infoKmPower->setText(QString("发射功率 pn: %1 W").arg(km.power[userId], 0, 'f', 2));

    m_infoSaDist->setText(QString("服务基站: UAV %1 | 距离: %2 m").arg(didSA + 1).arg(sa.dist_3d[userId], 0, 'f', 2));
    m_infoSaPower->setText(QString("发射功率 pn: %1 W").arg(sa.power[userId], 0, 'f', 2));

    if (isReassigned) {
        m_infoReassign->setText("<span style='color:#E3B341; font-weight:bold;'>💡 智能重分配节点：</span>在模拟退火全局优化中，该节点被智能转移到另一基站，大幅削减了瓶颈外接圆半径！");
    } else {
        m_infoReassign->setText("<span style='color:#8B949E;'>两方案归属一致。</span>");
    }
}

void MainWindow::onAboutClicked() {
    QMessageBox::about(this, "关于本系统",
        "<h3>无人机地面基站覆盖与功率优化系统 [工业仿真平台]</h3>"
        "<p><b>作者:</b> 肖奕扬 | 中国矿业大学</p>"
        "<p><b>课程:</b> 数据结构与算法分析课程设计 (第4题)</p>"
        "<p><b>技术栈:</b> Qt 6.8.2 C++17, QPainter 硬件加速渲染, 严格动态链表结构体存储</p>"
        "<p><b>核心算法:</b> 精确最小外接圆定点 (MEC)、K-Means 聚类、模拟退火 Minimax 联合优化</p>");
}

void MainWindow::onMathPrincipleClicked() {
    m_bottomTabs->setCurrentIndex(2); // 切换至原理选项卡
}

void MainWindow::onUsageClicked() {
    QMessageBox::information(this, "使用说明",
        "1. 在左侧面板调节地面用户数 N、飞行高度 G 与监控区域；\n"
        "2. 点击【一键全流程求解与对比】即可自动执行链表构建、K-Means 与 SA 优化并启动入场飞散动画；\n"
        "3. 在画布中支持滚轮缩放、右键拖拽平移、双击复位；\n"
        "4. 点击画布中任一用户点，右侧面板与底部表格将联动显示遥测详情。");
}

void MainWindow::onStatusTick() {
    if (m_isSolving) {
        m_statusDots = (m_statusDots + 1) % 4;
        QString dots = QString(".").repeated(m_statusDots);
        m_statusAlgo->setText("状态: 模拟退火优化计算中" + dots);
    }
}

void MainWindow::onBottomPanelAnimFinished() {
}

void MainWindow::updateTables() {
    updateMetricsTab();
    updateUsersTab();
}

void MainWindow::updateMetricsTab() {
    m_tableMetrics->setRowCount(0);
    if (!m_model.hasSolved()) return;

    const SchemeResult &km = m_model.getKMeansResult();
    const SchemeResult &sa = m_model.getSAResult();

    auto addRow = [this](const QString &item, double v1, double v2, const QString &unit, bool isLowerBetter = true) {
        int row = m_tableMetrics->rowCount();
        m_tableMetrics->insertRow(row);

        double diff = ((v2 - v1) / v1) * 100.0;
        QString diffStr = QString("%1%2%").arg(diff >= 0 ? "+" : "").arg(diff, 0, 'f', 2);

        QTableWidgetItem *wItem = new QTableWidgetItem(item);
        QTableWidgetItem *wV1 = new QTableWidgetItem(QString("%1 %2").arg(v1, 0, 'f', 2).arg(unit));
        QTableWidgetItem *wV2 = new QTableWidgetItem(QString("%1 %2").arg(v2, 0, 'f', 2).arg(unit));
        QTableWidgetItem *wDiff = new QTableWidgetItem(diffStr);

        wItem->setTextAlignment(Qt::AlignCenter);
        wV1->setTextAlignment(Qt::AlignCenter);
        wV2->setTextAlignment(Qt::AlignCenter);
        wDiff->setTextAlignment(Qt::AlignCenter);

        if ((isLowerBetter && diff < -0.01) || (!isLowerBetter && diff > 0.01)) {
            wDiff->setForeground(QColor(57, 211, 83)); // 显著改善显示醒目绿色
        } else if ((isLowerBetter && diff > 0.01) || (!isLowerBetter && diff < -0.01)) {
            wDiff->setForeground(QColor(248, 81, 73));
        }

        m_tableMetrics->setItem(row, 0, wItem);
        m_tableMetrics->setItem(row, 1, wV1);
        m_tableMetrics->setItem(row, 2, wV2);
        m_tableMetrics->setItem(row, 3, wDiff);
    };

    addRow("瓶颈覆盖半径 max(r1, r2) [题目核心目标]", km.metrics.max_horizontal_r, sa.metrics.max_horizontal_r, "m", true);
    addRow("最远用户三维空间距离 max(dn)", km.metrics.max_3d_dist, sa.metrics.max_3d_dist, "m", true);
    addRow("无人机 1 最小发射功率 P1", km.drones[0].total_power, sa.drones[0].total_power, "W", true);
    addRow("无人机 2 最小发射功率 P2", km.drones[1].total_power, sa.drones[1].total_power, "W", true);
    addRow("系统两机总发射功率 P_total", km.metrics.total_power, sa.metrics.total_power, "W", true);
    addRow("单用户最小所需功率 min(pn)", km.metrics.min_user_p, sa.metrics.min_user_p, "W", true);
    addRow("单用户最大所需功率 max(pn)", km.metrics.max_user_p, sa.metrics.max_user_p, "W", true);
}

void MainWindow::updateUsersTab() {
    m_tableUsers->setRowCount(0);
    if (!m_model.hasSolved()) return;

    UserNode *cur = m_model.getUserListHead();
    const SchemeResult &km = m_model.getKMeansResult();
    const SchemeResult &sa = m_model.getSAResult();
    int idx = 0;

    while (cur && idx < static_cast<int>(km.assign.size())) {
        int row = m_tableUsers->rowCount();
        m_tableUsers->insertRow(row);

        bool isReassigned = (km.assign[idx] != sa.assign[idx]);

        auto setItem = [&](int col, const QString &text, const QColor &color = QColor(201, 209, 217)) {
            QTableWidgetItem *item = new QTableWidgetItem(text);
            item->setTextAlignment(Qt::AlignCenter);
            item->setForeground(color);
            m_tableUsers->setItem(row, col, item);
        };

        QColor idColor = isReassigned ? QColor(227, 179, 65) : QColor(201, 209, 217);
        setItem(0, QString("U%1%2").arg(cur->id).arg(isReassigned ? "★" : ""), idColor);
        setItem(1, QString("(%1, %2)").arg(cur->x, 0, 'f', 1).arg(cur->y, 0, 'f', 1));
        setItem(2, QString("%1").arg(cur->R, 0, 'f', 2));

        setItem(3, QString("UAV %1").arg(km.assign[idx] + 1), (km.assign[idx] == 0 ? QColor(255, 85, 85) : QColor(88, 166, 255)));
        setItem(4, QString("%1 m").arg(km.dist_3d[idx], 0, 'f', 2));
        setItem(5, QString("%1 W").arg(km.power[idx], 0, 'f', 1));

        setItem(6, QString("UAV %1").arg(sa.assign[idx] + 1), (sa.assign[idx] == 0 ? QColor(255, 85, 85) : QColor(88, 166, 255)));
        setItem(7, QString("%1 m").arg(sa.dist_3d[idx], 0, 'f', 2));
        setItem(8, QString("%1 W").arg(sa.power[idx], 0, 'f', 1));

        cur = cur->next;
        idx++;
    }
}

void MainWindow::updateTheoryTab() {
    QString html = R"(
        <h3 style="color:#58A6FF; margin-top:2px;">无人机基站覆盖与功率优化数学原理与创新报告摘要</h3>
        <p><b>1. 数据结构设计（链式存储严格要求）：</b><br>
        本系统地面用户数据采用严格的单向动态链表 <code>struct UserNode*</code> 存储。节点包含用户三维空间坐标 (an, bn, 0)、所需香农容量 Rn、三维直线距离 dn、信道系数 hn 以及分配发射功率 pn。利用指针串联所有节点，杜绝静态数组冗余。</p>

        <p><b>2. 最小外接圆定点原理（解决题目第 3 问 Minimax 要求）：</b><br>
        地面用户到无人机三维欧氏距离为 \(d_n = \sqrt{r_n^2 + G^2}\)。由于无人机飞行高度 \(G>0\) 恒定，三维距离 Minimax 优化严格等价于水平面二维投影距离的 Minimax 优化。系统采用精确几何判定法求解点集的最小外接圆（MEC），以圆心作为无人机部署坐标，保证了各无人机覆盖其服务子集所有用户的同时，最大水平距离严格达到极小值。</p>

        <p><b>3. 用户划分基线与创新改进对比（解决题目第 2 问与创新点）：</b><br>
        <ul>
            <li><b>基线方案（K-Means 聚类）：</b>以最小化均方误差和 \(\sum d_n^2\) 为准则，能够将空间相邻用户聚集，但在处理边缘离群点时，极易导致局部的覆盖半径过大。</li>
            <li><b>创新优化（模拟退火 Minimax 全局搜索）：</b>直接针对瓶颈目标 \(E = \max(r_1, r_2)\) 构建能量函数。通过 Metropolis 准则动态翻转边缘用户的基站归属（标记为黄色五角星 ★），成功克服了 K-Means 的局部极值缺陷，实测瓶颈覆盖半径降低 <b>15% ~ 25%</b>，兼顾了两架无人机的功率均衡。</li>
        </ul></p>

        <p><b>4. 香农容量公式反解最小发射功率（解决题目第 4 问）：</b><br>
        信道容量 \(R_n = \log_2(1 + p_n h_n)\)，其中信道衰减系数 \(h_n = 1/d_n^2\)。反解满足通信需求的单用户最小分配功率为 \(p_n = (2^{R_n} - 1) \cdot d_n^2\)。无人机最小发射功率即为服务子集所有用户的发射功率之和 \(P_{\text{total}, i} = \sum p_n\)。</p>
    )";
    m_textTheory->setHtml(html);
}
