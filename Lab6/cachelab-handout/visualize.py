#!/bin/python3.10

import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D
import numpy as np

H = np.array([[0 for x in range(7)] for y in range(7)])
M = np.array([[0 for x in range(7)] for y in range(7)])
S = np.array([[0 for x in range(7)] for y in range(7)])

with open("log.txt", "r") as f:
    while X := f.readline().split():
        R, C = int(np.log2(int(X[0]))), int(np.log2(int(X[1])))
        Hit, Miss, Evict = int(X[-3][5:-1]), int(X[-2][7:-1]), int(X[-1][10:])

        H[R][C] = Hit
        M[R][C] = Miss
        S[R][C] = Evict


R = np.arange(0, 7, 1)
C = np.arange(0, 7, 1)
R, C = np.meshgrid(R, C)


fig = plt.figure()
ax = fig.add_subplot(131, projection="3d")
ax.plot_surface(R, C, H, cmap="Spectral")
ax.set_xlabel("log2(R)")
ax.set_ylabel("log2(C)")
ax.set_zlabel("Hit Count")
plt.title("# Hit vs. Subblock size")

ax = fig.add_subplot(132, projection="3d")
ax.plot_surface(R, C, M, cmap="Spectral")
ax.set_xlabel("log2(R)")
ax.set_ylabel("log2(C)")
ax.set_zlabel("Miss Count")
plt.title("# Miss vs. Subblock size")

ax = fig.add_subplot(133, projection="3d")
ax.plot_surface(R, C, S, cmap="Spectral")
ax.set_xlabel("log2(R)")
ax.set_ylabel("log2(C)")
ax.set_zlabel("Eviction Count")
plt.title("# Evict vs. Subblock size")

plt.show()