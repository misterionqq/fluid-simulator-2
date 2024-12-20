#pragma once

#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <random>
#include <tuple>
#include <algorithm>
#include <sstream>
#include <optional>
#include <fstream>

#include "vector-field.h"
#include "crutches.h"
#include "fixed.h"
#include "missions.h"
#include "buddies.h"

using namespace std;

namespace Pepega {

    //==================================//
    // Base class for fluid simulations //
    //==================================//

    class fluid_base {
    public:
        virtual void next(int) = 0;
        virtual void load(std::ifstream& file) = 0;
        virtual void save(std::ofstream& file) = 0;

        virtual void init_workers(int) = 0;
        virtual void kill_everyone() = 0;

        virtual ~fluid_base() = default;
    };

    //================================================//
    // Template class for a specific fluid simulation //
    //================================================//
    template <typename p_t, typename velocity_t, typename velocity_flow_t, int value_N, int value_M>
    class fluid : public fluid_base {
    private:
        int N = value_N;
        int M = value_M;

        using p_type = p_t;
        using v_type = velocity_t;
        using vf_type = velocity_flow_t;
        using full_type = fluid<p_t, velocity_t, velocity_flow_t, value_N, value_M>;

        Array<char, value_N, value_M> field{};
        Array<p_t, value_N, value_M> p{}, old_p{};
        Array<int64_t, value_M, value_M> dirs{};
        Array<int, value_M, value_M> last_use{};
        VectorField<velocity_t, value_N, value_M> velocity = {};
        VectorField<velocity_flow_t, value_N, value_M> velocity_flow = {};
        int UT = 0;
        int last_active = 0;
        p_t rho[256];

        Array<std::mutex, value_N, value_M> p_mutex{};

        std::vector<std::unique_ptr<Mission>> g_tasks;
        std::vector<std::unique_ptr<Mission>> p_tasks;
        std::vector<std::unique_ptr<Mission>> recalc_p_tasks;
        std::vector<std::unique_ptr<Mission>> output_field_task;

        BuddiesForeman main_handler{};
        BuddiesForeman output_handler{};

        void update_p(int x, int y, const p_t &val) {
            std::lock_guard lock(p_mutex[x][y]);
            p[x][y] += val;
        }

        void init() {
            velocity_flow.v.init(N, M);
            dirs.init(N, M);
            old_p.init(N, M);
            last_use.init(N, M);
            p.init(N, M);
            old_p.init(N, M);
            p_mutex.init(N, M);

            for (int i = 0; i < N; i++) {
                g_tasks.push_back(std::make_unique<g_mission<full_type>>(i, *this));
                p_tasks.push_back(std::make_unique<p_mission<full_type>>(i, *this));
                recalc_p_tasks.push_back(std::make_unique<p_recalculation<full_type>>(i, *this));
            }

            output_field_task.push_back(std::make_unique<field_output<full_type>>(*this));


            rho[' '] = 0.01;
            rho['.'] = 1000ll;
            for (int x = 0; x < N; ++x) {
                for (int y = 0; y < M; ++y) {
                    if (field[x][y] == '#')
                        continue;
                    for (auto [dx, dy]: deltas) {
                        dirs[x][y] += (field[x + dx][y + dy] != '#');
                    }
                }
            }
        }

        std::tuple<velocity_flow_t, bool, pair<int, int>> propagate_flow(int x, int y, velocity_flow_t lim) {
            last_use[x][y] = UT - 1;
            velocity_flow_t ret{};
            for (auto [dx, dy] : deltas) {
                int nx = x + dx, ny = y + dy;
                if (field[nx][ny] == '#' || last_use[nx][ny] >= UT) {
                    continue;
                }
                if (nx < 0 || nx >= N || ny < 0 || ny >= M) continue;

                velocity_t cap = velocity.get(x, y, dx, dy);
                velocity_flow_t flow = velocity_flow.get(x, y, dx, dy);
                if (fabs(flow - velocity_flow_t(cap)) <= 0.0001) {
                    continue;
                }
                velocity_flow_t vp = std::min(lim, velocity_flow_t(cap) - flow);
                if (last_use[nx][ny] == UT - 1) {
                    velocity_flow.add(x, y, dx, dy, vp);
                    last_use[x][y] = UT;
                    return {vp, true, {nx, ny}};
                }
                    //auto [t, prop, end] = propagate_flow(nx, ny, vp);
                velocity_flow_t t;
                bool prop;
                std::pair<int, int> end;
                do {
                    std::tie(t, prop, end) = propagate_flow(nx, ny, vp);
                } while (end == std::pair(nx, ny));

                ret += t;
                if (prop) {
                    velocity_flow.add(x, y, dx, dy, t);
                    last_use[x][y] = UT;
                    return {t, prop && end != pair(x, y), end};
                }

            }
            last_use[x][y] = UT;
            return {ret, false, {0, 0}};
        }

