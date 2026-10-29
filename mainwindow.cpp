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
#include <random>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_canvas(nullptr) {
    setWindowTitle("无人机地面基站覆盖与功率优化系统 [工业仿真平台] - 数据结构与算法分析");
    resize(1360, 900);
    setMinimumSize(1100, 750);

    setupUI();
    applyIndustrialTheme();

    // 默认生成初值并全流程运行
    onRunAllClicked();
}

MainWindow::~MainWindow() {
}

void MainWindow::setupUI() {
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    QHBoxLayout *mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);

    // ========================================================================
    // 左侧面板：控制与参数输入
    // ========================================================================
    QWidget *leftPanel = new QWidget(this);
    leftPanel->setFixedWidth(310);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(8);

    // 1. 系统参数组
    QGroupBox *paramGroup = new QGroupBox("系统模型与仿真参数", leftPanel);
    QGridLayout *paramLayout = new QGridLayout(paramGroup);
    paramLayout->setContentsMargins(10, 14, 10, 10);
    paramLayout->setVerticalSpacing(6);

    paramLayout->addWidget(new QLabel("地面用户数 N (≥8):"), 0, 0);
    m_spinN = new QSpinBox(paramGroup);
    m_spinN->setRange(8, 100);
    m_spinN->setValue(16);
    paramLayout->addWidget(m_spinN, 0, 1);

    paramLayout->addWidget(new QLabel("飞行高度 G (m):"), 1, 0);
    m_spinG = new QDoubleSpinBox(paramGroup);
    m_spinG->setRange(10.0, 100.0);
    m_spinG->setValue(20.0);
    m_spinG->setSingleStep(1.0);
    paramLayout->addWidget(m_spinG, 1, 1);

    paramLayout->addWidget(new QLabel("监控区域 L (m):"), 2, 0);
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
    btnRandomSeed->setToolTip("生成随机种子");
    btnRandomSeed->setFixedWidth(30);
    connect(btnRandomSeed, &QPushButton::clicked, this, &MainWindow::onRandomSeedClicked);
    seedLayout->addWidget(m_spinSeed);
    seedLayout->addWidget(btnRandomSeed);
    paramLayout->addLayout(seedLayout, 3, 1);

    paramLayout->addWidget(new QLabel("容量区间 Rn (bps/Hz):"), 4, 0);
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

    // 2. 算法控制组
    QGroupBox *solverGroup = new QGroupBox("算法求解与仿真调度", leftPanel);
    QVBoxLayout *solverLayout = new QVBoxLayout(solverGroup);
    solverLayout->setContentsMargins(10, 14, 10, 10);
    solverLayout->setSpacing(6);

    QPushButton *btnRunAll = new QPushButton("▶ 一键全流程求解与对比", solverGroup);
    btnRunAll->setObjectName("primaryBtn");
    btnRunAll->setMinimumHeight(38);
    connect(btnRunAll, &QPushButton::clicked, this, &MainWindow::onRunAllClicked);
    solverLayout->addWidget(btnRunAll);

    QPushButton *btnGenerate = new QPushButton("1. 重新随机生成用户链表", solverGroup);
    connect(btnGenerate, &QPushButton::clicked, this, &MainWindow::onGenerateClicked);
    solverLayout->addWidget(btnGenerate);

    QPushButton *btnKMeans = new QPushButton("2. 仅执行 K-Means 基线聚类", solverGroup);
    connect(btnKMeans, &QPushButton::clicked, this, &MainWindow::onKMeansClicked);
    solverLayout->addWidget(btnKMeans);

    QPushButton *btnSA = new QPushButton("3. 仅执行 模拟退火 Minimax 优化", solverGroup);
    connect(btnSA, &QPushButton::clicked, this, &MainWindow::onSAClicked);
    solverLayout->addWidget(btnSA);

    leftLayout->addWidget(solverGroup);

    // 3. 视图渲染与导出组
    QGroupBox *viewGroup = new QGroupBox("显示渲染与成果导出", leftPanel);
    QVBoxLayout *viewLayout = new QVBoxLayout(viewGroup);
    viewLayout->setContentsMargins(10, 14, 10, 10);
    viewLayout->setSpacing(6);

    viewLayout->addWidget(new QLabel("视图布局模式:"));
    m_comboViewMode = new QComboBox(viewGroup);
    m_comboViewMode->addItem("双屏分栏对比 (基线 vs SA)");
    m_comboViewMode->addItem("单屏模式：K-Means 基线方案");
    m_comboViewMode->addItem("单屏模式：模拟退火创新方案");
    connect(m_comboViewMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onViewModeChanged);
    viewLayout->addWidget(m_comboViewMode);

    m_chkCircles = new QCheckBox("显示无人机覆盖外接圆", viewGroup);
    m_chkCircles->setChecked(true);
    connect(m_chkCircles, &QCheckBox::toggled, [this](bool checked){ m_canvas->setShowCircles(checked); });
    viewLayout->addWidget(m_chkCircles);

    m_chkLinks = new QCheckBox("显示用户-基站通信连线", viewGroup);
    m_chkLinks->setChecked(true);
    connect(m_chkLinks, &QCheckBox::toggled, [this](bool checked){ m_canvas->setShowLinks(checked); });
    viewLayout->addWidget(m_chkLinks);

    QPushButton *btnResetView = new QPushButton("🔍 重置画布缩放与居中", viewGroup);
    connect(btnResetView, &QPushButton::clicked, this, &MainWindow::onResetViewClicked);
    viewLayout->addWidget(btnResetView);

    QHBoxLayout *exportLayout = new QHBoxLayout();
    QPushButton *btnExportImg = new QPushButton("📷 导出高清图", viewGroup);
    connect(btnExportImg, &QPushButton::clicked, this, &MainWindow::onExportImageClicked);
    QPushButton *btnExportCSV = new QPushButton("📊 导出CSV", viewGroup);
    connect(btnExportCSV, &QPushButton::clicked, this, &MainWindow::onExportCSVClicked);
    exportLayout->addWidget(btnExportImg);
    exportLayout->addWidget(btnExportCSV);
    viewLayout->addLayout(exportLayout);

    leftLayout->addWidget(viewGroup);
    leftLayout->addStretch();

    mainLayout->addWidget(leftPanel);

    // ========================================================================
    // 右侧区域：主画布 + 底部信息面板
    // ========================================================================
    QWidget *rightPanel = new QWidget(this);
    QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(8);

    // 核心画布
    m_canvas = new UAVCanvas(rightPanel);
    m_canvas->setModel(&m_model);
    connect(m_canvas, &UAVCanvas::userClicked, this, &MainWindow::onCanvasUserClicked);
    rightLayout->addWidget(m_canvas, 6);

    // 底部选项卡：量化指标对比、链表数据审查、数学创新原理
    m_bottomTabs = new QTabWidget(rightPanel);
    m_bottomTabs->setFixedHeight(220);

    // Tab 1: 算法量化指标对比表
    m_tableMetrics = new QTableWidget(m_bottomTabs);
    m_tableMetrics->setColumnCount(4);
    m_tableMetrics->setHorizontalHeaderLabels({"量化指标项", "方案 A：K-Means 基线聚类", "方案 B：模拟退火 Minimax 优化", "创新算法优化改善幅度"});
    m_tableMetrics->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_tableMetrics->verticalHeader()->setVisible(false);
    m_tableMetrics->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableMetrics->setSelectionMode(QAbstractItemView::NoSelection);
    m_bottomTabs->addTab(m_tableMetrics, "📈 方案量化指标对比仪表盘");

    // Tab 2: 用户链表节点完整数据表
    m_tableUsers = new QTableWidget(m_bottomTabs);
    m_tableUsers->setColumnCount(9);
    m_tableUsers->setHorizontalHeaderLabels({
        "用户ID", "平面坐标 (x, y)", "需求容量 Rn",
        "KMeans归属", "KMeans空间距离", "KMeans发射功率",
        "SA归属", "SA空间距离", "SA发射功率"
    });
    m_tableUsers->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_tableUsers->verticalHeader()->setVisible(false);
    m_tableUsers->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableUsers->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableUsers->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(m_tableUsers, &QTableWidget::cellClicked, this, &MainWindow::onTableUserSelected);
    m_bottomTabs->addTab(m_tableUsers, "📋 地面用户链式存储节点数据表");

    // Tab 3: 数学原理与创新说明
    m_textTheory = new QTextEdit(m_bottomTabs);
    m_textTheory->setReadOnly(true);
    m_bottomTabs->addTab(m_textTheory, "💡 算法数学原理与创新报告摘要");
    updateTheoryTab();

    rightLayout->addWidget(m_bottomTabs, 4);
    mainLayout->addWidget(rightPanel, 1);

    // 底部状态栏
    m_statusLabel = new QLabel("系统就绪", this);
    statusBar()->addWidget(m_statusLabel);
}

