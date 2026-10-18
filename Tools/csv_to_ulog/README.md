# csv_to_ulog

Tool for converting CSV flight logs to ULog format so they can be analyzed using [FlightPlot](https://github.com/PX4/FlightPlot) software.

To build, go to the `<flix>/tools/csv_to_ulog` directory and run:

```bash
mkdir build
cd build
cmake ..
make
```

Convert a CSV file to ULog:

```bash
./csv_to_ulog log_file.csv
```

ULog file will be saved in the same directory.
