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
#include <QProgressBar>
#include <QTimer>
#include <QLabel>
#include "canvas.h"
#include "uav_model.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onRunAllClicked();
    void onGenerateClicked();
    void onKMeansClicked();
    void onSAClicked();
    void onRandomSeedClicked();
    void onViewModeChanged(int index);
    void onResetViewClicked();
    void onExportImageClicked();
    void onExportCSVClicked();
    void onExportJSONClicked();
    void onTableUserSelected(int row, int column);
    void onCanvasUserClicked(int userId);

private:
    void setupUI();
    void applyIndustrialTheme();
    void updateTables();
    void updateMetricsTab();
    void updateUsersTab();
    void updateTheoryTab();

public:
    bool exportDirectImage(const QString &filepath) {
        return m_canvas->saveImage(filepath);
    }

    UAVSystemModel m_model;
    UAVCanvas *m_canvas;

    // 参数控件
    QSpinBox *m_spinN;
    QDoubleSpinBox *m_spinG;
    QDoubleSpinBox *m_spinArea;
    QSpinBox *m_spinSeed;
    QDoubleSpinBox *m_spinRMin;
    QDoubleSpinBox *m_spinRMax;

    // 视图与渲染
    QComboBox *m_comboViewMode;
    QCheckBox *m_chkCircles;
    QCheckBox *m_chkLinks;

    // 底部表格
    QTabWidget *m_bottomTabs;
    QTableWidget *m_tableMetrics;
    QTableWidget *m_tableUsers;
    QTextEdit *m_textTheory;

    // 状态栏
    QLabel *m_statusLabel;
};

#endif // MAINWINDOW_H