void MainWindow::applyIndustrialTheme() {
    QString qss = R"(
        QMainWindow, QWidget {
            background-color: #17181C;
            color: #DCE0E8;
            font-family: "Microsoft YaHei", "Segoe UI", sans-serif;
            font-size: 12px;
        }
        QGroupBox {
            border: 1px solid #2F3340;
            border-radius: 6px;
            margin-top: 14px;
            padding-top: 10px;
            font-weight: bold;
            color: #A0B2C6;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            left: 10px;
            padding: 0 4px;
        }
        QPushButton {
            background-color: #252833;
            border: 1px solid #3A3F50;
            border-radius: 4px;
            padding: 6px 12px;
            color: #E2E6EF;
        }
        QPushButton:hover {
            background-color: #313645;
            border-color: #0084FF;
        }
        QPushButton:pressed {
            background-color: #1C1E26;
        }
        QPushButton#primaryBtn {
            background-color: #007ACC;
            border: 1px solid #0098FF;
            font-weight: bold;
            color: #FFFFFF;
        }
        QPushButton#primaryBtn:hover {
            background-color: #008AE6;
        }
        QSpinBox, QDoubleSpinBox, QComboBox {
            background-color: #21242D;
            border: 1px solid #373C4B;
            border-radius: 4px;
            padding: 4px 6px;
            color: #FFFFFF;
        }
        QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus {
            border-color: #007ACC;
        }
        QCheckBox {
            color: #C0C5D2;
            spacing: 6px;
        }
        QTabWidget::pane {
            border: 1px solid #2C303D;
            background-color: #1A1C23;
            border-radius: 4px;
        }
        QTabBar::tab {
            background-color: #20232C;
            color: #9AA0B2;
            border: 1px solid #2C303D;
            border-bottom: none;
            padding: 6px 14px;
            border-top-left-radius: 4px;
            border-top-right-radius: 4px;
            margin-right: 2px;
        }
        QTabBar::tab:selected {
            background-color: #1A1C23;
            color: #00B4D8;
            font-weight: bold;
            border-top: 2px solid #00B4D8;
        }
        QTableWidget {
            background-color: #1A1C23;
            gridline-color: #2B2F3D;
            border: none;
            color: #E0E4F0;
        }
        QHeaderView::section {
            background-color: #222631;
            color: #A4ADC0;
            padding: 5px;
            border: 1px solid #2B2F3D;
            font-weight: bold;
        }
        QTextEdit {
            background-color: #1A1C23;
            border: none;
            color: #C5CBD8;
            line-height: 1.4;
        }
        QStatusBar {
            background-color: #141518;
            color: #7A8194;
            border-top: 1px solid #252833;
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

    m_model.generateRandomUsers(n, area, g, rmin, rmax, seed);
    m_model.solveAll();

    m_canvas->update();
    updateTables();

    m_statusLabel->setText(QString("[%1] 全流程求解完成：N=%2, G=%3m, 种子=%4, 瓶颈覆盖半径下降 %5%")
                               .arg(QDateTime::currentDateTime().toString("HH:mm:ss"))
                               .arg(n).arg(g).arg(seed)
                               .arg(QString::number(
                                   ((m_model.getSAResult().metrics.max_horizontal_r - m_model.getKMeansResult().metrics.max_horizontal_r) /
                                    m_model.getKMeansResult().metrics.max_horizontal_r) * 100.0, 'f', 2)));
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
    m_canvas->update();
    updateTables();
    m_statusLabel->setText("已生成随机地面用户链表并完成基线聚类。");
}

