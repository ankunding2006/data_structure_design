#include <iostream>
#include <iomanip>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <cstring>

using namespace std;

// ============================================================================
// 1. 数据结构设计：符合数据结构课程设计规范，采用链式存储
// ============================================================================

// 二维点与圆结构（用于几何覆盖计算）
struct Point2D {
    double x, y;
};

struct Circle {
    Point2D center;
    double radius;
};

// 地面用户链表节点结构体 (严格采用链式存储结构)
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

// 无人机状态信息结构体
struct Drone {
    int id;                 // 无人机编号
    double x, y, z;         // 空间坐标 (ci, di, G)
    double max_r;           // 水平覆盖半径
    double total_power;     // 该机服务用户的发射功率之和 (即最小总发射功率)
    int user_count;         // 服务用户数
};

// 系统综合指标统计
struct SystemMetrics {
    double max_horizontal_r;// 瓶颈水平覆盖半径 max(r1, r2)
    double max_3d_dist;     // 系统最远单点空间距离 max(dn)
    double total_power;     // 系统总发射功率 P_total
    double min_user_p;      // 所有用户中的最小单点功率 min(pn)
    double max_user_p;      // 所有用户中的最大单点功率 max(pn)
};

// ============================================================================
// 2. 链表基本操作 (创建、尾插、遍历与内存释放)
// ============================================================================

UserNode* create_node(int id, double x, double y, double R) {
    UserNode* node = new UserNode;
    node->id = id;
    node->x = x;
    node->y = y;
    node->z = 0.0;
    node->R = R;
    node->d = 0.0;
    node->h = 0.0;
    node->p = 0.0;
    node->drone_id = 0;
    node->next = nullptr;
    return node;
}

void append_user(UserNode *&head, int id, double x, double y, double R) {
    UserNode *node = create_node(id, x, y, R);
    if (!head) {
        head = node;
    } else {
        UserNode *cur = head;
        while (cur->next) cur = cur->next;
        cur->next = node;
    }
}

void free_list(UserNode *head) {
    while (head) {
        UserNode *tmp = head;
        head = head->next;
        delete tmp;
    }
}

// ============================================================================
// 3. 几何核心：最小外接圆判定算法 (解决 Minimax 覆盖定点)
// ============================================================================

Circle solve_mec(Point2D pts[], int n) {
    if (n == 0) return {{0, 0}, 0};
    if (n == 1) return {pts[0], 0};
    if (n == 2) {
        Point2D c = {(pts[0].x + pts[1].x) / 2.0, (pts[0].y + pts[1].y) / 2.0};
        return {c, hypot(pts[0].x - pts[1].x, pts[0].y - pts[1].y) / 2.0};
    }

    Circle best = {{0, 0}, 1e18};

    // 1) 两点定圆判定（以两点连线为直径）
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            Point2D c = {(pts[i].x + pts[j].x) / 2.0, (pts[i].y + pts[j].y) / 2.0};
            double r = hypot(pts[i].x - pts[j].x, pts[i].y - pts[j].y) / 2.0;
            if (r >= best.radius) continue;

            bool ok = true;
            for (int k = 0; k < n; ++k) {
                if (hypot(pts[k].x - c.x, pts[k].y - c.y) > r + 1e-6) {
                    ok = false;
                    break;
                }
            }
            if (ok) best = {c, r};
        }
    }

    // 2) 三点定圆判定（三角形外接圆）
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            for (int k = j + 1; k < n; ++k) {
                double x1 = pts[i].x, y1 = pts[i].y;
                double x2 = pts[j].x, y2 = pts[j].y;
                double x3 = pts[k].x, y3 = pts[k].y;
                double D = 2.0 * (x1 * (y2 - y3) + x2 * (y3 - y1) + x3 * (y1 - y2));
                if (fabs(D) < 1e-7) continue; // 三点共线

                double ux = ((x1*x1 + y1*y1)*(y2 - y3) + (x2*x2 + y2*y2)*(y3 - y1) + (x3*x3 + y3*y3)*(y1 - y2)) / D;
                double uy = ((x1*x1 + y1*y1)*(x3 - x2) + (x2*x2 + y2*y2)*(x1 - x3) + (x3*x3 + y3*y3)*(x2 - x1)) / D;
                double r = hypot(x1 - ux, y1 - uy);

                if (r >= best.radius) continue;

                bool ok = true;
                for (int m = 0; m < n; ++m) {
                    if (hypot(pts[m].x - ux, pts[m].y - uy) > r + 1e-6) {
                        ok = false;
                        break;
                    }
                }
                if (ok) best = {{ux, uy}, r};
            }
        }
    }
    return best;
}

