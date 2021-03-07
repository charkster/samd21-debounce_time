# samd21-debounce_time
The rising and falling debounce duration between two pins is measured. Maximum debounce duration is 4 seconds rising or falling (no limit on how long both signals can be high for). Debounce durations must be longer than 1us, and durations over 2us will have an accuracy of +/- 1 clock (20.7ns).

UF2 file works natively for Seeeduino Xiao (as QT PY uses different pins).
Early signal is on pin marked A7/D7/RX (PB9) and later signal is on pin marked A8/D8/SCK (PA7).

QT-PY UF2 file added. Early signal is on pin marked "RX" (PA07) and later signal is on pin marked "SCK" (PA11).
QT-PY compiled on arduino with "fastest" loop settings, so the latency adjustment is different rise is -45 and fall is -46 (compared to Xiao with -53 and -54).
QT-PY will have about 2 more clock cycles of accuracy for debounce times between 1us and 2us compared to Xiao. Debounce times greater than 2us the accuracy will be identical.
