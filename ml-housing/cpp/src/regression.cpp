#include "regression.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <numeric>
#include <sstream>

namespace ml {
namespace {

std::vector<std::string> split_line(const std::string& line, char delimiter = ',') {
    std::vector<std::string> fields;
    std::string field;
    std::istringstream stream(line);
    while (std::getline(stream, field, delimiter)) {
        // Tolerate CRLF files on any platform.
        if (!field.empty() && field.back() == '\r') field.pop_back();
        fields.push_back(field);
    }
    return fields;
}

}  // namespace

Scaler Scaler::fit(const Matrix& X) {
    if (X.empty()) throw std::invalid_argument("cannot fit a scaler on zero rows");

    const std::size_t features = X.front().size();
    Scaler scaler;
    scaler.mean.assign(features, 0.0);
    scaler.std.assign(features, 0.0);

    for (const auto& row : X)
        for (std::size_t j = 0; j < features; ++j) scaler.mean[j] += row[j];
    for (double& m : scaler.mean) m /= static_cast<double>(X.size());

    for (const auto& row : X)
        for (std::size_t j = 0; j < features; ++j) {
            const double d = row[j] - scaler.mean[j];
            scaler.std[j] += d * d;
        }
    for (double& s : scaler.std) {
        s = std::sqrt(s / static_cast<double>(X.size()));
        // A constant column carries no information; scaling by 1 leaves it at
        // zero after centring rather than producing NaNs.
        if (s == 0.0) s = 1.0;
    }

    return scaler;
}

Matrix Scaler::transform(const Matrix& X) const {
    Matrix out;
    out.reserve(X.size());
    for (const auto& row : X) {
        Row scaled(row.size());
        for (std::size_t j = 0; j < row.size(); ++j) scaled[j] = (row[j] - mean[j]) / std[j];
        out.push_back(std::move(scaled));
    }
    return out;
}

double LinearModel::predict(const Row& features) const {
    double total = intercept;
    for (std::size_t j = 0; j < features.size(); ++j) total += coefficients[j] * features[j];
    return total;
}

std::vector<double> LinearModel::predict(const Matrix& X) const {
    std::vector<double> out;
    out.reserve(X.size());
    for (const auto& row : X) out.push_back(predict(row));
    return out;
}

Dataset load_csv(const std::string& path, const std::string& target) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("could not open '" + path + "'");

    std::string line;
    if (!std::getline(in, line)) throw std::runtime_error("'" + path + "' is empty");

    const auto header = split_line(line);
    const auto target_it = std::find(header.begin(), header.end(), target);
    if (target_it == header.end())
        throw std::runtime_error("'" + path + "' has no '" + target + "' column");
    const auto target_col = static_cast<std::size_t>(target_it - header.begin());

    Dataset data;
    for (std::size_t j = 0; j < header.size(); ++j)
        if (j != target_col) data.feature_names.push_back(header[j]);

    std::size_t line_no = 1;
    while (std::getline(in, line)) {
        ++line_no;
        if (line.empty()) continue;

        const auto fields = split_line(line);
        if (fields.size() != header.size())
            throw std::runtime_error("'" + path + "' line " + std::to_string(line_no) +
                                     ": expected " + std::to_string(header.size()) + " fields, got " +
                                     std::to_string(fields.size()));

        Row features;
        features.reserve(header.size() - 1);
        try {
            for (std::size_t j = 0; j < fields.size(); ++j) {
                const double value = std::stod(fields[j]);
                if (j == target_col) data.y.push_back(value);
                else features.push_back(value);
            }
        } catch (const std::invalid_argument&) {
            throw std::runtime_error("'" + path + "' line " + std::to_string(line_no) +
                                     ": non-numeric value");
        }
        data.X.push_back(std::move(features));
    }

    return data;
}

Split load_split(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("could not open '" + path + "'");

    Split split;
    std::string line;
    std::vector<std::size_t>* target = nullptr;
    std::size_t expected = 0;

    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::istringstream stream(line);
        std::string first;
        stream >> first;

        if (first == "train" || first == "test") {
            target = first == "train" ? &split.train : &split.test;
            stream >> expected;
            continue;
        }
        if (target == nullptr) throw std::runtime_error("'" + path + "': indices before a section header");

        // This line is the index list for the section just declared.
        std::istringstream indices(line);
        std::size_t index{};
        while (indices >> index) target->push_back(index);

        if (target->size() != expected)
            throw std::runtime_error("'" + path + "': expected " + std::to_string(expected) +
                                     " indices, read " + std::to_string(target->size()));
        target = nullptr;
    }

    if (split.train.empty() || split.test.empty())
        throw std::runtime_error("'" + path + "': missing a train or test section");

    return split;
}

