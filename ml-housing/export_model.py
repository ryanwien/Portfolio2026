"""export_model.py — regenerate the model blob the browser demo runs on.

demo.html carries the trained model inline so it can predict with no backend.
Those numbers were originally pasted in by hand, which let them drift out of
step with the committed data and training script. This script regenerates the
whole blob from `data/housing.csv` and the frozen split in
`data/split_indices.txt`, so the demo, the Python script, and the C++
implementation all describe the same model.

Usage:
    python export_split.py      # first, if data/split_indices.txt is missing
    python export_model.py      # rewrites the `var M = {...}` line in demo.html
"""

import io
import json
import os
import re

import numpy as np
import pandas as pd

BASE = os.path.dirname(os.path.abspath(__file__))
CSV_PATH = os.path.join(BASE, "data", "housing.csv")
SPLIT_PATH = os.path.join(BASE, "data", "split_indices.txt")
DEMO_PATH = os.path.join(BASE, "demo.html")

SCATTER_POINTS = 150


def load_split(path):
    """Read the train/test row indices frozen by export_split.py."""
    rows = [line.split() for line in io.open(path, encoding="utf-8")
            if line.strip() and not line.startswith("#")]
    counts = {rows[0][0]: int(rows[0][1]), rows[2][0]: int(rows[2][1])}
    train = [int(i) for i in rows[1]]
    test = [int(i) for i in rows[3]]
    assert len(train) == counts["train"] and len(test) == counts["test"], "split file is inconsistent"
    return train, test


def solve_normal_equation(X, y):
    """Closed-form least squares on a design matrix with a leading 1s column."""
    Xb = np.column_stack([np.ones(len(X)), X])
    return np.linalg.solve(Xb.T @ Xb, Xb.T @ y)


def metrics(actual, predicted):
    residual = actual - predicted
    ss_res = float((residual ** 2).sum())
    ss_tot = float(((actual - actual.mean()) ** 2).sum())
    return {
        "r2": 1 - ss_res / ss_tot,
        "rmse": float(np.sqrt((residual ** 2).mean())),
        "mae": float(np.abs(residual).mean()),
        "n": int(len(actual)),
    }


def main():
    df = pd.read_csv(CSV_PATH)
    features = [c for c in df.columns if c != "price"]
    X_all = df[features].values.astype(float)
    y_all = df["price"].values.astype(float)

    train_idx, test_idx = load_split(SPLIT_PATH)
    X_train, y_train = X_all[train_idx], y_all[train_idx]
    X_test, y_test = X_all[test_idx], y_all[test_idx]

    # Standardize on the training set only — fitting the scaler on test data
    # would leak information about examples the model is meant not to have seen.
    mean = X_train.mean(axis=0)
    std = X_train.std(axis=0, ddof=0)

    theta = solve_normal_equation((X_train - mean) / std, y_train)
    bias, weights = theta[0], theta[1:]

    # Convert standardized weights back to per-unit coefficients, which is what
    # the demo needs to predict directly from slider values.
    coef = weights / std
    intercept = bias - float((weights * mean / std).sum())

    def predict(X):
        return intercept + X @ coef

    blob = {
        "features": features,
        "intercept": round(float(intercept), 4),
        "coef": {f: round(float(c), 4) for f, c in zip(features, coef)},
        # Weights on standardized features: comparable across features, which
        # raw per-unit coefficients are not.
        "importance": {f: int(round(float(w))) for f, w in zip(features, weights)},
        "correlations": {f: round(float(np.corrcoef(X_all[:, i], y_all)[0, 1]), 3)
                         for i, f in enumerate(features)},
        "featStats": {
            f: {
                "min": round(float(X_all[:, i].min()), 2),
                "max": round(float(X_all[:, i].max()), 2),
                "mean": round(float(X_all[:, i].mean()), 1),
                "median": round(float(np.median(X_all[:, i])), 1),
                "std": round(float(X_all[:, i].std(ddof=0)), 2),
            }
            for i, f in enumerate(features)
        },
        "priceStats": {
            "min": int(y_all.min()),
            "max": int(y_all.max()),
            "mean": int(round(y_all.mean())),
            "median": int(round(float(np.median(y_all)))),
        },
        "metrics": {
            "test": metrics(y_test, predict(X_test)),
            "train": metrics(y_train, predict(X_train)),
            "all": metrics(y_all, predict(X_all)),
        },
        "nTotal": int(len(df)),
        # Evenly spaced sample of the test set, so the plot is representative
        # and identical on every regeneration.
        "scatter": [
            {"actual": int(round(y_test[i])), "predicted": int(round(predict(X_test)[i]))}
            for i in np.linspace(0, len(test_idx) - 1, SCATTER_POINTS).astype(int)
        ],
    }

    line = "  var M = " + json.dumps(blob, separators=(",", ":")) + ";"

    html = io.open(DEMO_PATH, encoding="utf-8").read()
    updated, count = re.subn(r"^  var M = \{.*\};$", lambda _: line, html, count=1, flags=re.M)
    assert count == 1, "could not find the `var M = {...};` line in demo.html"
    io.open(DEMO_PATH, "w", encoding="utf-8", newline="").write(updated)

    print(f"Updated {DEMO_PATH}")
    print(f"  intercept : {blob['intercept']:,.4f}")
    for f, c in blob["coef"].items():
        print(f"  {f:<15} {c:>12,.4f}")
    t = blob["metrics"]["test"]
    print(f"  test R2   : {t['r2']:.16f}")
    print(f"  test RMSE : {t['rmse']:,.2f}")
    print(f"  test MAE  : {t['mae']:,.2f}")


if __name__ == "__main__":
    main()