// ============================================================================
// 4. 计算当前划分下的无人机定点、信道及发射功率 (香农公式反解)
// ============================================================================

void evaluate_partition(UserNode *head, int N, const int assign[], double G, Drone drones[2], SystemMetrics &metrics) {
    Point2D pts0[N], pts1[N];
    int n0 = 0, n1 = 0;

    UserNode *cur = head;
    int idx = 0;
    while (cur) {
        if (assign[idx] == 0) pts0[n0++] = {cur->x, cur->y};
        else pts1[n1++] = {cur->x, cur->y};
        cur = cur->next;
        idx++;
    }

    Circle c0 = solve_mec(pts0, n0);
    Circle c1 = solve_mec(pts1, n1);

    drones[0] = {0, c0.center.x, c0.center.y, G, c0.radius, 0.0, n0};
    drones[1] = {1, c1.center.x, c1.center.y, G, c1.radius, 0.0, n1};

    metrics.max_3d_dist = 0.0;
    metrics.min_user_p = 1e18;
    metrics.max_user_p = 0.0;

    cur = head;
    idx = 0;
    while (cur) {
        int d_id = assign[idx];
        cur->drone_id = d_id;
        double dx = cur->x - drones[d_id].x;
        double dy = cur->y - drones[d_id].y;
        cur->d = sqrt(dx * dx + dy * dy + G * G);
        cur->h = 1.0 / (cur->d * cur->d);
        cur->p = (pow(2.0, cur->R) - 1.0) * (cur->d * cur->d);

        drones[d_id].total_power += cur->p;
        if (cur->d > metrics.max_3d_dist) metrics.max_3d_dist = cur->d;
        if (cur->p < metrics.min_user_p) metrics.min_user_p = cur->p;
        if (cur->p > metrics.max_user_p) metrics.max_user_p = cur->p;

        cur = cur->next;
        idx++;
    }

    metrics.max_horizontal_r = max(drones[0].max_r, drones[1].max_r);
    metrics.total_power = drones[0].total_power + drones[1].total_power;
}

// ============================================================================
// 5. 划分方法 1：K-Means (K=2) 基准聚类
// ============================================================================

void run_kmeans(UserNode *head, int N, int assign[]) {
    Point2D pts[N];
    UserNode *cur = head;
    for (int i = 0; i < N && cur; ++i, cur = cur->next) {
        pts[i] = {cur->x, cur->y};
    }

    // 初始质心：选择第 0 个点以及离它最远的点
    Point2D mu[2] = {pts[0], pts[0]};
    double max_dist = -1;
    for (int i = 1; i < N; ++i) {
        double d = hypot(pts[i].x - pts[0].x, pts[i].y - pts[0].y);
        if (d > max_dist) { max_dist = d; mu[1] = pts[i]; }
    }

    for (int iter = 0; iter < 100; ++iter) {
        int count[2] = {0, 0};
        Point2D sum[2] = {{0, 0}, {0, 0}};

        for (int i = 0; i < N; ++i) {
            double d0 = hypot(pts[i].x - mu[0].x, pts[i].y - mu[0].y);
            double d1 = hypot(pts[i].x - mu[1].x, pts[i].y - mu[1].y);
            assign[i] = (d0 <= d1) ? 0 : 1;
            count[assign[i]]++;
            sum[assign[i]].x += pts[i].x;
            sum[assign[i]].y += pts[i].y;
        }

        if (count[0] == 0 || count[1] == 0) break;

        Point2D new_mu[2] = {
            {sum[0].x / count[0], sum[0].y / count[0]},
            {sum[1].x / count[1], sum[1].y / count[1]}
        };

        if (hypot(new_mu[0].x - mu[0].x, new_mu[0].y - mu[0].y) < 1e-4 &&
            hypot(new_mu[1].x - mu[1].x, new_mu[1].y - mu[1].y) < 1e-4) break;

        mu[0] = new_mu[0];
        mu[1] = new_mu[1];
    }
}