Matrix take_rows(const Matrix& X, const std::vector<std::size_t>& indices) {
    Matrix out;
    out.reserve(indices.size());
    for (const std::size_t i : indices) {
        if (i >= X.size()) throw std::out_of_range("split index " + std::to_string(i) + " is past the data");
        out.push_back(X[i]);
    }
    return out;
}

std::vector<double> take_rows(const std::vector<double>& y, const std::vector<std::size_t>& indices) {
    std::vector<double> out;
    out.reserve(indices.size());
    for (const std::size_t i : indices) {
        if (i >= y.size()) throw std::out_of_range("split index " + std::to_string(i) + " is past the data");
        out.push_back(y[i]);
    }
    return out;
}

std::vector<double> solve_linear_system(Matrix A, std::vector<double> b) {
    const std::size_t n = A.size();
    if (n == 0 || A.front().size() != n || b.size() != n)
        throw std::invalid_argument("solve_linear_system needs a square system");

    for (std::size_t col = 0; col < n; ++col) {
        // Partial pivoting: use the largest remaining magnitude in this column
        // as the pivot. Without it, a small pivot amplifies rounding error and
        // the solution silently loses precision.
        std::size_t pivot = col;
        for (std::size_t r = col + 1; r < n; ++r)
            if (std::fabs(A[r][col]) > std::fabs(A[pivot][col])) pivot = r;

        if (std::fabs(A[pivot][col]) < 1e-12)
            throw std::runtime_error("matrix is singular to working precision");

        std::swap(A[col], A[pivot]);
        std::swap(b[col], b[pivot]);

        const double diagonal = A[col][col];
        for (std::size_t j = col; j < n; ++j) A[col][j] /= diagonal;
        b[col] /= diagonal;

        for (std::size_t r = 0; r < n; ++r) {
            if (r == col) continue;
            const double factor = A[r][col];
            if (factor == 0.0) continue;
            for (std::size_t j = col; j < n; ++j) A[r][j] -= factor * A[col][j];
            b[r] -= factor * b[col];
        }
    }

    return b;
}

LinearModel fit(const Matrix& X, const std::vector<double>& y) {
    if (X.empty()) throw std::invalid_argument("cannot fit on zero rows");
    if (X.size() != y.size()) throw std::invalid_argument("X and y row counts differ");

    const std::size_t features = X.front().size();
    const auto scaler = Scaler::fit(X);
    const auto Z = scaler.transform(X);

    // Normal equations on the design matrix [1, Z]: (D'D) theta = D'y.
    const std::size_t dim = features + 1;
    Matrix DtD(dim, Row(dim, 0.0));
    std::vector<double> Dty(dim, 0.0);

    for (std::size_t i = 0; i < Z.size(); ++i) {
        Row design(dim);
        design[0] = 1.0;
        for (std::size_t j = 0; j < features; ++j) design[j + 1] = Z[i][j];

        for (std::size_t r = 0; r < dim; ++r) {
            Dty[r] += design[r] * y[i];
            for (std::size_t c = 0; c < dim; ++c) DtD[r][c] += design[r] * design[c];
        }
    }

    const auto theta = solve_linear_system(DtD, Dty);

    LinearModel model;
    model.standardized.assign(theta.begin() + 1, theta.end());

    // Convert standardized weights back to raw units so callers can predict
    // straight from unscaled features.
    model.coefficients.resize(features);
    double shift = 0.0;
    for (std::size_t j = 0; j < features; ++j) {
        model.coefficients[j] = model.standardized[j] / scaler.std[j];
        shift += model.standardized[j] * scaler.mean[j] / scaler.std[j];
    }
    model.intercept = theta[0] - shift;

    return model;
}

Metrics evaluate(const std::vector<double>& actual, const std::vector<double>& predicted) {
    if (actual.size() != predicted.size())
        throw std::invalid_argument("actual and predicted lengths differ");
    if (actual.empty()) throw std::invalid_argument("cannot evaluate zero rows");

    const double mean = std::accumulate(actual.begin(), actual.end(), 0.0) /
                        static_cast<double>(actual.size());

    double ss_res = 0.0, ss_tot = 0.0, abs_error = 0.0;
    for (std::size_t i = 0; i < actual.size(); ++i) {
        const double residual = actual[i] - predicted[i];
        ss_res += residual * residual;
        abs_error += std::fabs(residual);
        const double centred = actual[i] - mean;
        ss_tot += centred * centred;
    }

    Metrics m;
    m.n = actual.size();
    m.r2 = ss_tot == 0.0 ? 0.0 : 1.0 - ss_res / ss_tot;
    m.rmse = std::sqrt(ss_res / static_cast<double>(actual.size()));
    m.mae = abs_error / static_cast<double>(actual.size());
    return m;
}

}  // namespace ml
