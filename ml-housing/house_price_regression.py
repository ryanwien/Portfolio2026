"""
house_price_regression.py

Predicting house prices with supervised machine learning (regression).

This project applies the core concepts from Andrew Ng's "Supervised Machine
Learning: Regression and Classification" course to a fresh housing dataset:
    - Loading and exploring data
    - Feature scaling (standardization)
    - Training a linear regression model with gradient descent
    - Evaluating with train/test split and standard metrics
    - Comparing against a closed-form (normal equation) baseline

Run:
    pip install -r requirements.txt
    python house_price_regression.py
"""

import numpy as np
import pandas as pd
from sklearn.model_selection import train_test_split
from sklearn.preprocessing import StandardScaler
from sklearn.linear_model import LinearRegression, SGDRegressor
from sklearn.metrics import mean_squared_error, mean_absolute_error, r2_score
import os

DATA_PATH = os.path.join(os.path.dirname(__file__), "data", "housing.csv")


def load_and_explore(path):
    """Load the dataset and print a quick exploratory summary.

    Understanding the data before modeling is the first step taught
    in the course — shapes, ranges, and how features relate to price.
    """
    df = pd.read_csv(path)

    print("=" * 60)
    print("DATA EXPLORATION")
    print("=" * 60)
    print(f"Dataset shape: {df.shape[0]} rows, {df.shape[1]} columns")
    print(f"\nFeatures: {[c for c in df.columns if c != 'price']}")
    print(f"Target:   price\n")

    # Correlation of each feature with the target tells us which
    # features carry the most predictive signal.
    print("Correlation with price (higher magnitude = stronger signal):")
    correlations = df.corr()["price"].drop("price").sort_values(ascending=False)
    for feature, corr in correlations.items():
        print(f"  {feature:18s} {corr:+.3f}")
    print()

    return df


def prepare_data(df):
    """Split into features/target, then train/test, then scale.

    Feature scaling matters for gradient descent: without it, features
    on large scales (like sqft) dominate features on small scales (like
    bedrooms), and gradient descent converges slowly or poorly. This is
    a key lesson from the course.
    """
    X = df.drop(columns=["price"]).values
    y = df["price"].values

    # 80/20 train/test split. The test set is held out and never seen
    # during training, so it gives an honest estimate of performance.
    X_train, X_test, y_train, y_test = train_test_split(
        X, y, test_size=0.2, random_state=42
    )

    # Standardize features: each becomes mean 0, std 1.
    # We fit the scaler on the TRAINING data only, then apply it to test —
    # fitting on test data would leak information.
    scaler = StandardScaler()
    X_train_scaled = scaler.fit_transform(X_train)
    X_test_scaled = scaler.transform(X_test)

    return X_train_scaled, X_test_scaled, y_train, y_test, scaler


def train_gradient_descent(X_train, y_train):
    """Train linear regression using gradient descent (SGDRegressor).

    This mirrors the course's core algorithm: iteratively adjust weights
    to minimize the cost function (mean squared error). SGDRegressor is
    scikit-learn's gradient-descent implementation.
    """
    model = SGDRegressor(
        max_iter=2000,          # how many passes over the data
        learning_rate="adaptive",
        eta0=0.01,              # initial learning rate (alpha in the course)
        random_state=42,
    )
    model.fit(X_train, y_train)
    return model


def train_normal_equation(X_train, y_train):
    """Train using the closed-form solution (normal equation).

    LinearRegression solves for the optimal weights directly, without
    iteration. It's a useful baseline: gradient descent should get close
    to this if it converged properly.
    """
    model = LinearRegression()
    model.fit(X_train, y_train)
    return model


def evaluate(model, X_test, y_test, label):
    """Evaluate a trained model on the held-out test set."""
    predictions = model.predict(X_test)

    rmse = np.sqrt(mean_squared_error(y_test, predictions))
    mae = mean_absolute_error(y_test, predictions)
    r2 = r2_score(y_test, predictions)

    print(f"\n{label}")
    print("-" * 60)
    print(f"  RMSE (root mean squared error): ${rmse:,.0f}")
    print(f"  MAE  (mean absolute error):     ${mae:,.0f}")
    print(f"  R²   (variance explained):      {r2:.4f}")

    return {"rmse": rmse, "mae": mae, "r2": r2}


def show_sample_predictions(model, X_test, y_test, n=5):
    """Print a few predictions next to actual values for intuition."""
    predictions = model.predict(X_test)
    print(f"\nSAMPLE PREDICTIONS (first {n} test houses)")
    print("-" * 60)
    print(f"  {'Predicted':>12s}  {'Actual':>12s}  {'Error':>12s}")
    for i in range(n):
        pred = predictions[i]
        actual = y_test[i]
        error = pred - actual
        print(f"  ${pred:>10,.0f}  ${actual:>10,.0f}  ${error:>+10,.0f}")


def main():
    # 1. Load and explore
    df = load_and_explore(DATA_PATH)

    # 2. Prepare (split + scale)
    X_train, X_test, y_train, y_test, scaler = prepare_data(df)
    print(f"Training set: {X_train.shape[0]} houses")
    print(f"Test set:     {X_test.shape[0]} houses")

    # 3. Train both approaches
    print("\n" + "=" * 60)
    print("MODEL TRAINING & EVALUATION")
    print("=" * 60)

    gd_model = train_gradient_descent(X_train, y_train)
    ne_model = train_normal_equation(X_train, y_train)

    # 4. Evaluate both on the test set
    evaluate(gd_model, X_test, y_test, "Gradient Descent (SGDRegressor)")
    evaluate(ne_model, X_test, y_test, "Normal Equation (LinearRegression)")

    # 5. Sample predictions from the gradient descent model
    show_sample_predictions(gd_model, X_test, y_test)

    # 6. Show what the model learned — feature importance via weights.
    # Because features are standardized, the weight magnitudes are directly
    # comparable: larger magnitude = bigger effect on price.
    print("\n" + "=" * 60)
    print("LEARNED FEATURE WEIGHTS (on standardized features)")
    print("=" * 60)
    feature_names = df.drop(columns=["price"]).columns
    weights = sorted(
        zip(feature_names, gd_model.coef_),
        key=lambda pair: abs(pair[1]),
        reverse=True,
    )
    for name, weight in weights:
        print(f"  {name:18s} {weight:+12,.0f}")
    print("\nInterpretation: features with larger absolute weights have a")
    print("stronger influence on the predicted price.")


if __name__ == "__main__":
    main()