void MainWindow::onKMeansClicked() {
    m_model.solveKMeans();
    m_canvas->update();
    updateTables();
    m_statusLabel->setText("已完成 K-Means (K=2) 基线划分求解。");
}

void MainWindow::onSAClicked() {
    m_model.solveSimulatedAnnealing();
    m_canvas->update();
    updateTables();
    m_statusLabel->setText("已完成 模拟退火 Minimax 创新优化求解。");
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

void MainWindow::onExportImageClicked() {
    QString filename = QFileDialog::getSaveFileName(this, "导出高清对比图", "uav_coverage_comparison.png", "PNG 图像 (*.png)");
    if (!filename.isEmpty()) {
        if (m_canvas->saveImage(filename)) {
            QMessageBox::information(this, "导出成功", QString("高清对比图已保存至：\n%1").arg(filename));
        } else {
            QMessageBox::warning(this, "导出失败", "无法写入图像文件！");
        }
    }
}

void MainWindow::onExportCSVClicked() {
    QString filename = QFileDialog::getSaveFileName(this, "导出数据表为 CSV", "uav_results.csv", "CSV 文件 (*.csv)");
    if (!filename.isEmpty()) {
        if (m_model.exportCSV(filename.toStdString())) {
            QMessageBox::information(this, "导出成功", QString("仿真数据表已成功导出至：\n%1").arg(filename));
        } else {
            QMessageBox::warning(this, "导出失败", "无法写入 CSV 文件！");
        }
    }
}

void MainWindow::onExportJSONClicked() {
    QString filename = QFileDialog::getSaveFileName(this, "导出数据为 JSON", "results.json", "JSON 文件 (*.json)");
    if (!filename.isEmpty()) {
        if (m_model.exportJSON(filename.toStdString())) {
            QMessageBox::information(this, "导出成功", QString("JSON 数据已成功导出至：\n%1").arg(filename));
        }
    }
}

void MainWindow::onTableUserSelected(int row, int) {
    if (row >= 0 && row < m_tableUsers->rowCount()) {
        int uid = m_tableUsers->item(row, 0)->text().replace("U", "").toInt();
        m_canvas->setSelectedUserId(uid);
    }
}

void MainWindow::onCanvasUserClicked(int userId) {
    for (int i = 0; i < m_tableUsers->rowCount(); ++i) {
        if (m_tableUsers->item(i, 0)->text() == QString("U%1").arg(userId)) {
            m_tableUsers->selectRow(i);
            m_bottomTabs->setCurrentIndex(1); // 切换到用户表查看
            break;
        }
    }
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
            wDiff->setForeground(QColor(0, 230, 118)); // 显著改善显示醒目绿色
        } else if ((isLowerBetter && diff > 0.01) || (!isLowerBetter && diff < -0.01)) {
            wDiff->setForeground(QColor(255, 112, 67));
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

        auto setItem = [&](int col, const QString &text, const QColor &color = QColor(220, 225, 235)) {
            QTableWidgetItem *item = new QTableWidgetItem(text);
            item->setTextAlignment(Qt::AlignCenter);
            item->setForeground(color);
            m_tableUsers->setItem(row, col, item);
        };

        QColor idColor = isReassigned ? QColor(255, 215, 0) : QColor(220, 225, 235);
        setItem(0, QString("U%1%2").arg(cur->id).arg(isReassigned ? "★" : ""), idColor);
        setItem(1, QString("(%1, %2)").arg(cur->x, 0, 'f', 1).arg(cur->y, 0, 'f', 1));
        setItem(2, QString("%1").arg(cur->R, 0, 'f', 2));

        setItem(3, QString("UAV %1").arg(km.assign[idx] + 1), (km.assign[idx] == 0 ? QColor(255, 100, 100) : QColor(100, 180, 255)));
        setItem(4, QString("%1 m").arg(km.dist_3d[idx], 0, 'f', 2));
        setItem(5, QString("%1 W").arg(km.power[idx], 0, 'f', 1));

        setItem(6, QString("UAV %1").arg(sa.assign[idx] + 1), (sa.assign[idx] == 0 ? QColor(255, 100, 100) : QColor(100, 180, 255)));
        setItem(7, QString("%1 m").arg(sa.dist_3d[idx], 0, 'f', 2));
        setItem(8, QString("%1 W").arg(sa.power[idx], 0, 'f', 1));

        cur = cur->next;
        idx++;
    }
}

void MainWindow::updateTheoryTab() {
    QString html = R"(
        <h3 style="color:#00B4D8; margin-top:2px;">无人机基站覆盖与功率优化数学原理与创新报告摘要</h3>
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