// ============================================================================
// 6. 划分方法 2 (创新改进)：基于模拟退火 (SA) 的 Minimax 覆盖优化
// ============================================================================

void run_simulated_annealing(UserNode *head, int N, const int init_assign[], int best_assign[]) {
    Point2D pts[N];
    UserNode *cur = head;
    for (int i = 0; i < N && cur; ++i, cur = cur->next) {
        pts[i] = {cur->x, cur->y};
    }

    int cur_assign[N];
    for (int i = 0; i < N; ++i) {
        cur_assign[i] = init_assign[i];
        best_assign[i] = init_assign[i];
    }

    // 目标代价函数：Minimax 瓶颈半径为主，兼顾非瓶颈机半径
    auto eval_cost = [&](const int a[]) -> double {
        Point2D p0[N], p1[N];
        int n0 = 0, n1 = 0;
        for (int i = 0; i < N; ++i) {
            if (a[i] == 0) p0[n0++] = pts[i];
            else p1[n1++] = pts[i];
        }
        if (n0 == 0 || n1 == 0) return 1e12; // 空簇惩罚
        Circle c0 = solve_mec(p0, n0);
        Circle c1 = solve_mec(p1, n1);
        return max(c0.radius, c1.radius) + 0.05 * min(c0.radius, c1.radius);
    };

    double cur_cost = eval_cost(cur_assign);
    double best_cost = cur_cost;

    double T = 50.0;
    double T_min = 1e-2;
    double alpha = 0.94;

    while (T > T_min) {
        for (int step = 0; step < N * 2; ++step) {
            int k = rand() % N;
            cur_assign[k] = 1 - cur_assign[k]; // 单点归属翻转

            double new_cost = eval_cost(cur_assign);
            double delta = new_cost - cur_cost;

            if (delta < 0 || (double)rand() / RAND_MAX < exp(-delta / T)) {
                cur_cost = new_cost;
                if (cur_cost < best_cost) {
                    best_cost = cur_cost;
                    for (int i = 0; i < N; ++i) best_assign[i] = cur_assign[i];
                }
            } else {
                cur_assign[k] = 1 - cur_assign[k]; // 拒绝扰动，恢复
            }
        }
        T *= alpha;
    }
}

// ============================================================================
// 7. 格式化控制台输出与 JSON 数据导出
// ============================================================================

void print_scheme_results(const char *title, UserNode *head, int N, const Drone drones[2], const SystemMetrics &m) {
    cout << "\n=======================================================\n";
    cout << "               " << title << "\n";
    cout << "=======================================================\n";
    cout << fixed << setprecision(2);
    for (int i = 0; i < 2; ++i) {
        cout << "[无人机 " << i + 1 << "] 坐标: (" << drones[i].x << ", " << drones[i].y << ", " << drones[i].z
             << ") | 覆盖半径: " << drones[i].max_r << " m"
             << " | 服务用户数: " << drones[i].user_count
             << " | 最小发射功率: " << drones[i].total_power << " W\n";
    }
    cout << "-------------------------------------------------------\n";
    cout << "系统最大水平覆盖半径 max(r1, r2) : " << m.max_horizontal_r << " m\n";
    cout << "系统最远用户三维空间距离 max(dn) : " << m.max_3d_dist << " m\n";
    cout << "单用户最小所需功率 min(pn)       : " << m.min_user_p << " W\n";
    cout << "单用户最大所需功率 max(pn)       : " << m.max_user_p << " W\n";
    cout << "两架无人机总发射功率之和 P_total : " << m.total_power << " W\n";
    cout << "-------------------------------------------------------\n";
    cout << "用户数据结构详情 (链表节点数据采样展示):\n";
    cout << " ID |  坐标 (x, y)  | 需求 Rn | 分配机 |  距离 dn  | 信道系数 hn | 功率 pn (W)\n";
    UserNode *cur = head;
    int cnt = 0;
    while (cur && cnt < 8) {
        cout << setw(3) << cur->id << " | (" << setw(5) << cur->x << ", " << setw(5) << cur->y << ") | "
             << setw(7) << cur->R << " |   UAV" << cur->drone_id + 1 << "  | "
             << setw(9) << cur->d << " | " << setprecision(4) << setw(11) << cur->h << " | "
             << setprecision(2) << setw(10) << cur->p << "\n";
        cur = cur->next;
        cnt++;
    }
    if (N > 8) cout << " ... (链表中其余 " << N - 8 << " 个节点数据已完整在内存中链式处理)\n";
}

