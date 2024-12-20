#pragma once

#include <iostream>
#include "crutches.h"

// ThreadPool взял у https://github.com/AtomicBiscuit/SE2_CPP_HW3, т.к. написать свой не успел, и прикрутил его костыльно
// к коду дз2
class Mission {
public:
    virtual void do_this() = 0;

    virtual ~Mission() = default;
};

template<typename T>
class g_mission : public Mission {
    T *field;
    int x;
public:
    g_mission(int x, T &field) : field(&field), x(x) {};

    void do_this() override;
};

template<typename T>
void g_mission<T>::do_this() {
    auto G = Pepega::g<typename T::v_type>();
    for (int y = 0; y < field->M; ++y) {
        if (field->field[x][y] == '#')
            continue;
        if (field->field[x + 1][y] != '#')
            field->velocity.add(x, y, 1, 0, G);
    }
}

template<typename T>
class p_mission : public Mission {
    T *f;
    int x;
public:
    p_mission(int x, T &field) : f(&field), x(x) {};

    void do_this() override;
};

template<typename T>
void p_mission<T>::do_this() {
    for (int y = 0; y < f->M; ++y) {
        if (f->field[x][y] == '#')
            continue;
        for (auto [dx, dy]: Pepega::deltas) {
            int nx = x + dx, ny = y + dy;
            if (f->field[nx][ny] == '#' or f->old_p[nx][ny] >= f->old_p[x][y]) {
                continue;
            }
            auto force = f->old_p[x][y] - f->old_p[nx][ny];
            auto &contr = f->velocity.get(nx, ny, -dx, -dy);
            const auto &tmp = typename T::p_type(contr) * f->rho[(int) f->field[nx][ny]];
            if (tmp >= force) {
                contr -= typename T::v_type(force / f->rho[(int) f->field[nx][ny]]);
                continue;
            }
            force -= tmp;
            contr = int64_t(0);
            f->velocity.add(x, y, dx, dy, typename T::v_type(force / f->rho[(int) f->field[x][y]]));
            f->p[x][y] -= force / f->dirs[x][y];
        }
    }
}


template<typename T>
class p_recalculation : public Mission {
    T *f;
    int x;
public:
    p_recalculation(int x, T &field) : f(&field), x(x) {};

    void do_this() override;
};

template<typename T>
void p_recalculation<T>::do_this() {
    for (int y = 0; y < f->M; ++y) {
        if (f->field[x][y] == '#')
            continue;
        for (auto [dx, dy]: Pepega::deltas) {
            auto &old_v = f->velocity.get(x, y, dx, dy);
            const auto &new_v = f->velocity_flow.get(x, y, dx, dy);
            if (old_v > int64_t(0)) {
                //assert(typename T::v_type(new_v) <= old_v);
                auto force = typename T::p_type(old_v - typename T::v_type(new_v)) * f->rho[(int) f->field[x][y]];
                old_v = typename T::v_type(new_v);
                if (f->field[x][y] == '.')
                    force *= 0.8;
                if (f->field[x + dx][y + dy] == '#') {
                    f->update_p(x, y, force / f->dirs[x][y]);
                } else {
                    f->update_p(x + dx, y + dy, force / f->dirs[x + dx][y + dy]);
                }
            }
        }
    }
}

template<typename T>
class field_output : public Mission {
    T *f;
public:
    explicit field_output(T &field) : f(&field) {};

    void do_this() override;
};

template<typename T>
void field_output<T>::do_this() {
    for (int j = 0; j < f->N; j++) {
        for (int k = 0; k < f->M; k++) {
            std::cout << f->field[j][k];
        }
        std::cout << "\n";
    }
    std::cout.flush();
}