#include "uav_model.h"
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <algorithm>

UAVSystemModel::UAVSystemModel()
    : m_head(nullptr), m_userCount(0), m_G(20.0), m_area(100.0), m_seed(42), m_hasSolved(false) {
}

UAVSystemModel::~UAVSystemModel() {
    clearUsers();
}

void UAVSystemModel::clearUsers() {
    UserNode *cur = m_head;
    while (cur) {
        UserNode *tmp = cur;
        cur = cur->next;
        delete tmp;
    }
    m_head = nullptr;
    m_userCount = 0;
    m_hasSolved = false;
}

void UAVSystemModel::appendUserNode(int id, double x, double y, double R) {
    UserNode *node = new UserNode;
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

    if (!m_head) {
        m_head = node;
    } else {
        UserNode *cur = m_head;
        while (cur->next) cur = cur->next;
        cur->next = node;
    }
    m_userCount++;
}

void UAVSystemModel::generateRandomUsers(int n, double area, double G, double r_min, double r_max, unsigned int seed) {
    clearUsers();
    m_area = area;
    m_G = G;
    m_seed = seed;

    srand(seed);
    for (int i = 0; i < n; ++i) {
        double x = (rand() / (double)RAND_MAX) * area;
        double y = (rand() / (double)RAND_MAX) * area;
        double R = r_min + (rand() / (double)RAND_MAX) * (r_max - r_min);
        appendUserNode(i, x, y, R);
    }
}

