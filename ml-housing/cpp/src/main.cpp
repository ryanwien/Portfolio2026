// main.cpp — train the house-price model and report the same figures the
// Python script does, so the two can be compared directly.

#include "regression.hpp"

#include <filesystem>
#include <iomanip>
#include <iostream>

namespace fs = std::filesystem;

namespace {

/// Find the project's data directory by walking up from the working directory,
/// so the binary runs from anywhere in the tree.
fs::path find_data_dir() {
    for (fs::path dir = fs::current_path(); ; dir = dir.parent_path()) {
        const auto candidate = dir / "data" / "housing.csv";
        if (fs::exists(candidate)) return dir / "data";
        if (!dir.has_relative_path() || dir == dir.parent_path()) break;
    }
    throw std::runtime_error("could not find data/housing.csv above the working directory");
}

void print_metrics(const std::string& label, const ml::Metrics& m) {
    std::cout << "\n" << label << "\n" << std::string(60, '-') << "\n"
              << std::fixed
              << "  RMSE (root mean squared error): $" << std::setprecision(0) << m.rmse << "\n"
              << "  MAE  (mean absolute error):     $" << std::setprecision(0) << m.mae << "\n"
              << "  R2   (variance explained):      " << std::setprecision(4) << m.r2 << "\n";
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const fs::path data_dir = argc > 1 ? fs::path(argv[1]) : find_data_dir();
        const auto data = ml::load_csv((data_dir / "housing.csv").string());
        const auto split = ml::load_split((data_dir / "split_indices.txt").string());

        const auto X_train = ml::take_rows(data.X, split.train);
        const auto y_train = ml::take_rows(data.y, split.train);
        const auto X_test = ml::take_rows(data.X, split.test);
        const auto y_test = ml::take_rows(data.y, split.test);

        std::cout << std::string(60, '=') << "\n"
                  << "HOUSE PRICE REGRESSION  (C++ implementation)\n"
                  << std::string(60, '=') << "\n"
                  << "  rows: " << data.rows() << "   features: " << data.features()
                  << "   train: " << X_train.size() << "   test: " << X_test.size() << "\n";

        const auto model = ml::fit(X_train, y_train);

        print_metrics("Normal Equation (solved with Gauss-Jordan)",
                      ml::evaluate(y_test, model.predict(X_test)));

        std::cout << "\n" << std::string(60, '=') << "\n"
                  << "LEARNED FEATURE WEIGHTS (on standardized features)\n"
                  << std::string(60, '=') << "\n";

        // Largest absolute weight first: the ordering the Python script prints.
        std::vector<std::size_t> order(data.features());
        for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
        std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
            return std::fabs(model.standardized[a]) > std::fabs(model.standardized[b]);
        });

        for (const std::size_t i : order) {
            std::cout << "  " << std::left << std::setw(22) << data.feature_names[i] << std::right
                      << (model.standardized[i] < 0 ? "-" : "+") << "$"
                      << std::fixed << std::setprecision(0) << std::fabs(model.standardized[i]) << "\n";
        }

        std::cout << "\n" << std::string(60, '=') << "\n"
                  << "COEFFICIENTS IN RAW UNITS (what the browser demo predicts with)\n"
                  << std::string(60, '=') << "\n"
                  << "  " << std::left << std::setw(22) << "intercept" << std::right
                  << std::setprecision(4) << model.intercept << "\n";
        for (std::size_t i = 0; i < data.features(); ++i) {
            std::cout << "  " << std::left << std::setw(22) << data.feature_names[i] << std::right
                      << std::setprecision(4) << model.coefficients[i] << "\n";
        }

        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
}
