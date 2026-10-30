#ifndef UAV_MODEL_H
#define UAV_MODEL_H

#include <vector>
#include <string>

// 1. 基础几何定义
struct Point2D {
    double x;
    double y;
};

struct Circle {
    Point2D center;
    double radius;
};

// 2. 地面用户链表节点结构体 (数据结构课程设计规范：严格采用链式存储)
struct UserNode {
    int id;                 // 用户编号 (0 ~ N-1)
    double x, y, z;         // 三维坐标 (an, bn, 0)
    double R;               // 目标通信容量需求 Rn
    double d;               // 到所属无人机的三维空间距离 dn
    double h;               // 信道衰减系数 hn = 1 / dn^2
    double p;               // 满足容量的最小发射功率 pn
    int drone_id;           // 服务无人机编号 (0 或 1)
    UserNode *next;         // 链表后继指针
};

// 3. 无人机属性结构体
struct Drone {
    int id;                 // 无人机编号 (0 或 1)
    double x, y, z;         // 空间坐标 (ci, di, G)
    double max_r;           // 水平覆盖半径
    double total_power;     // 该无人机总发射功率
    int user_count;         // 服务用户数量
};

// 4. 方案综合评估指标
struct SystemMetrics {
    double max_horizontal_r;    // 瓶颈覆盖半径 max(r1, r2)
    double max_3d_dist;         // 系统最远单点空间距离 max(dn)
    double total_power;         // 系统总发射功率 P_total = P1 + P2
    double min_user_p;          // 所有用户中最小所需功率 min(pn)
    double max_user_p;          // 所有用户中最大所需功率 max(pn)
};

// 5. 仿真方案快照 (存储基线方案与创新优化方案的求解数据)
struct SchemeResult {
    std::string name;           // 方案名称 ("K-Means 基准" 或 "模拟退火 Minimax 优化")
    Drone drones[2];            // 两架无人机状态
    SystemMetrics metrics;      // 系统综合指标
    std::vector<int> assign;    // 各用户的服务机分配 (0 或 1)
    std::vector<double> dist_3d;// 各用户到服务机的三维距离
    std::vector<double> channel;// 各用户的信道衰减系数
    std::vector<double> power;  // 各用户的最小分配功率
};

// 6. 模型与算法核心管理器
class UAVSystemModel {
public:
    UAVSystemModel();
    ~UAVSystemModel();

    // 链表管理与数据生成
    void clearUsers();
    void generateRandomUsers(int n, double area, double G, double r_min, double r_max, unsigned int seed);

    // 双算法执行接口
    void solveKMeans();
    void solveSimulatedAnnealing(double t_start = 50.0, double t_min = 0.01, double alpha = 0.94);
    void solveAll();

    // 核心几何求解器 (最小外接圆定点)
    static Circle solveMEC(const std::vector<Point2D> &pts);

    // 状态查询
    UserNode* getUserListHead() const { return m_head; }
    int getUserCount() const { return m_userCount; }
    double getAltitude() const { return m_G; }
    double getAreaSize() const { return m_area; }
    unsigned int getSeed() const { return m_seed; }

    const SchemeResult& getKMeansResult() const { return m_kmResult; }
    const SchemeResult& getSAResult() const { return m_saResult; }
    bool hasSolved() const { return m_hasSolved; }

    // 数据导出辅助
    bool exportJSON(const std::string &filepath) const;
    bool exportCSV(const std::string &filepath) const;

private:
    void appendUserNode(int id, double x, double y, double R);
    void evaluateScheme(const std::vector<int> &assign, SchemeResult &result);

    UserNode *m_head;
    int m_userCount;
    double m_G;
    double m_area;
    unsigned int m_seed;
    bool m_hasSolved;

    SchemeResult m_kmResult;
    SchemeResult m_saResult;
};

#endif // UAV_MODEL_H
