// regression.hpp — ordinary least squares, solved from scratch.
//
// No linear-algebra dependency: the normal equations are formed and solved with
// Gauss-Jordan elimination and partial pivoting, which is the part of this
// project worth writing by hand.

#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace ml {

using Row = std::vector<double>;
using Matrix = std::vector<Row>;

/// Feature columns plus the target, as read from a CSV.
struct Dataset {
    std::vector<std::string> feature_names;
    Matrix X;               // rows x features
    std::vector<double> y;  // rows

    std::size_t rows() const { return X.size(); }
    std::size_t features() const { return feature_names.size(); }
};

/// Per-feature mean and standard deviation, fitted on the training rows only.
struct Scaler {
    std::vector<double> mean;
    std::vector<double> std;

    /// Population standard deviation (ddof = 0), matching NumPy's default.
    static Scaler fit(const Matrix& X);
    Matrix transform(const Matrix& X) const;
};

struct Metrics {
    double r2{};
    double rmse{};
    double mae{};
    std::size_t n{};
};

/// A fitted linear model, expressed in the original feature units.
struct LinearModel {
    double intercept{};
    std::vector<double> coefficients;      // per raw unit, e.g. dollars per sq ft
    std::vector<double> standardized;      // weights on standardized features

    double predict(const Row& features) const;
    std::vector<double> predict(const Matrix& X) const;
};

/// Read a CSV whose last-named column is `target`. Throws on malformed input.
Dataset load_csv(const std::string& path, const std::string& target = "price");

/// Read the train/test row indices frozen by export_split.py.
struct Split {
    std::vector<std::size_t> train;
    std::vector<std::size_t> test;
};
Split load_split(const std::string& path);

/// Select rows by index.
Matrix take_rows(const Matrix& X, const std::vector<std::size_t>& indices);
std::vector<double> take_rows(const std::vector<double>& y, const std::vector<std::size_t>& indices);

/// Solve A x = b by Gauss-Jordan elimination with partial pivoting.
/// Throws std::runtime_error if A is singular to working precision.
std::vector<double> solve_linear_system(Matrix A, std::vector<double> b);

/// Fit by ordinary least squares. Features are standardized internally, then
/// the weights are converted back so the returned model takes raw units.
LinearModel fit(const Matrix& X, const std::vector<double>& y);

Metrics evaluate(const std::vector<double>& actual, const std::vector<double>& predicted);

}  // namespace ml
