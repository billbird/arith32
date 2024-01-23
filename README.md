# arith32

A simple C++ arithmetic coder implementation with 8-bit symbols and 32-bit internal precision.

This implementation uses a static placeholder frequency distribution, but could be easily modified to use an adaptive method instead.

Try it out with commands like
```
./arith_compress < some_input_file > encoded_output
./arith_decompress < encoded_output > reconstructed_input_file
diff some_input_file reconstructed_input_file # Should produce no output since files should match exactly
```

The algorithms used in this implementation are discussed in more detail in the videos below.
 - https://www.youtube.com/watch?v=xt3uNibQWlQ
 - https://www.youtube.com/watch?v=EqKbT3QdtOI

This implementation was based on the pseudocode shown at the end of the second video. There were some minor typos in the pseudocode, but they have been fixed in this implementation.