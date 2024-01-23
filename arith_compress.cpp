/* arith_compress.cpp

   B. Bird - 2023-07-08
*/

#include <iostream>
#include <array>
#include <string>
#include <cassert>
#include <cstdint>
#include "output_stream.hpp"

const u32 EOF_SYMBOL = 256;


int main(int argc, char** argv){

    OutputBitStream stream{std::cout};

    //Create a static frequency table with a frequency of 1 for 
    //all symbols except lowercase/uppercase letters (symbols 65-122)

    std::array<u32, EOF_SYMBOL+1> frequencies {};
    frequencies.fill(1);

    //Set the frequencies of letters (65 - 122) to 2 
    for(unsigned int i = 65; i <= 122; i++)
        frequencies.at(i) = 2;
    
    //Now set the frequencies of uppercase/lowercase vowels to 4 
    std::string vowels{"AEIOUaeiou"};
    for(unsigned char c: vowels)
        frequencies.at(c) = 4;


    //Now compute cumulative frequencies for each symbol.
    //We actually want the range [CF_low,CF_high] for each symbol,
    //but since CF_low(i) = CF_high(i-1), we only really have to compute
    //the array of lower bounds.

    //The cumulative frequency range for each symbol i will be 
    //[ CF_low.at(i), CF_low.at(i+1) ) 
    //(note that it's a half-open interval)
    std::array<u64, EOF_SYMBOL+2> CF_low {};
    CF_low.at(0) = 0;
    for (unsigned int i = 1; i < EOF_SYMBOL+2; i++){
        CF_low.at(i) = CF_low.at(i-1) + frequencies.at(i-1);
    }

    //We also need to know the global cumulative frequency (of all 
    //symbols), which will be the denominator of a formula below.
    //It turns out this value is already stored as CF_low.at(max_symbol+1)
    u64 global_cumulative_frequency = CF_low.at(EOF_SYMBOL+1);

    assert(global_cumulative_frequency <= 0xffffffff); //If this fails, frequencies must be scaled down


    u32 lower_bound = 0;  //Bit sequence of all zeros
    u32 upper_bound = ~0; //Bit sequence of all ones

    int underflow_counter = 0;

    while(1){
        char raw_char;
        u32 symbol;
        if (std::cin.get(raw_char)){
            symbol = (u32)(unsigned char)raw_char; //The double cast is necessary to avoid a sign extension issue when widening char to u32
        }else{
            //If we couldn't retrieve a character, set the next symbol
            //to the EOF marker 
            symbol = EOF_SYMBOL;
        }

        //For safety, we will use u64 for all of our intermediate calculations.
        u64 current_range = ((u64)upper_bound + 1) - (u64)lower_bound;
        u64 symbol_range_low = CF_low.at(symbol);
        u64 symbol_range_high = CF_low.at(symbol+1);
        upper_bound = lower_bound + (current_range*symbol_range_high)/global_cumulative_frequency - 1;
        lower_bound = lower_bound + (current_range*symbol_range_low)/global_cumulative_frequency;

        // <-- This is probably where we would adjust the frequency table if we used an adaptive model.

        //Now determine if lower_bound and upper_bound share any of their most significant bits and push
        //them to the output stream if so.

        while(1){
            //Check if most significant bits (bit index 31) match.
            if ((upper_bound>>31) == (lower_bound>>31)){ 
                //Push the most significant bit of upper/lower
                u32 b = (upper_bound>>31);
                stream.push_bit(b);
                //Now push underflow_counter copies of the opposite bit
                for(int i = 0; i < underflow_counter; i++){
                    stream.push_bit(!b);
                }
                underflow_counter = 0;

                //Shift out the MSB of upper_bound (and shift in a 1 from the right)
                upper_bound <<= 1;
                upper_bound |= 1;

                //Shift out the MSB of lower_bound (and allow a 0 to be shifted in from the right)
                lower_bound <<= 1;
                
            }else if ( ((lower_bound>>30)&0x1) == 1 && ((upper_bound>>30)&0x1) == 0){
                //If the MSBs didn't match, then the MSB of upper_bound must be 1 and
                //the MSB of lower_bound must be 0.
                //If we discover that lower_bound = 01... and upper_bound = 10... 
                //(which is what the if-statement above tests), then we have
                //to account for underflow.

                underflow_counter++;

                //If upper_bound = 10(xyz...), set upper_bound = 1(xyz...)
                //(that is, splice out the second-most-significant bit)
                upper_bound <<= 1;
                upper_bound |= (1U<<31);
                upper_bound |= 1;

                //If lower_bound = 01(abc...), set lower_bound = 0(abd...)
                lower_bound <<= 1;
                lower_bound &= (1U<<31) - 1; //i.e. 0x7fffffff

            }else{
                break;
            }
        }

        if (symbol == EOF_SYMBOL)
            break; //If we just wrote the EOF symbol, we're done
    }

    //When encoding is finished, we need to dump out just enough of the remaining
    //bits that the decompressor can keep up with us.
    //At this point,
    //   upper = 1...
    //   lower = 0... 
    // (since if the MSBs matched they would have been shifted out during the loop above)
    //Therefore, the string 0111... (with an infinite string of 1's) will be in the range
    //[lower,upper).
    //We can rig the decompressor to duplicate the last bit in the stream infinitely 
    //when the end of the stream is reached, so all we have to do is emit the
    //sequence 01... followed by enough extra one bits to pad out the last byte of
    //the stream.

    //Note that this trick doesn't work if you have other data past the end of 
    //the encoded stream in the file (since the decompressor uses the EOF signal
    //to achieve this trick). Instead, if you want to have something in the file
    //after the encoded stream, you will likely have to follow the bits 01 with
    //a few bytes of all ones (i.e. 0xff), or indicate to the decompressor in advance
    //that the stream is going to end (e.g. with a block size value).
    stream.push_bit(0);
    stream.push_bit(1);
    stream.flush_to_byte(1); //Emit enough 1s to fill out the byte
    

    return 0;
}