 ```
 > cd .pio/libdeps/esp32dev/FlixPeriph/
```

change `MPU9250.h`:
```
  static constexpr int32_t SPI_CFG_CLOCK_ = 1000000;
//  static constexpr int32_t SPI_READ_CLOCK_ = 15000000;
  static constexpr int32_t SPI_READ_CLOCK_ = 1000000;   // было 15000000
```