// 几何核心：最小外接圆定点求解器 (解决题目第 3 问 Minimax 要求)
Circle UAVSystemModel::solveMEC(const std::vector<Point2D> &pts) {
    int n = static_cast<int>(pts.size());
    if (n == 0) return {{0, 0}, 0};
    if (n == 1) return {pts[0], 0};
    if (n == 2) {
        Point2D c = {(pts[0].x + pts[1].x) / 2.0, (pts[0].y + pts[1].y) / 2.0};
        return {c, std::hypot(pts[0].x - pts[1].x, pts[0].y - pts[1].y) / 2.0};
    }

    Circle best = {{0, 0}, 1e18};

    // 1) 两点定圆判定（以两点连线为直径）
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            Point2D c = {(pts[i].x + pts[j].x) / 2.0, (pts[i].y + pts[j].y) / 2.0};
            double r = std::hypot(pts[i].x - pts[j].x, pts[i].y - pts[j].y) / 2.0;
            if (r >= best.radius) continue;

            bool ok = true;
            for (int k = 0; k < n; ++k) {
                if (std::hypot(pts[k].x - c.x, pts[k].y - c.y) > r + 1e-6) {
                    ok = false;
                    break;
                }
            }
            if (ok) best = {c, r};
        }
    }

    // 2) 三点定圆判定（外接圆判定）
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            for (int k = j + 1; k < n; ++k) {
                double x1 = pts[i].x, y1 = pts[i].y;
                double x2 = pts[j].x, y2 = pts[j].y;
                double x3 = pts[k].x, y3 = pts[k].y;
                double D = 2.0 * (x1 * (y2 - y3) + x2 * (y3 - y1) + x3 * (y1 - y2));
                if (std::fabs(D) < 1e-7) continue;

                double ux = ((x1*x1 + y1*y1)*(y2 - y3) + (x2*x2 + y2*y2)*(y3 - y1) + (x3*x3 + y3*y3)*(y1 - y2)) / D;
                double uy = ((x1*x1 + y1*y1)*(x3 - x2) + (x2*x2 + y2*y2)*(x1 - x3) + (x3*x3 + y3*y3)*(x2 - x1)) / D;
                double r = std::hypot(x1 - ux, y1 - uy);

                if (r >= best.radius) continue;

                bool ok = true;
                for (int m = 0; m < n; ++m) {
                    if (std::hypot(pts[m].x - ux, pts[m].y - uy) > r + 1e-6) {
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

void UAVSystemModel::evaluateScheme(const std::vector<int> &assign, SchemeResult &result) {
    result.assign = assign;
    result.dist_3d.assign(m_userCount, 0.0);
    result.channel.assign(m_userCount, 0.0);
    result.power.assign(m_userCount, 0.0);

    std::vector<Point2D> pts0, pts1;
    UserNode *cur = m_head;
    int idx = 0;
    while (cur) {
        if (assign[idx] == 0) pts0.push_back({cur->x, cur->y});
        else pts1.push_back({cur->x, cur->y});
        cur = cur->next;
        idx++;
    }

    Circle c0 = solveMEC(pts0);
    Circle c1 = solveMEC(pts1);

    result.drones[0] = {0, c0.center.x, c0.center.y, m_G, c0.radius, 0.0, static_cast<int>(pts0.size())};
    result.drones[1] = {1, c1.center.x, c1.center.y, m_G, c1.radius, 0.0, static_cast<int>(pts1.size())};

    result.metrics.max_3d_dist = 0.0;
    result.metrics.min_user_p = 1e18;
    result.metrics.max_user_p = 0.0;

    cur = m_head;
    idx = 0;
    while (cur) {
        int did = assign[idx];
        const Drone &dr = result.drones[did];
        double dx = cur->x - dr.x;
        double dy = cur->y - dr.y;
        double d = std::sqrt(dx * dx + dy * dy + m_G * m_G);
        double h = 1.0 / (d * d);
        double p = (std::pow(2.0, cur->R) - 1.0) * (d * d);

        result.dist_3d[idx] = d;
        result.channel[idx] = h;
        result.power[idx] = p;

        result.drones[did].total_power += p;
        if (d > result.metrics.max_3d_dist) result.metrics.max_3d_dist = d;
        if (p < result.metrics.min_user_p) result.metrics.min_user_p = p;
        if (p > result.metrics.max_user_p) result.metrics.max_user_p = p;

        cur = cur->next;
        idx++;
    }

    result.metrics.max_horizontal_r = std::max(result.drones[0].max_r, result.drones[1].max_r);
    result.metrics.total_power = result.drones[0].total_power + result.drones[1].total_power;
}

// 方案 A：K-Means (K=2) 基准聚类划分
void UAVSystemModel::solveKMeans() {
    if (m_userCount < 2) return;

    std::vector<Point2D> pts(m_userCount);
    UserNode *cur = m_head;
    for (int i = 0; i < m_userCount && cur; ++i, cur = cur->next) {
        pts[i] = {cur->x, cur->y};
    }

    // 初始质心选择：第 0 个点和距离其最远的点
    Point2D mu[2] = {pts[0], pts[0]};
    double max_d = -1;
    for (int i = 1; i < m_userCount; ++i) {
        double d = std::hypot(pts[i].x - pts[0].x, pts[i].y - pts[0].y);
        if (d > max_d) { max_d = d; mu[1] = pts[i]; }
    }

    std::vector<int> assign(m_userCount, 0);
    for (int iter = 0; iter < 100; ++iter) {
        int count[2] = {0, 0};
        Point2D sum[2] = {{0, 0}, {0, 0}};

        for (int i = 0; i < m_userCount; ++i) {
            double d0 = std::hypot(pts[i].x - mu[0].x, pts[i].y - mu[0].y);
            double d1 = std::hypot(pts[i].x - mu[1].x, pts[i].y - mu[1].y);
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

        if (std::hypot(new_mu[0].x - mu[0].x, new_mu[0].y - mu[0].y) < 1e-4 &&
            std::hypot(new_mu[1].x - mu[1].x, new_mu[1].y - mu[1].y) < 1e-4) break;

        mu[0] = new_mu[0];
        mu[1] = new_mu[1];
    }

    m_kmResult.name = "K-Means 基线方案";
    evaluateScheme(assign, m_kmResult);
}

// 方案 B：模拟退火 (SA) Minimax 创新优化划分
void UAVSystemModel::solveSimulatedAnnealing(double t_start, double t_min, double alpha) {
    if (m_userCount < 2) return;
    if (m_kmResult.assign.empty()) solveKMeans();

    std::vector<Point2D> pts(m_userCount);
    UserNode *cur = m_head;
    for (int i = 0; i < m_userCount && cur; ++i, cur = cur->next) {
        pts[i] = {cur->x, cur->y};
    }

    std::vector<int> cur_assign = m_kmResult.assign;
    std::vector<int> best_assign = cur_assign;

    auto evalCost = [&](const std::vector<int> &a) -> double {
        std::vector<Point2D> p0, p1;
        for (int i = 0; i < m_userCount; ++i) {
            if (a[i] == 0) p0.push_back(pts[i]);
            else p1.push_back(pts[i]);
        }
        if (p0.empty() || p1.empty()) return 1e12; // 空簇强惩罚
        Circle c0 = solveMEC(p0);
        Circle c1 = solveMEC(p1);
        return std::max(c0.radius, c1.radius) + 0.05 * std::min(c0.radius, c1.radius);
    };

    double cur_cost = evalCost(cur_assign);
    double best_cost = cur_cost;

    double T = t_start;
    while (T > t_min) {
        for (int step = 0; step < m_userCount * 2; ++step) {
            int k = rand() % m_userCount;
            cur_assign[k] = 1 - cur_assign[k];

            double new_cost = evalCost(cur_assign);
            double delta = new_cost - cur_cost;

            if (delta < 0 || (rand() / (double)RAND_MAX) < std::exp(-delta / T)) {
                cur_cost = new_cost;
                if (cur_cost < best_cost) {
                    best_cost = cur_cost;
                    best_assign = cur_assign;
                }
            } else {
                cur_assign[k] = 1 - cur_assign[k];
            }
        }
        T *= alpha;
    }

    m_saResult.name = "模拟退火 (SA) Minimax 优化方案";
    evaluateScheme(best_assign, m_saResult);

    // 将最优结果更新同步回链表节点
    cur = m_head;
    for (int i = 0; i < m_userCount && cur; ++i, cur = cur->next) {
        cur->drone_id = best_assign[i];
        cur->d = m_saResult.dist_3d[i];
        cur->h = m_saResult.channel[i];
        cur->p = m_saResult.power[i];
    }

    m_hasSolved = true;
}

void UAVSystemModel::solveAll() {
    solveKMeans();
    solveSimulatedAnnealing();
}

bool UAVSystemModel::exportJSON(const std::string &filepath) const {
    if (!m_hasSolved) return false;
    std::ofstream ofs(filepath);
    if (!ofs.is_open()) return false;

    ofs << std::fixed << std::setprecision(4);
    ofs << "{\n";
    ofs << "  \"N\": " << m_userCount << ",\n";
    ofs << "  \"G\": " << m_G << ",\n";
    ofs << "  \"area_size\": " << m_area << ",\n";
    ofs << "  \"seed\": " << m_seed << ",\n";

    // 原始用户节点
    ofs << "  \"users\": [\n";
    UserNode *cur = m_head;
    while (cur) {
        ofs << "    {\"id\": " << cur->id << ", \"x\": " << cur->x << ", \"y\": " << cur->y
            << ", \"R\": " << cur->R << "}" << (cur->next ? ",\n" : "\n");
        cur = cur->next;
    }
    ofs << "  ],\n";

    auto writeSchemeJSON = [&](const SchemeResult &res, const std::string &key, bool hasComma) {
        ofs << "  \"" << key << "\": {\n";
        ofs << "    \"drones\": [\n";
        for (int i = 0; i < 2; ++i) {
            const Drone &d = res.drones[i];
            ofs << "      {\"id\": " << d.id << ", \"x\": " << d.x << ", \"y\": " << d.y
                << ", \"z\": " << d.z << ", \"max_r\": " << d.max_r << ", \"total_power\": " << d.total_power
                << ", \"user_count\": " << d.user_count << "}" << (i == 0 ? ",\n" : "\n");
        }
        ofs << "    ],\n";
        ofs << "    \"max_horizontal_r\": " << res.metrics.max_horizontal_r << ",\n";
        ofs << "    \"max_3d_dist\": " << res.metrics.max_3d_dist << ",\n";
        ofs << "    \"min_user_p\": " << res.metrics.min_user_p << ",\n";
        ofs << "    \"max_user_p\": " << res.metrics.max_user_p << ",\n";
        ofs << "    \"total_power\": " << res.metrics.total_power << ",\n";
        ofs << "    \"assign\": [";
        for (size_t i = 0; i < res.assign.size(); ++i) {
            ofs << res.assign[i] << (i + 1 < res.assign.size() ? ", " : "");
        }
        ofs << "]\n  }" << (hasComma ? ",\n" : "\n");
    };

    writeSchemeJSON(m_kmResult, "kmeans", true);
    writeSchemeJSON(m_saResult, "sa", false);
    ofs << "}\n";
    ofs.close();
    return true;
}

bool UAVSystemModel::exportCSV(const std::string &filepath) const {
    if (!m_hasSolved) return false;
    std::ofstream ofs(filepath);
    if (!ofs.is_open()) return false;

    ofs << "用户ID,X坐标(m),Y坐标(m),容量需求Rn(bps/Hz),KMeans服务基站,KMeans空间距离(m),KMeans信道系数,KMeans功率(W),SA服务基站,SA空间距离(m),SA信道系数,SA功率(W)\n";
    UserNode *cur = m_head;
    int idx = 0;
    while (cur) {
        ofs << cur->id << ","
            << std::fixed << std::setprecision(2) << cur->x << ","
            << cur->y << ","
            << cur->R << ","
            << "UAV" << (m_kmResult.assign[idx] + 1) << ","
            << m_kmResult.dist_3d[idx] << ","
            << std::setprecision(6) << m_kmResult.channel[idx] << ","
            << std::setprecision(2) << m_kmResult.power[idx] << ","
            << "UAV" << (m_saResult.assign[idx] + 1) << ","
            << m_saResult.dist_3d[idx] << ","
            << std::setprecision(6) << m_saResult.channel[idx] << ","
            << std::setprecision(2) << m_saResult.power[idx] << "\n";
        cur = cur->next;
        idx++;
    }
    ofs.close();
    return true;
}
