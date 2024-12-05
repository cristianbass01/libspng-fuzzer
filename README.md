# SS-libspng-fuzz
Repository for the fuzzing project of the Software Security course 2024

To run the generic_test.c:
- Change the macro TEST_TYPE in generic_test.c according to:
    TEST_TYPE = 0 -> choose always read (decode)
    TEST_TYPE = 1 -> choose always write (encode)
    TEST_TYPE = 2 -> random choice between read (decode) and write (encode) (default)
- Compile using 'make' in the main directory
- Call the executable './fuzz/generic_test.fuzz' with one argument:
  To a correct execution of write (encoding), the first 8 char of the filename needs to follow the specific pattern of the filenames in the directory './images' (PNG test images).

  For example: 
  - ./fuzz/generic_test.fuzz ./images/basi0g01.png
  - ./fuzz/generic_test.fuzz ./images/basi0g01_284.png
  - ./fuzz/generic_test.fuzz ./images/basi0g01fjsrejf
  - ./fuzz/generic_test.fuzz ./images/s37n3p04.png