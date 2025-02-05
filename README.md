# Fuzzing libspng
Repository for the fuzzing project of the Software Security course 2024

## Fuzzer tested

We tested with various fuzzers:
- Radamsa
- zzuf
- AFL++

Using also different sanitazers.

More information in the final report. 

## How to run

To run the generic_test.c:
1. Change the macro TEST_TYPE in generic_test.c according to:
    - TEST_TYPE = 0 -> choose always read (decode)
    - TEST_TYPE = 1 -> choose always write (encode)
    - TEST_TYPE = 2 -> random choice between read (decode) and write (encode) (default)
2. Compile using 'make' in the main directory
3. Call the executable './fuzz/generic_test.fuzz' with one argument:
    - To a correct execution of write (encoding), the first 8 char of the filename needs to follow the specific pattern of the filenames in the directory './images' (PNG test images).

    For example:
   - ./fuzz/generic_test.fuzz ./images/basi0g01.png
   - ./fuzz/generic_test.fuzz ./images/basi0g01_284.png
   - ./fuzz/generic_test.fuzz ./images/basi0g01fjsrejf
   - ./fuzz/generic_test.fuzz ./images/s37n3p04.png
  
