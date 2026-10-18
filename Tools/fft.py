#!/usr/bin/env python3

"""Show Fast Fourier Transform plot for log entry.

Usage:
  fft.py <csv_log_file> <log_entry>
"""

import docopt
import csv
import numpy as np
import matplotlib.pyplot as plt

args = docopt.docopt(__doc__)
log_file = args['<csv_log_file>']
log_entry = args['<log_entry>']

csv_reader = csv.reader(open(log_file, 'r'), delimiter=',')
header = next(csv_reader)
log_entry_index = header.index(log_entry)

data = [[float(value) for value in row] for row in csv_reader]
data = sorted(data, key=lambda row: row[0])
records = [row[log_entry_index] for row in data]
duration = data[-1][0] - data[0][0]
sample_rate = len(data) / duration

print('Duration: ', duration)
print('Mean sample rate: ', sample_rate)
print('Mean dt: ', 1 / sample_rate)

N = int(sample_rate * duration)

yf = np.fft.rfft(records)
xf = np.fft.rfftfreq(N, 1 / sample_rate)

# print out fft data
print('Frequency, Amplitude')
for i in range(len(xf)):
    print(xf[i], ',', abs(yf[i]))

plt.plot(xf, np.abs(yf))
plt.title('FFT of ' + log_entry)
plt.xlabel('Frequency')
plt.ylabel('Amplitude')
plt.grid(True, which='both', color='#eeeeee')
plt.minorticks_on()
plt.get_current_fig_manager().set_window_title(log_entry)
plt.show()
