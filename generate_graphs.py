
import pandas as pd
import matplotlib.pyplot as plt

data = pd.read_csv("results.csv")


# ============================================================
# GRAPH 1
# ============================================================

plt.figure(figsize=(8, 5))

plt.plot(
    data["DictionaryLimit"],
    data["CompressedSize"],
    marker="o"
)

plt.xlabel("Dictionary Limit (Entries)")
plt.ylabel("Compressed File Size (Bytes)")
plt.title("Compressed File Size vs Dictionary Limit")

plt.grid(True)

plt.tight_layout()

plt.savefig(
    "Graph1_Compressed_File_Size.png",
    dpi=300
)

plt.close()


# ============================================================
# GRAPH 2
# ============================================================

plt.figure(figsize=(8, 5))

plt.plot(
    data["DictionaryLimit"],
    data["SpaceSaving"],
    marker="o"
)

plt.xlabel("Dictionary Limit (Entries)")
plt.ylabel("Space Saving (%)")
plt.title("Space Saving vs Dictionary Limit")

plt.grid(True)

plt.tight_layout()

plt.savefig(
    "Graph2_Space_Saving.png",
    dpi=300
)

plt.close()


# ============================================================
# GRAPH 3
# ============================================================

plt.figure(figsize=(8, 5))

plt.plot(
    data["DictionaryLimit"],
    data["CompressionTime"],
    marker="o"
)

plt.xlabel("Dictionary Limit (Entries)")
plt.ylabel("Compression Time (seconds)")
plt.title("Compression Time vs Dictionary Limit")

plt.grid(True)

plt.tight_layout()

plt.savefig(
    "Graph3_Compression_Time.png",
    dpi=300
)

plt.close()


print()
print("===============================================")
print("             GRAPHS GENERATED")
print("===============================================")
print()
print("1. Graph1_Compressed_File_Size.png")
print("2. Graph2_Space_Saving.png")
print("3. Graph3_Compression_Time.png")
print()
