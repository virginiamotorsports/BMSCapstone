import matplotlib.pyplot as plt
import numpy as np
import pandas as pd



dataframe = pd.read_csv("./GUI/Charging Data.csv")

fig, ax = plt.subplots(1,1)
fig.set_figwidth(10)
fig.set_figheight(5)

for item in range(16):
    # print(dataframe[f'Cell{item + 1}'])
    ax.plot(dataframe[f'Cell{item + 1}'])

ax.
    
fig.show()
plt.show()