        inline bool is_stoppable(int x, int y) {
            for (auto [dx, dy]: deltas) {
                int nx = x + dx, ny = y + dy;
                if (field[nx][ny] != '#' && last_use[nx][ny] < UT - 1 && velocity.get(x, y, dx, dy) > int64_t(0)) {
                    return false;
                }
            }
            return true;
        }

        void propagate_stop(int x_, int y_) {
            std::stack<std::pair<int, int>> nxt;
            nxt.emplace(x_, y_);
            last_use[x_][y_] = UT;
            while (not nxt.empty()) {
                auto [x, y] = nxt.top();
                nxt.pop();
                for (auto [dx, dy]: deltas) {
                    int nx = x + dx, ny = y + dy;
                    if (field[nx][ny] == '#' || last_use[nx][ny] == UT || velocity.get(x, y, dx, dy) > int64_t(0) ||
                        not is_stoppable(nx, ny)) {
                        continue;
                    }
                    last_use[nx][ny] = UT;
                    nxt.emplace(nx, ny);
                }
            }
        }

        velocity_t move_prob(int x, int y) {
            velocity_t sum{};
            for (auto [dx, dy] : deltas) {
                int nx = x + dx, ny = y + dy;
                if (nx < 0 || nx >= N || ny < 0 || ny >= M) continue;
                if (field[nx][ny] == '#' || last_use[nx][ny] == UT) {
                    continue;
                }
                velocity_t v = velocity.get(x, y, dx, dy);
                if (v < 0ll) {
                    continue;
                }
                sum += v;
            }
            return sum;
        }

        void swap(int x1, int y1, int x2, int y2) {
            std::swap(field[x1][y1], field[x2][y2]);
            std::swap(p[x1][y1], p[x2][y2]);
            std::swap(velocity.v[x1][y1], velocity.v[x2][y2]);
        }

        bool propagate_move(int x, int y, bool is_first) {
            last_use[x][y] = UT - is_first;
            bool ret = false;
            int nx = -1, ny = -1;
            do {
                std::array<velocity_t, deltas.size()> tres;
                velocity_t sum{};
                for (size_t i = 0; i < deltas.size(); ++i) {
                    auto [dx, dy] = deltas[i];
                    int fx = x + dx, fy = y + dy;
                    if (fx < 0 || fx >= N || fy < 0 || fy >= M) continue;
                    if (field[fx][fy] == '#' || last_use[fx][fy] == UT) {
                        tres[i] = sum;
                        continue;
                    }
                    velocity_t v = velocity.get(x, y, dx, dy);
                    if (v < 0ll) {
                        tres[i] = sum;
                        continue;
                    }
                    sum += v;
                    tres[i] = sum;
                }

                if (sum == 0ll) {
                    break;
                }

                velocity_t randNum = random01<velocity_t>() * sum;
                size_t d = std::ranges::upper_bound(tres, randNum) - tres.begin();

                auto [dx, dy] = deltas[d];
                nx = x + dx;
                ny = y + dy;
                assert(velocity.get(x, y, dx, dy) > 0ll && field[nx][ny] != '#' && last_use[nx][ny] < UT);

                ret = (last_use[nx][ny] == UT - 1 || propagate_move(nx, ny, false));
            } while (!ret);
            last_use[x][y] = UT;
            for (auto [dx, dy] : deltas) {
                int fx = x + dx, fy = y + dy;
                if (fx < 0 || fx >= N || fy < 0 || fy >= M) continue;
                if (field[fx][fy] != '#' && last_use[fx][fy] < UT - 1 && velocity.get(x, y, dx, dy) < 0ll) {
                    propagate_stop(nx, ny);
                }
            }
            if (ret && !is_first) {
                swap(x, y, nx, ny);
            }
            return ret;
        }

        void g_tasks_mission() {
            main_handler.set(&g_tasks);
            main_handler.wait();
        }

        void p_tasks_mission() {
            old_p = p;
            main_handler.set(&p_tasks);
            main_handler.wait();
        }

        void flow_mission() {
            velocity_flow.v.clear();
            int cnt = 0;
            bool prop;
            do {
                cnt++;
                UT += 2;
                prop = false;
                for (int x = 0; x < N; x++) {
                    for (int y = 0; y < M; y++) {
                        if (field[x][y] == '#' or last_use[x][y] == UT) {
                            continue;
                        }
                        auto [t, _unused1, _unused2] = propagate_flow(x, y, int64_t(1));
                        if (t > int64_t(0)) {
                            prop = true;
                            --y;
                        }
                    }
                }
            } while (prop);
        }

        void recalculate_p() {
            main_handler.set(&recalc_p_tasks);
            main_handler.wait();
        }

        bool apply_move_on_flow() {
            UT += 2;
            bool prop = false;
            for (int x = 0; x < N; ++x) {
                for (int y = 0; y < M; ++y) {
                    if (field[x][y] != '#' && last_use[x][y] != UT) {
                        if (random01<velocity_t>() < move_prob(x, y)) {
                            prop = true;
                            propagate_move(x, y, true);
                        } else {
                            propagate_stop(x, y);
                        }
                    }
                }
            }
            return prop;
        }

    public:  //----------------------------------------------

