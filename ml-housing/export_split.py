"""export_split.py — freeze the train/test split so other languages can reuse it.

`train_test_split(..., random_state=42)` is reproducible in scikit-learn, but
reproducing it *outside* Python would mean reimplementing NumPy's Mersenne
Twister permutation exactly. That is a brittle thing to depend on, and it isn't
what the project is demonstrating.

So the split is exported once, here, and committed. The C++ implementation reads
these indices and therefore trains on precisely the same rows — which is what
lets the two produce the same model rather than two similar ones.

Usage:
    python export_split.py            # writes data/split_indices.txt
"""

import os

import pandas as pd
from sklearn.model_selection import train_test_split

BASE = os.path.dirname(os.path.abspath(__file__))
CSV_PATH = os.path.join(BASE, "data", "housing.csv")
OUT_PATH = os.path.join(BASE, "data", "split_indices.txt")


def main():
    df = pd.read_csv(CSV_PATH)
    indices = list(range(len(df)))

    # Same call, same random_state, as house_price_regression.py.
    train_idx, test_idx = train_test_split(indices, test_size=0.2, random_state=42)

    with open(OUT_PATH, "w", encoding="utf-8") as fh:
        fh.write("# Row indices (0-based, excluding the CSV header) for the 80/20 split\n")
        fh.write("# produced by train_test_split(test_size=0.2, random_state=42).\n")
        fh.write("# Regenerate with: python export_split.py\n")
        fh.write(f"train {len(train_idx)}\n")
        fh.write(" ".join(str(i) for i in train_idx) + "\n")
        fh.write(f"test {len(test_idx)}\n")
        fh.write(" ".join(str(i) for i in test_idx) + "\n")

    print(f"Wrote {OUT_PATH}")
    print(f"  train rows: {len(train_idx)}")
    print(f"  test rows:  {len(test_idx)}")
    print(f"  first 5 train indices: {train_idx[:5]}")
    print(f"  first 5 test indices:  {test_idx[:5]}")


if __name__ == "__main__":
    main()