void export_json(const char *filename, UserNode *head, int N, double G, double area,
                 const Drone d_km[2], const SystemMetrics &m_km, const int assign_km[],
                 const Drone d_sa[2], const SystemMetrics &m_sa, const int assign_sa[]) {
    ofstream ofs(filename);
    if (!ofs.is_open()) return;

    ofs << fixed << setprecision(4);
    ofs << "{\n";
    ofs << "  \"N\": " << N << ",\n";
    ofs << "  \"G\": " << G << ",\n";
    ofs << "  \"area_size\": " << area << ",\n";

    // 用户原始需求与坐标
    ofs << "  \"users\": [\n";
    UserNode *cur = head;
    while (cur) {
        ofs << "    {\"id\": " << cur->id << ", \"x\": " << cur->x << ", \"y\": " << cur->y << ", \"R\": " << cur->R << "}"
            << (cur->next ? ",\n" : "\n");
        cur = cur->next;
    }
    ofs << "  ],\n";

    // K-Means 方案
    ofs << "  \"kmeans\": {\n";
    ofs << "    \"drones\": [\n";
    for (int i = 0; i < 2; ++i) {
        ofs << "      {\"id\": " << i << ", \"x\": " << d_km[i].x << ", \"y\": " << d_km[i].y << ", \"z\": " << d_km[i].z
            << ", \"max_r\": " << d_km[i].max_r << ", \"total_power\": " << d_km[i].total_power
            << ", \"user_count\": " << d_km[i].user_count << "}" << (i == 0 ? ",\n" : "\n");
    }
    ofs << "    ],\n";
    ofs << "    \"max_horizontal_r\": " << m_km.max_horizontal_r << ",\n";
    ofs << "    \"max_3d_dist\": " << m_km.max_3d_dist << ",\n";
    ofs << "    \"min_user_p\": " << m_km.min_user_p << ",\n";
    ofs << "    \"max_user_p\": " << m_km.max_user_p << ",\n";
    ofs << "    \"total_power\": " << m_km.total_power << ",\n";
    ofs << "    \"assign\": [";
    for (int i = 0; i < N; ++i) ofs << assign_km[i] << (i < N - 1 ? ", " : "");
    ofs << "]\n  },\n";

    // 模拟退火优化方案
    ofs << "  \"sa\": {\n";
    ofs << "    \"drones\": [\n";
    for (int i = 0; i < 2; ++i) {
        ofs << "      {\"id\": " << i << ", \"x\": " << d_sa[i].x << ", \"y\": " << d_sa[i].y << ", \"z\": " << d_sa[i].z
            << ", \"max_r\": " << d_sa[i].max_r << ", \"total_power\": " << d_sa[i].total_power
            << ", \"user_count\": " << d_sa[i].user_count << "}" << (i == 0 ? ",\n" : "\n");
    }
    ofs << "    ],\n";
    ofs << "    \"max_horizontal_r\": " << m_sa.max_horizontal_r << ",\n";
    ofs << "    \"max_3d_dist\": " << m_sa.max_3d_dist << ",\n";
    ofs << "    \"min_user_p\": " << m_sa.min_user_p << ",\n";
    ofs << "    \"max_user_p\": " << m_sa.max_user_p << ",\n";
    ofs << "    \"total_power\": " << m_sa.total_power << ",\n";
    ofs << "    \"assign\": [";
    for (int i = 0; i < N; ++i) ofs << assign_sa[i] << (i < N - 1 ? ", " : "");
    ofs << "]\n  }\n";
    ofs << "}\n";
    ofs.close();
}

// ============================================================================
// 8. 主程序：支持参数传递、双算法执行与综合对比
// ============================================================================

