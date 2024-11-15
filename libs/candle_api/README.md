# CANDLE_API
The source used to compile candle_api.dll is available at [https://github.com/BennyEvans/candle_dll](https://github.com/BennyEvans/candle_dll) commit [f9ee85e] (https://github.com/BennyEvans/candle_dll/commit/f9ee85e830be93c1d76a5102a8cb4f79e2719259)

It was compiled with the following commands:
```
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -B ./build .
cmake --build ./build
```