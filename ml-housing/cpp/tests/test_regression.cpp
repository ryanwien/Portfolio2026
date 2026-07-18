// test_regression.cpp — self-contained test suite, no framework to install.

#include "../src/regression.hpp"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;
using namespace ml;

namespace {

int g_checks = 0;
int g_failures = 0;
const bool g_verbose = std::getenv("ML_TEST_VERBOSE") != nullptr;

void check(bool condition, const std::string& what) {
    ++g_checks;
    if (g_verbose) std::cout << "    - " << what << "\n";
    if (!condition) {
        ++g_failures;
        std::cout << "  FAIL: " << what << "\n";
    }
}

void check_near(double actual, double expected, double tolerance, const std::string& what) {
    ++g_checks;
    if (g_verbose) std::cout << "    - " << what << "\n";
    if (std::fabs(actual - expected) > tolerance) {
        ++g_failures;
        std::cout << "  FAIL: " << what << "\n"
                  << "        expected: " << expected << " (+/- " << tolerance << ")\n"
                  << "        actual:   " << actual << "\n";
    }
}

fs::path find_data_dir() {
    for (fs::path dir = fs::current_path(); ; dir = dir.parent_path()) {
        if (fs::exists(dir / "data" / "housing.csv")) return dir / "data";
        if (!dir.has_relative_path() || dir == dir.parent_path()) break;
    }
    return {};
}

void test_linear_solver() {
    std::cout << "solve_linear_system\n";

    // 2x + y = 5 ; x + 3y = 10  ->  x = 1, y = 3
    auto x = solve_linear_system({{2, 1}, {1, 3}}, {5, 10});
    check_near(x[0], 1.0, 1e-12, "solves a 2x2 system");
    check_near(x[1], 3.0, 1e-12, "solves a 2x2 system (second unknown)");

    // Identity leaves the right-hand side untouched.
    auto id = solve_linear_system({{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}, {4, 5, 6});
    check_near(id[0], 4.0, 1e-12, "identity returns b unchanged");
    check_near(id[2], 6.0, 1e-12, "identity returns b unchanged (third)");

    // A leading zero pivot must be swapped away, not divided by. Without
    // partial pivoting this system fails outright.
    auto pivoted = solve_linear_system({{0, 2}, {3, 1}}, {4, 5});
    check_near(pivoted[0], 1.0, 1e-12, "handles a zero in the pivot position");
    check_near(pivoted[1], 2.0, 1e-12, "handles a zero in the pivot position (second)");

    bool threw = false;
    try {
        // Second row is twice the first: no unique solution.
        solve_linear_system({{1, 2}, {2, 4}}, {3, 6});
    } catch (const std::runtime_error&) { threw = true; }
    check(threw, "a singular matrix is rejected rather than returning nonsense");
}

void test_scaler() {
    std::cout << "standardization\n";

    const Matrix X = {{1, 10}, {2, 20}, {3, 30}, {4, 40}};
    const auto scaler = Scaler::fit(X);

    check_near(scaler.mean[0], 2.5, 1e-12, "mean of the first column");
    check_near(scaler.mean[1], 25.0, 1e-12, "mean of the second column");
    // Population standard deviation (ddof = 0), matching NumPy's default.
    check_near(scaler.std[0], std::sqrt(1.25), 1e-12, "population standard deviation");

    const auto Z = scaler.transform(X);
    double sum = 0.0, sum_sq = 0.0;
    for (const auto& row : Z) { sum += row[0]; sum_sq += row[0] * row[0]; }
    check_near(sum / 4.0, 0.0, 1e-12, "scaled column has mean zero");
    check_near(std::sqrt(sum_sq / 4.0), 1.0, 1e-12, "scaled column has unit variance");

    // A constant column would divide by zero; it must survive as zeros.
    const auto constant = Scaler::fit({{5}, {5}, {5}});
    const auto Zc = constant.transform({{5}, {5}});
    check_near(Zc[0][0], 0.0, 1e-12, "a constant column becomes zeros, not NaN");
}

void test_fit_recovers_a_known_relationship() {
    std::cout << "fitting a noiseless relationship\n";

    // y = 100 + 2*a + 5*b, exactly. OLS must recover it to machine precision.
    Matrix X;
    std::vector<double> y;
    for (int a = 0; a < 10; ++a) {
        for (int b = 0; b < 10; ++b) {
            X.push_back({static_cast<double>(a), static_cast<double>(b)});
            y.push_back(100.0 + 2.0 * a + 5.0 * b);
        }
    }

    const auto model = fit(X, y);
    check_near(model.intercept, 100.0, 1e-8, "recovers the intercept");
    check_near(model.coefficients[0], 2.0, 1e-9, "recovers the first coefficient");
    check_near(model.coefficients[1], 5.0, 1e-9, "recovers the second coefficient");

    const auto m = evaluate(y, model.predict(X));
    check_near(m.r2, 1.0, 1e-12, "a perfect fit explains all the variance");
    check_near(m.rmse, 0.0, 1e-6, "a perfect fit has no error");
}

void test_metrics() {
    std::cout << "metrics\n";

    // Errors of +1, -1, +1, -1 against a spread-out target.
    const std::vector<double> actual = {10, 20, 30, 40};
    const std::vector<double> predicted = {9, 21, 29, 41};

    const auto m = evaluate(actual, predicted);
    check_near(m.rmse, 1.0, 1e-12, "RMSE of unit errors is 1");
    check_near(m.mae, 1.0, 1e-12, "MAE of unit errors is 1");
    check(m.n == 4, "row count is reported");

    // ss_tot = 500, ss_res = 4  ->  R2 = 1 - 4/500
    check_near(m.r2, 1.0 - 4.0 / 500.0, 1e-12, "R2 matches the hand calculation");

    // Predicting the mean for everything explains nothing.
    const auto baseline = evaluate(actual, {25, 25, 25, 25});
    check_near(baseline.r2, 0.0, 1e-12, "predicting the mean gives R2 of zero");

    // Worse than the mean must go negative, not clamp at zero.
    const auto bad = evaluate(actual, {100, 100, 100, 100});
    check(bad.r2 < 0.0, "a model worse than the mean scores below zero");
}

void test_input_validation() {
    std::cout << "input validation\n";

    auto rejects = [](auto&& fn, const std::string& what) {
        bool threw = false;
        try { fn(); } catch (const std::exception&) { threw = true; }
        check(threw, what);
    };

    rejects([] { fit({}, {}); }, "fitting on zero rows is rejected");
    rejects([] { fit({{1.0}}, {1.0, 2.0}); }, "mismatched X and y lengths are rejected");
    rejects([] { evaluate({1, 2}, {1}); }, "mismatched metric lengths are rejected");
    rejects([] { load_csv("no-such-file.csv"); }, "a missing CSV is reported");

    // A CSV without the target column should say so rather than misbehave.
    const auto tmp = fs::temp_directory_path() / "ml_test_no_target.csv";
    std::ofstream(tmp) << "a,b\n1,2\n";
    rejects([&] { load_csv(tmp.string()); }, "a CSV with no price column is reported");
    fs::remove(tmp);

    // Ragged rows are a data error, not something to silently pad.
    const auto ragged = fs::temp_directory_path() / "ml_test_ragged.csv";
    std::ofstream(ragged) << "a,price\n1,2\n3\n";
    rejects([&] { load_csv(ragged.string()); }, "a ragged row is reported");
    fs::remove(ragged);
}

void test_reproduces_the_committed_model() {
    std::cout << "end-to-end against the committed data\n";

    const auto data_dir = find_data_dir();
    if (data_dir.empty()) {
        std::cout << "  SKIP: data/housing.csv not found from the working directory\n";
        return;
    }

    const auto data = load_csv((data_dir / "housing.csv").string());
    check(data.rows() == 2000, "the dataset has 2000 rows");
    check(data.features() == 6, "the dataset has 6 features");

    const auto split = load_split((data_dir / "split_indices.txt").string());
    check(split.train.size() == 1600, "1600 training rows");
    check(split.test.size() == 400, "400 test rows");

    // Train and test must not overlap, or the score is meaningless.
    std::vector<bool> seen(data.rows(), false);
    for (const auto i : split.train) seen[i] = true;
    bool overlap = false;
    for (const auto i : split.test) if (seen[i]) overlap = true;
    check(!overlap, "the test set is genuinely held out");

    const auto model = fit(take_rows(data.X, split.train), take_rows(data.y, split.train));

    // These are the values the Python closed-form solution produces on the
    // same rows. Tolerances are loose enough for cross-language floating point
    // and tight enough that a real divergence fails.
    check_near(model.intercept, 6516.3289, 0.01, "intercept matches the Python model");
    check_near(model.coefficients[0], 147.9107, 1e-3, "sqft coefficient matches");
    check_near(model.coefficients[1], 7449.5246, 1e-2, "bedrooms coefficient matches");
    check_near(model.coefficients[2], 11998.2683, 1e-2, "bathrooms coefficient matches");
    check_near(model.coefficients[3], -811.0954, 1e-3, "age coefficient matches");
    check_near(model.coefficients[4], 6348.7842, 1e-2, "garage coefficient matches");
    check_near(model.coefficients[5], 14704.7599, 1e-2, "location_score coefficient matches");

    const auto m = evaluate(take_rows(data.y, split.test),
                            model.predict(take_rows(data.X, split.test)));
    check_near(m.r2, 0.9447348264948280, 1e-9, "test R2 matches the Python model");
    check_near(m.rmse, 23572.80, 0.01, "test RMSE matches");
    check_near(m.mae, 19091.71, 0.01, "test MAE matches");

    // Sanity: the model should beat predicting the mean by a wide margin.
    check(m.r2 > 0.9, "the model explains most of the variance");
}

}  // namespace

int main() {
    std::cout << std::unitbuf;
    std::cout << "house price regression tests\n" << std::string(60, '-') << "\n";

    test_linear_solver();
    test_scaler();
    test_fit_recovers_a_known_relationship();
    test_metrics();
    test_input_validation();
    test_reproduces_the_committed_model();

    std::cout << std::string(60, '-') << "\n";
    if (g_failures == 0) {
        std::cout << "All " << g_checks << " checks passed.\n";
        return 0;
    }
    std::cout << g_failures << " of " << g_checks << " checks FAILED.\n";
    return 1;
}