int main(int argc, char* argv[]) {
    int N = 16;             // 地面用户数量 (N >= 8)
    double G = 20.0;        // 无人机固定飞行高度
    double area = 100.0;    // 区域坐标范围 [0, area]
    unsigned int seed = 42; // 随机数种子
    const char *json_path = "results.json";

    // 命令行参数解析：./uav_solver [seed] [N] [G] [area] [json_path]
    if (argc > 1) seed = atoi(argv[1]);
    if (argc > 2) N = atoi(argv[2]);
    if (argc > 3) G = atof(argv[3]);
    if (argc > 4) area = atof(argv[4]);
    if (argc > 5) json_path = argv[5];

    if (seed == 0) seed = (unsigned int)time(nullptr);
    srand(seed);

    cout << "=======================================================\n";
    cout << "     数据结构课程设计：无人机覆盖与功率优化系统\n";
    cout << "=======================================================\n";
    cout << "[配置参数] 用户规模 N = " << N << " (N>=8), 飞行高度 G = " << G << " m, 随机种子 = " << seed << "\n";

    // 步骤 1：链表动态创建与地面用户随机初始化 (对应要求 1)
    UserNode *user_list = nullptr;
    for (int i = 0; i < N; ++i) {
        double x = (rand() / (double)RAND_MAX) * area;
        double y = (rand() / (double)RAND_MAX) * area;
        double R = 1.0 + (rand() / (double)RAND_MAX) * 4.0; // 容量需求 1.0 ~ 5.0 bps/Hz
        append_user(user_list, i, x, y, R);
    }
    cout << ">> 步骤 1 完成：已通过动态链式存储结构创建 " << N << " 个地面用户节点。\n";

    // 步骤 2 & 3 & 4：K-Means 基线聚类划分与求解
    int assign_km[N];
    run_kmeans(user_list, N, assign_km);
    Drone drones_km[2];
    SystemMetrics m_km;
    evaluate_partition(user_list, N, assign_km, G, drones_km, m_km);
    print_scheme_results("方案 A：K-Means (K=2) 基线聚类划分", user_list, N, drones_km, m_km);

    // 创新算法：基于模拟退火 (SA) 的 Minimax 覆盖优化
    int assign_sa[N];
    run_simulated_annealing(user_list, N, assign_km, assign_sa);
    Drone drones_sa[2];
    SystemMetrics m_sa;
    evaluate_partition(user_list, N, assign_sa, G, drones_sa, m_sa);
    print_scheme_results("方案 B：模拟退火 (SA) Minimax 创新优化划分", user_list, N, drones_sa, m_sa);

    // 步骤 4：指标对比与分析
    cout << "\n=======================================================\n";
    cout << "                 双方案算法性能对比摘要\n";
    cout << "=======================================================\n";
    cout << "性能指标项              | K-Means 基线  | 模拟退火优化  | 改善幅度\n";
    cout << "------------------------+---------------+---------------+----------\n";
    cout << "瓶颈覆盖半径 max(r1,r2) | " << setw(11) << m_km.max_horizontal_r << " m | "
         << setw(11) << m_sa.max_horizontal_r << " m | "
         << showpos << setw(7) << ((m_sa.max_horizontal_r - m_km.max_horizontal_r) / m_km.max_horizontal_r) * 100.0 << " %\n" << noshowpos;
    cout << "系统最远距离 max(dn)    | " << setw(11) << m_km.max_3d_dist << " m | "
         << setw(11) << m_sa.max_3d_dist << " m | "
         << showpos << setw(7) << ((m_sa.max_3d_dist - m_km.max_3d_dist) / m_km.max_3d_dist) * 100.0 << " %\n" << noshowpos;
    cout << "系统最小单点功率 min(pn)| " << setw(11) << m_km.min_user_p << " W | "
         << setw(11) << m_sa.min_user_p << " W | "
         << showpos << setw(7) << ((m_sa.min_user_p - m_km.min_user_p) / m_km.min_user_p) * 100.0 << " %\n" << noshowpos;
    cout << "系统总发射功率 P_total  | " << setw(9) << m_km.total_power << " W | "
         << setw(9) << m_sa.total_power << " W | "
         << showpos << setw(7) << ((m_sa.total_power - m_km.total_power) / m_km.total_power) * 100.0 << " %\n" << noshowpos;
    cout << "=======================================================\n";

    // 导出 JSON 数据供 GUI 演示调用
    export_json(json_path, user_list, N, G, area, drones_km, m_km, assign_km, drones_sa, m_sa, assign_sa);
    cout << ">> 仿真结果已输出至 " << json_path << "，图形界面 (GUI) 可直接联动渲染。\n";

    // 释放链表动态内存
    free_list(user_list);
    return 0;
}
