#pragma once

#include <iostream>
#include "crutches.h"


class Task {
public:
    virtual void doit() = 0;

    virtual ~Task() = default;
};

template<typename T>
class ApplyGTask : public Task {
    T *field;
    int x;
public:
    ApplyGTask(int x, T &field) : field(&field), x(x) {};

    void doit() override;
};

template<typename T>
void ApplyGTask<T>::doit() {
    auto G = Pepega::g<typename T::v_type>();
    for (int y = 0; y < field->K; ++y) {
        if (field->field[x][y] == '#')
            continue;
        if (field->field[x + 1][y] != '#')
            field->velocity.add(x, y, 1, 0, G);
    }
}

template<typename T>
class ApplyPTask : public Task {
    T *f;
    int x;
public:
    ApplyPTask(int x, T &field) : f(&field), x(x) {};

    void doit() override;
};

template<typename T>
void ApplyPTask<T>::doit() {
    for (int y = 0; y < f->K; ++y) {
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
class RecalcPTask : public Task {
    T *f;
    int x;
public:
    RecalcPTask(int x, T &field) : f(&field), x(x) {};

    void doit() override;
};

template<typename T>
void RecalcPTask<T>::doit() {
    for (int y = 0; y < f->K; ++y) {
        if (f->field[x][y] == '#')
            continue;
        for (auto [dx, dy]: Pepega::deltas) {
            auto &old_v = f->velocity.get(x, y, dx, dy);
            const auto &new_v = f->velocity_flow.get(x, y, dx, dy);
            if (old_v > int64_t(0)) {
                assert(typename T::v_type(new_v) <= old_v);
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
class OutFieldTask : public Task {
    T *f;
public:
    explicit OutFieldTask(T &field) : f(&field) {};

    void doit() override;
};

template<typename T>
void OutFieldTask<T>::doit() {
    std::cout << "Tick " << f->last_active << ":\n";
    for (int j = 0; j < f->N; j++) {
        for (int k = 0; k < f->K; k++) {
            std::cout << f->field[j][k];
        }
        std::cout << "\n";
    }
    std::cout.flush();
}