        fluid() = default;

        void next(int out) override {
            /*
            p_t total_delta_p = 0ll;

            // Apply gravity to downward velocities
            for (size_t x = 0; x < N; ++x) {
                for (size_t y = 0; y < M; ++y) {
                    if (field[x][y] == '#')
                        continue;
                    if (field[x + 1][y] != '#')
                        velocity.add(x, y, 1, 0, g<velocity_t>());
                }
            }

            old_p = p;
            // Calculate pressure changes based on pressure differences
            for (size_t x = 0; x < N; ++x) {
                for (size_t y = 0; y < M; ++y) {
                    if (field[x][y] == '#')
                        continue;
                    for (auto [dx, dy] : deltas) {
                        int nx = x + dx, ny = y + dy;
                        if (field[nx][ny] != '#' && old_p[nx][ny] < old_p[x][y]) {
                            p_t delta_p = old_p[x][y] - old_p[nx][ny];
                            p_t force = delta_p;
                            velocity_t &contr = velocity.get(nx, ny, -dx, -dy);
                            if (p_t(contr) * rho[(int) field[nx][ny]] >= force) {
                                contr -= velocity_t(force / rho[(int) field[nx][ny]]);
                                continue;
                            }
                            force -= p_t(contr) * rho[(int) field[nx][ny]];
                            contr = 0ll;
                            velocity.add(x, y, dx, dy, velocity_t(force / rho[(int) field[x][y]]));
                            p[x][y] -= force / dirs[x][y];
                            total_delta_p -= force / dirs[x][y];
                        }
                    }
                }
                */
            g_tasks_mission();
            p_tasks_mission();
            flow_mission();
            recalculate_p();
            output_handler.wait();

            bool prop = apply_move_on_flow();

            if (prop) {
                last_active = out;
                output_handler.set(&output_field_task);
            }
        }

        void load(std::ifstream& file) override {
            // Helper lambda to load a 2D array from a file
            auto array_load = [&]<typename T, int N, int M>(Array<T, N, M>& arr, int n, int m) {
                arr.init(n, m);
                for (int i = 0; i < arr.N; ++i) {
                    for (int j = 0; j < arr.M; ++j) {
                        if constexpr (std::is_same_v<T, char>) {
                            char tmp;
                            do {
                                file.get(tmp);
                            } while (tmp == '\n');
                            arr[i][j] = static_cast<char>(tmp);
                        } else {
                            double tmp = 0;
                            file >> tmp;
                            arr[i][j] = tmp;
                        }
                    }
                }
            };

            // Helper lambda to load a 2D array of arrays from a file
            auto load_field = [&]<typename T, int N, int M> (Array<std::array<T, 4>, N, M>& arr, int n, int m) {
                arr.init(n, m);
                for (int i = 0; i < arr.N; ++i) {
                    for (int j = 0; j < arr.M; ++j) {
                        double tmp[4] = {0};
                        file >> tmp[0] >> tmp[1] >> tmp[2] >> tmp[3];
                        arr[i][j] = {T(tmp[0]), T(tmp[1]), T(tmp[2]), T(tmp[3])};
                    }
                }
            };


            if (!file.is_open()) {
                throw std::invalid_argument("Something went wrong with file opening\n");
            }
            file >> N >> M >> UT;
            array_load(field, N, M); // Load field data
            array_load(last_use, N, M); // Load last use data
            array_load(p, N, M); // Load pressure data
            load_field(velocity.v, N, M); // Load velocity data

            init();
        }

        friend class g_mission<full_type>;
        friend class p_mission<full_type>;
        friend class p_recalculation<full_type>;
        friend class field_output<full_type>;

        void init_workers(int n) override {
            if (n < 1) {
                throw std::runtime_error("Must be at least 1 thread");
            }
            main_handler.init(n);
            output_handler.init(1);
        }

        void kill_everyone() {
            main_handler.stop_all();
            output_handler.stop_all();
        }

        void save(std::ofstream &file) override {
            // Helper lambda to save a 2D array to a file
            auto array_save = [&]<typename T, int N, int M>(Array<T, N, M>& arr) {
                for (int i = 0; i < arr.N; ++i) {
                    for (int j = 0; j < arr.M; ++j) {
                        if constexpr (std::is_same_v<T, char>) {
                            file << arr[i][j];
                        } else {
                            file << arr[i][j] << " ";
                        }
                    }
                    file << std::endl;
                }
            };

            if (!file.is_open()) {
                throw std::invalid_argument("file is not open");
            }
            file << N << " " << M << " " << UT << std::endl;
            array_save(field); // Save field data
            array_save(last_use); // Save last use data
            array_save(p); // Save pressure data

            // Save velocity data
            for (int i = 0; i < velocity.v.N; ++i) {
                for (int j = 0; j < velocity.v.M; ++j) {
                    file << velocity.v[i][j][0] << " "
                         << velocity.v[i][j][1] << " "
                         << velocity.v[i][j][2] << " "
                         << velocity.v[i][j][3];
                }
                file << std::endl;
            }
        }
    };
}
