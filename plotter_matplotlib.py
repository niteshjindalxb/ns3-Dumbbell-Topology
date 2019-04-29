import matplotlib.pyplot as plt
import os
import numpy as np
import shutil
import time

path = os.getcwd() + '/DataFiles/'
save_path = os.getcwd() + '/Plots/'

# Remove save_path, and create a new directory for plots.
if os.path.exists(save_path):
    shutil.rmtree(save_path)
    time.sleep(.1)

os.mkdir(save_path)

# The TCP types analysed.
tcp_types = ['data_hybla_a', 'data_westwood_a', 'data_yeah_a']

# Congestion windows.
cw_files = [tcp_type + '.cw' for tcp_type in tcp_types]
for filename in cw_files:
    # Read file.
    with open(path + filename, 'r') as f:
        lines = f.readlines()

    # Split whitespace and cast to float.
    lines = np.array([list(map(float, line.split())) for line in lines])
    timesteps = lines[:, 0]
    cw_values = lines[:, 1]

    plt.xlabel('Time Steps (ns)')
    plt.ylabel('Congestion Window (bits)')
    plt.title('TCP ' + filename[5:-5].capitalize() + ' Congestion Window Graph')
    plt.plot(timesteps, cw_values, 'r')
    plt.savefig(save_path + filename + '.png')
    plt.show()

# Goodput.
gp_files = [tcp_type + '.gp' for tcp_type in tcp_types]
for filename in gp_files:
    # Read file.
    with open(path + filename, 'r') as f:
        lines = f.readlines()

    # Split whitespace and cast to float.
    lines = np.array([list(map(float, line.split())) for line in lines])
    timesteps = lines[:, 0]
    cw_values = lines[:, 1]

    plt.xlabel('Time Steps (ns)')
    plt.ylabel('Goodput (kbps)')
    plt.title('TCP ' + filename[5:-5].capitalize() + ' Goodput Graph')
    plt.plot(timesteps, cw_values, 'r')
    plt.savefig(save_path + filename + '.png')
    plt.show()

# Throughput.
tp_files = [tcp_type + '.tp' for tcp_type in tcp_types]
for filename in tp_files:
    # Read file.
    with open(path + filename, 'r') as f:
        lines = f.readlines()

    # Split whitespace and cast to float.
    lines = np.array([list(map(float, line.split())) for line in lines])
    timesteps = lines[:, 0]
    cw_values = lines[:, 1]

    plt.xlabel('Time Steps (ns)')
    plt.ylabel('Throughput (kbps)')
    plt.title('TCP ' + filename[5:-5].capitalize() + ' Throughput Graph')
    plt.plot(timesteps, cw_values, 'r')
    plt.savefig(save_path + filename + '.png')
    plt.show()
