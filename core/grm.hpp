// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// Global Relational Model engine - C++ port of grm.py (header only, no dependency).
//
// Nodes
//   positive quantities (default) are handled as x = log10(value);
//   signed quantities (Node::lin = true) are handled as x = value.
// Relations (sigma in the relation's own residual unit; dex for log-space relations)
//   log-linear   sum(coeff * log10(node)) = rhs           products, ratios, powers
//   sum          sum(coeff * value^p) = K                  linear combinations of values
//   fn           f(values) = 0                             any equation (optional analytic gradient)
// Solve: weighted least squares, QR factorisation (no normal equations); purely log-linear networks in a
// few exact steps, others by Gauss-Newton with backtracking then Levenberg-Marquardt damping, with a warm
// start on the log-linear part and continuation for very tight nonlinear relations - the same algorithm,
// step by step, as grm.py, so that both give the same numbers.
#pragma once

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace grm {

constexpr double LN10 = 2.302585092994045684;

// The values of all nodes (real values, not log), by index; a network gives the index of a name.
struct Values {
    std::vector<double> v;
    double operator[](int i) const { return v[(size_t) i]; }
};

using Fn = std::function<double(const Values&)>;
using Grad = std::function<std::vector<std::pair<int, double>>(const Values&)>;   // d f / d value

struct Node {
    std::string name;
    double guess = 1.0;
    bool lin = false;
    bool fixed = false;
};

struct Relation {
    enum Kind { Log, Sum, Function } kind = Log;
    std::string name;
    double sigma = 1e-3;
    // log-linear
    std::vector<std::pair<int, double>> terms;          // (node, coefficient)
    double rhs = 0.0;
    Fn rhs_fn;                                          // optional rhs depending on values
    std::vector<int> depends;                           // nodes rhs_fn reads (empty: all)
    // sum
    std::vector<double> powers;                         // one per term (default 1)
    double K = 0.0;
    // fn
    std::vector<int> nodes;
    Fn f;
    Grad grad;
};

struct Measurement {
    double value;
    double sigma_dex;
};

struct Result {
    std::vector<double> values;          // by node index
    std::vector<double> sigma_dex;       // 1-sigma in log10 units (value units for signed nodes)
    std::vector<double> uncertainty;     // relative for positive nodes, absolute for signed ones
    double cost = 0.0;
    int iterations = 0;
    bool converged = true;
    std::map<std::string, double> by_name;
    double operator[](const std::string& name) const {
        auto it = by_name.find(name);
        if (it == by_name.end()) throw std::out_of_range("unknown node " + name);
        return it->second;
    }
};

// ---------------------------------------------------------------------------------------------------- linear algebra
namespace detail {

// Least squares min |A d - b| for a full-column-rank A (m x n, row-major). Householder QR.
// Returns d, and R (n x n upper triangular, row-major). Throws std::domain_error when rank-deficient.
inline std::vector<double> qr_solve(std::vector<double> A, std::vector<double> b, size_t m, size_t n,
                                    std::vector<double>* R_out) {
    std::vector<double> v(m);
    for (size_t k = 0; k < n; ++k) {
        double norm = 0.0;
        for (size_t i = k; i < m; ++i) norm += A[i * n + k] * A[i * n + k];
        norm = std::sqrt(norm);
        if (!(norm > 0.0)) throw std::domain_error("rank-deficient system");
        const double alpha = A[k * n + k] >= 0 ? -norm : norm;
        std::fill(v.begin(), v.end(), 0.0);
        v[k] = A[k * n + k] - alpha;
        for (size_t i = k + 1; i < m; ++i) v[i] = A[i * n + k];
        double vv = 0.0;
        for (size_t i = k; i < m; ++i) vv += v[i] * v[i];
        if (!(vv > 0.0)) continue;
        for (size_t j = k; j < n; ++j) {
            double s = 0.0;
            for (size_t i = k; i < m; ++i) s += v[i] * A[i * n + j];
            s *= 2.0 / vv;
            if (std::abs(s) > 0.0)
                for (size_t i = k; i < m; ++i) A[i * n + j] -= s * v[i];
        }
        double s = 0.0;
        for (size_t i = k; i < m; ++i) s += v[i] * b[i];
        s *= 2.0 / vv;
        for (size_t i = k; i < m; ++i) b[i] -= s * v[i];
    }
    std::vector<double> x(n);
    for (size_t ii = n; ii-- > 0;) {
        double s = b[ii];
        for (size_t j = ii + 1; j < n; ++j) s -= A[ii * n + j] * x[j];
        if (!(std::abs(A[ii * n + ii]) > 0.0)) throw std::domain_error("rank-deficient system");
        x[ii] = s / A[ii * n + ii];
    }
    if (R_out) {
        R_out->assign(n * n, 0.0);
        for (size_t i = 0; i < n; ++i)
            for (size_t j = i; j < n; ++j) (*R_out)[i * n + j] = A[i * n + j];
    }
    return x;
}

// diagonal of (R^T R)^-1 = R^-1 R^-T
inline std::vector<double> covariance_diagonal(const std::vector<double>& R, size_t n) {
    std::vector<double> Rinv(n * n, 0.0);
    for (size_t j = 0; j < n; ++j) {          // solve R x = e_j
        for (size_t ii = n; ii-- > 0;) {
            double s = (ii == j) ? 1.0 : 0.0;
            for (size_t k = ii + 1; k < n; ++k) s -= R[ii * n + k] * Rinv[k * n + j];
            Rinv[ii * n + j] = s / R[ii * n + ii];
        }
    }
    std::vector<double> d(n, 0.0);
    for (size_t i = 0; i < n; ++i)
        for (size_t k = 0; k < n; ++k) d[i] += Rinv[i * n + k] * Rinv[i * n + k];
    return d;
}

}  // namespace detail

// ---------------------------------------------------------------------------------------------------- network
class Network {
public:
    double prior_sigma = 1e3;
    double continuation_start = 1e-2;
    double continuation_factor = 100.0;

    int add_node(const std::string& name, double guess, bool lin = false, bool fixed = false) {
        if (index_.count(name)) throw std::invalid_argument("duplicate node " + name);
        if (!lin && !(guess > 0 && std::isfinite(guess)))
            throw std::invalid_argument("guess of node '" + name + "' must be a finite positive number");
        nodes_.push_back({name, guess, lin, fixed});
        index_[name] = (int) nodes_.size() - 1;
        return (int) nodes_.size() - 1;
    }
    int index(const std::string& name) const {
        auto it = index_.find(name);
        if (it == index_.end()) throw std::out_of_range("unknown node " + name);
        return it->second;
    }
    size_t size() const { return nodes_.size(); }

    // log-linear: sum c log10(node) = rhs
    void add_log(const std::string& name, const std::vector<std::pair<std::string, double>>& terms, double rhs,
                 double sigma = 1e-3) {
        Relation r;
        r.kind = Relation::Log;
        r.name = name;
        r.sigma = sigma;
        r.rhs = rhs;
        for (auto& [n, c] : terms) {
            const int i = index(n);
            if (nodes_[(size_t) i].lin) throw std::invalid_argument("relation '" + name + "' is log-linear but uses signed node " + n);
            r.terms.push_back({i, c});
        }
        rels_.push_back(std::move(r));
    }
    // sum: sum c value^p = K
    void add_sum(const std::string& name, const std::vector<std::pair<std::string, double>>& terms, double K,
                 double sigma = 1e-3, const std::map<std::string, double>& powers = {}) {
        Relation r;
        r.kind = Relation::Sum;
        r.name = name;
        r.sigma = sigma;
        r.K = K;
        for (auto& [n, c] : terms) {
            r.terms.push_back({index(n), c});
            auto it = powers.find(n);
            r.powers.push_back(it == powers.end() ? 1.0 : it->second);
        }
        rels_.push_back(std::move(r));
    }
    // fn: f(values) = 0
    void add_fn(const std::string& name, const std::vector<std::string>& nodes, Fn f, double sigma = 1e-3,
                Grad grad = nullptr) {
        Relation r;
        r.kind = Relation::Function;
        r.name = name;
        r.sigma = sigma;
        for (auto& n : nodes) r.nodes.push_back(index(n));
        r.f = std::move(f);
        r.grad = std::move(grad);
        rels_.push_back(std::move(r));
    }

    Result solve(const std::map<std::string, Measurement>& measured = {}, int max_iterations = 50,
                 double damping_lambda = 1e-3, double tolerance = 1e-12, bool compute_uncertainty = true,
                 bool continuation = true) {
        nonlinear_ = false;
        for (auto& r : rels_)
            if (r.kind != Relation::Log || r.rhs_fn) nonlinear_ = true;
        meas_.clear();
        for (auto& [name, m] : measured) {
            const int i = index(name);
            if (!(m.sigma_dex > 0)) throw std::invalid_argument("measurement of '" + name + "' needs a positive sigma");
            const double target = nodes_[(size_t) i].lin ? m.value : log10_checked(m.value, "measurement of " + name);
            meas_.push_back({i, target, m.sigma_dex});
        }
        std::vector<double> x(nodes_.size());
        for (size_t i = 0; i < nodes_.size(); ++i) x[i] = nodes_[i].lin ? nodes_[i].guess : std::log10(nodes_[i].guess);
        for (auto& m : meas_)
            if (nodes_[(size_t) m.i].fixed) x[(size_t) m.i] = m.target;
        free_.clear();
        col_.assign(nodes_.size(), -1);
        for (size_t i = 0; i < nodes_.size(); ++i)
            if (!nodes_[i].fixed) {
                col_[i] = (int) free_.size();
                free_.push_back((int) i);
            }
        const size_t n = free_.size();
        int iterations = 0;
        bool converged = true;
        std::vector<double> R;
        bool have_R = false;
        if (n && !nonlinear_) {
            for (iterations = 1; iterations < 5; ++iterations) {
                auto rows = build_rows(x, true);
                auto delta = step(rows, n, 0.0, &R);
                have_R = true;
                double big = 0.0;
                for (size_t a = 0; a < n; ++a) {
                    x[(size_t) free_[a]] += delta[a];
                    big = std::max(big, std::abs(delta[a]));
                }
                if (big < 1e-13) break;
            }
        } else if (n) {
            x = warm_start(x);
            const double c0 = cost(x);
            if (!std::isfinite(c0))
                throw std::domain_error("the starting point is outside the domain of a relation: change the guesses");
            const auto x_start = x;
            descend(x, max_iterations, damping_lambda, tolerance, iterations, converged);
            if (!converged && continuation) continuation_solve(x_start, x, max_iterations, damping_lambda, tolerance, iterations, converged);
        }
        x_ = x;
        Result res;
        res.iterations = iterations;
        res.converged = converged;
        res.cost = cost(x);
        res.values.resize(nodes_.size());
        for (size_t i = 0; i < nodes_.size(); ++i) {
            res.values[i] = to_value(i, x[i]);
            res.by_name[nodes_[i].name] = res.values[i];
        }
        res.sigma_dex.assign(nodes_.size(), 0.0);
        res.uncertainty.assign(nodes_.size(), 0.0);
        if (compute_uncertainty && n) {
            if (!have_R || nonlinear_) {
                auto rows = build_rows(x, true);
                step(rows, n, 0.0, &R);
            }
            const auto d = detail::covariance_diagonal(R, n);
            for (size_t a = 0; a < n; ++a) {
                const size_t i = (size_t) free_[a];
                res.sigma_dex[i] = std::sqrt(std::max(d[a], 0.0));
                res.uncertainty[i] = nodes_[i].lin ? res.sigma_dex[i] : LN10 * res.sigma_dex[i];
            }
        }
        return res;
    }

private:
    struct Meas { int i; double target; double sigma; };
    struct Row { std::vector<std::pair<int, double>> entries; double r; double w; };

    std::vector<Node> nodes_;
    std::map<std::string, int> index_;
    std::vector<Relation> rels_;
    std::vector<Meas> meas_;
    std::vector<int> free_;
    std::vector<int> col_;
    std::vector<double> x_;
    bool nonlinear_ = false;

    static double log10_checked(double v, const std::string& what) {
        if (!(v > 0) || std::isinf(v)) throw std::invalid_argument(what + " must be a finite positive number");
        return std::log10(v);
    }
    double to_value(size_t i, double xi) const { return nodes_[i].lin ? xi : std::pow(10.0, xi); }
    Values values_of(const std::vector<double>& x) const {
        Values v;
        v.v.resize(x.size());
        for (size_t i = 0; i < x.size(); ++i) v.v[i] = to_value(i, x[i]);
        return v;
    }
    double fd_step(size_t i, double xi) const { return nodes_[i].lin ? 1e-6 * std::max(1.0, std::abs(xi)) : 1e-6; }

    double residual(const Relation& rel, const std::vector<double>& x, const Values& v) const {
        if (rel.kind == Relation::Log) {
            double r = -(rel.rhs_fn ? rel.rhs_fn(v) : rel.rhs);
            for (auto& [i, c] : rel.terms) r += c * x[(size_t) i];
            return r;
        }
        if (rel.kind == Relation::Sum) {
            double r = -rel.K;
            for (size_t k = 0; k < rel.terms.size(); ++k)
                r += rel.terms[k].second * std::pow(to_value((size_t) rel.terms[k].first, x[(size_t) rel.terms[k].first]), rel.powers[k]);
            return r;
        }
        return rel.f(v);
    }

    // central difference on node i, one-sided at the edge of a domain (a non-finite value or an exception)
    double derivative(const Relation& rel, std::vector<double>& x, Values& v, size_t i, double h, bool rhs_only) const {
        const double x0 = x[i], v0 = v.v[i];
        auto g = [&](double xi, bool& ok) {
            x[i] = xi;
            v.v[i] = to_value(i, xi);
            double out = 0.0;
            try {
                out = rhs_only ? -rel.rhs_fn(v) : residual(rel, x, v);
                ok = std::isfinite(out);
            } catch (...) {
                ok = false;
            }
            return out;
        };
        bool okp = false, okm = false, ok0 = false;
        const double fp = g(x0 + h, okp), fm = g(x0 - h, okm);
        double f0 = 0.0;
        if (!(okp && okm)) f0 = g(x0, ok0);
        x[i] = x0;
        v.v[i] = v0;
        if (okp && okm) return (fp - fm) / (2 * h);
        if (okp && ok0) return (fp - f0) / h;
        if (okm && ok0) return (f0 - fm) / h;
        return 0.0;
    }

    std::vector<Row> build_rows(std::vector<double>& x, bool jacobian, double* cost_out = nullptr) const {
        Values v = values_of(x);
        std::vector<Row> rows;
        rows.reserve(rels_.size() + meas_.size());
        double c = 0.0;
        for (auto& rel : rels_) {
            const double r = residual(rel, x, v);
            const double w = 1.0 / std::max(rel.sigma, 1e-300);
            c += (w * r) * (w * r);
            Row row{{}, r, w};
            if (jacobian) {
                std::map<int, double> e;
                if (rel.kind == Relation::Log) {
                    for (auto& [i, cf] : rel.terms)
                        if (col_[(size_t) i] >= 0) e[col_[(size_t) i]] += cf;
                    if (rel.rhs_fn) {
                        std::vector<int> probe = rel.depends;
                        if (probe.empty()) probe = free_;
                        for (int i : probe)
                            if (col_[(size_t) i] >= 0) {
                                const double d = derivative(rel, x, v, (size_t) i, fd_step((size_t) i, x[(size_t) i]), true);
                                if (std::abs(d) > 0.0) e[col_[(size_t) i]] += d;
                            }
                    }
                } else if (rel.kind == Relation::Sum) {
                    for (size_t k = 0; k < rel.terms.size(); ++k) {
                        const int i = rel.terms[k].first;
                        if (col_[(size_t) i] < 0) continue;
                        const double val = to_value((size_t) i, x[(size_t) i]);
                        const double dv = nodes_[(size_t) i].lin ? 1.0 : val * LN10;
                        const double p = rel.powers[k];
                        e[col_[(size_t) i]] += rel.terms[k].second * p * std::pow(val, p - 1) * dv;
                    }
                } else if (rel.grad) {
                    for (auto& [i, dfdv] : rel.grad(v))
                        if (col_[(size_t) i] >= 0) {
                            const double dv = nodes_[(size_t) i].lin ? 1.0 : v.v[(size_t) i] * LN10;
                            e[col_[(size_t) i]] += dfdv * dv;
                        }
                } else {
                    for (int i : rel.nodes)
                        if (col_[(size_t) i] >= 0) {
                            const double d = derivative(rel, x, v, (size_t) i, fd_step((size_t) i, x[(size_t) i]), false);
                            if (std::abs(d) > 0.0) e[col_[(size_t) i]] += d;
                        }
                }
                row.entries.assign(e.begin(), e.end());
            }
            rows.push_back(std::move(row));
        }
        for (auto& m : meas_) {
            const double r = x[(size_t) m.i] - m.target;
            const double w = 1.0 / m.sigma;
            c += (w * r) * (w * r);
            Row row{{}, r, w};
            if (jacobian && col_[(size_t) m.i] >= 0) row.entries.push_back({col_[(size_t) m.i], 1.0});
            rows.push_back(std::move(row));
        }
        if (cost_out) *cost_out = c;
        return rows;
    }

    double cost(std::vector<double> x) const {
        double c = 0.0;
        try {
            build_rows(x, false, &c);
        } catch (...) {
            return std::numeric_limits<double>::infinity();
        }
        return std::isfinite(c) ? c : std::numeric_limits<double>::infinity();
    }

    std::vector<double> step(const std::vector<Row>& rows, size_t n, double lam, std::vector<double>* R) const {
        std::vector<const Row*> live;
        for (auto& r : rows)
            if (!r.entries.empty()) live.push_back(&r);
        const size_t m = live.size();
        const size_t total = m + n * (lam > 0 ? 2 : 1);
        std::vector<double> A(total * n, 0.0), b(total, 0.0);
        for (size_t k = 0; k < m; ++k) {
            for (auto& [a, d] : live[k]->entries) A[k * n + (size_t) a] = live[k]->w * d;
            b[k] = -live[k]->w * live[k]->r;
        }
        for (size_t a = 0; a < n; ++a) A[(m + a) * n + a] = 1.0 / prior_sigma;
        if (lam > 0) {
            for (size_t a = 0; a < n; ++a) {
                double cn = 0.0;
                for (size_t k = 0; k < m; ++k) cn += A[k * n + a] * A[k * n + a];
                cn = std::sqrt(cn);
                if (!(cn > 0.0)) cn = 1.0;
                A[(m + n + a) * n + a] = std::sqrt(lam) * cn;
            }
        }
        return detail::qr_solve(std::move(A), std::move(b), total, n, R);
    }

    void descend(std::vector<double>& x, int max_iterations, double damping, double tolerance, int& iterations,
                 bool& converged) const {
        double lam = damping;
        converged = false;
        const size_t n = free_.size();
        double c = cost(x);
        for (iterations = 1; iterations <= max_iterations; ++iterations) {
            auto rows = build_rows(x, true, &c);
            bool accepted = false;
            std::vector<double> delta, x_new;
            double new_cost = 0.0;
            std::vector<double> gn;
            bool have_gn = true;
            try {
                gn = step(rows, n, 0.0, nullptr);
            } catch (const std::domain_error&) {
                have_gn = false;
            }
            if (have_gn) {
                double alpha = 1.0;
                for (int t = 0; t < 25; ++t) {
                    x_new = x;
                    delta.assign(n, 0.0);
                    for (size_t a = 0; a < n; ++a) {
                        delta[a] = alpha * gn[a];
                        x_new[(size_t) free_[a]] += delta[a];
                    }
                    new_cost = cost(x_new);
                    if (new_cost <= c) {
                        accepted = true;
                        break;
                    }
                    alpha *= 0.5;
                }
            }
            if (!accepted) {
                for (int t = 0; t < 30; ++t) {
                    try {
                        delta = step(rows, n, lam, nullptr);
                    } catch (const std::domain_error&) {
                        lam *= 10.0;
                        continue;
                    }
                    x_new = x;
                    for (size_t a = 0; a < n; ++a) x_new[(size_t) free_[a]] += delta[a];
                    new_cost = cost(x_new);
                    if (new_cost <= c) {
                        accepted = true;
                        break;
                    }
                    lam *= 10.0;
                }
            }
            if (!accepted) {
                converged = true;
                break;
            }
            double big = 0.0;
            for (double d : delta) big = std::max(big, std::abs(d));
            const double drop = c - new_cost;
            x = x_new;
            c = new_cost;
            lam = std::max(lam / 10.0, 1e-15);
            if (big < 1e-12 || drop <= tolerance * std::max(1.0, c)) {
                converged = true;
                break;
            }
        }
        if (iterations > max_iterations) iterations = max_iterations;
    }

    void continuation_solve(const std::vector<double>& x_start, std::vector<double>& x_direct, int max_iterations,
                            double damping, double tolerance, int& iterations, bool& converged) {
        std::vector<size_t> tight;
        for (size_t k = 0; k < rels_.size(); ++k)
            if (rels_[k].kind != Relation::Log && rels_[k].sigma < continuation_start) tight.push_back(k);
        if (tight.empty()) return;
        std::vector<double> true_sigma;
        double smallest = std::numeric_limits<double>::infinity();
        for (size_t k : tight) {
            true_sigma.push_back(rels_[k].sigma);
            smallest = std::min(smallest, rels_[k].sigma);
        }
        std::vector<double> levels{continuation_start};
        while (levels.back() > smallest) levels.push_back(levels.back() / continuation_factor);
        const int budget = std::max(5, max_iterations / (int) levels.size());
        std::vector<double> x = x_start;
        bool conv = false;
        int total = iterations;
        for (double level : levels) {
            for (size_t j = 0; j < tight.size(); ++j) rels_[tight[j]].sigma = std::max(true_sigma[j], level);
            if (!std::isfinite(cost(x))) break;
            int it = 0;
            descend(x, budget, damping, tolerance, it, conv);
            total += it;
        }
        for (size_t j = 0; j < tight.size(); ++j) rels_[tight[j]].sigma = true_sigma[j];
        iterations = total;
        if (conv && cost(x) <= cost(x_direct)) {
            x_direct = x;
            converged = true;
        } else {
            converged = false;
        }
    }

    std::vector<double> warm_start(const std::vector<double>& x) {
        std::vector<Relation> saved = rels_;
        std::vector<Relation> linear;
        for (auto& r : rels_)
            if (r.kind == Relation::Log && !r.rhs_fn) linear.push_back(r);
        if (linear.empty()) return x;
        rels_ = linear;
        std::vector<double> xw = x;
        bool ok = true;
        try {
            for (int t = 0; t < 3; ++t) {
                auto rows = build_rows(xw, true);
                auto delta = step(rows, free_.size(), 0.0, nullptr);
                for (size_t a = 0; a < free_.size(); ++a) xw[(size_t) free_[a]] += delta[a];
            }
        } catch (const std::domain_error&) {
            ok = false;
        }
        rels_ = saved;
        if (!ok) return x;
        const double cw = cost(xw), c0 = cost(x);
        return (std::isfinite(cw) && (cw <= c0 || !std::isfinite(c0))) ? xw : x;
    }
};

}  // namespace grm
