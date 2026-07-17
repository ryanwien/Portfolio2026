# House Price Prediction — Supervised ML Regression

A linear regression project that predicts house prices from property
features, applying the core concepts from Andrew Ng's *Supervised Machine
Learning: Regression and Classification* course to a fresh dataset.

**▶ [Live demo](https://ryanwien.github.io/Portfolio2026/ml-housing/demo.html)** — an interactive predictor running the real trained model in your browser: drag the property features and watch the price update, with model metrics and an actual-vs-predicted plot.

![House Price Prediction demo](screenshot.png)

## What it does

Given six features of a house — square footage, bedrooms, bathrooms, age,
garage spaces, and a neighborhood desirability score — the model predicts its
sale price. On held-out test data it explains **94% of the variance in price
(R² = 0.94)**, with a typical error around $19,000.

## Concepts demonstrated

This project walks through the standard supervised-learning workflow taught in
the course:

1. **Data exploration** — inspecting shape, ranges, and feature/target
   correlations to understand the problem before modeling.
2. **Train/test split** — holding out 20% of the data so performance is
   measured on examples the model never saw during training.
3. **Feature scaling (standardization)** — putting every feature on the same
   scale so gradient descent converges efficiently. Without scaling, large-scale
   features like square footage would dominate small-scale ones like bedroom
   count.
4. **Gradient descent** — training a linear model by iteratively minimizing
   mean squared error (`SGDRegressor`).
5. **Closed-form baseline** — solving the same problem with the normal equation
   (`LinearRegression`) to confirm gradient descent converged to the right
   answer. Both reach an identical R² of 0.94, which validates the training.
6. **Evaluation** — reporting RMSE, MAE, and R², the standard regression
   metrics.
7. **Interpretation** — because features are standardized, the learned weights
   are directly comparable, revealing which features drive price most
   (square footage and location, as expected).

## Running it

```bash
pip install -r requirements.txt
python house_price_regression.py
```

## Sample output

```
LEARNED FEATURE WEIGHTS (on standardized features)
  sqft                    +86,179
  location_score          +37,966
  age                     -18,898
  bedrooms                +10,582
  bathrooms                +9,764
  garage                   +5,267
```

The model correctly learns that square footage has the largest positive effect
on price and that age has a negative effect — matching real-world intuition.

## About the dataset

`data/housing.csv` is a synthetic dataset of 2,000 houses, generated with known
underlying relationships plus realistic noise. Using synthetic data keeps the
project fully reproducible (no external downloads) while still exercising the
complete regression workflow. The generation logic could be swapped for any
real CSV with a `price` column and numeric features.

## Possible next steps

- Add polynomial features to capture non-linear relationships
- Regularization (Ridge / Lasso) to study overfitting
- Cross-validation for more robust performance estimates
- A classification variant (e.g. predicting price *tier* instead of exact price)

## Tech stack

Python · scikit-learn · pandas · NumPy
