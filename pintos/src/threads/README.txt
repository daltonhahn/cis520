Changes made to provide alarm-mega functionality:
  pintos/src/tests/threads/Make.tests: Added an entry after alarm-multiple for alarm-mega.
  pintos/src/tests/threads/tests.c: Added {"alarm-mega", test_alarm_mega} following the entry for alarm-multiple to correspond to the function for test_alarm_mega.
  pintos/src/tests/threads/alarm-wait.c: Added test_alarm_mega function to the file for implementation of the alarm-mega function.
  pintos/src/tests/threads/tests.h: Added extern test_func test_alarm_mega definition for the implementation of the test_alarm_mega function within tests.c file.


  Added pintos/src/tests/threads/alarm-mega.ck in order to run 70 tests
